/**
 * @file    bsp_stepper.c
 * @brief   TMC2209 步进电机驱动 BSP 实现
 *
 *  TIM8 已由 CubeMX 初始化为 Base 模式（Prescaler=167, Period=1000-1）。
 *  本文件在此基础上补充 CH1 PWM 输出通道配置（TIM_OC），通过修改 ARR/CCR
 *  实现运行时频率调整，无需停止定时器。
 *
 *  频率计算：
 *    TIM8 CLK = 168 MHz, Prescaler = 167 → 计数时钟 = 1 MHz
 *    ARR = (1 000 000 / freq_hz) - 1
 *    CCR = ARR / 2  （50% 占空比，高脉冲宽度 = 0.5/freq_hz ≥ 2 µs）
 */
#include "bsp_stepper.h"
#include "main.h"
#include "tim.h"   /* htim8 由 CubeMX 生成 */

/* -------------------------------------------------------------------------
 * 私有状态
 * ---------------------------------------------------------------------- */
static stepper_state_t s_state;
static bool            s_tim8_pwm_started = false;

/* -------------------------------------------------------------------------
 * 私有工具
 * ---------------------------------------------------------------------- */

/** 计算 ARR（自动重载值） */
static uint32_t freq_to_arr(uint32_t freq_hz)
{
    if (freq_hz < BSP_STEPPER_FREQ_MIN)
    {
        freq_hz = BSP_STEPPER_FREQ_MIN;
    }
    if (freq_hz > BSP_STEPPER_FREQ_MAX)
    {
        freq_hz = BSP_STEPPER_FREQ_MAX;
    }
    /* 1 MHz 计数时钟 / 频率 - 1 */
    return (1000000U / freq_hz) - 1U;
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void BSP_Stepper_Init(void)
{
    TIM_OC_InitTypeDef oc_cfg = {0};

    /* 上电默认状态 */
    s_state.freq_hz    = BSP_STEPPER_FREQ_DEFAULT;
    s_state.enabled    = false;
    s_state.dir_fwd    = true;
    s_state.diag_fault = false;

    /* ENN 保持高（禁用） — GPIO 已在 CubeMX gpio.c 中初始化为高 */
    HAL_GPIO_WritePin(TMC_ENN_GPIO_Port, TMC_ENN_Pin, GPIO_PIN_SET);

    /* MS1=MS2=SPREAD=高（16 微步，SpreadCycle） */
    HAL_GPIO_WritePin(TMC_MS1_GPIO_Port,    TMC_MS1_Pin,    GPIO_PIN_SET);
    HAL_GPIO_WritePin(TMC_MS2_GPIO_Port,    TMC_MS2_Pin,    GPIO_PIN_SET);
    HAL_GPIO_WritePin(TMC_SPREAD_GPIO_Port, TMC_SPREAD_Pin, GPIO_PIN_SET);

    /* DIR 默认正向 */
    HAL_GPIO_WritePin(TMC_DIR_GPIO_Port, TMC_DIR_Pin, GPIO_PIN_SET);

    /* 配置 TIM8 CH1 PWM 输出（补充 OC 配置） */
    uint32_t arr = freq_to_arr(BSP_STEPPER_FREQ_DEFAULT);
    __HAL_TIM_SET_AUTORELOAD(&htim8, arr);

    oc_cfg.OCMode       = TIM_OCMODE_PWM1;
    oc_cfg.Pulse        = arr / 2U;           /* 50% 占空比 */
    oc_cfg.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc_cfg.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    oc_cfg.OCFastMode   = TIM_OCFAST_DISABLE;
    oc_cfg.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc_cfg.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    if (HAL_TIM_PWM_ConfigChannel(&htim8, &oc_cfg, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    /* 注意：此时不启动 PWM，等待 BSP_Stepper_Enable(true) */
    s_tim8_pwm_started = false;
}

void BSP_Stepper_SetFreq(uint32_t freq_hz)
{
    if (freq_hz == 0U)
    {
        /* 零频率保持定时器运行但输出低电平，避免重新初始化定时器。 */
        __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
        s_state.freq_hz = 0U;
        return;
    }
    freq_hz = CLAMP(freq_hz, BSP_STEPPER_FREQ_MIN, BSP_STEPPER_FREQ_MAX);

    uint32_t arr = freq_to_arr(freq_hz);
    uint32_t ccr = arr / 2U;

    /* 运行时更新：先更新 ARR（带预装载），再更新 CCR */
    __HAL_TIM_SET_AUTORELOAD(&htim8, arr);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, ccr);

    s_state.freq_hz = freq_hz;
}

void BSP_Stepper_SetDir(bool fwd)
{
    s_state.dir_fwd = fwd;
    HAL_GPIO_WritePin(TMC_DIR_GPIO_Port, TMC_DIR_Pin,
                      fwd ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BSP_Stepper_Enable(bool en)
{
    if (en)
    {
        if (s_state.diag_fault)
        {
            /* 有故障时禁止使能 */
            return;
        }
        /* 先拉低 ENN，再启动 PWM */
        HAL_GPIO_WritePin(TMC_ENN_GPIO_Port, TMC_ENN_Pin, GPIO_PIN_RESET);
        if (!s_tim8_pwm_started)
        {
            /* 使能 TIM8 主输出（MOE）并启动 CH1 */
            __HAL_TIM_MOE_ENABLE(&htim8);
            if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1) != HAL_OK)
            {
                Error_Handler();
            }
            s_tim8_pwm_started = true;
        }
        s_state.enabled = true;
    }
    else
    {
        /* 先停止 PWM，再拉高 ENN */
        if (s_tim8_pwm_started)
        {
            HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);
            s_tim8_pwm_started = false;
        }
        HAL_GPIO_WritePin(TMC_ENN_GPIO_Port, TMC_ENN_Pin, GPIO_PIN_SET);
        s_state.enabled = false;
    }
}

void BSP_Stepper_EmergencyStop(void)
{
    /* 一个控制周期内完成：停脉冲 + 禁用驱动 */
    if (s_tim8_pwm_started)
    {
        /* 直接操作寄存器，最快速度停止 */
        htim8.Instance->CCER &= ~TIM_CCER_CC1E;  /* 关闭 CH1 输出 */
        s_tim8_pwm_started = false;
    }
    HAL_GPIO_WritePin(TMC_ENN_GPIO_Port, TMC_ENN_Pin, GPIO_PIN_SET);
    s_state.enabled = false;
}

void BSP_Stepper_SetMicrostep(stepper_microstep_t ms)
{
    /* 只能在驱动禁用时修改 */
    if (s_state.enabled)
    {
        return;
    }

    uint8_t ms1 = (((uint8_t)ms) >> 0U) & 1U;
    uint8_t ms2 = (((uint8_t)ms) >> 1U) & 1U;

    HAL_GPIO_WritePin(TMC_MS1_GPIO_Port, TMC_MS1_Pin,
                      ms1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TMC_MS2_GPIO_Port, TMC_MS2_Pin,
                      ms2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BSP_Stepper_DiagIsr(void)
{
    /* 中断上下文：立即急停 */
    BSP_Stepper_EmergencyStop();
    s_state.diag_fault = true;
}

void BSP_Stepper_GetState(stepper_state_t *out)
{
    if (out != NULL)
    {
        *out = s_state;
    }
}

void BSP_Stepper_ClearFault(void)
{
    s_state.diag_fault = false;
}
