/**
 * @file    bsp_encoder.c
 * @brief   磁编码器 BSP 实现
 *
 *  TIM2/TIM5 由 CubeMX 外设层配置；本模块只负责启动、读取和校验。
 *  速度计算采用差分法：v = (cnt_now - cnt_prev) / dt_s
 */
#include "bsp_encoder.h"
#include "main.h"
#include "tim.h"   /* htim2/htim5 由 CubeMX 外设层生成 */
#include <string.h>

/* -------------------------------------------------------------------------
 * 私有状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  cnt_prev;           /* 上一次读取的计数值 */
    uint32_t ts_prev_us;         /* 上一次读取的时间戳 */
    int32_t  index_offset;       /* Z 脉冲捕获时的计数值（机械零点偏移） */
    encoder_state_t state;       /* 对外暴露的状态 */
    bool     initialized;
    uint32_t counts_per_rev;     /* 每转计数，用于换算角度 */

    /* PWM 捕获状态 */
    uint32_t pwm_period_cnt;     /* 捕获到的 PWM 周期（TIM5 计数） */
    uint32_t pwm_high_cnt;       /* 捕获到的高电平（TIM5 计数） */
    bool     pwm_capture_done;
} enc_ctx_t;

static enc_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：启动 TIM2 Encoder 模式
 * ---------------------------------------------------------------------- */
static void tim2_encoder_init(void)
{
    if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK)
    {
        Error_Handler();
    }
}

/* -------------------------------------------------------------------------
 * 私有：启动 TIM5 PWM 输入捕获（备用角度）
 * ---------------------------------------------------------------------- */
static void tim5_pwm_capture_init(void)
{
    if (HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void BSP_Encoder_Init(void)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.counts_per_rev = BSP_ENC_COUNTS_PER_REV;

    tim2_encoder_init();
    tim5_pwm_capture_init();

    /* 读取初始计数作为基准 */
    s_ctx.cnt_prev    = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
    s_ctx.ts_prev_us  = BSP_GetTimestampUs();
    /* Relative-zero fallback allows commissioning before the Z wire is used.
       A later Z edge atomically replaces this software zero. */
    s_ctx.index_offset = s_ctx.cnt_prev;
    s_ctx.state.position_count = 0;
    s_ctx.state.index_valid = true;
    s_ctx.initialized = true;
}

void BSP_Encoder_Process(void)
{
    if (!s_ctx.initialized)
    {
        return;
    }

    uint32_t now_us   = BSP_GetTimestampUs();
    int32_t  cnt_now  = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);

    /* 差分计算速度（有符号，支持计数器回绕） */
    int32_t  delta_cnt = cnt_now - s_ctx.cnt_prev;
    uint32_t dt_us     = now_us - s_ctx.ts_prev_us;

    if (dt_us > 0U)
    {
        /* velocity = Δcount / Δt(s) → 乘以 1e6 后除以 dt_us（避免浮点） */
        s_ctx.state.velocity_count_s = (int32_t)(((int64_t)delta_cnt * 1000000) / (int64_t)dt_us);
    }

    s_ctx.cnt_prev   = cnt_now;
    s_ctx.ts_prev_us = now_us;

    /* 相对零点的位置 */
    s_ctx.state.position_count = cnt_now - s_ctx.index_offset;

    /* PWM 备用通道：计算角度（若捕获完成） */
    if (s_ctx.pwm_capture_done)
    {
        s_ctx.pwm_capture_done    = false;
        s_ctx.state.pwm_valid     = (s_ctx.pwm_period_cnt > 0U);
        if (s_ctx.state.pwm_valid)
        {
            uint32_t duty = (uint32_t)(((uint64_t)s_ctx.pwm_high_cnt *
                                        1000ULL) / s_ctx.pwm_period_cnt);
            s_ctx.state.pwm_duty_permille =
                (uint16_t)((duty > 1000U) ? 1000U : duty);
        }
    }
}

void BSP_Encoder_ZIndexIsr(void)
{
    /* 中断上下文：只记录计数和标志 */
    s_ctx.index_offset       = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
    s_ctx.state.index_valid  = true;
}

void BSP_Encoder_PwmCaptureCallback(void)
{
    /* CH1=周期，CH2=高电平；两路由同一 PWM 输入内部映射。 */
    s_ctx.pwm_period_cnt   = HAL_TIM_ReadCapturedValue(&htim5, TIM_CHANNEL_1);
    s_ctx.pwm_high_cnt     = HAL_TIM_ReadCapturedValue(&htim5, TIM_CHANNEL_2);
    s_ctx.pwm_capture_done = true;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if ((htim != NULL) && (htim->Instance == TIM5))
    {
        BSP_Encoder_PwmCaptureCallback();
    }
}

void BSP_Encoder_GetState(encoder_state_t *out)
{
    if (out != NULL)
    {
        *out = s_ctx.state;
    }
}

void BSP_Encoder_GetAngleSample(sensor_sample_t *out)
{
    if (out == NULL)
    {
        return;
    }
    out->value        = s_ctx.state.position_count;
    out->timestamp_us = s_ctx.ts_prev_us;
    out->age_ms       = 0U;
    out->quality      = s_ctx.state.index_valid ? 200U : 128U;
    out->valid        = s_ctx.initialized;
}

void BSP_Encoder_SetCountsPerRev(uint32_t counts)
{
    if (counts > 0U)
    {
        s_ctx.counts_per_rev = counts;
    }
}

void BSP_Encoder_ResetIndex(void)
{
    s_ctx.index_offset      = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
    s_ctx.state.index_valid = true;
}
