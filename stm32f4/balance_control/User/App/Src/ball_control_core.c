/**
 * @file ball_control_core.c
 * @brief Hardware-independent ball-on-beam controller implementation.
 */
#include "ball_control_core.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define BALL_GRAVITY_MM_S2 (9806.65f)
#define BALL_BEAM_MODEL_FACTOR (1.4f) /* solid ball: theta = 7/5 * a/g */

static float clampf(float v, float lo, float hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

static float slew(float current, float target, float delta)
{
    return current + clampf(target - current, -delta, delta);
}

static bool cfg_valid(const ball_ctrl_core_cfg_t *c)
{
    return (c != NULL) &&
           (c->pos_kp_mrad_per_mm >= 0.0f) &&
           (c->pos_ki_mrad_per_mm_s >= 0.0f) &&
           (c->vel_kd_mrad_per_mm_s >= 0.0f) &&
           (c->robust_gain_mrad >= 0.0f) &&
           (c->robust_boundary_mm > 0.0f) &&
           (c->model_ff_scale >= 0.0f) &&
           (c->model_ff_scale <= 2.0f) &&
           (c->max_ref_vel_mm_s > 0.0f) &&
           (c->max_ref_accel_mm_s2 > 0.0f) &&
           (c->max_ref_jerk_mm_s3 > 0.0f) &&
           (c->max_angle_mrad > 0.0f) &&
           (c->angle_kp_per_s > 0.0f) &&
           (c->rate_kp_hz_per_mrad_s > 0.0f) &&
           (c->rate_ki_hz_per_mrad >= 0.0f) &&
           (c->rate_integral_limit_hz >= 0.0f) &&
           (c->max_step_freq_hz > 0.0f);
}

static ball_ctrl_core_cfg_t profile_cfg(ball_ctrl_profile_t profile)
{
    ball_ctrl_core_cfg_t c = {
        2.4f, 0.12f, 0.75f, 8.0f, 4.0f, 1.0f,
        90.0f, 320.0f, 1800.0f, 180.0f,
        5.0f, 10.0f, 1.0f, 2500.0f, 0.0f, 20000.0f
    };
    switch (profile) {
    case BALL_CTRL_PROFILE_SAFE:
        c.pos_kp_mrad_per_mm = 1.6f;
        c.robust_gain_mrad = 5.0f;
        c.max_ref_vel_mm_s = 55.0f;
        c.max_ref_accel_mm_s2 = 180.0f;
        c.max_ref_jerk_mm_s3 = 900.0f;
        c.max_angle_mrad = 110.0f;
        c.max_step_freq_hz = 12000.0f;
        break;
    case BALL_CTRL_PROFILE_PRECISION:
        c.pos_kp_mrad_per_mm = 2.8f;
        c.pos_ki_mrad_per_mm_s = 0.18f;
        c.vel_kd_mrad_per_mm_s = 0.9f;
        c.robust_gain_mrad = 10.0f;
        c.robust_boundary_mm = 2.5f;
        c.max_ref_vel_mm_s = 75.0f;
        c.max_ref_accel_mm_s2 = 260.0f;
        c.max_angle_mrad = 150.0f;
        break;
    case BALL_CTRL_PROFILE_FAST:
        c.pos_kp_mrad_per_mm = 3.0f;
        c.vel_kd_mrad_per_mm_s = 0.85f;
        c.robust_gain_mrad = 10.0f;
        c.max_ref_vel_mm_s = 140.0f;
        c.max_ref_accel_mm_s2 = 600.0f;
        c.max_ref_jerk_mm_s3 = 3500.0f;
        c.max_angle_mrad = 240.0f;
        c.max_step_freq_hz = 28000.0f;
        break;
    case BALL_CTRL_PROFILE_BALANCED:
    default:
        break;
    }
    return c;
}

void BallCtrlCore_Init(ball_ctrl_core_t *ctx,
                       const ball_ctrl_core_cfg_t *cfg)
{
    if (ctx == NULL) { return; }
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg_valid(cfg) ? *cfg : profile_cfg(BALL_CTRL_PROFILE_BALANCED);
    ctx->initialized = true;
}

void BallCtrlCore_Reset(ball_ctrl_core_t *ctx, float measured_pos_mm)
{
    ball_ctrl_core_cfg_t cfg;
    float target;
    if ((ctx == NULL) || !ctx->initialized) { return; }
    cfg = ctx->cfg;
    target = ctx->output.target_pos_mm;
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg;
    ctx->output.target_pos_mm = target;
    ctx->output.reference_pos_mm = measured_pos_mm;
    ctx->initialized = true;
}

bool BallCtrlCore_Configure(ball_ctrl_core_t *ctx,
                            const ball_ctrl_core_cfg_t *cfg)
{
    if ((ctx == NULL) || !cfg_valid(cfg)) { return false; }
    if (!ctx->initialized) {
        (void)memset(ctx, 0, sizeof(*ctx));
        ctx->initialized = true;
    }
    ctx->cfg = *cfg;
    BallCtrlCore_Reset(ctx, ctx->output.reference_pos_mm);
    return true;
}

bool BallCtrlCore_SetProfile(ball_ctrl_core_t *ctx,
                             ball_ctrl_profile_t profile)
{
    ball_ctrl_core_cfg_t cfg;
    if ((ctx == NULL) || (profile > BALL_CTRL_PROFILE_FAST)) { return false; }
    cfg = profile_cfg(profile);
    return BallCtrlCore_Configure(ctx, &cfg);
}

void BallCtrlCore_SetTarget(ball_ctrl_core_t *ctx, float target_pos_mm)
{
    if ((ctx != NULL) && ctx->initialized) {
        ctx->output.target_pos_mm = target_pos_mm;
    }
}

bool BallCtrlCore_StepOuter(ball_ctrl_core_t *ctx,
                            const ball_ctrl_outer_input_t *in)
{
    float target_vel;
    float desired_accel;
    float position_error;
    float velocity_error;
    float feedback_angle;
    float model_angle;
    float robust_angle;
    float angle;
    float dt;
    if ((ctx == NULL) || !ctx->initialized || (in == NULL)) { return false; }
    dt = clampf(in->dt_s, 0.005f, 0.05f);
    ctx->output.measurement_valid = in->valid;
    if (!in->valid || !isfinite(in->pos_mm) || !isfinite(in->vel_mm_s)) {
        ctx->output.target_angle_mrad = slew(ctx->output.target_angle_mrad,
                                             0.0f, 1000.0f * dt);
        ctx->pos_integral_mm_s = 0.0f;
        return false;
    }

    position_error = ctx->output.target_pos_mm - ctx->output.reference_pos_mm;
    target_vel = clampf(3.0f * position_error,
                        -ctx->cfg.max_ref_vel_mm_s,
                         ctx->cfg.max_ref_vel_mm_s);
    desired_accel = clampf((target_vel - ctx->output.reference_vel_mm_s) / dt,
                           -ctx->cfg.max_ref_accel_mm_s2,
                            ctx->cfg.max_ref_accel_mm_s2);
    ctx->output.reference_accel_mm_s2 = slew(
        ctx->output.reference_accel_mm_s2, desired_accel,
        ctx->cfg.max_ref_jerk_mm_s3 * dt);
    ctx->output.reference_vel_mm_s +=
        ctx->output.reference_accel_mm_s2 * dt;
    ctx->output.reference_vel_mm_s = clampf(
        ctx->output.reference_vel_mm_s, -ctx->cfg.max_ref_vel_mm_s,
        ctx->cfg.max_ref_vel_mm_s);
    ctx->output.reference_pos_mm += ctx->output.reference_vel_mm_s * dt;

    position_error = ctx->output.reference_pos_mm - in->pos_mm;
    velocity_error = ctx->output.reference_vel_mm_s - in->vel_mm_s;
    ctx->pos_integral_mm_s += position_error * dt;
    ctx->pos_integral_mm_s = clampf(ctx->pos_integral_mm_s, -80.0f, 80.0f);

    feedback_angle = ctx->cfg.pos_kp_mrad_per_mm * position_error +
        ctx->cfg.pos_ki_mrad_per_mm_s * ctx->pos_integral_mm_s +
        ctx->cfg.vel_kd_mrad_per_mm_s * velocity_error;
    model_angle = ctx->cfg.model_ff_scale * BALL_BEAM_MODEL_FACTOR *
        ctx->output.reference_accel_mm_s2 * 1000.0f / BALL_GRAVITY_MM_S2;
    robust_angle = ctx->cfg.robust_gain_mrad * position_error /
        (fabsf(position_error) + ctx->cfg.robust_boundary_mm);
    angle = feedback_angle + model_angle + robust_angle;
    ctx->output.saturated = fabsf(angle) > ctx->cfg.max_angle_mrad;
    ctx->output.target_angle_mrad = clampf(angle,
        -ctx->cfg.max_angle_mrad, ctx->cfg.max_angle_mrad);
    if (ctx->output.saturated &&
        ((position_error > 0.0f && ctx->pos_integral_mm_s > 0.0f) ||
         (position_error < 0.0f && ctx->pos_integral_mm_s < 0.0f))) {
        ctx->pos_integral_mm_s -= position_error * dt;
    }
    return true;
}

bool BallCtrlCore_StepInner(ball_ctrl_core_t *ctx,
                            const ball_ctrl_inner_input_t *in)
{
    float angle_error;
    float rate_setpoint;
    float rate_error;
    float freq;
    float dt;
    if ((ctx == NULL) || !ctx->initialized || (in == NULL)) { return false; }
    dt = clampf(in->dt_s, 0.0005f, 0.01f);
    if (!in->enabled || !isfinite(in->angle_mrad) ||
        !isfinite(in->rate_mrad_s)) {
        ctx->output.signed_step_freq_hz = 0.0f;
        ctx->rate_integral_hz = 0.0f;
        return false;
    }
    angle_error = ctx->output.target_angle_mrad - in->angle_mrad;
    rate_setpoint = ctx->cfg.angle_kp_per_s * angle_error;
    rate_error = rate_setpoint - in->rate_mrad_s;
    ctx->rate_integral_hz += ctx->cfg.rate_ki_hz_per_mrad * rate_error * dt;
    ctx->rate_integral_hz = clampf(ctx->rate_integral_hz,
        -ctx->cfg.rate_integral_limit_hz, ctx->cfg.rate_integral_limit_hz);
    freq = ctx->cfg.rate_kp_hz_per_mrad_s * rate_error +
           ctx->rate_integral_hz +
           ctx->cfg.chassis_accel_ff_hz_per_mm_s2 *
           in->chassis_accel_mm_s2 * clampf(in->chassis_ff_weight, 0.0f, 1.0f);
    ctx->output.saturated = ctx->output.saturated ||
                           (fabsf(freq) > ctx->cfg.max_step_freq_hz);
    ctx->output.signed_step_freq_hz = clampf(freq,
        -ctx->cfg.max_step_freq_hz, ctx->cfg.max_step_freq_hz);
    return true;
}

void BallCtrlCore_GetOutput(const ball_ctrl_core_t *ctx,
                            ball_ctrl_core_output_t *out)
{
    if ((ctx != NULL) && (out != NULL)) { *out = ctx->output; }
}
