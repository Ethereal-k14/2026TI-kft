/**
 * @file    bsp_uart_dma.h
 * @brief   通用 UART DMA 环形缓冲框架（规范 §3）
 *
 *  支持 4 路 UART（LIDAR/CHASSIS/VISION/DEBUG），每路独立：
 *  - RX：DMA 循环模式 + UART IDLE 中断，位置差分取数
 *  - TX：静态队列 + DMA 普通模式，完成回调推进队列
 *
 *  解析器通过 BSP_UartDma_Read() 消费 RX 环形缓冲区，不停止 DMA。
 *  高优先级控制帧通过 BSP_UartDma_TransmitPriority() 非阻塞尝试插队发送。
 */
#ifndef BSP_UART_DMA_H
#define BSP_UART_DMA_H

#include "bsp_common.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 端口标识
 * ---------------------------------------------------------------------- */
typedef enum
{
    UART_PORT_LIDAR   = 0U,  /* UART4,  921600, SDM18  */
    UART_PORT_CHASSIS = 1U,  /* USART2, 921600, 下位机 */
    UART_PORT_VISION  = 2U,  /* USART3, 921600, 视觉   */
    UART_PORT_DEBUG   = 3U,  /* USART1, 115200, 调试   */
    UART_PORT_COUNT   = 4U,
} uart_port_t;

/* -------------------------------------------------------------------------
 * 缓冲区大小（可按端口单独调整）
 * ---------------------------------------------------------------------- */
#define UART_RX_BUF_SIZE    USER_UART_RX_BUF_SIZE /* RX DMA 环形缓冲区 */
#define UART_TX_BUF_SIZE    USER_UART_TX_BUF_SIZE /* TX 静态队列 */

/* -------------------------------------------------------------------------
 * 端口统计（只读，用于诊断）
 * ---------------------------------------------------------------------- */
typedef struct
{
    uint32_t rx_bytes;       /* 累计接收字节数 */
    uint32_t tx_bytes;       /* 累计发送字节数 */
    uint32_t rx_overflow;    /* 环形缓冲区溢出次数 */
    uint32_t tx_busy_drop;   /* TX 忙丢帧次数 */
} uart_port_stats_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 初始化指定端口的 DMA 接收
 * @param port     端口标识
 * @param huart    对应的 UART 句柄（CubeMX 生成）
 *                 调用后立即启动 DMA 循环接收
 */
void BSP_UartDma_Init(uart_port_t port, UART_HandleTypeDef *huart);

/**
 * @brief UART IDLE 中断入口（在各 UART ISR 的 USER CODE 中调用）
 *        使用 DMA NDTR 计算新到数据的位置并更新写指针
 * @param port  端口标识
 */
void BSP_UartDma_IdleIsr(uart_port_t port);

/**
 * @brief DMA TX 完成回调（在 HAL_UART_TxCpltCallback 中转发）
 *        推进 TX 队列，继续发送剩余数据
 * @param port  端口标识
 */
void BSP_UartDma_TxCpltCallback(uart_port_t port);

/**
 * @brief 从 RX 环形缓冲区读取数据（非阻塞）
 * @param port   端口标识
 * @param buf    输出缓冲区
 * @param maxlen 最多读取字节数
 * @return       实际读取字节数
 */
uint16_t BSP_UartDma_Read(uart_port_t port, uint8_t *buf, uint16_t maxlen);

/**
 * @brief 查询 RX 缓冲区中可读字节数
 */
uint16_t BSP_UartDma_Available(uart_port_t port);

/**
 * @brief 发送数据（非阻塞，放入 TX 队列）
 * @param port    端口标识
 * @param buf     待发送数据
 * @param len     字节数
 * @return        BSP_OK 或 BSP_ERR_BUSY（队列满）
 */
bsp_err_t BSP_UartDma_Transmit(uart_port_t port, const uint8_t *buf, uint16_t len);

/**
 * @brief 高优先级发送（TX 忙时立即返回 BSP_ERR_BUSY）
 *        仅用于急停/故障帧；不在中断或控制环中等待。
 */
bsp_err_t BSP_UartDma_TransmitPriority(uart_port_t port,
                                        const uint8_t *buf,
                                        uint16_t       len);

/**
 * @brief 获取端口统计信息
 */
void BSP_UartDma_GetStats(uart_port_t port, uart_port_stats_t *stats);

#ifdef __cplusplus
}
#endif
#endif /* BSP_UART_DMA_H */
