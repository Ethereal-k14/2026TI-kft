/**
 * @file ball_control_core.h
 * @brief Portable robust ball-on-beam cascade controller.
 */
#ifndef BALL_CONTROL_CORE_H
#define BALL_CONTROL_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BALL_CTRL_PROFILE_SAFE = 0,
    BALL_CTRL_PROFILE_PRECISION,
    BALL_CTRL_PROFILE_BALANCED,
    BALL_CTRL_PROFILE_FAST
} ball_ctrl_profile_t;

typedef struct {
    float pos_kp_mrad_per_mm;
    float pos_ki_mrad_per_mm_s;
    float vel_kd_mrad_per_mm_s;
    float robust_gain_mrad;
    float robust_boundary_mm;
    float model_ff_scale;
    float max_ref_vel_mm_s;
    float max_ref_accel_mm_s2;
    float max_ref_jerk_mm_s3;
    float max_angle_mrad;
    float angle_kp_per_s;
    float rate_kp_hz_per_mrad_s;
    float rate_ki_hz_per_mrad;
    float rate_integral_limit_hz;
    float chassis_accel_ff_hz_per_mm_s2;
    float max_step_freq_hz;
} ball_ctrl_core_cfg_t;

typedef struct {
    float pos_mm;
    float vel_mm_s;
    float dt_s;
    bool valid;
} ball_ctrl_outer_input_t;

typedef struct {
    float angle_mrad;
    float rate_mrad_s;
    float chassis_accel_mm_s2;
    float chassis_ff_weight;
    float dt_s;
    bool enabled;
} ball_ctrl_inner_input_t;

typedef struct {
    float target_pos_mm;
    float reference_pos_mm;
    float reference_vel_mm_s;
    float reference_accel_mm_s2;
    float target_angle_mrad;
    float signed_step_freq_hz;
    bool measurement_valid;
    bool saturated;
} ball_ctrl_core_output_t;

typedef struct {
    ball_ctrl_core_cfg_t cfg;
    ball_ctrl_core_output_t output;
    float pos_integral_mm_s;
    float rate_integral_hz;
    bool initialized;
} ball_ctrl_core_t;

void BallCtrlCore_Init(ball_ctrl_core_t *ctx,
                       const ball_ctrl_core_cfg_t *cfg);
void BallCtrlCore_Reset(ball_ctrl_core_t *ctx, float measured_pos_mm);
bool BallCtrlCore_SetProfile(ball_ctrl_core_t *ctx,
                             ball_ctrl_profile_t profile);
bool BallCtrlCore_Configure(ball_ctrl_core_t *ctx,
                            const ball_ctrl_core_cfg_t *cfg);
void BallCtrlCore_SetTarget(ball_ctrl_core_t *ctx, float target_pos_mm);
bool BallCtrlCore_StepOuter(ball_ctrl_core_t *ctx,
                            const ball_ctrl_outer_input_t *input);
bool BallCtrlCore_StepInner(ball_ctrl_core_t *ctx,
                            const ball_ctrl_inner_input_t *input);
void BallCtrlCore_GetOutput(const ball_ctrl_core_t *ctx,
                            ball_ctrl_core_output_t *output);

#ifdef __cplusplus
}
#endif
#endif
