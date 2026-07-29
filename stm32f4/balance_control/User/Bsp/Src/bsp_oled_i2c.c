/**
 * @file    bsp_oled_i2c.c
 * @brief   SSD1306 I2C 后端；默认工程使用 SPI 后端，二者不可同时刷新。
 */
#include "bsp_oled_i2c.h"
#include "i2c.h"
#include "main.h"
#include <string.h>

static uint8_t s_framebuf[(BSP_OLED_I2C_WIDTH * BSP_OLED_I2C_HEIGHT) / 8U];
static const uint8_t k_init_cmds[] = {
    0xAEU, 0x20U, 0x00U, 0xB0U, 0xC8U, 0x00U, 0x10U, 0x40U,
    0x81U, 0x7FU, 0xA1U, 0xA6U, 0xA8U, 0x3FU, 0xA4U, 0xD3U,
    0x00U, 0xD5U, 0xF0U, 0xD9U, 0x22U, 0xDAU, 0x12U, 0xDBU,
    0x20U, 0x8DU, 0x14U, 0xAFU
};

static bsp_err_t oled_i2c_command(uint8_t command)
{
    uint8_t tx[2U] = {0x00U, command};
    return (HAL_I2C_Master_Transmit(&hi2c1, BSP_OLED_I2C_ADDR, tx,
                                    (uint16_t)sizeof(tx), 10U) == HAL_OK) ?
           BSP_OK : BSP_ERR_TIMEOUT;
}

void BSP_OledI2c_Init(void)
{
    uint32_t i;
    (void)memset(s_framebuf, 0, sizeof(s_framebuf));
    for (i = 0U; i < (uint32_t)sizeof(k_init_cmds); i++)
    {
        (void)oled_i2c_command(k_init_cmds[i]);
    }
}

void BSP_OledI2c_Clear(void)
{
    (void)memset(s_framebuf, 0, sizeof(s_framebuf));
}

void BSP_OledI2c_DrawPixel(uint8_t x, uint8_t y, uint8_t pixel)
{
    if ((x < BSP_OLED_I2C_WIDTH) && (y < BSP_OLED_I2C_HEIGHT))
    {
        uint16_t index = (uint16_t)((y / 8U) * BSP_OLED_I2C_WIDTH + x);
        uint8_t mask = (uint8_t)(1U << (y % 8U));
        if (pixel != 0U)
        {
            s_framebuf[index] |= mask;
        }
        else
        {
            s_framebuf[index] &= (uint8_t)~mask;
        }
    }
}

bsp_err_t BSP_OledI2c_Flush(void)
{
    uint8_t tx[129U];
    uint32_t page;
    tx[0U] = 0x40U;
    for (page = 0U; page < 8U; page++)
    {
        uint8_t page_cmd = (uint8_t)(0xB0U | page);
        if ((oled_i2c_command(page_cmd) != BSP_OK) ||
            (oled_i2c_command(0x00U) != BSP_OK) ||
            (oled_i2c_command(0x10U) != BSP_OK))
        {
            return BSP_ERR_TIMEOUT;
        }
        (void)memcpy(&tx[1U], &s_framebuf[page * BSP_OLED_I2C_WIDTH],
                     BSP_OLED_I2C_WIDTH);
        if (HAL_I2C_Master_Transmit(&hi2c1, BSP_OLED_I2C_ADDR, tx,
                                    (uint16_t)sizeof(tx), 20U) != HAL_OK)
        {
            return BSP_ERR_TIMEOUT;
        }
    }
    return BSP_OK;
}
