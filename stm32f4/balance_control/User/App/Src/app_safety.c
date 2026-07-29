/**
 * @file    app_safety.c
 * @brief   安全状态机实现
 */
#include "app_safety.h"
#include "app_controller.h"
#include "app_estimator.h"
#include "app_chassis.h"
#include "app_vision.h"
#include "bsp_stepper.h"
#include "bsp_key.h"
#include "bsp_adc.h"
#include "bsp_encoder.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 私有状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    safety_cfg_t    cfg;
    safety_state_t  state;
    uint32_t        fault_mask;
    uint32_t        mismatch_start_ms; /* 传感器不一致开始时间 */
    bool            mismatch_active;
} safety_ctx_t;

static safety_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：立即执行急停动作
 * ---------------------------------------------------------------------- */
static void safety_do_estop(void)
{
    BSP_Stepper_EmergencyStop();
    App_Controller_Reset();
    App_Estimator_Reset();
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void App_Safety_Init(const safety_cfg_t *cfg)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = SAFETY_STATE_IDLE;

    if (cfg != NULL)
    {
        s_ctx.cfg = *cfg;
    }
    else
    {
        s_ctx.cfg.sensor_mismatch_threshold_mrad = 100;  /* 100 mrad ≈ 5.7° */
        s_ctx.cfg.sensor_mismatch_persist_ms     = 200U; /* 200 ms 持续超限 */
    }
}

void App_Safety_Check(void)
{
    /* ---- 限位开关 ---- */
    key_limit_state_t ks;
    BSP_Key_GetState(&ks);

    if (ks.limit_min_active || ks.limit_min_triggered)
    {
        App_Safety_EmergencyStop(FAULT_LIMIT_MIN);
        return;
    }
    if (ks.limit_max_active || ks.limit_max_triggered)
    {
        App_Safety_EmergencyStop(FAULT_LIMIT_MAX);
        return;
    }

    /* ---- TMC DIAG ---- */
    stepper_state_t st;
    BSP_Stepper_GetState(&st);
    if (st.diag_fault)
    {
        App_Safety_EmergencyStop(FAULT_DIAG);
        return;
    }

    /* ---- 传感器不一致（ABZ 与电位器） ---- */
    if (s_ctx.state == SAFETY_STATE_RUNNING)
    {
        sensor_sample_t adc;
        encoder_state_t enc;
        BSP_Adc_GetSample(&adc);
        BSP_Encoder_GetState(&enc);

        if (adc.valid)
        {
            /* 两者均应映射到 mrad，此处用原始计数差（标定后替换） */
            int32_t diff = adc.value - enc.position_count;
            if (diff < 0) { diff = -diff; }

            if (diff > s_ctx.cfg.sensor_mismatch_threshold_mrad)
            {
                if (!s_ctx.mismatch_active)
                {
                    s_ctx.mismatch_active    = true;
                    s_ctx.mismatch_start_ms  = HAL_GetTick();
                }
                else
                {
                    uint32_t elapsed = HAL_GetTick() - s_ctx.mismatch_start_ms;
                    if (elapsed > s_ctx.cfg.sensor_mismatch_persist_ms)
                    {
                        App_Safety_EmergencyStop(FAULT_SENSOR_MISMATCH);
                        return;
                    }
                }
            }
            else
            {
                s_ctx.mismatch_active = false;
            }
        }

        /* ---- 通信超时检测 ---- */
        chassis_imu_t imu;
        App_Chassis_GetImu(&imu);
        if (imu.comm_degraded)
        {
            /* 仅置标志，不立即急停（通信降级可继续运行，但权重归零） */
            s_ctx.fault_mask |= FAULT_COMM_TIMEOUT;
        }
        else
        {
            s_ctx.fault_mask &= ~FAULT_COMM_TIMEOUT;
        }
    }
}

void App_Safety_EmergencyStop(uint32_t fault_mask)
{
    s_ctx.fault_mask |= fault_mask;
    s_ctx.state       = SAFETY_STATE_FAULT;
    safety_do_estop();
}

bool App_Safety_RequestStart(void)
{
    if (s_ctx.state != SAFETY_STATE_IDLE)
    {
        return false;
    }
    if (s_ctx.fault_mask != 0U)
    {
        return false;
    }
    s_ctx.state = SAFETY_STATE_RUNNING;
    /* 使能步进驱动 */
    BSP_Stepper_Enable(true);
    return true;
}

void App_Safety_RequestStop(void)
{
    if (s_ctx.state == SAFETY_STATE_RUNNING)
    {
        safety_do_estop();
        s_ctx.state = SAFETY_STATE_IDLE;
    }
}

void App_Safety_ClearFault(void)
{
    if (s_ctx.state != SAFETY_STATE_FAULT)
    {
        return;
    }
    /* 仅当物理故障已解除（限位已恢复、DIAG 已清）时允许清除 */
    key_limit_state_t ks;
    BSP_Key_GetState(&ks);
    stepper_state_t   st;
    BSP_Stepper_GetState(&st);

    bool hw_clear = (!ks.limit_min_active) && (!ks.limit_max_active) && (!st.diag_fault);
    if (hw_clear)
    {
        s_ctx.fault_mask = 0U;
        s_ctx.state      = SAFETY_STATE_IDLE;
        BSP_Key_ClearLimitFlags();
        BSP_Stepper_ClearFault();
        s_ctx.mismatch_active = false;
    }
}

safety_state_t App_Safety_GetState(void)
{
    return s_ctx.state;
}

uint32_t App_Safety_GetFaultMask(void)
{
    return s_ctx.fault_mask;
}

bool App_Safety_IsRunning(void)
{
    return (s_ctx.state == SAFETY_STATE_RUNNING);
}
