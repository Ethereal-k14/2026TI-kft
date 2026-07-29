/**
 * @file app_imu_kinematics.c
 * @brief IMU frame conversion and rigid-body acceleration compensation.
 */
#include "app_imu_kinematics.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define DEG_TO_RAD (0.01745329251994329577f)

static float clampf(float value, float low, float high)
{
    if (value < low) { return low; }
    if (value > high) { return high; }
    return value;
}

static void mat_vec(const float matrix[3][3], const float in[3], float out[3])
{
    for (unsigned int row = 0U; row < 3U; row++) {
        out[row] = matrix[row][0] * in[0] +
                   matrix[row][1] * in[1] +
                   matrix[row][2] * in[2];
    }
}

static bool cfg_valid(const app_imu_kinematics_cfg_t *cfg)
{
    float determinant;
    float row_norm[3] = {0.0f, 0.0f, 0.0f};
    if (cfg == NULL) { return false; }
    for (unsigned int row = 0U; row < 3U; row++) {
        if (!isfinite(cfg->gyro_bias_body_dps[row]) ||
            !isfinite(cfg->offset_body_m[row])) {
            return false;
        }
        for (unsigned int column = 0U; column < 3U; column++) {
            const float value = cfg->body_from_sensor[row][column];
            if (!isfinite(value)) { return false; }
            row_norm[row] += value * value;
        }
        if (fabsf(row_norm[row] - 1.0f) > 0.05f) { return false; }
    }
    for (unsigned int first = 0U; first < 3U; first++) {
        for (unsigned int second = first + 1U; second < 3U; second++) {
            float dot = 0.0f;
            for (unsigned int column = 0U; column < 3U; column++) {
                dot += cfg->body_from_sensor[first][column] *
                       cfg->body_from_sensor[second][column];
            }
            if (fabsf(dot) > 0.05f) { return false; }
        }
    }
    determinant =
        cfg->body_from_sensor[0][0] *
        (cfg->body_from_sensor[1][1] * cfg->body_from_sensor[2][2] -
         cfg->body_from_sensor[1][2] * cfg->body_from_sensor[2][1]) -
        cfg->body_from_sensor[0][1] *
        (cfg->body_from_sensor[1][0] * cfg->body_from_sensor[2][2] -
         cfg->body_from_sensor[1][2] * cfg->body_from_sensor[2][0]) +
        cfg->body_from_sensor[0][2] *
        (cfg->body_from_sensor[1][0] * cfg->body_from_sensor[2][1] -
         cfg->body_from_sensor[1][1] * cfg->body_from_sensor[2][0]);
    return isfinite(determinant) && (determinant > 0.5f) &&
           (determinant < 1.5f) &&
           isfinite(cfg->angular_accel_alpha) &&
           isfinite(cfg->max_angular_accel_rad_s2) &&
           (cfg->angular_accel_alpha >= 0.0f) &&
           (cfg->angular_accel_alpha <= 1.0f) &&
           (cfg->max_angular_accel_rad_s2 > 0.0f);
}

void app_imu_kinematics_default_cfg(app_imu_kinematics_cfg_t *cfg)
{
    if (cfg == NULL) { return; }
    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->body_from_sensor[0][0] = 1.0f;
    cfg->body_from_sensor[1][1] = 1.0f;
    cfg->body_from_sensor[2][2] = 1.0f;
    cfg->angular_accel_alpha = 0.2f;
    cfg->max_angular_accel_rad_s2 = 30.0f;
}

bool app_imu_kinematics_init(app_imu_kinematics_t *ctx,
                             const app_imu_kinematics_cfg_t *cfg)
{
    if ((ctx == NULL) || !cfg_valid(cfg)) { return false; }
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = *cfg;
    ctx->initialized = true;
    return true;
}

void app_imu_kinematics_reset(app_imu_kinematics_t *ctx)
{
    if ((ctx == NULL) || !ctx->initialized) { return; }
    ctx->previous_yaw_rate_rad_s = 0.0f;
    ctx->filtered_yaw_accel_rad_s2 = 0.0f;
    ctx->rate_initialized = false;
}

bool app_imu_kinematics_step(app_imu_kinematics_t *ctx,
                             const app_imu_kinematics_input_t *input,
                             app_imu_kinematics_output_t *output)
{
    float dt_s;
    float yaw_rate_rad_s;
    float raw_yaw_accel = 0.0f;
    float yaw_rate_sq;
    const float *r;
    if ((ctx == NULL) || !ctx->initialized ||
        (input == NULL) || (output == NULL)) {
        return false;
    }

    mat_vec(ctx->cfg.body_from_sensor, input->accel_sensor_m_s2,
            output->accel_body_at_imu_m_s2);
    mat_vec(ctx->cfg.body_from_sensor, input->gyro_sensor_dps,
            output->gyro_body_dps);
    for (unsigned int axis = 0U; axis < 3U; axis++) {
        output->gyro_body_dps[axis] -= ctx->cfg.gyro_bias_body_dps[axis];
    }

    dt_s = clampf(input->dt_s, 0.001f, 0.05f);
    yaw_rate_rad_s = output->gyro_body_dps[2] * DEG_TO_RAD;
    if (ctx->rate_initialized) {
        raw_yaw_accel = (yaw_rate_rad_s -
                         ctx->previous_yaw_rate_rad_s) / dt_s;
        raw_yaw_accel = clampf(raw_yaw_accel,
            -ctx->cfg.max_angular_accel_rad_s2,
             ctx->cfg.max_angular_accel_rad_s2);
        ctx->filtered_yaw_accel_rad_s2 += ctx->cfg.angular_accel_alpha *
            (raw_yaw_accel - ctx->filtered_yaw_accel_rad_s2);
    } else {
        ctx->rate_initialized = true;
        ctx->filtered_yaw_accel_rad_s2 = 0.0f;
    }
    ctx->previous_yaw_rate_rad_s = yaw_rate_rad_s;

    /* a_imu = a_centre + alpha x r + omega x (omega x r), planar yaw. */
    r = ctx->cfg.offset_body_m;
    yaw_rate_sq = yaw_rate_rad_s * yaw_rate_rad_s;
    output->lever_correction_m_s2[0] =
        ctx->filtered_yaw_accel_rad_s2 * r[1] + yaw_rate_sq * r[0];
    output->lever_correction_m_s2[1] =
       -ctx->filtered_yaw_accel_rad_s2 * r[0] + yaw_rate_sq * r[1];
    output->lever_correction_m_s2[2] = 0.0f;
    for (unsigned int axis = 0U; axis < 3U; axis++) {
        output->accel_body_centre_m_s2[axis] =
            output->accel_body_at_imu_m_s2[axis] +
            output->lever_correction_m_s2[axis];
    }
    output->yaw_angular_accel_rad_s2 =
        ctx->filtered_yaw_accel_rad_s2;
    return true;
}
