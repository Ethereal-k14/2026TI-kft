/**
 * @file    bsp_stepper.h
 * @brief   TMC2209 步进电机驱动 BSP 接口（规范 §2、§6）
 *
 *  STEP  : TIM8_CH1 / PC6，PWM 输出，ARR 决定频率，CCR = ARR/2（50% 占空比）
 *  DIR   : PC7，GPIO 输出
 *  ENN   : PC8，GPIO 输出，低有效；上电默认高（禁用）
 *  DIAG  : PC9，EXTI9，上升沿故障检测
 *  MS1   : PE2，MS2：PE3，SPREAD：PE4，GPIO 输出
 *
 *  安全规则：
 *  - 上电 ENN 保持高，调用 BSP_Stepper_Enable(true) 才使能
 *  - 微步配置（MS1/MS2）只能在 ENN=高（驱动禁用）时更改
 *  - DIAG 触发后自动禁用并设 diag_fault 标志
 */
#ifndef BSP_STEPPER_H
#define BSP_STEPPER_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 配置常量
 * ---------------------------------------------------------------------- */
#define BSP_STEPPER_TIM_CLK_HZ   168000000UL  /* APB2 定时器时钟 168 MHz */
#define BSP_STEPPER_PRESCALER    (168U - 1U)  /* → 1 MHz 计数基准 */
#define BSP_STEPPER_FREQ_DEFAULT 1000U        /* 上电默认步频 1 kHz */
#define BSP_STEPPER_FREQ_MIN     10U          /* 最小步频（Hz） */
#define BSP_STEPPER_FREQ_MAX     50000U       /* 最大步频（Hz），根据机构限制 */

/** 微步模式（MS1, MS2 电平组合）
 *  参见 TMC2209 数据手册 Table 6 */
typedef enum
{
    STEPPER_MICROSTEP_8   = 0U, /* MS1=0, MS2=0 */
    STEPPER_MICROSTEP_2   = 1U, /* MS1=1, MS2=0 */
    STEPPER_MICROSTEP_4   = 2U, /* MS1=0, MS2=1 */
    STEPPER_MICROSTEP_16  = 3U, /* MS1=1, MS2=1（默认，规范要求） */
} stepper_microstep_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 初始化步进电机 BSP
 *        - 配置 TIM8 CH1 PWM 输出（CubeMX 仅初始化 Base，此处补全 OC）
 *        - ENN 保持高（驱动禁用）
 *        - MS1=MS2=SPREAD=高（16 微步，SpreadCycle）
 *        必须在 MX_TIM8_Init()、MX_GPIO_Init() 之后调用
 */
void BSP_Stepper_Init(void);

/**
 * @brief 设置 STEP 脉冲频率（运行时实时更新 ARR/CCR）
 * @param freq_hz  目标步频（Hz），自动限幅到 [FREQ_MIN, FREQ_MAX]
 */
void BSP_Stepper_SetFreq(uint32_t freq_hz);

/**
 * @brief 设置运动方向
 * @param fwd  true = 正向（DIR=高），false = 反向（DIR=低）
 */
void BSP_Stepper_SetDir(bool fwd);

/**
 * @brief 使能/禁用驱动器（写 ENN 引脚）
 * @param en  true = 使能（ENN 拉低），false = 禁用（ENN 拉高）
 */
void BSP_Stepper_Enable(bool en);

/**
 * @brief 立即停止脉冲输出并禁用驱动器（安全停机）
 *        在一个控制周期内完成：停止 TIM8 PWM + 拉高 ENN
 */
void BSP_Stepper_EmergencyStop(void);

/**
 * @brief 设置微步模式（仅在驱动禁用时调用）
 * @param ms 微步枚举值
 */
void BSP_Stepper_SetMicrostep(stepper_microstep_t ms);

/**
 * @brief DIAG 引脚 EXTI9 中断入口（在 EXTI9_5_IRQHandler 中调用）
 *        自动执行急停并置故障标志
 */
void BSP_Stepper_DiagIsr(void);

/**
 * @brief 读取步进电机当前状态
 * @param out 非 NULL 输出指针
 */
void BSP_Stepper_GetState(stepper_state_t *out);

/**
 * @brief 清除 DIAG 故障标志（需外部安全逻辑确认后调用）
 */
void BSP_Stepper_ClearFault(void);

#ifdef __cplusplus
}
#endif
#endif /* BSP_STEPPER_H */
