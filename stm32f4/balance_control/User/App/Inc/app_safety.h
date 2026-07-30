/**
 * @file    app_safety.h
 * @brief   安全状态机接口（规范 §2、§5、§8）
 *
 *  状态机：IDLE → RUNNING → FAULT。可救异常保持 RUNNING 并降级；
 *  只有已无可用闭环或机械危险才进入 FAULT，且 FAULT 必须断电重启。
 *
 *  触发源：
 *  - 限位开关触发（双边沿，断线 = 触发）
 *  - TMC2209 DIAG 上升沿
 *  - 视觉短时丢失：模型预测 -> 水平持杆 -> 恢复视觉闭环
 *  - ABZ/PWM 丢失：已标定电位器接管角度内环
 *  - 底盘链路/IMU 丢失：底盘继续自主循迹，STM32 前馈平滑归零
 */
#ifndef APP_SAFETY_H
#define APP_SAFETY_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 安全状态枚举
 * ---------------------------------------------------------------------- */
typedef enum
{
    SAFETY_STATE_IDLE    = 0U,
    SAFETY_STATE_RUNNING = 1U,
    SAFETY_STATE_FAULT   = 2U,
} safety_state_t;

/* -------------------------------------------------------------------------
 * 故障位掩码
 * ---------------------------------------------------------------------- */
#define FAULT_LIMIT_MIN       (1U << 0U)
#define FAULT_LIMIT_MAX       (1U << 1U)
#define FAULT_DIAG            (1U << 2U)
#define FAULT_VISION_LOST     (1U << 4U)
#define FAULT_ENCODER_INVALID (1U << 5U)

#define SAFETY_WARN_CHASSIS          (1U << 0U)
#define SAFETY_WARN_START_ACK        (1U << 1U)
#define SAFETY_WARN_VISION_PREDICT   (1U << 2U)
#define SAFETY_WARN_ANGLE_FALLBACK   (1U << 3U)
#define SAFETY_WARN_SENSOR_MISMATCH  (1U << 4U)

/* -------------------------------------------------------------------------
 * 配置
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  sensor_mismatch_threshold_mrad; /* ABZ 与电位器不一致阈值 */
    uint32_t sensor_mismatch_persist_ms;     /* 持续超限时间后锁存 */
    uint32_t encoder_invalid_persist_ms;
    uint32_t vision_invalid_persist_ms;
    float    encoder_mrad_per_count;
    bool     vision_required;
} safety_cfg_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

void App_Safety_Init(const safety_cfg_t *cfg);

/** @brief 动态模式要求底盘链路就绪；静态阶跃模式可关闭。 */
void App_Safety_SetChassisRequired(bool required);

/** @brief 周期性检查（1 kHz 调用） */
void App_Safety_Check(void);

/** @brief 触发急停（任意层调用，一个控制周期内完成） */
void App_Safety_EmergencyStop(uint32_t fault_mask);

/** @brief 请求进入 RUNNING 状态（需在 IDLE 状态下调用） */
bool App_Safety_RequestStart(void);

/** @brief 请求进入 IDLE（正常停止） */
void App_Safety_RequestStop(void);

/** @brief 读取当前安全状态 */
safety_state_t App_Safety_GetState(void);

/** @brief 读取当前故障位掩码 */
uint32_t App_Safety_GetFaultMask(void);

/** @brief 设置/清除可救降级标志，不改变安全状态。 */
void App_Safety_SetWarning(uint32_t warning_mask, bool active);

uint32_t App_Safety_GetWarningMask(void);

/** @brief 系统是否正在运行（RUNNING 状态） */
bool App_Safety_IsRunning(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_SAFETY_H */
