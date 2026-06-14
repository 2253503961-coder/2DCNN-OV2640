#ifndef _SOFT_I2C_H
#define _SOFT_I2C_H

#include <rtthread.h>
#include <rtdevice.h>

#define SOFT_I2C_WR	0		/* 写控制bit */
#define SOFT_I2C_RD	1		/* 读控制bit */

typedef struct I2C{
    struct I2C* self;
    rt_base_t       scl_pin;
    rt_base_t       sda_pin;
    uint8_t         sccb;   /* 1:sccb协议, 0:i2c协议 */
    
    /** 从I2C从设备中读取数据 */
    uint8_t (*read_byte)(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr);
    
    /** 向I2C从设备写入数据 */
    uint8_t (*write_byte)(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t data);
    
    /** 从I2C从设备中读取一组数据 */
    uint8_t (*read_array)(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t *data, uint16_t len);
    
    /** 从I2C从设备中写入一组数据 */
    uint8_t (*write_array)(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t *data, uint16_t len);
    
    /** 发送i2c起始信号 */
    void (*i2c_start)(struct I2C* i2c);
    
    /** 发送i2c结束信号 */
    void (*i2c_stop)(struct I2C* i2c);
    
    /** 发送数据 */
    void (*i2c_sendbyte)(struct I2C* i2c, uint8_t byte);
    
    /** 接收数据 */
    uint8_t (*i2c_readbyte)(struct I2C* i2c);
    
    /** 发送ACK响应 */
    void (*i2c_ack)(struct I2C* i2c);
    
    /** 接收ACK响应 */
    uint8_t (*i2c_waitack)(struct I2C* i2c);
    
    /** 发送NACK响应 */
    void (*i2c_nack)(struct I2C* i2c);
    
    /** sccb协议使能 */
    void (*set_i2c_sccb)(struct I2C* i2c, int8_t enable);
    
}I2C;

int8_t i2c_init(I2C *i2c, rt_base_t scl_pin, rt_base_t sda_pin);
void i2c_uninit(I2C *i2c);

#endif