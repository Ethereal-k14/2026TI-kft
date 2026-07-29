/**
 * @file    bsp_oled_i2c.h
 * @brief   SSD1306 128x64 I2C 后端（与 SPI 后端二选一）。
 */
#ifndef BSP_OLED_I2C_H
#define BSP_OLED_I2C_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_OLED_I2C_WIDTH  (128U)
#define BSP_OLED_I2C_HEIGHT (64U)
#define BSP_OLED_I2C_ADDR   (0x78U)

void BSP_OledI2c_Init(void);
void BSP_OledI2c_Clear(void);
void BSP_OledI2c_DrawPixel(uint8_t x, uint8_t y, uint8_t pixel);
bsp_err_t BSP_OledI2c_Flush(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_I2C_H */
