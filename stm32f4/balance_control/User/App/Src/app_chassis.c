/**
 * @file    app_chassis.c
 * @brief   下位机通信模块实现
 */
#include "app_chassis.h"
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------
 * 私有状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    proto_port_ctx_t *proto_ctx;
    chassis_status_t  status;
    chassis_imu_t     imu;
    uint32_t          last_imu_ms;
    uint32_t          last_heartbeat_ms;
} chassis_ctx_t;

static chassis_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：协议接收回调
 * ---------------------------------------------------------------------- */
static void chassis_rx_cb(proto_msg_id_t id,
                           const uint8_t *payload,
                           uint16_t       payload_len,
                           uint32_t       timestamp_us)
{
    switch (id)
    {
        case MSG_CHASSIS_STATUS:
            if (payload_len >= 5U)
            {
                s_ctx.status.state        = payload[0U];
                s_ctx.status.fault_code   = (uint16_t)(payload[1U] | ((uint16_t)payload[2U] << 8U));
                s_ctx.status.last_cmd_seq = (uint16_t)(payload[3U] | ((uint16_t)payload[4U] << 8U));
            }
            break;

        case MSG_CHASSIS_IMU:
            if (payload_len >= 13U)
            {
                s_ctx.imu.ax_mm_s2 =
                    (int32_t)((uint32_t)payload[0U]  | ((uint32_t)payload[1U]  << 8U) |
                              ((uint32_t)payload[2U]  << 16U) | ((uint32_t)payload[3U] << 24U));
                s_ctx.imu.ay_mm_s2 =
                    (int32_t)((uint32_t)payload[4U]  | ((uint32_t)payload[5U]  << 8U) |
                              ((uint32_t)payload[6U]  << 16U) | ((uint32_t)payload[7U] << 24U));
                s_ctx.imu.az_mm_s2 =
                    (int32_t)((uint32_t)payload[8U]  | ((uint32_t)payload[9U]  << 8U) |
                              ((uint32_t)payload[10U] << 16U) | ((uint32_t)payload[11U] << 24U));
                s_ctx.imu.quality      = payload[12U];
                s_ctx.imu.timestamp_us = timestamp_us;
                s_ctx.imu.valid        = true;
                s_ctx.imu.comm_degraded = false;
                s_ctx.imu.feedfwd_weight = 1.0f; /* 恢复满权重（需缓慢爬升，由外环控制） */
                s_ctx.last_imu_ms      = HAL_GetTick();
            }
            break;

        case MSG_CHASSIS_HEARTBEAT:
            s_ctx.last_heartbeat_ms = HAL_GetTick();
            break;

        default:
            break;
    }
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void App_Chassis_Init(proto_port_ctx_t *proto_ctx)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.proto_ctx = proto_ctx;
    App_Protocol_Init(proto_ctx, UART_PORT_CHASSIS, chassis_rx_cb);
}

void App_Chassis_Process(void)
{
    /* 驱动协议解析 */
    App_Protocol_Process(s_ctx.proto_ctx);

    /* 超时检测 */
    uint32_t now_ms  = HAL_GetTick();
    uint32_t elapsed = now_ms - s_ctx.last_imu_ms;

    if (elapsed > 50U && s_ctx.imu.valid)
    {
        /* 50 ms 超时：前馈权重每 1 ms 衰减（100 ms 内归零） */
        float decay = (float)(elapsed - 50U) / 100.0f;
        if (decay > 1.0f) { decay = 1.0f; }
        s_ctx.imu.feedfwd_weight = 1.0f - decay;
    }

    if (elapsed > 200U)
    {
        s_ctx.imu.valid         = false;
        s_ctx.imu.comm_degraded = true;
        s_ctx.imu.feedfwd_weight = 0.0f;
    }
}

bsp_err_t App_Chassis_SendCmd(chassis_cmd_t cmd, uint8_t reason)
{
    uint8_t payload[2U];
    payload[0U] = (uint8_t)cmd;
    payload[1U] = reason;
    return App_Protocol_Send(s_ctx.proto_ctx,
                              MSG_CHASSIS_CMD,
                              payload, (uint16_t)sizeof(payload));
}

bsp_err_t App_Chassis_SendHeartbeat(void)
{
    return App_Protocol_Send(s_ctx.proto_ctx, MSG_CHASSIS_HEARTBEAT, NULL, 0U);
}

void App_Chassis_GetStatus(chassis_status_t *out)
{
    if (out != NULL) { *out = s_ctx.status; }
}

void App_Chassis_GetImu(chassis_imu_t *out)
{
    if (out != NULL) { *out = s_ctx.imu; }
}
