/**
 * @file    app_chassis.h
 * @brief   下位机通信模块接口（规范 §4.1）
 *
 *  USART2，消息 ID 0x10–0x13
 *  接收：0x11（状态）、0x12（IMU 加速度）、0x13（心跳）
 *  发送：0x10（启停命令）、0x13（心跳）
 *
 *  IMU 数据超时 50 ms → 加速度前馈权重平滑衰减至 0
 *  超时 200 ms → 置 comm_degraded 标志
 */
#ifndef APP_CHASSIS_H
#define APP_CHASSIS_H

#include "bsp_common.h"
#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 命令枚举（消息 0x10 payload.command）
 * ---------------------------------------------------------------------- */
typedef enum
{
    CHASSIS_CMD_STOP       = 0U,
    CHASSIS_CMD_START      = 1U,
    CHASSIS_CMD_ESTOP      = 2U,
} chassis_cmd_t;

/* -------------------------------------------------------------------------
 * 下位机状态（消息 0x11）
 * ---------------------------------------------------------------------- */
typedef struct
{
    uint8_t  state;             /* 0=idle, 1=running, 2=fault */
    uint16_t fault_code;        /* 故障位掩码 */
    uint16_t last_cmd_seq;      /* 已确认的命令序号 */
} chassis_status_t;

/* -------------------------------------------------------------------------
 * IMU 加速度数据（消息 0x12，单位 mm/s²）
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  ax_mm_s2;
    int32_t  ay_mm_s2;
    int32_t  az_mm_s2;
    uint8_t  quality;
    uint32_t timestamp_us;
    bool     valid;
    bool     comm_degraded;    /* >200 ms 超时 */
    float    feedfwd_weight;   /* 0.0–1.0，超时后平滑衰减 */
} chassis_imu_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/** @brief 初始化下位机通信模块（在 App_Protocol_Init 之后调用） */
void App_Chassis_Init(proto_port_ctx_t *proto_ctx);

/** @brief 周期性处理（100 Hz 调用）：驱动协议解析、更新超时 */
void App_Chassis_Process(void);

/** @brief 发送启停命令（同步发送，等待确认可选） */
bsp_err_t App_Chassis_SendCmd(chassis_cmd_t cmd, uint8_t reason);

/** @brief 发送心跳（10–50 Hz 调用） */
bsp_err_t App_Chassis_SendHeartbeat(void);

/** @brief 读取下位机状态快照 */
void App_Chassis_GetStatus(chassis_status_t *out);

/** @brief 读取 IMU 加速度数据（含有效性和权重） */
void App_Chassis_GetImu(chassis_imu_t *out);

/** @brief 最近 250 ms 内收到过底盘心跳。 */
bool App_Chassis_IsLinkReady(void);

/** @brief 心跳和状态均新鲜，且底盘未报告故障。 */
bool App_Chassis_IsHealthy(void);

/** @brief 最近的底盘状态已确认进入运行态且未报告故障。 */
bool App_Chassis_IsRunningConfirmed(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_CHASSIS_H */
