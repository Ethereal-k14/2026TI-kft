/**
 * @file    app_vision.h
 * @brief   视觉模块通信接口（规范 §4.2）
 *
 *  USART3，消息 ID 0x20–0x22
 *  接收：0x20（位置/速度/置信度）、0x21（帧状态）
 *  发送：0x22（目标位置、时间同步请求）
 *
 *  confidence < 阈值 或 帧龄超限 → 只允许状态预测，不修正控制量
 */
#ifndef APP_VISION_H
#define APP_VISION_H

#include "bsp_common.h"
#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 配置
 * ---------------------------------------------------------------------- */
#define VISION_CONFIDENCE_THRESHOLD  300U   /* 0–1000，低于此值不更新估计器 */
#define VISION_FRAME_AGE_MAX_US      50000U /* 帧龄超限（50 ms） */
#define VISION_TIMEOUT_MS            200U   /* 完全超时 */

/* -------------------------------------------------------------------------
 * 视觉测量数据（消息 0x20）
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  position_um;      /* 目标位置，单位 µm */
    int32_t  velocity_um_s;    /* 目标速度，单位 µm/s */
    uint16_t confidence;       /* 0–1000 */
    uint32_t frame_age_us;     /* 采集时刻到发送时刻延迟（µs） */
    uint32_t timestamp_us;     /* 采集时刻（发送端填写，非接收时刻） */
    bool     valid;            /* false = 超时或置信度不足 */
    bool     usable;           /* false = 只允许预测，不修正控制量 */
} vision_pose_t;

/* -------------------------------------------------------------------------
 * 视觉帧状态（消息 0x21）
 * ---------------------------------------------------------------------- */
typedef struct
{
    uint16_t status;
    uint32_t frame_counter;
    uint32_t exposure_us;
} vision_status_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

void App_Vision_Init(proto_port_ctx_t *proto_ctx);
void App_Vision_Process(void);

/** @brief 读取最新视觉位置测量（含有效性和可用性标志） */
void App_Vision_GetPose(vision_pose_t *out);

/** @brief 读取帧状态 */
void App_Vision_GetStatus(vision_status_t *out);

/**
 * @brief 发送目标位置和时间同步请求（消息 0x22）
 * @param target_um     目标位置（µm）
 * @param sys_state     系统运行状态（1 bit）
 */
bsp_err_t App_Vision_SendTarget(int32_t target_um, uint8_t sys_state);

#ifdef __cplusplus
}
#endif
#endif /* APP_VISION_H */
