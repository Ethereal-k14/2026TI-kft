/**
 * @file    bsp_uart_dma.c
 * @brief   通用 UART DMA 环形缓冲框架实现
 *
 *  RX 环形缓冲：DMA 循环模式写入，通过 NDTR 差分计算写指针，解析器读指针消费。
 *  TX 静态队列：线性缓冲区，DMA 完成后检查是否有剩余数据继续发送。
 *
 *  IDLE 中断使用方式：
 *    在 stm32f4xx_it.c 的各 USART_IRQHandler USER CODE 段中：
 *      if (__HAL_UART_GET_FLAG(huartN, UART_FLAG_IDLE)) {
 *          __HAL_UART_CLEAR_IDLEFLAG(huartN);
 *          BSP_UartDma_IdleIsr(UART_PORT_XXX);
 *      }
 *    然后正常调用 HAL_UART_IRQHandler(huartN);
 */
#include "bsp_uart_dma.h"
#include "usart.h"  /* CubeMX 生成：huart1..4 */
#include "main.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 端口控制块
 * ---------------------------------------------------------------------- */
typedef struct
{
    UART_HandleTypeDef *huart;

    /* RX 环形缓冲 */
    uint8_t  rx_dma_buf[UART_RX_BUF_SIZE]; /* DMA 写入目标 */
    uint16_t rx_rd;                         /* 读指针（主循环消费） */
    uint16_t rx_wr;                         /* 写指针（IDLE ISR 更新） */

    /* TX 队列 */
    uint8_t  tx_buf[UART_TX_BUF_SIZE];      /* TX 数据缓冲区 */
    uint16_t tx_head;                        /* 待发送起始偏移 */
    uint16_t tx_tail;                        /* 写入末尾偏移 */
    volatile bool tx_busy;                   /* DMA TX 进行中 */

    /* 统计 */
    uart_port_stats_t stats;

    bool initialized;
} uart_port_ctx_t;

static uart_port_ctx_t s_ports[UART_PORT_COUNT];

/* -------------------------------------------------------------------------
 * 私有工具
 * ---------------------------------------------------------------------- */

/** 环形缓冲区可读字节数（不加锁，读写指针均为 uint16_t） */
static uint16_t ring_available(uint16_t wr, uint16_t rd, uint16_t size)
{
    return (wr >= rd) ? (wr - rd) : (size - rd + wr);
}

/** 启动下一次 DMA TX（若队列非空） */
static void tx_kick(uart_port_ctx_t *ctx)
{
    if (ctx->tx_busy)
    {
        return;
    }

    uint16_t pending = 0U;
    if (ctx->tx_tail >= ctx->tx_head)
    {
        pending = ctx->tx_tail - ctx->tx_head;
    }

    if (pending == 0U)
    {
        return;
    }

    ctx->tx_busy = true;
    /* 线性发送一段（到缓冲区末尾或到 tail） */
    uint16_t len = pending;
    if (HAL_UART_Transmit_DMA(ctx->huart,
                               &ctx->tx_buf[ctx->tx_head],
                               len) != HAL_OK)
    {
        ctx->tx_busy = false;
        ctx->stats.tx_busy_drop++;
    }
    else
    {
        ctx->stats.tx_bytes += len;
    }
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void BSP_UartDma_Init(uart_port_t port, UART_HandleTypeDef *huart)
{
    if (port >= UART_PORT_COUNT || huart == NULL)
    {
        return;
    }

    uart_port_ctx_t *ctx = &s_ports[port];
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->huart       = huart;
    ctx->initialized = true;

    /* 使能 IDLE 中断 */
    __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);

    /* 启动 DMA 循环接收 */
    if (HAL_UART_Receive_DMA(huart, ctx->rx_dma_buf, UART_RX_BUF_SIZE) != HAL_OK)
    {
        Error_Handler();
    }
}

void BSP_UartDma_IdleIsr(uart_port_t port)
{
    if (port >= UART_PORT_COUNT)
    {
        return;
    }

    uart_port_ctx_t *ctx = &s_ports[port];
    if (!ctx->initialized || ctx->huart == NULL)
    {
        return;
    }

    /* 通过 DMA NDTR 计算当前 DMA 写入位置 */
    uint16_t dma_pos = (uint16_t)(UART_RX_BUF_SIZE -
                       __HAL_DMA_GET_COUNTER(ctx->huart->hdmarx));

    if (dma_pos != ctx->rx_wr)
    {
        /* 检查是否溢出（写指针追上读指针） */
        uint16_t avail = ring_available(dma_pos, ctx->rx_rd, UART_RX_BUF_SIZE);
        if (avail > UART_RX_BUF_SIZE - 1U)
        {
            ctx->stats.rx_overflow++;
            /* 丢弃旧数据，rd 跟上 dma_pos */
            ctx->rx_rd = dma_pos;
        }

        ctx->rx_wr          = dma_pos;
        ctx->stats.rx_bytes += ring_available(dma_pos, ctx->rx_rd, UART_RX_BUF_SIZE);
    }
}

void BSP_UartDma_TxCpltCallback(uart_port_t port)
{
    if (port >= UART_PORT_COUNT)
    {
        return;
    }

    uart_port_ctx_t *ctx = &s_ports[port];
    ctx->tx_busy  = false;
    /* 推进读指针（简化：整个线性段发完则复位） */
    ctx->tx_head  = ctx->tx_tail;
    tx_kick(ctx); /* 继续发送剩余 */
}

uint16_t BSP_UartDma_Read(uart_port_t port, uint8_t *buf, uint16_t maxlen)
{
    if (port >= UART_PORT_COUNT || buf == NULL || maxlen == 0U)
    {
        return 0U;
    }

    uart_port_ctx_t *ctx  = &s_ports[port];
    uint16_t         avail = ring_available(ctx->rx_wr, ctx->rx_rd, UART_RX_BUF_SIZE);
    uint16_t         n     = (avail < maxlen) ? avail : maxlen;
    uint16_t         i;

    for (i = 0U; i < n; i++)
    {
        buf[i]     = ctx->rx_dma_buf[ctx->rx_rd];
        ctx->rx_rd = (uint16_t)((ctx->rx_rd + 1U) % UART_RX_BUF_SIZE);
    }

    return n;
}

uint16_t BSP_UartDma_Available(uart_port_t port)
{
    if (port >= UART_PORT_COUNT)
    {
        return 0U;
    }
    uart_port_ctx_t *ctx = &s_ports[port];
    return ring_available(ctx->rx_wr, ctx->rx_rd, UART_RX_BUF_SIZE);
}

bsp_err_t BSP_UartDma_Transmit(uart_port_t port, const uint8_t *buf, uint16_t len)
{
    if (port >= UART_PORT_COUNT || buf == NULL || len == 0U)
    {
        return BSP_ERR_INVALID;
    }
    if (len > UART_TX_BUF_SIZE)
    {
        return BSP_ERR_RANGE;
    }

    uart_port_ctx_t *ctx = &s_ports[port];

    /* 检查空间（简化：tx_head == tx_tail 时缓冲区为空） */
    uint16_t free_space = (uint16_t)(UART_TX_BUF_SIZE - 1U -
                          ((ctx->tx_tail >= ctx->tx_head) ?
                           (ctx->tx_tail - ctx->tx_head) :
                           (UART_TX_BUF_SIZE - ctx->tx_head + ctx->tx_tail)));
    if (len > free_space)
    {
        ctx->stats.tx_busy_drop++;
        return BSP_ERR_BUSY;
    }

    /* 线性写入（简化版：重置到头部） */
    if (ctx->tx_head == ctx->tx_tail)
    {
        /* 缓冲区空，从头开始 */
        (void)memcpy(ctx->tx_buf, buf, len);
        ctx->tx_head = 0U;
        ctx->tx_tail = len;
    }
    else if (ctx->tx_tail + len <= UART_TX_BUF_SIZE)
    {
        (void)memcpy(&ctx->tx_buf[ctx->tx_tail], buf, len);
        ctx->tx_tail = (uint16_t)(ctx->tx_tail + len);
    }
    else
    {
        ctx->stats.tx_busy_drop++;
        return BSP_ERR_BUSY;
    }

    tx_kick(ctx);
    return BSP_OK;
}

bsp_err_t BSP_UartDma_TransmitPriority(uart_port_t    port,
                                        const uint8_t *buf,
                                        uint16_t       len)
{
    if (port >= UART_PORT_COUNT || buf == NULL || len == 0U)
    {
        return BSP_ERR_INVALID;
    }
    if (len > UART_TX_BUF_SIZE)
    {
        return BSP_ERR_RANGE;
    }

    uart_port_ctx_t *ctx = &s_ports[port];

    /* 控制路径必须非阻塞：DMA 忙时由上层决定重试或丢弃。 */
    if (ctx->tx_busy)
    {
        ctx->stats.tx_busy_drop++;
        return BSP_ERR_BUSY;
    }

    /* 强制插入：覆盖缓冲区头部 */
    uint16_t send_len = len;
    (void)memcpy(ctx->tx_buf, buf, send_len);
    ctx->tx_head = 0U;
    ctx->tx_tail = send_len;
    ctx->tx_busy = false;

    tx_kick(ctx);
    return BSP_OK;
}

void BSP_UartDma_GetStats(uart_port_t port, uart_port_stats_t *stats)
{
    if (port < UART_PORT_COUNT && stats != NULL)
    {
        *stats = s_ports[port].stats;
    }
}

/* -------------------------------------------------------------------------
 * HAL TX 完成回调（统一转发）
 * ---------------------------------------------------------------------- */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART4)
    {
        BSP_UartDma_TxCpltCallback(UART_PORT_LIDAR);
    }
    else if (huart->Instance == USART2)
    {
        BSP_UartDma_TxCpltCallback(UART_PORT_CHASSIS);
    }
    else if (huart->Instance == USART3)
    {
        BSP_UartDma_TxCpltCallback(UART_PORT_VISION);
    }
    else if (huart->Instance == USART1)
    {
        BSP_UartDma_TxCpltCallback(UART_PORT_DEBUG);
    }
    else
    {
        /* 不处理其他实例 */
    }
}
