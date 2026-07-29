/**
 * @file app_line_follower.h
 * @brief Hardware-independent four-channel line follower.
 */
#ifndef APP_LINE_FOLLOWER_H
#define APP_LINE_FOLLOWER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_LINE_SENSOR_COUNT (4U)

typedef enum {
    APP_LINE_PROFILE_SAFE = 0,
    APP_LINE_PROFILE_PRECISION,
    APP_LINE_PROFILE_BALANCED,
    APP_LINE_PROFILE_FAST
} app_line_profile_t;

typedef struct {
    float base_speed_mm_s;
    float min_speed_mm_s;
    float max_speed_mm_s;
    float steer_kp;
    float steer_ki;
    float steer_kd;
    float yaw_damping;
    float error_filter_alpha;
    float integral_limit;
    float accel_limit_mm_s2;
    float jerk_limit_mm_s3;
    float steer_limit_mm_s;
    float track_width_mm;
    float max_lateral_accel_mm_s2;
    float max_yaw_rate_dps;
} app_line_follower_cfg_t;

typedef struct {
    uint8_t black[APP_LINE_SENSOR_COUNT]; /* left to right; 1=black */
    float yaw_rate_dps;
    float dt_s;
} app_line_follower_input_t;

typedef struct {
    float left_speed_mm_s;
    float right_speed_mm_s;
    float base_speed_mm_s;
    float steering_mm_s;
    float left_accel_mm_s2;
    float right_accel_mm_s2;
    float curve_speed_limit_mm_s;
    float target_yaw_rate_dps;
    float line_error;
    uint32_t lost_ms;
    uint8_t sensor_bits; /* bit3=left; 0=black, 1=white */
    bool line_detected;
    bool cross_detected;
    bool planner_limited;
} app_line_follower_output_t;

typedef struct {
    app_line_follower_cfg_t cfg;
    float filtered_error;
    float previous_error;
    float error_integral;
    float left_speed_mm_s;
    float right_speed_mm_s;
    float left_accel_mm_s2;
    float right_accel_mm_s2;
    uint32_t lost_ms;
    bool initialized;
} app_line_follower_t;

void app_line_follower_init(app_line_follower_t *ctx,
                            const app_line_follower_cfg_t *cfg);
void app_line_follower_reset(app_line_follower_t *ctx);
bool app_line_follower_set_profile(app_line_follower_t *ctx,
                                   app_line_profile_t profile);
bool app_line_follower_configure(app_line_follower_t *ctx,
                                 const app_line_follower_cfg_t *cfg);
bool app_line_follower_step(app_line_follower_t *ctx,
                            const app_line_follower_input_t *input,
                            app_line_follower_output_t *output);

#ifdef __cplusplus
}
#endif
#endif /* APP_LINE_FOLLOWER_H */
