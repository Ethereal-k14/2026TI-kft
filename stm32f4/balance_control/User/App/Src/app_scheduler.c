/**
 * @file    app_scheduler.c
 * @brief   TIM6 1 kHz 任务调度器实现
 *
 *  利用 DWT_CYCCNT 精确测量每次 Run 的执行时间（用于验收：抖动 ≤5%）
 *  任务执行顺序：高频率先执行（安全 > 内环 > 100Hz > 外环 > 低频）
 */
#include "app_scheduler.h"
#include "app_safety.h"
#include "app_controller.h"
#include "app_estimator.h"
#include "app_identification.h"
#include "app_chassis.h"
#include "app_vision.h"
#include "app_debug.h"
#include "bsp_key.h"
#include "bsp_adc.h"
#include "bsp_encoder.h"
#include "bsp_lidar.h"
#include "bsp_oled_spi.h"
#include "tim.h"   /* htim6 */
#include "main.h"
#include "user_config.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 私有状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    volatile uint16_t pending_ticks;
    volatile uint32_t dropped_tick_count;

    /* 分频计数器 */
    uint32_t cnt_500hz;   /* 每 2 次 1 kHz 触发 500 Hz */
    uint32_t cnt_100hz;   /* 每 10 次触发 100 Hz */
    uint32_t cnt_50hz;    /* 每 20 次触发 50 Hz */
    uint32_t cnt_20hz;    /* 每 50 次触发 20 Hz */
    uint32_t cnt_10hz;    /* 每 100 次触发 10 Hz */

    /* 性能监测 */
    uint32_t loop_start_cyc;
    uint32_t max_loop_us;
    uint32_t late_tick_count;
    uint16_t max_pending_ticks;
} sched_ctx_t;

static sched_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：使能 DWT 周期计数器
 * ---------------------------------------------------------------------- */
static void dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0U;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void App_Scheduler_Init(void)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    dwt_enable();

    /* 启动 TIM6 中断（TIM6 已由 CubeMX 初始化为 1 kHz） */
    if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK)
    {
        Error_Handler();
    }
}

void App_Scheduler_Isr(void)
{
    if (s_ctx.pending_ticks < UINT16_MAX) {
        s_ctx.pending_ticks++;
    } else {
        s_ctx.dropped_tick_count++;
    }
}

void App_Scheduler_Run(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (s_ctx.pending_ticks == 0U) {
        if (primask == 0U) { __enable_irq(); }
        return;
    }
    if (s_ctx.pending_ticks > s_ctx.max_pending_ticks) {
        s_ctx.max_pending_ticks = s_ctx.pending_ticks;
    }
    if (s_ctx.pending_ticks > 1U) { s_ctx.late_tick_count++; }
    s_ctx.pending_ticks--;
    if (primask == 0U) { __enable_irq(); }

    /* 记录循环开始时间 */
    uint32_t cyc_start = DWT->CYCCNT;

    /* ---- 分频逻辑 ---- */
    s_ctx.cnt_500hz++;
    s_ctx.cnt_100hz++;
    s_ctx.cnt_50hz++;
    s_ctx.cnt_20hz++;
    s_ctx.cnt_10hz++;

    /* ---- 1 kHz 任务：安全检查 + 按键去抖 ---- */
    BSP_Key_Process();
    BSP_Encoder_Process();
    {
        sensor_sample_t adc_sample;
        BSP_Adc_GetSample(&adc_sample);
    }
    App_Safety_Check();

    /* ---- 检查按键事件 → 发送启动命令 ---- */
    if (BSP_Key_ConsumeStartEvent())
    {
        if (App_Safety_RequestStart())
        {
            if (USER_START_ACTION == USER_START_ACTION_STATIC_STEP) {
                (void)App_Identification_Start();
            } else {
                App_Controller_SetTargetPos(0);
                (void)App_Chassis_SendCmd(CHASSIS_CMD_START, 0U);
            }
        }
    }

    /* ---- 500 Hz 任务：内环 PID ---- */
    if (s_ctx.cnt_500hz >= 2U)
    {
        s_ctx.cnt_500hz = 0U;
        if (App_Safety_IsRunning())
        {
            App_Controller_InnerLoop();
        }
    }

    /* ---- 100 Hz 任务：通信解析 + 估计器 ---- */
    if (s_ctx.cnt_100hz >= 10U)
    {
        s_ctx.cnt_100hz = 0U;
        App_Chassis_Process();
        App_Vision_Process();
        BSP_Lidar_Process();
        App_Estimator_Update();
    }

    /* ---- 50 Hz 任务：外环 PID ---- */
    if (s_ctx.cnt_50hz >= 20U)
    {
        s_ctx.cnt_50hz = 0U;
        if (App_Safety_IsRunning())
        {
            App_Identification_Process();
            App_Controller_OuterLoop();
        }
    }

    /* ---- 20 Hz 任务：OLED 刷新 ---- */
    if (s_ctx.cnt_20hz >= 50U)
    {
        s_ctx.cnt_20hz = 0U;
        App_Debug_UpdateDisplay();
        (void)BSP_OledSpi_Flush();
    }

    /* ---- 10 Hz 任务：心跳 ---- */
    if (s_ctx.cnt_10hz >= 100U)
    {
        s_ctx.cnt_10hz = 0U;
        (void)App_Chassis_SendHeartbeat();
    }

    /* ---- 性能统计 ---- */
    uint32_t cyc_elapsed = DWT->CYCCNT - cyc_start;
    uint32_t us_elapsed  = cyc_elapsed / (SystemCoreClock / 1000000U);
    if (us_elapsed > s_ctx.max_loop_us)
    {
        s_ctx.max_loop_us = us_elapsed;
    }
}

uint32_t App_Scheduler_GetMaxLoopUs(void)
{
    return s_ctx.max_loop_us;
}

void App_Scheduler_GetDiag(scheduler_diag_t *out)
{
    if (out != NULL) {
        out->max_loop_us = s_ctx.max_loop_us;
        out->late_tick_count = s_ctx.late_tick_count;
        out->dropped_tick_count = s_ctx.dropped_tick_count;
        out->max_pending_ticks = s_ctx.max_pending_ticks;
    }
}
