/**
 * @file app_imu_kinematics.h
 * @brief Portable IMU mounting transform and planar lever-arm compensation.
 */
#ifndef APP_IMU_KINEMATICS_H
#define APP_IMU_KINEMATICS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float body_from_sensor[3][3];
    float gyro_bias_body_dps[3];
    float offset_body_m[3];       /* rotation centre -> IMU, +X forward, +Y left */
    float angular_accel_alpha;    /* first-order filter coefficient, 0..1 */
    float max_angular_accel_rad_s2;
} app_imu_kinematics_cfg_t;

typedef struct {
    float accel_sensor_m_s2[3];
    float gyro_sensor_dps[3];
    float dt_s;
} app_imu_kinematics_input_t;

typedef struct {
    float accel_body_at_imu_m_s2[3];
    float accel_body_centre_m_s2[3];
    float gyro_body_dps[3];
    float lever_correction_m_s2[3];
    float yaw_angular_accel_rad_s2;
} app_imu_kinematics_output_t;

typedef struct {
    app_imu_kinematics_cfg_t cfg;
    float previous_yaw_rate_rad_s;
    float filtered_yaw_accel_rad_s2;
    bool rate_initialized;
    bool initialized;
} app_imu_kinematics_t;

void app_imu_kinematics_default_cfg(app_imu_kinematics_cfg_t *cfg);
bool app_imu_kinematics_init(app_imu_kinematics_t *ctx,
                             const app_imu_kinematics_cfg_t *cfg);
void app_imu_kinematics_reset(app_imu_kinematics_t *ctx);
bool app_imu_kinematics_step(app_imu_kinematics_t *ctx,
                             const app_imu_kinematics_input_t *input,
                             app_imu_kinematics_output_t *output);

#ifdef __cplusplus
}
#endif
#endif
