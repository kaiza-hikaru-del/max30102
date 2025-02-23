/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      iic.c
 * @brief     iic source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2021-2-12
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/02/12  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "iic.h"
#include "delay.h"

/**
 * @brief bit operate definition
 */
#define BITBAND(addr, bitnum)    ((addr & 0xF0000000) + 0x2000000 + ((addr & 0xFFFFF) << 5) + (bitnum << 2)) 
#define MEM_ADDR(addr)           *((volatile unsigned long *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum))

/**
 * @brief iic gpio operate definition
 */
#define GPIOB_ODR_Addr    (GPIOB_BASE + 0x14)
#define GPIOB_IDR_Addr    (GPIOB_BASE + 0x10)
#define PBout(n)          BIT_ADDR(GPIOB_ODR_Addr, n)
#define PBin(n)           BIT_ADDR(GPIOB_IDR_Addr, n)
#define SDA_IN()          {GPIOB->MODER &= ~(3 << (9 * 2)); GPIOB->MODER |= 0 << 9 * 2;}
#define SDA_OUT()         {GPIOB->MODER &= ~(3 << (9 * 2)); GPIOB->MODER |= 1 << 9 * 2;}
#define IIC_SCL           PBout(8)
#define IIC_SDA           PBout(9)
#define READ_SDA          PBin(9)

/**
 * @brief  iic bus init
 * @return status code
 *         - 0 success
 * @note   SCL is PB8 and SDA is PB9
 */
uint8_t iic_init(void)
{
    GPIO_InitTypeDef GPIO_Initure;
    
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_Initure.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);
    
    IIC_SDA = 1;
    IIC_SCL = 1;
  
    return 0;
}

/**
 * @brief  iic bus deinit
 * @return status code
 *         - 0 success
 * @note   SCL is PB8 and SDA is PB9
 */
uint8_t iic_deinit(void)
{
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8 | GPIO_PIN_9);
    
    return 0;
}

/**
 * @brief  iic bus send start
 * @note   none
 */
static void _iic_start(void)
{
    SDA_OUT();
    IIC_SCL = 1;
    delay_us(15);
    IIC_SDA = 0;
    delay_us(15);
    IIC_SCL = 0;
    delay_us(15);
    IIC_SDA = 1;
    delay_us(15);
}

/**
 * @brief  iic bus send stop
 * @note   none
 */
static void _iic_stop(void)
{
    SDA_OUT();
    IIC_SDA = 0;
    delay_us(15);
    IIC_SCL = 1;
    delay_us(15);
    IIC_SDA = 1;
    delay_us(15);
}

/**
 * @brief  iic wait ack
 * @return status code
 *         - 0 get ack
 *         - 1 no ack
 * @note   none
 */
static uint8_t _iic_wait_ack(void)
{
    volatile uint16_t uc_err_time = 0;
    
    SDA_IN();
    IIC_SDA = 1; 
    delay_us(20);
    IIC_SCL = 1; 
    delay_us(20);
    while (READ_SDA)
    {
        uc_err_time++;
        if (uc_err_time > 500)
        {
            _iic_stop();
            
            return 1;
        }
    }
    IIC_SCL = 0;
    delay_us(20);
   
    return 0;
}

/**
 * @brief  iic bus send ack
 * @note   none
 */
static void _iic_ack(void)
{
    IIC_SCL = 0;
    delay_us(20);
    SDA_OUT();
    IIC_SDA = 0;
    delay_us(20);
    IIC_SCL = 1;
    delay_us(20);
    IIC_SCL = 0;
    delay_us(20);
}

/**
 * @brief  iic bus send nack
 * @note   none
 */
static void _iic_nack(void)
{
    IIC_SCL = 0;
    delay_us(20);
    SDA_OUT();
    IIC_SDA = 1;
    delay_us(20);
    IIC_SCL = 1;
    delay_us(20);
    IIC_SCL = 0; 
    delay_us(20);
}

/**
 * @brief     iic send one byte
 * @param[in] txd send byte
 * @note      none
 */
static void _iic_send_byte(uint8_t txd)
{
    volatile uint8_t t;
    
    SDA_OUT();
    IIC_SCL = 0;
    for (t=0; t<8; t++)
    {
        IIC_SDA = (txd&0x80) >> 7;
        txd <<= 1;
        delay_us(10);
        IIC_SCL = 1;
        delay_us(10);
        IIC_SCL = 0;
        delay_us(10);
    }
}

/**
 * @brief     iic read one byte
 * @param[in] ack is whether to send ack
 * @return    read byte
 * @note      none
 */
static uint8_t _iic_read_byte(uint8_t ack)
{
    volatile uint8_t i,receive = 0;
    
    SDA_IN();
    
    for (i=0; i<8; i++)
    {
        IIC_SCL = 0;
        delay_us(10);
        IIC_SCL = 1;
        receive <<= 1;
        if (READ_SDA)
        {
            receive++;
        }
        delay_us(10);
    }
    if (!ack)
    {
        _iic_nack();
    }
    else
    {
        _iic_ack();
    }
    
    return receive;
}

/**
 * @brief     iic scl write value
 * @param[in] value is the written value
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      SCL is PB8 and SDA is PB9
 */
uint8_t iic_scl_write(uint8_t value)
{
    IIC_SCL = value;
    
    return 0;
}

/**
 * @brief     iic sda write value
 * @param[in] value is the written value
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      SCL is PB8 and SDA is PB9
 */
uint8_t iic_sda_write(uint8_t value)
{
    SDA_OUT();
    IIC_SDA = value;
    
    return 0;
}

/**
 * @brief     iic bus write command
 * @param[in] addr is iic device write address
 * @param[in] *buf points to a data buffer
 * @param[in] len is the length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      SCL is PB8 and SDA is PB9
 */
uint8_t iic_write_cmd(uint8_t addr, uint8_t *buf, uint16_t len)
{
    volatile uint16_t i; 
    
    _iic_start();
    _iic_send_byte(addr);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    for (i=0; i<len; i++)
    {
        _iic_send_byte(buf[i]);
        if (_iic_wait_ack())
        {
            _iic_stop();
            
            return 1;
        }
    }
    _iic_stop();
    
    return 0;
} 

/**
 * @brief     iic bus write
 * @param[in] addr is iic device write address
 * @param[in] reg is iic register address
 * @param[in] *buf points to a data buffer
 * @param[in] len is the length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      SCL is PB8 and SDA is PB9
 */
uint8_t iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    volatile uint16_t i; 
  
    _iic_start();
    _iic_send_byte(addr);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    _iic_send_byte(reg);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    for (i=0; i<len; i++)
    {
        _iic_send_byte(buf[i]);
        if (_iic_wait_ack())
        {
            _iic_stop(); 
            
            return 1;
        }
    }
    _iic_stop();
    
    return 0;
} 

/**
 * @brief     iic bus write with 16 bits register address 
 * @param[in] addr is iic device write address
 * @param[in] reg is iic register address
 * @param[in] *buf points to a data buffer
 * @param[in] len is the length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      SCL is PB8 and SDA is PB9
 */
uint8_t iic_write_address16(uint8_t addr, uint16_t reg, uint8_t *buf, uint16_t len)
{
    volatile uint16_t i; 
  
    _iic_start();
    _iic_send_byte(addr);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    _iic_send_byte((reg>>8)&0xFF);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    _iic_send_byte(reg&0xFF);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    for (i=0; i<len; i++)
    {
        _iic_send_byte(buf[i]);
        if (_iic_wait_ack())
        {
            _iic_stop();
            
            return 1;
        }
    }
    _iic_stop();
    
    return 0;
} 

/**
 * @brief      iic bus read
 * @param[in]  addr is iic device write address
 * @param[in]  reg is iic register address
 * @param[out] *buf points to a data buffer
 * @param[in]  len is the length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       SCL is PB8 and SDA is PB9
 */
uint8_t iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{ 
    _iic_start();
    _iic_send_byte(addr);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    _iic_send_byte(reg);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    _iic_start();
    _iic_send_byte(addr+1);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    
    while (len)
    {
        if (len == 1)
        {
            *buf = _iic_read_byte(0);
        }
        else
        {
            *buf = _iic_read_byte(1);
        }
        len--;
        buf++;
    }
    _iic_stop();
    
    return 0;
}

/**
 * @brief      iic bus read with 16 bits register address 
 * @param[in]  addr is iic device write address
 * @param[in]  reg is iic register address
 * @param[out] *buf points to a data buffer
 * @param[in]  len is the length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       SCL is PB8 and SDA is PB9
 */
uint8_t iic_read_address16(uint8_t addr, uint16_t reg, uint8_t *buf, uint16_t len)
{ 
    _iic_start();
    _iic_send_byte(addr);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    _iic_send_byte((reg>>8)&0xFF);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }    
    _iic_send_byte(reg&0xFF);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    _iic_start();
    _iic_send_byte(addr+1);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    while (len)
    {
        if (len == 1)
        {
            *buf = _iic_read_byte(0);
        }
        else
        {
            *buf = _iic_read_byte(1);
        }
        len--;
        buf++;
    }
    _iic_stop();
    
    return 0;
}

/**
 * @brief      iic bus read command
 * @param[in]  addr is iic device write address
 * @param[out] *buf points to a data buffer
 * @param[in]  len is the length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       SCL is PB8 and SDA is PB9
 */
uint8_t iic_read_cmd(uint8_t addr, uint8_t *buf, uint16_t len)
{ 
    _iic_start();
    _iic_send_byte(addr + 1);
    if (_iic_wait_ack())
    {
        _iic_stop();
        
        return 1;
    }
    while (len)
    {
        if (len == 1)
        {
            *buf = _iic_read_byte(0);
        }
        else
        {
            *buf = _iic_read_byte(1); 
        }
        len--;
        buf++;
    }
    _iic_stop(); 
    
    return 0;
}
