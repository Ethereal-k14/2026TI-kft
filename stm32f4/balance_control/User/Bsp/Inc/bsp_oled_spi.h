/**
 * @file    bsp_oled_spi.h
 * @brief   SSD1306 128×64 SPI OLED 驱动 BSP 接口（规范 §2、§3）
 *
 *  SPI1：PA5(SCK)/PA7(MOSI)，DMA2 Stream3
 *  CS   : PE7（低有效）
 *  DC   : PE8（低=命令，高=数据）
 *  RST  : PE9（低有效复位）
 *
 *  刷新速率限制：10–20 Hz（规范 §3）
 *  DMA TX 完成前禁止覆盖帧缓冲（规范 §3）
 */
#ifndef BSP_OLED_SPI_H
#define BSP_OLED_SPI_H

#include "bsp_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 屏幕分辨率 */
#define OLED_WIDTH   128U
#define OLED_HEIGHT   64U
#define OLED_PAGES    (OLED_HEIGHT / 8U)  /* 8 pages */

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/** @brief 初始化 OLED（RST 时序 + 初始化命令序列） */
void BSP_OledSpi_Init(void);

/**
 * @brief 向帧缓冲写入单个像素（不立即刷新）
 * @param x     列（0–127）
 * @param y     行（0–63）
 * @param pixel 1=亮，0=灭
 */
void BSP_OledSpi_DrawPixel(uint8_t x, uint8_t y, uint8_t pixel);

/**
 * @brief 在帧缓冲中绘制 ASCII 字符（5×7 字体，8 像素行高）
 * @param x    列起始
 * @param page 页起始（0–7）
 * @param ch   ASCII 字符
 */
void BSP_OledSpi_DrawChar(uint8_t x, uint8_t page, char ch);

/**
 * @brief 在帧缓冲中绘制字符串
 * @param x    列起始
 * @param page 页起始
 * @param str  NULL 结尾字符串
 */
void BSP_OledSpi_DrawString(uint8_t x, uint8_t page, const char *str);

/** @brief 清空帧缓冲（全黑） */
void BSP_OledSpi_Clear(void);

/**
 * @brief 将帧缓冲通过 SPI DMA 发送到屏幕（≤20 Hz 调用）
 *        如果上次 DMA 未完成则直接返回（不阻塞）
 * @return BSP_OK = 发送启动；BSP_ERR_BUSY = DMA 未就绪
 */
bsp_err_t BSP_OledSpi_Flush(void);

/**
 * @brief SPI DMA 完成回调（在 HAL_SPI_TxCpltCallback 中调用）
 */
void BSP_OledSpi_TxCpltCallback(void);

#ifdef __cplusplus
}
#endif
#endif /* BSP_OLED_SPI_H */
