/**
 * @file    app_vision.c
 * @brief   视觉模块通信实现
 */
#include "app_vision.h"
#include <string.h>

typedef struct
{
    proto_port_ctx_t *proto_ctx;
    vision_pose_t     pose;
    vision_status_t   status;
    uint32_t          last_rx_ms;
} vision_ctx_t;

static vision_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：协议接收回调
 * ---------------------------------------------------------------------- */
static void vision_rx_cb(proto_msg_id_t id,
                          const uint8_t *payload,
                          uint16_t       payload_len,
                          uint32_t       timestamp_us)
{
    switch (id)
    {
        case MSG_VISION_POSE:
            if (payload_len >= 14U)
            {
                s_ctx.pose.position_um =
                    (int32_t)((uint32_t)payload[0U]  | ((uint32_t)payload[1U]  << 8U) |
                              ((uint32_t)payload[2U]  << 16U) | ((uint32_t)payload[3U] << 24U));
                s_ctx.pose.velocity_um_s =
                    (int32_t)((uint32_t)payload[4U]  | ((uint32_t)payload[5U]  << 8U) |
                              ((uint32_t)payload[6U]  << 16U) | ((uint32_t)payload[7U] << 24U));
                s_ctx.pose.confidence =
                    (uint16_t)(payload[8U]  | ((uint16_t)payload[9U] << 8U));
                s_ctx.pose.frame_age_us =
                    (uint32_t)payload[10U] | ((uint32_t)payload[11U] << 8U) |
                    ((uint32_t)payload[12U] << 16U) | ((uint32_t)payload[13U] << 24U);

                /* 采集时刻 = 接收时刻 - 帧龄（近似） */
                s_ctx.pose.timestamp_us = timestamp_us;
                s_ctx.pose.valid        = true;

                /* 可用性：置信度足够且帧龄不超限 */
                s_ctx.pose.usable =
                    (s_ctx.pose.confidence >= VISION_CONFIDENCE_THRESHOLD) &&
                    (s_ctx.pose.frame_age_us < VISION_FRAME_AGE_MAX_US);

                s_ctx.last_rx_ms = HAL_GetTick();
            }
            break;

        case MSG_VISION_STATUS:
            if (payload_len >= 10U)
            {
                s_ctx.status.status =
                    (uint16_t)(payload[0U] | ((uint16_t)payload[1U] << 8U));
                s_ctx.status.frame_counter =
                    (uint32_t)payload[2U] | ((uint32_t)payload[3U] << 8U) |
                    ((uint32_t)payload[4U] << 16U) | ((uint32_t)payload[5U] << 24U);
                s_ctx.status.exposure_us =
                    (uint32_t)payload[6U] | ((uint32_t)payload[7U] << 8U) |
                    ((uint32_t)payload[8U] << 16U) | ((uint32_t)payload[9U] << 24U);
            }
            break;

        default:
            break;
    }
}

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

void App_Vision_Init(proto_port_ctx_t *proto_ctx)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.proto_ctx = proto_ctx;
    App_Protocol_Init(proto_ctx, UART_PORT_VISION, vision_rx_cb);
}

void App_Vision_Process(void)
{
    App_Protocol_Process(s_ctx.proto_ctx);

    /* 超时检测 */
    if (s_ctx.pose.valid)
    {
        uint32_t elapsed = HAL_GetTick() - s_ctx.last_rx_ms;
        if (elapsed > VISION_TIMEOUT_MS)
        {
            s_ctx.pose.valid   = false;
            s_ctx.pose.usable  = false;
        }
    }
}

void App_Vision_GetPose(vision_pose_t *out)
{
    if (out != NULL) { *out = s_ctx.pose; }
}

void App_Vision_GetStatus(vision_status_t *out)
{
    if (out != NULL) { *out = s_ctx.status; }
}

bsp_err_t App_Vision_SendTarget(int32_t target_um, uint8_t sys_state)
{
    uint8_t payload[5U];
    payload[0U] = (uint8_t)((uint32_t)target_um & 0xFFU);
    payload[1U] = (uint8_t)(((uint32_t)target_um >> 8U)  & 0xFFU);
    payload[2U] = (uint8_t)(((uint32_t)target_um >> 16U) & 0xFFU);
    payload[3U] = (uint8_t)(((uint32_t)target_um >> 24U) & 0xFFU);
    payload[4U] = sys_state;
    return App_Protocol_Send(s_ctx.proto_ctx,
                              MSG_VISION_TARGET,
                              payload, (uint16_t)sizeof(payload));
}
