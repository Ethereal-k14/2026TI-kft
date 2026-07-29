/**
 * @file    bsp_lidar.c
 * @brief   YDLIDAR SDM18 单点激光测距 BSP 实现
 *
 *  使用 BSP_UartDma（UART_PORT_LIDAR）读取字节流，状态机逐字节解析帧。
 *  帧校验失败只增加诊断计数，不修改上次有效距离。
 *  超过 LIDAR_TIMEOUT_MS 无有效帧时，valid 置 false。
 */
#include "bsp_lidar.h"
#include "bsp_uart_dma.h"
#include "main.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 启动/停止命令字节序列
 * ---------------------------------------------------------------------- */
static const uint8_t k_cmd_start[] = {0xA5U, 0x00U, 0x00U, 0x01U, 0x00U, 0x00U};
static const uint8_t k_cmd_stop[]  = {0xA5U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U};

/* -------------------------------------------------------------------------
 * 帧解析状态机
 * ---------------------------------------------------------------------- */
typedef enum
{
    LIDAR_STATE_WAIT_HDR1 = 0U,
    LIDAR_STATE_WAIT_HDR2,
    LIDAR_STATE_DATA,
} lidar_parse_state_t;

/* -------------------------------------------------------------------------
 * 私有上下文
 * ---------------------------------------------------------------------- */
typedef struct
{
    lidar_parse_state_t state;
    uint8_t  frame_buf[BSP_LIDAR_FRAME_LEN]; /* 原始帧缓冲 */
    uint8_t  byte_idx;                        /* 当前接收位置 */

    sensor_sample_t sample;                   /* 最新有效采样 */
    uint32_t last_valid_ms;                   /* 最后一次有效帧的 HAL_GetTick */

    /* 诊断 */
    uint32_t frame_count;
    uint32_t crc_error_count;
    uint32_t range_error_count;
} lidar_ctx_t;

static lidar_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：校验字节计算
 * sum 校验：([2]+[3]+[4]+[5]) & 0xFF
 * ---------------------------------------------------------------------- */
static bool lidar_check_frame(const uint8_t *buf)
{
    uint8_t sum = (uint8_t)((buf[2U] + buf[3U] + buf[4U] + buf[5U]) & 0xFFU);
    return (sum == buf[6U]);
}

/* -------------------------------------------------------------------------
 * 私有：解析一个完整帧
 * ---------------------------------------------------------------------- */
static void lidar_parse_frame(const uint8_t *buf)
{
    if (!lidar_check_frame(buf))
    {
        s_ctx.crc_error_count++;
        return;
    }

    uint16_t dist_mm     = (uint16_t)(((uint16_t)buf[2U] << 8U) | buf[3U]);
    uint16_t strength    = (uint16_t)(((uint16_t)buf[4U] << 8U) | buf[5U]);

    /* 范围检查 */
    if (dist_mm < BSP_LIDAR_DIST_MIN_MM || dist_mm > BSP_LIDAR_DIST_MAX_MM)
    {
        s_ctx.range_error_count++;
        /* 范围外：标记无效但不复位质量，以免引发控制抖动 */
        s_ctx.sample.valid = false;
        return;
    }

    /* 更新采样 */
    s_ctx.sample.value        = (int32_t)dist_mm;
    s_ctx.sample.timestamp_us = BSP_GetTimestampUs();
    s_ctx.sample.age_ms       = 0U;
    /* 将 16 位强度映射到 0–255 */
    s_ctx.sample.quality      = (uint8_t)(strength > 0xFFU * 256U ?
                                          255U : (uint8_t)(strength >> 8U));
    s_ctx.sample.valid        = true;
    s_ctx.last_valid_ms       = HAL_GetTick();
    s_ctx.frame_count++;
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void BSP_Lidar_Init(void)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = LIDAR_STATE_WAIT_HDR1;

    /* 发送启动命令 */
    (void)BSP_UartDma_Transmit(UART_PORT_LIDAR,
                                k_cmd_start,
                                (uint16_t)sizeof(k_cmd_start));
}

void BSP_Lidar_Process(void)
{
    uint8_t  byte_buf[32U];
    uint16_t n = BSP_UartDma_Read(UART_PORT_LIDAR, byte_buf, (uint16_t)sizeof(byte_buf));
    uint16_t i;

    for (i = 0U; i < n; i++)
    {
        uint8_t b = byte_buf[i];

        switch (s_ctx.state)
        {
            case LIDAR_STATE_WAIT_HDR1:
                if (b == 0xAAU)
                {
                    s_ctx.frame_buf[0U] = b;
                    s_ctx.state         = LIDAR_STATE_WAIT_HDR2;
                }
                break;

            case LIDAR_STATE_WAIT_HDR2:
                if (b == 0xFFU)
                {
                    s_ctx.frame_buf[1U] = b;
                    s_ctx.byte_idx      = 2U;
                    s_ctx.state         = LIDAR_STATE_DATA;
                }
                else
                {
                    /* 非法，重新等待帧头 */
                    s_ctx.state = LIDAR_STATE_WAIT_HDR1;
                }
                break;

            case LIDAR_STATE_DATA:
                s_ctx.frame_buf[s_ctx.byte_idx] = b;
                s_ctx.byte_idx++;
                if (s_ctx.byte_idx >= BSP_LIDAR_FRAME_LEN)
                {
                    lidar_parse_frame(s_ctx.frame_buf);
                    s_ctx.state    = LIDAR_STATE_WAIT_HDR1;
                    s_ctx.byte_idx = 0U;
                }
                break;

            default:
                s_ctx.state = LIDAR_STATE_WAIT_HDR1;
                break;
        }
    }

    /* 超时检测 */
    if (s_ctx.sample.valid)
    {
        uint32_t elapsed_ms = HAL_GetTick() - s_ctx.last_valid_ms;
        if (elapsed_ms > BSP_LIDAR_TIMEOUT_MS)
        {
            s_ctx.sample.valid   = false;
            s_ctx.sample.age_ms  = (uint16_t)(elapsed_ms > 0xFFFFU ? 0xFFFFU : elapsed_ms);
        }
    }
}

void BSP_Lidar_Stop(void)
{
    (void)BSP_UartDma_Transmit(UART_PORT_LIDAR,
                                k_cmd_stop,
                                (uint16_t)sizeof(k_cmd_stop));
    s_ctx.sample.valid = false;
}

void BSP_Lidar_GetSample(sensor_sample_t *out)
{
    if (out != NULL)
    {
        *out = s_ctx.sample;
    }
}
