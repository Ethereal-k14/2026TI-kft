/**
 * @file    app_safety.c
 * @brief   安全状态机实现
 */
#include "app_safety.h"
#include "app_controller.h"
#include "app_estimator.h"
#include "app_identification.h"
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
    uint32_t        warning_mask;
    uint32_t        mismatch_start_ms; /* 传感器不一致开始时间 */
    bool            mismatch_active;
    uint32_t        encoder_invalid_start_ms;
    uint32_t        vision_invalid_start_ms;
    bool            encoder_invalid_active;
    bool            vision_invalid_active;
    bool            chassis_required;
} safety_ctx_t;

static safety_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：立即执行急停动作
 * ---------------------------------------------------------------------- */
static void safety_do_estop(void)
{
    (void)App_Chassis_SendCmd(CHASSIS_CMD_ESTOP, 0xFFU);
    BSP_Stepper_EmergencyStop();
    App_Identification_Abort();
    App_Controller_Reset();
    App_Estimator_Reset();
}

static void safety_do_normal_stop(void)
{
    (void)App_Chassis_SendCmd(CHASSIS_CMD_STOP, 0U);
    BSP_Stepper_SetFreq(0U);
    BSP_Stepper_Enable(false);
    App_Identification_Abort();
    App_Controller_Reset();
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void App_Safety_Init(const safety_cfg_t *cfg)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = SAFETY_STATE_IDLE;
    s_ctx.chassis_required = true;

    if (cfg != NULL)
    {
        s_ctx.cfg = *cfg;
    }
    else
    {
        s_ctx.cfg.sensor_mismatch_threshold_mrad = 100;  /* 100 mrad ≈ 5.7° */
        s_ctx.cfg.sensor_mismatch_persist_ms     = 200U; /* 200 ms 持续超限 */
        s_ctx.cfg.encoder_invalid_persist_ms     = 100U;
        s_ctx.cfg.vision_invalid_persist_ms      = 400U;
        s_ctx.cfg.encoder_mrad_per_count         = 1.570796f;
        s_ctx.cfg.vision_required                = true;
    }
}

void App_Safety_SetChassisRequired(bool required)
{
    if (s_ctx.state == SAFETY_STATE_IDLE) {
        s_ctx.chassis_required = required;
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

        if (adc.valid && BSP_Adc_IsCalibrated() &&
            (enc.index_valid || enc.pwm_valid))
        {
            const int32_t encoder_mrad = (int32_t)(
                (float)enc.position_count * s_ctx.cfg.encoder_mrad_per_count);
            int32_t diff = adc.value - encoder_mrad;
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
                        s_ctx.warning_mask |= SAFETY_WARN_SENSOR_MISMATCH;
                    }
                }
            }
            else
            {
                s_ctx.mismatch_active = false;
                s_ctx.warning_mask &= ~SAFETY_WARN_SENSOR_MISMATCH;
            }
        }
        else
        {
            /* No comparable pair: a previous mismatch is no longer active. */
            s_ctx.mismatch_active = false;
            s_ctx.warning_mask &= ~SAFETY_WARN_SENSOR_MISMATCH;
        }

        const bool encoder_valid = enc.index_valid || enc.pwm_valid;
        const bool adc_fallback_valid = adc.valid && BSP_Adc_IsCalibrated();
        if (!encoder_valid && adc_fallback_valid)
        {
            s_ctx.encoder_invalid_active = false;
            s_ctx.warning_mask |= SAFETY_WARN_ANGLE_FALLBACK;
        }
        else if (!encoder_valid)
        {
            if (!s_ctx.encoder_invalid_active)
            {
                s_ctx.encoder_invalid_active = true;
                s_ctx.encoder_invalid_start_ms = HAL_GetTick();
            }
            else if ((HAL_GetTick() - s_ctx.encoder_invalid_start_ms) >=
                     s_ctx.cfg.encoder_invalid_persist_ms)
            {
                App_Safety_EmergencyStop(FAULT_ENCODER_INVALID);
                return;
            }
        }
        else
        {
            s_ctx.encoder_invalid_active = false;
            s_ctx.warning_mask &= ~SAFETY_WARN_ANGLE_FALLBACK;
        }

        if (s_ctx.cfg.vision_required)
        {
            estimator_state_t est;
            App_Estimator_GetState(&est);
            if (!est.valid)
            {
                s_ctx.warning_mask |= SAFETY_WARN_VISION_PREDICT;
                if (!s_ctx.vision_invalid_active)
                {
                    s_ctx.vision_invalid_active = true;
                    s_ctx.vision_invalid_start_ms = HAL_GetTick();
                }
                else if ((HAL_GetTick() - s_ctx.vision_invalid_start_ms) >=
                         s_ctx.cfg.vision_invalid_persist_ms)
                {
                    App_Safety_EmergencyStop(FAULT_VISION_LOST);
                    return;
                }
            }
            else
            {
                s_ctx.vision_invalid_active = false;
                s_ctx.warning_mask &= ~SAFETY_WARN_VISION_PREDICT;
            }
        }

        /* 链路不是本地闭环的必需条件：底盘自主循迹，
           上层在 IMU 陈旧时已将前馈平滑归零。 */
        if (s_ctx.chassis_required && !App_Chassis_IsHealthy()) {
            s_ctx.warning_mask |= SAFETY_WARN_CHASSIS;
        } else {
            s_ctx.warning_mask &= ~SAFETY_WARN_CHASSIS;
        }
    }
}

void App_Safety_EmergencyStop(uint32_t fault_mask)
{
    const bool first_transition = s_ctx.state != SAFETY_STATE_FAULT;
    s_ctx.fault_mask |= fault_mask;
    s_ctx.state       = SAFETY_STATE_FAULT;
    if (first_transition) {
        safety_do_estop();
    }
}

bool App_Safety_RequestStart(void)
{
    encoder_state_t enc;
    estimator_state_t est;
    sensor_sample_t adc;
    if (s_ctx.state != SAFETY_STATE_IDLE)
    {
        return false;
    }
    s_ctx.warning_mask = 0U;
    /* Chassis IMU is feedforward-only; its timeout must not block static
       camera/encoder feedback control. All physical/feedback faults do. */
    if (s_ctx.fault_mask != 0U)
    {
        return false;
    }
    BSP_Encoder_GetState(&enc);
    App_Estimator_GetState(&est);
    BSP_Adc_GetSample(&adc);
    if (!(enc.index_valid || enc.pwm_valid) &&
        !(adc.valid && BSP_Adc_IsCalibrated())) {
        s_ctx.warning_mask |= SAFETY_WARN_ANGLE_FALLBACK;
        return false;
    }
    if (s_ctx.cfg.vision_required && !est.valid) {
        s_ctx.warning_mask |= SAFETY_WARN_VISION_PREDICT;
        return false;
    }
    if (s_ctx.chassis_required && !App_Chassis_IsHealthy()) {
        s_ctx.warning_mask |= SAFETY_WARN_CHASSIS;
    }
    if ((enc.index_valid || enc.pwm_valid) &&
        adc.valid && BSP_Adc_IsCalibrated()) {
        const int32_t encoder_mrad = (int32_t)(
            (float)enc.position_count * s_ctx.cfg.encoder_mrad_per_count);
        int32_t diff = adc.value - encoder_mrad;
        if (diff < 0) { diff = -diff; }
        if (diff > s_ctx.cfg.sensor_mismatch_threshold_mrad) {
            s_ctx.warning_mask |= SAFETY_WARN_SENSOR_MISMATCH;
            return false;
        }
    }
    s_ctx.state = SAFETY_STATE_RUNNING;
    /* 使能步进驱动 */
    BSP_Stepper_Enable(true);
    return true;
}

void App_Safety_RequestStop(void)
{
    if (s_ctx.state == SAFETY_STATE_RUNNING) {
        safety_do_normal_stop();
        s_ctx.state = SAFETY_STATE_IDLE;
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

void App_Safety_SetWarning(uint32_t warning_mask, bool active)
{
    if (active) {
        s_ctx.warning_mask |= warning_mask;
    } else {
        s_ctx.warning_mask &= ~warning_mask;
    }
}

uint32_t App_Safety_GetWarningMask(void)
{
    return s_ctx.warning_mask;
}

bool App_Safety_IsRunning(void)
{
    return (s_ctx.state == SAFETY_STATE_RUNNING);
}
