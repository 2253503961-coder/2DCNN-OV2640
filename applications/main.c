#include <rtthread.h>
#include <rtdevice.h>
#include "stm32f1xx.h"  // 引入寄存器定义
#include <stdio.h>
#include "ov2640.h"

/* 定义图片缓存区（放在全局数据区/BSS区，避免占用线程栈） */
uint8_t ov2640_framebuf[12 * 1024]; // 稍微扩大到 12KB 保障 160x120 不越界

/* * 纯裸机串口输出JPG数据
 * 绕开 RT-Thread 的串口设备框架，绝对保证数据的二进制纯净度
 */
static void write_jpeg_data_to_uart(uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        USART1->DR = data[i];
        // 0x40 是 TXE (发送数据寄存器为空) 标志位
        while((USART1->SR & 0x40) == 0); 
    }
}

int main(void)
{
    uint32_t jpeg_len;
    

    rt_kprintf("OV2640 Camera App Start...\n");

    // 1. 初始化ov2640，注意一定要用 160x120 以适应内存！
    if (ov2640.init(JPEG_160x120)) 
    {
        rt_kprintf("ov2640 init failed.\n");
        while(1) {
            rt_thread_mdelay(1000);
        }
    }
    rt_kprintf("ov2640 init success\n");
    
    while (1)
    {
        jpeg_len = ov2640.get_jpg_data(ov2640_framebuf, sizeof(ov2640_framebuf));
        
        // 3. 图片数据写入串口
        if (jpeg_len > 0) 
        {
            write_jpeg_data_to_uart(ov2640_framebuf, jpeg_len);
        }
        
        rt_thread_mdelay(5); // 短暂出让CPU给其他线程
    }

    return 0;
}