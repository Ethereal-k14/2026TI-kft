/**
 * @file    app_safety.h
 * @brief   安全状态机接口（规范 §2、§5、§8）
 *
 *  状态机：IDLE → RUNNING → FAULT；反馈丢失由 RUNNING 受控回到 IDLE
 *  FAULT 只能通过手动清除（BSP_Key 重新按键 + App_Safety_ClearFault）恢复
 *
 *  触发源：
 *  - 限位开关触发（双边沿，断线 = 触发）
 *  - TMC2209 DIAG 上升沿
 *  - 反馈异常：持续丢视觉/编码器或 ABZ 与电位器不一致 -> 受控停止
 *  - 底盘异常：底盘一次性停车，STM32 继续稳球 -> 告警
 *  - 只有限位和 TMC2209 DIAG 这类机械/驱动危险才锁存 FAULT
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

/* 可恢复现场状态只做诊断，不锁存 FAULT。 */
#define SAFETY_WARN_CHASSIS          (1U << 0U)
#define SAFETY_WARN_START_ACK        (1U << 1U)
#define SAFETY_WARN_VISION_LOST      (1U << 2U)
#define SAFETY_WARN_SENSOR_FEEDBACK  (1U << 3U)

/* -------------------------------------------------------------------------
 * 配置
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  sensor_mismatch_threshold_mrad; /* ABZ 与电位器不一致阈值 */
    uint32_t sensor_mismatch_persist_ms;     /* 持续超限时间后受控停止 */
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

/** @brief 可恢复受控停止：停止执行器并回到 IDLE，不锁存硬故障。 */
void App_Safety_ControlledStop(uint32_t warning_mask);

/** @brief 清除故障（仅当物理故障已解除时有效） */
void App_Safety_ClearFault(void);

/** @brief 读取当前安全状态 */
safety_state_t App_Safety_GetState(void);

/** @brief 读取当前故障位掩码 */
uint32_t App_Safety_GetFaultMask(void);

/** @brief 读取可自动清除或重新启动后清除的降级告警。 */
uint32_t App_Safety_GetWarningMask(void);

/** @brief 系统是否正在运行（RUNNING 状态） */
bool App_Safety_IsRunning(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_SAFETY_H */
