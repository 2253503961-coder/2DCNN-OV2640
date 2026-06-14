#include <rtthread.h>
#include <rtdevice.h>
#include "stm32f1xx.h"  // 引入寄存器定义
#include <stdio.h>
#include <math.h>
#include "ov2640.h"
#include "cnn_vision_weights.h" // 务必把 Python 重新生成的头文件放在同级目录

/* =========================================================================
 * [1] 全局缓存与内存池定义
 * ========================================================================= */

/* 为 YUV422 160x120 准备的缓存 (160 * 120 * 2 = 38400 字节)。 */
uint8_t ov2640_framebuf[38400]; 
uint8_t gray_28x28_buf[784]; // 专供神经网络的纯净输入数组 (28 * 28 = 784)

// 内存池分配 (Ping-Pong 缓冲，极致节省 SRAM)
// ！！！优化点：最大一层的特征图是输入层，只需要 28x28 = 784 个 float！！！
static float tensor_A[784];
static float tensor_B[784];

// 类别名称映射 (必须和 Python 里打印出来的 class_names 顺序一致)
const char* class_names[] = {"none", "paper", "rock", "scissors"};

/* =========================================================================
 * [2] 纯 C 轻量级前向推理引擎 (TinyVisionCNN) - 已适配 Stride 优化
 * ========================================================================= */

// 1. 带有 Padding=1 的 2D 卷积层 (支持动态 Stride)
static void conv2d_pad_stride(const float* in, float* out, const float* weight, const float* bias, 
                              int in_c, int out_c, int in_h, int in_w, int stride) 
{
    int out_h = in_h / stride;
    int out_w = in_w / stride;

    for (int oc = 0; oc < out_c; oc++) {
        for (int oh = 0; oh < out_h; oh++) {
            for (int ow = 0; ow < out_w; ow++) {
                float sum = bias[oc];
                for (int ic = 0; ic < in_c; ic++) {
                    for (int kh = 0; kh < 3; kh++) {
                        for (int kw = 0; kw < 3; kw++) {
                            int ih = oh * stride + kh - 1; // padding = 1
                            int iw = ow * stride + kw - 1;
                            // 边界检查 (超出边界则视为补零，不加该项)
                            if (ih >= 0 && ih < in_h && iw >= 0 && iw < in_w) {
                                int in_idx = ic * in_h * in_w + ih * in_w + iw;
                                int w_idx = oc * in_c * 9 + ic * 9 + kh * 3 + kw;
                                sum += in[in_idx] * weight[w_idx];
                            }
                        }
                    }
                }
                out[oc * out_h * out_w + oh * out_w + ow] = sum;
            }
        }
    }
}

// 2. ReLU 激活函数
static void relu(float* data, int size) {
    for (int i = 0; i < size; i++) {
        if (data[i] < 0) data[i] = 0.0f;
    }
}

// 3. MaxPool2D (Kernel=2x2, Stride=2)
static void maxpool2d(const float* in, float* out, int c, int in_h, int in_w) {
    int out_h = in_h / 2;
    int out_w = in_w / 2;
    for (int ic = 0; ic < c; ic++) {
        for (int oh = 0; oh < out_h; oh++) {
            for (int ow = 0; ow < out_w; ow++) {
                float max_val = -100000.0f; // 极小值初始化
                for (int kh = 0; kh < 2; kh++) {
                    for (int kw = 0; kw < 2; kw++) {
                        int ih = oh * 2 + kh;
                        int iw = ow * 2 + kw;
                        float val = in[ic * in_h * in_w + ih * in_w + iw];
                        if (val > max_val) max_val = val;
                    }
                }
                out[ic * out_h * out_w + oh * out_w + ow] = max_val;
            }
        }
    }
}

// 4. 全连接层 (Linear)
static void linear(const float* in, float* out, const float* weight, const float* bias, 
                   int in_features, int out_features) 
{
    for(int o = 0; o < out_features; o++){
        float sum = bias[o];
        for(int i = 0; i < in_features; i++){
            // PyTorch 的 Linear 权重在 C 数组中的形状为 [out_features * in_features]
            sum += in[i] * weight[o * in_features + i];
        }
        out[o] = sum;
    }
}

// 5. 推理
static int run_tiny_vision_inference(const uint8_t* raw_gray_28x28, float* confidences) 
{
    // 输入预处理: uint8 (0~255) 转为 float，并归一化到 [-1.0, 1.0] (与训练端对其)
    for (int i = 0; i < 784; i++) {
        tensor_A[i] = (raw_gray_28x28[i] / 255.0f - 0.5f) / 0.5f;
    }

    // Layer 1: Conv1(Stride=2) -> ReLU1 -> Pool1
    // 输入: 28x28 -> Conv1输出: 14x14
    conv2d_pad_stride(tensor_A, tensor_B, conv1_weight, conv1_bias, 1, 4, 28, 28, 2);
    relu(tensor_B, 4 * 14 * 14);
    // Pool1输出: 7x7
    maxpool2d(tensor_B, tensor_A, 4, 14, 14);

    // Layer 2: Conv2(Stride=1) -> ReLU2 -> Pool2
    // 输入: 7x7 -> Conv2输出: 7x7
    conv2d_pad_stride(tensor_A, tensor_B, conv2_weight, conv2_bias, 4, 8, 7, 7, 1);
    relu(tensor_B, 8 * 7 * 7);
    // Pool2输出: 3x3 (7/2向下取整)
    maxpool2d(tensor_B, tensor_A, 8, 7, 7);

    // FC Layer: Flatten(8 * 3 * 3 = 72) -> Linear(4)
    linear(tensor_A, confidences, fc_weight, fc_bias, 72, 4);

    // Argmax 找出得分最高的索引
    int best_class = 0;
    float max_score = confidences[0];
    for (int i = 1; i < 4; i++) {
        if (confidences[i] > max_score) {
            max_score = confidences[i];
            best_class = i;
        }
    }
    
    return best_class; // 返回 0, 1, 2, 3
}

/* =========================================================================
 * [3] 图像预处理
 * ========================================================================= */

static void extract_28x28_grayscale_from_yuv(uint8_t* src_yuv, int src_w, int src_h, uint8_t* dest_28x28)
{
    int dest_w = 28;
    int dest_h = 28;
    
    float scale_x = (float)src_w / dest_w;
    float scale_y = (float)src_h / dest_h;

    for (int y = 0; y < dest_h; y++) {
        for (int x = 0; x < dest_w; x++) {
            // 最近邻插值找原图对应的坐标
            int src_x = (int)(x * scale_x);
            int src_y = (int)(y * scale_y);
            
            // 提取 Y(亮度) 值填入 28x28 数组
            int src_index = (src_y * src_w + src_x) * 2;
            dest_28x28[y * dest_w + x] = src_yuv[src_index];
        }
    }
}

/* =========================================================================
 * [4] 主入口函数
 * ========================================================================= */

int main(void)
{
    uint32_t img_len;
    float confidences[4];

    rt_kprintf("\n============================\n");
    rt_kprintf(" OV2640 AI Vision App Start \n");
    rt_kprintf("============================\n");

    if (ov2640.init(JPEG_160x120)) 
    {
        rt_kprintf("ov2640 init failed.\n");
        while(1) { rt_thread_mdelay(1000); }
    }
    rt_kprintf("ov2640 init success\n");
    
    while (1)
    {
        // 1. 抓取一帧图像 
        img_len = ov2640.get_jpg_data(ov2640_framebuf, sizeof(ov2640_framebuf));
        
        if (img_len > 0) 
        {
            rt_tick_t start_time = rt_tick_get();

            // 2. 图像预处理：降采样并提取单通道灰度
            extract_28x28_grayscale_from_yuv(ov2640_framebuf, 160, 120, gray_28x28_buf);

            // 3. 运行前向神经网络推理！
            int result_idx = run_tiny_vision_inference(gray_28x28_buf, confidences);

            rt_tick_t end_time = rt_tick_get();
            uint32_t cost_time = (end_time - start_time) * (1000 / RT_TICK_PER_SECOND);

            // 4. 打印结果
            rt_kprintf("\n[AI Inference Cost: %d ms]\n", cost_time);
            rt_kprintf(">>> AI Result: [%s] <<<\n", class_names[result_idx]);
            rt_kprintf("Details -> None: %d, Paper: %d, Rock: %d, Scissors: %d\n",
                       (int)(confidences[0] * 100), (int)(confidences[1] * 100), 
                       (int)(confidences[2] * 100), (int)(confidences[3] * 100));
        }
        
        rt_thread_mdelay(10); // 短暂出让 CPU 给系统其他线程
    }

    return 0;
}