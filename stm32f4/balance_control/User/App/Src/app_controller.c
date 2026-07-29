/**
 * @file app_controller.c
 * @brief Sensor/actuator adapter for ball_control_core.
 */
#include "app_controller.h"
#include "app_estimator.h"
#include "app_chassis.h"
#include "bsp_encoder.h"
#include "bsp_stepper.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    ctrl_cfg_t cfg;
    ball_ctrl_core_t core;
    ctrl_output_t output;
    ctrl_target_owner_t target_owner;
    bool initialized;
} ctrl_ctx_t;

static ctrl_ctx_t s_ctx;

static bool adapter_cfg_valid(const ctrl_cfg_t *cfg)
{
    return (cfg != NULL) && (cfg->mrad_per_count > 0.0f) &&
           (cfg->max_pos_um > 0);
}

static void copy_output(void)
{
    ball_ctrl_core_output_t core;
    float signed_freq;
    BallCtrlCore_GetOutput(&s_ctx.core, &core);
    signed_freq = core.signed_step_freq_hz;
    s_ctx.output.dir_fwd = signed_freq >= 0.0f;
    s_ctx.output.target_step_freq_hz = (int32_t)fabsf(signed_freq);
    s_ctx.output.target_angle_mrad = (int32_t)core.target_angle_mrad;
    s_ctx.output.reference_pos_um = (int32_t)(core.reference_pos_mm * 1000.0f);
    s_ctx.output.reference_vel_um_s =
        (int32_t)(core.reference_vel_mm_s * 1000.0f);
    s_ctx.output.measurement_valid = core.measurement_valid;
    s_ctx.output.saturated = core.saturated;
}

void App_Controller_Init(const ctrl_cfg_t *cfg)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.cfg.mrad_per_count = 1.570796f;
    s_ctx.cfg.max_pos_um = 200000;
    BallCtrlCore_Init(&s_ctx.core, NULL);
    if (cfg != NULL) {
        (void)App_Controller_Configure(cfg);
    }
    s_ctx.initialized = true;
}

bool App_Controller_Configure(const ctrl_cfg_t *cfg)
{
    if (!adapter_cfg_valid(cfg) ||
        !BallCtrlCore_Configure(&s_ctx.core, &cfg->core)) {
        return false;
    }
    s_ctx.cfg = *cfg;
    s_ctx.initialized = true;
    return true;
}

bool App_Controller_SetProfile(ball_ctrl_profile_t profile)
{
    return BallCtrlCore_SetProfile(&s_ctx.core, profile);
}

void App_Controller_InnerLoop(void)
{
    encoder_state_t enc;
    chassis_imu_t imu;
    ball_ctrl_inner_input_t in;
    ball_ctrl_core_output_t core;
    uint32_t freq;
    if (!s_ctx.initialized) { return; }
    BSP_Encoder_GetState(&enc);
    App_Chassis_GetImu(&imu);
    in.angle_mrad = (float)enc.position_count * s_ctx.cfg.mrad_per_count;
    in.rate_mrad_s = (float)enc.velocity_count_s * s_ctx.cfg.mrad_per_count;
    in.chassis_accel_mm_s2 = imu.valid ? (float)imu.ax_mm_s2 : 0.0f;
    in.chassis_ff_weight = imu.valid ? imu.feedfwd_weight : 0.0f;
    in.dt_s = 0.002f;
    in.enabled = enc.index_valid || enc.pwm_valid;
    (void)BallCtrlCore_StepInner(&s_ctx.core, &in);
    BallCtrlCore_GetOutput(&s_ctx.core, &core);
    if (!in.enabled) {
        BSP_Stepper_SetFreq(0U);
        copy_output();
        return;
    }
    BSP_Stepper_SetDir(core.signed_step_freq_hz >= 0.0f);
    freq = (uint32_t)fabsf(core.signed_step_freq_hz);
    if (freq < BSP_STEPPER_FREQ_MIN) { freq = 0U; }
    BSP_Stepper_SetFreq(freq);
    copy_output();
}

void App_Controller_OuterLoop(void)
{
    estimator_state_t est;
    ball_ctrl_outer_input_t in;
    if (!s_ctx.initialized) { return; }
    App_Estimator_GetState(&est);
    in.pos_mm = (float)est.pos_um * 0.001f;
    in.vel_mm_s = (float)est.vel_um_s * 0.001f;
    in.dt_s = 0.02f;
    in.valid = est.valid;
    (void)BallCtrlCore_StepOuter(&s_ctx.core, &in);
    copy_output();
}

void App_Controller_GetOutput(ctrl_output_t *out)
{
    if (out != NULL) { *out = s_ctx.output; }
}

void App_Controller_Reset(void)
{
    estimator_state_t est;
    App_Estimator_GetState(&est);
    BallCtrlCore_Reset(&s_ctx.core, (float)est.pos_um * 0.001f);
    BSP_Stepper_SetFreq(0U);
    (void)memset(&s_ctx.output, 0, sizeof(s_ctx.output));
    s_ctx.target_owner = CTRL_TARGET_OWNER_OPERATOR;
    BallCtrlCore_SetTarget(&s_ctx.core, 0.0f);
}

void App_Controller_SetTargetPos(int32_t target_um)
{
    (void)App_Controller_SetTargetOwned(CTRL_TARGET_OWNER_OPERATOR, target_um);
}

bool App_Controller_SetTargetOwned(ctrl_target_owner_t owner,
                                   int32_t target_um)
{
    int32_t bounded = target_um;
    if ((owner > CTRL_TARGET_OWNER_COMPETITION) ||
        (owner < s_ctx.target_owner)) {
        return false;
    }
    if (bounded > s_ctx.cfg.max_pos_um) { bounded = s_ctx.cfg.max_pos_um; }
    if (bounded < -s_ctx.cfg.max_pos_um) { bounded = -s_ctx.cfg.max_pos_um; }
    s_ctx.target_owner = owner;
    BallCtrlCore_SetTarget(&s_ctx.core, (float)bounded * 0.001f);
    return true;
}

void App_Controller_ReleaseTarget(ctrl_target_owner_t owner)
{
    if (owner == s_ctx.target_owner) {
        s_ctx.target_owner = CTRL_TARGET_OWNER_OPERATOR;
    }
}

ctrl_target_owner_t App_Controller_GetTargetOwner(void)
{
    return s_ctx.target_owner;
}
