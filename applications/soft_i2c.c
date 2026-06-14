#include <rtthread.h>
#include <rtdevice.h>
#include "soft_i2c.h"

static uint8_t read_byte(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr);
static uint8_t write_byte(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t data);
static uint8_t read_array(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t *data, uint16_t len);
static uint8_t write_array(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t *data, uint16_t len);

static void i2c_start(struct I2C* i2c);
static void i2c_stop(struct I2C* i2c);
static void i2c_sendbyte(struct I2C* i2c, uint8_t byte);
static uint8_t i2c_readbyte(struct I2C* i2c);
static void i2c_ack(struct I2C* i2c);
static uint8_t i2c_waitack(struct I2C* i2c);
static void i2c_nack(struct I2C* i2c);

static void set_scl_level(I2C *i2c, int8_t level);
static void set_sda_level(I2C *i2c, int8_t level);
static uint8_t get_sda_level(I2C *i2c);
static void i2c_delay(void);

static void set_i2c_sccb(I2C *i2c, int8_t enable);

int8_t i2c_init(I2C *i2c, rt_base_t scl_pin, rt_base_t sda_pin)
{
    i2c->scl_pin    = scl_pin;
    i2c->sda_pin    = sda_pin;
    
    /* 配置引脚功能为开漏输出，RT-Thread内部会自动处理时钟使能 */
    rt_pin_mode(scl_pin, PIN_MODE_OUTPUT_OD);
    rt_pin_mode(sda_pin, PIN_MODE_OUTPUT_OD);
    
    // 初始化i2c功能
    i2c->read_byte      = read_byte;
    i2c->write_byte     = write_byte;
    i2c->read_array     = read_array;
    i2c->write_array    = write_array;
    
    i2c->i2c_start      = i2c_start;
    i2c->i2c_stop       = i2c_stop;
    i2c->i2c_sendbyte   = i2c_sendbyte;
    i2c->i2c_readbyte   = i2c_readbyte;
    i2c->i2c_ack        = i2c_ack;
    i2c->i2c_waitack    = i2c_waitack;
    i2c->i2c_nack       = i2c_nack;
    i2c->set_i2c_sccb   = set_i2c_sccb;
    
    i2c->self           = i2c;
    i2c->i2c_stop(i2c);
    
    /* 默认使用I2C协议 */
    i2c->sccb = 0;
    
    return 0;
}


static uint8_t read_byte(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr)
{
    uint8_t read_data;
    
	i2c->i2c_start(i2c);
    i2c->i2c_sendbyte(i2c, device_addr+SOFT_I2C_WR);
	if (i2c->i2c_waitack(i2c) == 1)return 0;
    
	i2c->i2c_sendbyte(i2c, register_addr);
	if (i2c->i2c_waitack(i2c) == 1)return 0;
    
    if (i2c->sccb) {
        i2c_delay();
        i2c->i2c_stop(i2c);
        i2c_delay();
    }
    
	i2c->i2c_start(i2c);
	i2c->i2c_sendbyte(i2c, device_addr+SOFT_I2C_RD);
	if (i2c->i2c_waitack(i2c) == 1)return 0;
	read_data = i2c->i2c_readbyte(i2c);
    
    i2c->i2c_nack(i2c);
    i2c->i2c_stop(i2c);
    
	return read_data;
}


static uint8_t write_byte(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t data)
{
    i2c->i2c_start(i2c);
	i2c->i2c_sendbyte(i2c, device_addr+SOFT_I2C_WR);
	if (i2c->i2c_waitack(i2c) == 1) {
        rt_kprintf("write error\r\n");
        return 1;
    }
    rt_hw_us_delay(100);
	i2c->i2c_sendbyte(i2c, register_addr); 
	if (i2c->i2c_waitack(i2c) == 1) {
        rt_kprintf("write error\r\n");
        return 1;
    }
	i2c->i2c_sendbyte(i2c, data);
	if (i2c->i2c_waitack(i2c) == 1) {
        rt_kprintf("write error\r\n");
        return 1;
    }
    
	i2c->i2c_stop(i2c);
    
	return 0;
}


static uint8_t read_array(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t *data, uint16_t len)
{
    uint16_t i;
    i2c->i2c_start(i2c);
    i2c->i2c_sendbyte(i2c, device_addr+SOFT_I2C_WR);
    if (i2c->i2c_waitack(i2c) == 1)return 1;
    
    i2c->i2c_sendbyte(i2c, register_addr); 
    if (i2c->i2c_waitack(i2c) == 1)return 1;
    
    i2c->i2c_start(i2c);
    i2c->i2c_sendbyte(i2c, device_addr+SOFT_I2C_RD);
    if (i2c->i2c_waitack(i2c) == 1)return 1;
    
    for(i=0; i<len; i++)
    {
        *data++ = i2c->i2c_readbyte(i2c);
        if(i == len-1)
            i2c->i2c_nack(i2c);
        else
            i2c->i2c_ack(i2c);
    }
    i2c->i2c_stop(i2c);
    
    return 0;
}


static uint8_t write_array(struct I2C *i2c, uint8_t device_addr, uint8_t register_addr, uint8_t *data, uint16_t len)
{
    uint16_t i;
	i2c->i2c_start(i2c);
	i2c->i2c_sendbyte(i2c, device_addr+SOFT_I2C_WR);
	if (i2c->i2c_waitack(i2c) == 1)return 1;
    
    i2c->i2c_sendbyte(i2c, register_addr);
	if (i2c->i2c_waitack(i2c) == 1)return 1;
    
	i2c->i2c_start(i2c);
	i2c->i2c_sendbyte(i2c, device_addr+SOFT_I2C_RD);
	if (i2c->i2c_waitack(i2c) == 1)return 1;
    
	for(i=0; i<len; i++)
	{
		i2c->i2c_sendbyte(i2c, *data++);
		if (i2c->i2c_waitack(i2c) == 1)return 1;
	}
	i2c->i2c_stop(i2c);
    
	return 0;
}


static void i2c_start(I2C *i2c)
{
	set_sda_level(i2c, 1);
	set_scl_level(i2c, 1);
	i2c_delay();
	set_sda_level(i2c, 0);
	i2c_delay();
	set_scl_level(i2c, 0);
	i2c_delay();
}


static void i2c_stop(I2C *i2c)
{
    set_sda_level(i2c, 0);
	set_scl_level(i2c, 1);
	i2c_delay();
	set_sda_level(i2c, 1);
}


static void i2c_sendbyte(I2C *i2c, uint8_t byte)
{
	uint8_t i;

	/* 先发送字节的高位bit7 */
	for (i = 0; i < 8; i++)
	{
		if (byte & 0x80) {
			set_sda_level(i2c, 1);
		} else {
			set_sda_level(i2c, 0);
		}
		i2c_delay();
		set_scl_level(i2c, 1);
		i2c_delay();
		set_scl_level(i2c, 0);
		if (i == 7) {
            set_sda_level(i2c, 1); // 释放总线
		}
		byte <<= 1;	/* 左移一个bit */
		i2c_delay();
	}
}


static uint8_t i2c_readbyte(I2C *i2c)
{
	uint8_t i;
	uint8_t value;

	/* 读到第1个bit为数据的bit7 */
	value = 0;
	for (i = 0; i < 8; i++)
	{
		value <<= 1;
		set_scl_level(i2c, 1);
		i2c_delay();
		if (get_sda_level(i2c)) {
			value++;
		}
		set_scl_level(i2c, 0);
		i2c_delay();
	}
	return value;
}


static void i2c_ack(I2C *i2c)
{
    set_sda_level(i2c, 0);
	i2c_delay();
	set_scl_level(i2c, 1);
	i2c_delay();
	set_scl_level(i2c, 0);
	i2c_delay();
	set_sda_level(i2c, 1);
}


static uint8_t i2c_waitack(I2C *i2c)
{
    uint8_t ret;

	set_sda_level(i2c, 1);	        /* CPU释放SDA总线 */
	i2c_delay();
	set_scl_level(i2c, 1);	        /* CPU驱动SCL = 1, 此时器件会返回ACK应答 */
	i2c_delay();
	if(get_sda_level(i2c))	        /* CPU读取SDA口线状态 */
	{
		ret = 1;
	}
	else
	{
		ret = 0;
	}
	set_scl_level(i2c, 0);
	i2c_delay();
    
	return ret;
}


static void i2c_nack(I2C *i2c)
{
    set_sda_level(i2c, 1);	    /* CPU驱动SDA = 1 */
	i2c_delay();
	set_scl_level(i2c, 1);       /* CPU产生1个时钟 */
	i2c_delay();
	set_scl_level(i2c, 0);
	i2c_delay();
}


void i2c_uninit(I2C *i2c)
{
    return ;
}

static void set_scl_level(I2C *i2c, int8_t level)
{
    rt_pin_write(i2c->scl_pin, level ? PIN_HIGH : PIN_LOW);
}

static void set_sda_level(I2C *i2c, int8_t level)
{
    rt_pin_write(i2c->sda_pin, level ? PIN_HIGH : PIN_LOW);
}

static uint8_t get_sda_level(I2C *i2c)
{
    return (rt_pin_read(i2c->sda_pin) == PIN_HIGH ? 1 : 0);
}

static void i2c_delay(void)
{
    rt_hw_us_delay(10);
}

static void set_i2c_sccb(I2C *i2c, int8_t enable)
{
    i2c->sccb = enable;
}