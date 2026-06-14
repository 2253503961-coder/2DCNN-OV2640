#ifndef _OV2640_H
#define _OV2640_H

#include <rtthread.h>
#include <rtdevice.h>
#include "stm32f1xx.h"  // 必须引入以操作直接寄存器
#include "soft_i2c.h"

#define OV2640_ADDR             0x60            
#define OV2640_MID              0x7FA2
#define OV2640_PID              0x2642

// 软件I2C和控制引脚配置 (用于 rt_pin_xxx)
#define OV2640_PIN_RESET        GET_PIN(B, 9)
#define OV2640_PIN_PWDN         -1              
#define OV2640_PIN_SCL          GET_PIN(B, 6)
#define OV2640_PIN_SDA          GET_PIN(B, 7)

// =========================================================================
// ?? 极速读取宏定义：替代原有的 rt_pin_read，速度提升上百倍，保证不漏时钟！
// =========================================================================
#define OV2640_VSYNC  ((GPIOA->IDR & GPIO_PIN_0) != 0)
#define OV2640_HREF   ((GPIOB->IDR & GPIO_PIN_8) != 0)
#define OV2640_PCLK   ((GPIOB->IDR & GPIO_PIN_4) != 0)

#define OV2640_DATA \
    (uint8_t)( (((GPIOB->IDR & GPIO_PIN_3)  != 0) ? 0x01 : 0) | \
               (((GPIOC->IDR & GPIO_PIN_7)  != 0) ? 0x02 : 0) | \
               (((GPIOC->IDR & GPIO_PIN_13) != 0) ? 0x04 : 0) | \
               (((GPIOB->IDR & GPIO_PIN_10) != 0) ? 0x08 : 0) | \
               (((GPIOB->IDR & GPIO_PIN_11) != 0) ? 0x10 : 0) | \
               (((GPIOB->IDR & GPIO_PIN_13) != 0) ? 0x20 : 0) | \
               (((GPIOB->IDR & GPIO_PIN_14) != 0) ? 0x40 : 0) | \
               (((GPIOB->IDR & GPIO_PIN_15) != 0) ? 0x80 : 0) )
// =========================================================================

typedef enum {
  BMP_QQVGA             =   0x00,        
  BMP_QVGA              =   0x01,        
  JPEG_160x120          =   0x02,        
  JPEG_176x144          =   0x03,        
  JPEG_320x240          =   0x04,        
  JPEG_352x288          =   0x05,        
  JPEG_800x600          =   0x06,        
  JPEG_1600x1200        =   0x07         
} IMAGE_FORMAT;


typedef struct _OV2640 {
    I2C             i2c;
    uint8_t (*init)(IMAGE_FORMAT format);
    void (*power_on)(void);
    void (*hw_reset)(void);
    void (*sw_reset)(void);
    uint8_t (*read_id)(void);
    void (*uninit)(void);
    void (*set_jpeg_mode)(void);
    void (*set_rgb565_mode)(void);
    void (*set_outsize)(IMAGE_FORMAT format);
    void (*set_speed)(uint8_t clkdiv, uint8_t pclkdiv);
    int32_t (*get_jpg_data)(uint8_t *jpg_data, uint32_t jpg_len);
} OV2640;

extern OV2640 ov2640;

#endif