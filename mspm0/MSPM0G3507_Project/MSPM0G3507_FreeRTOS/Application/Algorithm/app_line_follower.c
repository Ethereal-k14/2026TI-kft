/**
 * @file app_line_follower.c
 * @brief Line follower with yaw damping and jerk/curvature constrained planning.
 */
#include "app_line_follower.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static float clampf(float value, float low, float high)
{
    if (value < low) { return low; }
    if (value > high) { return high; }
    return value;
}

static float jerk_limited_step(float current, float target, float dt_s,
                               float accel_limit, float jerk_limit,
                               float *accel)
{
    float desired_accel = clampf((target - current) / dt_s,
                                 -accel_limit, accel_limit);
    float next;
    *accel += clampf(desired_accel - *accel,
                     -jerk_limit * dt_s, jerk_limit * dt_s);
    next = current + *accel * dt_s;
    if (((target - current) * (target - next)) <= 0.0f) {
        next = target;
        *accel = 0.0f;
    }
    return next;
}

static bool cfg_valid(const app_line_follower_cfg_t *cfg)
{
    return (cfg != NULL) &&
           isfinite(cfg->base_speed_mm_s) &&
           isfinite(cfg->min_speed_mm_s) &&
           isfinite(cfg->max_speed_mm_s) &&
           isfinite(cfg->steer_kp) && isfinite(cfg->steer_ki) &&
           isfinite(cfg->steer_kd) && isfinite(cfg->yaw_damping) &&
           isfinite(cfg->error_filter_alpha) &&
           isfinite(cfg->integral_limit) &&
           isfinite(cfg->accel_limit_mm_s2) &&
           isfinite(cfg->jerk_limit_mm_s3) &&
           isfinite(cfg->steer_limit_mm_s) &&
           isfinite(cfg->track_width_mm) &&
           isfinite(cfg->max_lateral_accel_mm_s2) &&
           isfinite(cfg->max_yaw_rate_dps) &&
           (cfg->min_speed_mm_s >= 0.0f) &&
           (cfg->base_speed_mm_s >= cfg->min_speed_mm_s) &&
           (cfg->max_speed_mm_s >= cfg->base_speed_mm_s) &&
           (cfg->error_filter_alpha >= 0.0f) &&
           (cfg->error_filter_alpha <= 1.0f) &&
           (cfg->integral_limit >= 0.0f) &&
           (cfg->accel_limit_mm_s2 > 0.0f) &&
           (cfg->jerk_limit_mm_s3 > 0.0f) &&
           (cfg->steer_limit_mm_s > 0.0f) &&
           (cfg->track_width_mm > 0.0f) &&
           (cfg->max_lateral_accel_mm_s2 > 0.0f) &&
           (cfg->max_yaw_rate_dps > 0.0f);
}

static app_line_follower_cfg_t profile_cfg(app_line_profile_t profile)
{
    app_line_follower_cfg_t cfg = {
        .base_speed_mm_s = 420.0f,
        .min_speed_mm_s = 220.0f,
        .max_speed_mm_s = 520.0f,
        .steer_kp = 135.0f,
        .steer_ki = 0.0f,
        .steer_kd = 0.12f,
        .yaw_damping = 0.8f,
        .error_filter_alpha = 0.55f,
        .integral_limit = 0.8f,
        .accel_limit_mm_s2 = 1200.0f,
        .jerk_limit_mm_s3 = 5000.0f,
        .steer_limit_mm_s = 300.0f,
        .track_width_mm = 190.0f,
        .max_lateral_accel_mm_s2 = 650.0f,
        .max_yaw_rate_dps = 140.0f
    };

    switch (profile) {
    case APP_LINE_PROFILE_SAFE:
        cfg.base_speed_mm_s = 220.0f;
        cfg.min_speed_mm_s = 120.0f;
        cfg.max_speed_mm_s = 280.0f;
        cfg.accel_limit_mm_s2 = 600.0f;
        cfg.jerk_limit_mm_s3 = 2500.0f;
        cfg.max_lateral_accel_mm_s2 = 350.0f;
        cfg.max_yaw_rate_dps = 100.0f;
        break;
    case APP_LINE_PROFILE_PRECISION:
        cfg.base_speed_mm_s = 300.0f;
        cfg.min_speed_mm_s = 170.0f;
        cfg.max_speed_mm_s = 380.0f;
        cfg.steer_kp = 150.0f;
        cfg.accel_limit_mm_s2 = 800.0f;
        cfg.jerk_limit_mm_s3 = 3500.0f;
        cfg.max_lateral_accel_mm_s2 = 500.0f;
        cfg.max_yaw_rate_dps = 120.0f;
        break;
    case APP_LINE_PROFILE_FAST:
        cfg.base_speed_mm_s = 500.0f;
        cfg.min_speed_mm_s = 260.0f;
        cfg.max_speed_mm_s = 620.0f;
        cfg.steer_kp = 145.0f;
        cfg.accel_limit_mm_s2 = 1600.0f;
        cfg.jerk_limit_mm_s3 = 9000.0f;
        cfg.max_lateral_accel_mm_s2 = 850.0f;
        cfg.max_yaw_rate_dps = 180.0f;
        break;
    case APP_LINE_PROFILE_BALANCED:
    default:
        break;
    }
    return cfg;
}

void app_line_follower_init(app_line_follower_t *ctx,
                            const app_line_follower_cfg_t *cfg)
{
    if (ctx == NULL) { return; }
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg_valid(cfg) ? *cfg : profile_cfg(APP_LINE_PROFILE_BALANCED);
    ctx->initialized = true;
}

void app_line_follower_reset(app_line_follower_t *ctx)
{
    app_line_follower_cfg_t cfg;
    if ((ctx == NULL) || !ctx->initialized) { return; }
    cfg = ctx->cfg;
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg;
    ctx->initialized = true;
}

bool app_line_follower_configure(app_line_follower_t *ctx,
                                 const app_line_follower_cfg_t *cfg)
{
    if ((ctx == NULL) || !cfg_valid(cfg)) { return false; }
    if (!ctx->initialized) {
        (void)memset(ctx, 0, sizeof(*ctx));
        ctx->initialized = true;
    }
    ctx->cfg = *cfg;
    app_line_follower_reset(ctx);
    return true;
}

bool app_line_follower_set_profile(app_line_follower_t *ctx,
                                   app_line_profile_t profile)
{
    app_line_follower_cfg_t cfg;
    if ((ctx == NULL) || (profile > APP_LINE_PROFILE_FAST)) { return false; }
    cfg = profile_cfg(profile);
    return app_line_follower_configure(ctx, &cfg);
}

bool app_line_follower_step(app_line_follower_t *ctx,
                            const app_line_follower_input_t *input,
                            app_line_follower_output_t *output)
{
    static const float weight[APP_LINE_SENSOR_COUNT] = {
        3.0f, 1.0f, -1.0f, -3.0f
    };
    float weighted_sum = 0.0f;
    float active_count = 0.0f;
    float raw_error;
    float derivative;
    float steering;
    float base_speed;
    float yaw_rate_rad_s;
    float measured_yaw_rate_rad_s;
    float effective_yaw_rate_rad_s;
    float curve_speed_limit;
    float yaw_steer_limit;
    float target_left;
    float target_right;
    float dt_s;
    uint8_t sensor_bits = 0U;

    if ((ctx == NULL) || !ctx->initialized ||
        (input == NULL) || (output == NULL)) {
        return false;
    }

    for (uint32_t i = 0U; i < APP_LINE_SENSOR_COUNT; i++) {
        const uint8_t black = (input->black[i] != 0U) ? 1U : 0U;
        sensor_bits |= (uint8_t)((black == 0U ? 1U : 0U) << (3U - i));
        if (black != 0U) {
            weighted_sum += weight[i];
            active_count += 1.0f;
        }
    }

    dt_s = clampf(input->dt_s, 0.001f, 0.05f);
    output->line_detected = (active_count > 0.0f);
    output->cross_detected = (active_count >= (float)APP_LINE_SENSOR_COUNT);

    if (!output->line_detected) {
        ctx->lost_ms += (uint32_t)(dt_s * 1000.0f + 0.5f);
        raw_error = (ctx->filtered_error >= 0.0f) ? 1.0f : -1.0f;
        ctx->error_integral = 0.0f;
    } else if (output->cross_detected) {
        ctx->lost_ms = 0U;
        raw_error = 0.0f;
        ctx->error_integral = 0.0f;
    } else {
        ctx->lost_ms = 0U;
        raw_error = weighted_sum / (3.0f * active_count);
        ctx->error_integral += raw_error * dt_s;
        ctx->error_integral = clampf(ctx->error_integral,
            -ctx->cfg.integral_limit, ctx->cfg.integral_limit);
    }

    ctx->filtered_error += ctx->cfg.error_filter_alpha *
                           (raw_error - ctx->filtered_error);
    derivative = (ctx->filtered_error - ctx->previous_error) / dt_s;
    ctx->previous_error = ctx->filtered_error;

    steering = ctx->cfg.steer_kp * ctx->filtered_error +
               ctx->cfg.steer_ki * ctx->error_integral +
               ctx->cfg.steer_kd * derivative -
               ctx->cfg.yaw_damping * input->yaw_rate_dps;
    steering = clampf(steering, -ctx->cfg.steer_limit_mm_s,
                      ctx->cfg.steer_limit_mm_s);

    yaw_steer_limit = 0.5f * ctx->cfg.track_width_mm *
        ctx->cfg.max_yaw_rate_dps * 0.01745329251994329577f;
    steering = clampf(steering, -yaw_steer_limit, yaw_steer_limit);

    base_speed = ctx->cfg.base_speed_mm_s -
        ((ctx->cfg.base_speed_mm_s - ctx->cfg.min_speed_mm_s) *
         clampf(fabsf(ctx->filtered_error), 0.0f, 1.0f));
    if (!output->line_detected) {
        base_speed = ctx->cfg.min_speed_mm_s * 0.5f;
    }
    base_speed = clampf(base_speed, 0.0f, ctx->cfg.max_speed_mm_s);

    yaw_rate_rad_s = (2.0f * steering) / ctx->cfg.track_width_mm;
    measured_yaw_rate_rad_s = fabsf(input->yaw_rate_dps) *
                              0.01745329251994329577f;
    effective_yaw_rate_rad_s = fabsf(yaw_rate_rad_s);
    if (measured_yaw_rate_rad_s > effective_yaw_rate_rad_s) {
        effective_yaw_rate_rad_s = measured_yaw_rate_rad_s;
    }
    curve_speed_limit = ctx->cfg.max_speed_mm_s;
    if (effective_yaw_rate_rad_s > 0.01f) {
        curve_speed_limit = ctx->cfg.max_lateral_accel_mm_s2 /
                            effective_yaw_rate_rad_s;
        curve_speed_limit = clampf(curve_speed_limit, 0.0f,
                                   ctx->cfg.max_speed_mm_s);
        if (base_speed > curve_speed_limit) {
            base_speed = curve_speed_limit;
        }
    }

    target_left = clampf(base_speed - steering, -ctx->cfg.max_speed_mm_s,
                         ctx->cfg.max_speed_mm_s);
    target_right = clampf(base_speed + steering, -ctx->cfg.max_speed_mm_s,
                          ctx->cfg.max_speed_mm_s);
    ctx->left_speed_mm_s = jerk_limited_step(ctx->left_speed_mm_s,
        target_left, dt_s, ctx->cfg.accel_limit_mm_s2,
        ctx->cfg.jerk_limit_mm_s3, &ctx->left_accel_mm_s2);
    ctx->right_speed_mm_s = jerk_limited_step(ctx->right_speed_mm_s,
        target_right, dt_s, ctx->cfg.accel_limit_mm_s2,
        ctx->cfg.jerk_limit_mm_s3, &ctx->right_accel_mm_s2);
    ctx->left_speed_mm_s = clampf(ctx->left_speed_mm_s,
        -ctx->cfg.max_speed_mm_s, ctx->cfg.max_speed_mm_s);
    ctx->right_speed_mm_s = clampf(ctx->right_speed_mm_s,
        -ctx->cfg.max_speed_mm_s, ctx->cfg.max_speed_mm_s);

    output->left_speed_mm_s = ctx->left_speed_mm_s;
    output->right_speed_mm_s = ctx->right_speed_mm_s;
    output->base_speed_mm_s = base_speed;
    output->steering_mm_s = steering;
    output->left_accel_mm_s2 = ctx->left_accel_mm_s2;
    output->right_accel_mm_s2 = ctx->right_accel_mm_s2;
    output->curve_speed_limit_mm_s = curve_speed_limit;
    output->target_yaw_rate_dps = yaw_rate_rad_s * 57.29577951308232f;
    output->line_error = ctx->filtered_error;
    output->lost_ms = ctx->lost_ms;
    output->sensor_bits = sensor_bits;
    output->planner_limited = (curve_speed_limit + 0.01f <
                               ctx->cfg.max_speed_mm_s) ||
                              (fabsf(steering) + 0.01f >= yaw_steer_limit);
    return true;
}
