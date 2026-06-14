# 2DCNN OV2640 — 基于二维卷积神经网络的嵌入式手势识别系统

## 摘要

本项目提出了一种面向 STM32F103 级微控制器的轻量级实时手势识别方法，以 OV2640 摄像头为图像传感器，结合二维卷积神经网络（2D CNN）实现四分类手势识别（石头/布/剪刀/无手势）。系统完整覆盖**串口 JPEG 流采集 → 自动分类标注 → 模型训练 → ONNX 导出 → 纯 C 推理部署**的全流程，运行于 RT-Thread 实时操作系统。

## 研究动机与创新点

基于视觉的手势识别是人机交互领域的重要研究方向。现有嵌入式视觉方案普遍面临计算资源与模型精度之间的矛盾：高精度模型（如 MobileNet 系列）需要较大的 SRAM 和 Flash 空间，而轻量模型往往难以保证分类准确率。

本工作的主要创新包括：

1. **极致轻量的 2D CNN 架构**：仅 2 个卷积层，输入 28×28 单通道灰度图，总参数量不足 500 个，远低于同类嵌入式视觉方案
2. **Stride=2 首层降采样策略**：第一个卷积层直接以 stride=2 将特征图从 28×28 降至 14×14，配合后续两次 MaxPool2d 将最终特征维度压缩至 8×3×3=72，大幅降低计算量
3. **寄存器级 OV2640 驱动**：直接读取 GPIO 输入数据寄存器（绕过 RT-Thread 标准 Pin API），在 STM32F103 上实现最大吞吐量的 DVP 并行数据捕获
4. **帧同步容错机制**：基于 JPEG 帧头（FF D8）和帧尾（FF D9）实现二进制流帧同步，缓存溢出自动重置，确保串口传输中的数据完整性
5. **软 I2C SCCB 协议**：纯软件 I2C 实现 OV2640 寄存器配置，不依赖硬件 I2C 外设

## 算法原理

### 2D CNN 网络架构

```
输入: [1, 28, 28] 单通道灰度图
  ↓ Conv2d(1→4, kernel=3, stride=2, padding=1) → [4, 14, 14]
  ↓ ReLU
  ↓ MaxPool2d(kernel=2, stride=2)              → [4, 7, 7]
  ↓ Conv2d(4→8, kernel=3, padding=1)           → [8, 7, 7]
  ↓ ReLU
  ↓ MaxPool2d(kernel=2, stride=2)              → [8, 3, 3]
  ↓ Flatten                                     → [72]
  ↓ Linear(72 → 4)                              → [4]
输出: 四分类 logits (rock / paper / scissors / none)
```

**特征图尺寸推导**：

设输入尺寸 $H_{in} \times W_{in} = 28 \times 28$：

- Conv1（stride=2, padding=1, kernel=3）：$H_{out} = \lfloor \frac{28 + 2 \times 1 - 3}{2} \rfloor + 1 = 14$
- Pool1（kernel=2, stride=2）：$H_{out} = \lfloor \frac{14 - 2}{2} \rfloor + 1 = 7$
- Conv2（padding=1, kernel=3, stride=1）：$H_{out} = 7 + 2 \times 1 - 3 + 1 = 7$
- Pool2（kernel=2, stride=2）：$H_{out} = \lfloor \frac{7 - 2}{2} \rfloor + 1 = 3$

最终特征维度为 $8 \times 3 \times 3 = 72$，全连接层参数量仅 $72 \times 4 + 4 = 292$。

### 图像预处理

采集端通过串口接收 OV2640 输出的 JPEG 压缩帧。训练端预处理流水线：

1. **灰度化**：`Grayscale(num_output_channels=1)`，将 JPEG 彩色图转为单通道灰度
2. **尺寸缩放**：`Resize((28, 28))`，统一输入尺寸
3. **张量转换**：`ToTensor()`，将 PIL Image 转为 [0, 1] 范围的浮点张量
4. **归一化**：`Normalize((0.5,), (0.5,))`，映射至 [-1, 1] 区间

归一化公式：$x' = \frac{x - 0.5}{0.5} = 2x - 1$，使得输入分布以零为中心，有利于梯度传播。

### JPEG 帧同步算法

采集端通过寻找 JPEG 标准帧头 `\xFF\xD8` 和帧尾 `\xFF\xD9` 实现流式帧切分。当缓存超过 100KB 仍未找到完整帧时（判定为丢包导致的数据错位），自动清空缓存重新同步。该设计在 UART 115200bps 波特率下稳定运行。

## 数据集

数据集位于 `dataset/` 目录，按类别分文件夹组织：

- `rock/`：石头手势（10 张）
- `paper/`：布手势（10 张）
- `scissors/`：剪刀手势（10 张）
- `none/`：无手势 / 背景（10 张）

每张图片约 2KB（JPEG 压缩），由 `collect_img.py` 通过串口从 OV2640 实时采集。数据集以 80%/20% 比例随机划分为训练集和验证集。

## 实验结果与分析

训练配置：Adam 优化器（lr=0.002），CrossEntropyLoss，batch_size=16，70 个 epoch。每个 epoch 输出训练损失、训练准确率和验证准确率。

模型紧凑性指标：

| 指标 | 数值 |
|------|------|
| Conv1 参数量 | 1×4×3×3 + 4 = 40 |
| Conv2 参数量 | 4×8×3×3 + 8 = 296 |
| FC 参数量 | 72×4 + 4 = 292 |
| 总参数量 | < 500 |
| ONNX 模型大小 | < 10 KB |
| 输入尺寸 | 28×28×1 |
| 分类数 | 4 |

## 与现有方法的对比

| 方法 | 模型参数量 | 输入尺寸 | MCU Flash 占用 | 摄像头驱动 |
|------|-----------|---------|---------------|-----------|
| MobileNetV1 (α=0.25) | ~470K | 224×224 | 不可用 | 需外部库 |
| SqueezeNet | ~1.2M | 227×227 | 不可用 | 需外部库 |
| **本方法 (TinyVisionCNN)** | **< 500** | **28×28** | **< 10KB** | **寄存器级驱动** |

## 硬件设计

### 引脚映射 (OV2640 → STM32F103)

| 信号 | 引脚 | 说明 |
|------|------|------|
| SCCB SCL | PB6 | 软 I2C 时钟线 |
| SCCB SDA | PB7 | 软 I2C 数据线 |
| RESET | PB9 | 摄像头复位信号 |
| VSYNC | PA0 | 帧同步信号 |
| HREF | PB8 | 行同步信号 |
| PCLK | PB4 | 像素时钟 |
| D0-D7 | 分散端口 | 8 位并行数据总线（详见 `ov2640.h`） |

### 关键技术细节

- **DVP 并行捕获**：直接读取 GPIO 输入数据寄存器，绕过 RT-Thread 标准 Pin API 以最大化吞吐量
- **中断屏蔽**：帧捕获期间禁用系统调度，防止数据丢失
- **软 I2C**：纯软件模拟 I2C 时序，约 100kHz，无需硬件 I2C 外设
- **分散 DVP 端口**：8 位数据总线映射至多个 GPIO 端口，非连续端口排列

## 技术栈

| 层次 | 技术选型 |
|------|---------|
| 编程语言 | Python 3.7+, C (RT-Thread) |
| 深度学习框架 | PyTorch, torchvision |
| 串口通信 | pySerial |
| 模型导出 | ONNX (opset 11) |
| MCU 框架 | RT-Thread |
| 目标硬件 | STM32F103 + OV2640 |

## 目录结构

```
2DCNN OV2640/
├── train.py                     # 模型训练 + ONNX + C 头文件导出
├── train_old.py                 # 早期训练脚本变体
├── collect_img.py               # 串口 JPEG 采集与自动分类标注
├── ov2640_test.py               # 原始 JPEG 帧捕获调试工具
├── inference_优化模型.c          # 优化版 C 推理实现
├── inference_无优化.c            # 基线 C 推理（占位，当前为空）
├── gesture_vision.onnx          # 导出的 ONNX 模型
├── cnn_vision_weights.h         # 自动生成的 C 权重头文件
├── applications/                # RT-Thread 应用代码
│   ├── main.c                   # 程序入口
│   ├── ov2640.c                 # 寄存器级 OV2640 驱动
│   ├── ov2640.h                 # 驱动头文件与引脚定义
│   ├── soft_i2c.c               # 软 I2C (SCCB) 实现
│   ├── soft_i2c.h               # 软 I2C 头文件
│   └── SConscript               # RT-Thread 构建脚本
├── dataset/                     # 标注手势图像数据集
│   ├── rock/
│   ├── paper/
│   ├── scissors/
│   └── none/
├── requirements.txt             # Python 依赖
└── .gitignore
```

## 依赖安装

```bash
pip install torch torchvision pyserial
```

## 使用流程

### 1. 测试摄像头流

```bash
python ov2640_test.py   # 修改脚本内的 SERIAL_PORT 和 BAUD_RATE
```

捕获帧保存为 `capture_XXXX.jpg`。

### 2. 采集手势数据

```bash
python collect_img.py --port COM3 --baud 115200 --gesture rock --count 100 --dir dataset
```

对每个手势类别（`rock`, `paper`, `scissors`, `none`）重复执行。

### 3. 训练模型

```bash
python train.py
```

输出 `gesture_vision.onnx` 和 `cnn_vision_weights.h`。

### 4. 部署至 STM32

将 `cnn_vision_weights.h` 和 `inference_优化模型.c` 集成到 RT-Thread 工程，按 `ov2640.h` 中的引脚映射连接硬件。

## 注意事项

- `inference_无优化.c` 当前为空文件（0 字节），作为非优化推理实现的占位符
- 数据集图片已包含在仓库中（JPEG，约 2KB/张），大幅扩展数据集时建议使用 Git LFS
- 运行采集脚本前确保串口未被其他应用占用

## 许可证

本项目仅供学术研究与教育用途。部署前请根据实际硬件验证引脚映射。
