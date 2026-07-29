/**
 * @file    bsp_key.h
 * @brief   按键与限位开关 BSP 接口（规范 §2）
 *
 *  START_KEY : PE0 / EXTI0，低有效，软件去抖 20 ms，下降沿
 *  LIMIT_MIN : PE5 / EXTI5，常闭低有效，双边沿（断线=触发）
 *  LIMIT_MAX : PE6 / EXTI6，常闭低有效，双边沿
 *  TMC_DIAG  : PC9 / EXTI9，上升沿（故障信号，由 bsp_stepper 处理）
 *
 *  注意：EXTI 边沿在 BSP_Key_Init() 中重新配置，覆盖 CubeMX 的初始设置。
 *  限位 ISR 只记录事件，安全层必须再次读取引脚稳定电平。
 */
#ifndef BSP_KEY_H
#define BSP_KEY_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 限位状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    bool limit_min_triggered; /* 最小限位已触发（引脚曾为低） */
    bool limit_max_triggered; /* 最大限位已触发 */
    bool limit_min_active;    /* 常闭回路断开/触发后为高，含断线保护 */
    bool limit_max_active;
    bool start_key_pressed;   /* 去抖后的按键事件（只产生一次，消费后清零） */
} key_limit_state_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 初始化按键/限位 BSP
 *        - 重新配置 PE0 EXTI 为下降沿（覆盖 CubeMX 上升沿配置）
 *        - 重新配置 PE5/PE6 EXTI 为双边沿
 *        - 启用对应 NVIC
 *        必须在 MX_GPIO_Init() 之后调用
 */
void BSP_Key_Init(void);

/**
 * @brief 周期性去抖处理（建议 1 kHz 调用）
 *        - START_KEY 去抖：连续 20 ms 低电平后产生一次事件
 */
void BSP_Key_Process(void);

/**
 * @brief START_KEY EXTI0 中断入口（在 EXTI0_IRQHandler 中调用）
 */
void BSP_Key_StartKeyIsr(void);

/**
 * @brief LIMIT_MIN EXTI5 中断入口（在 EXTI9_5_IRQHandler 中调用）
 */
void BSP_Key_LimitMinIsr(void);

/**
 * @brief LIMIT_MAX EXTI6 中断入口（在 EXTI9_5_IRQHandler 中调用）
 */
void BSP_Key_LimitMaxIsr(void);

/**
 * @brief 读取当前状态（只读快照）
 * @param out 非 NULL 输出指针
 */
void BSP_Key_GetState(key_limit_state_t *out);

/**
 * @brief 消费 start_key_pressed 事件（读取后自动清零）
 * @return true = 有一次未消费的按键事件
 */
bool BSP_Key_ConsumeStartEvent(void);

/**
 * @brief 清除限位触发记录（由安全层在恢复后调用）
 */
void BSP_Key_ClearLimitFlags(void);

#ifdef __cplusplus
}
#endif
#endif /* BSP_KEY_H */
