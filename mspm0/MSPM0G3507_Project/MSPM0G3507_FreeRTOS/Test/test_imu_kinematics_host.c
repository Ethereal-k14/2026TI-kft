#include "app_imu_kinematics.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int near(float a, float b) { return fabsf(a - b) < 1.0e-4f; }

int main(void)
{
    app_imu_kinematics_cfg_t cfg;
    app_imu_kinematics_t ctx;
    app_imu_kinematics_input_t in = {{0}, {0}, 0.01f};
    app_imu_kinematics_output_t out;

    app_imu_kinematics_default_cfg(&cfg);
    cfg.body_from_sensor[0][0] = 2.0f;
    assert(!app_imu_kinematics_init(&ctx, &cfg));
    app_imu_kinematics_default_cfg(&cfg);
    cfg.offset_body_m[0] = 0.10f;
    cfg.offset_body_m[1] = 0.05f;
    cfg.angular_accel_alpha = 1.0f;
    cfg.max_angular_accel_rad_s2 = 100.0f;
    assert(app_imu_kinematics_init(&ctx, &cfg));

    /* Constant 1 rad/s yaw: remove centripetal acceleration at the IMU. */
    in.gyro_sensor_dps[2] = 57.2957795f;
    in.accel_sensor_m_s2[0] = -0.10f;
    in.accel_sensor_m_s2[1] = -0.05f;
    assert(app_imu_kinematics_step(&ctx, &in, &out));
    assert(near(out.accel_body_centre_m_s2[0], 0.0f));
    assert(near(out.accel_body_centre_m_s2[1], 0.0f));
    assert(near(out.gyro_body_dps[2], 57.2957795f));

    /* 1 rad/s2 yaw acceleration with Y lever arm: remove tangential X term. */
    app_imu_kinematics_reset(&ctx);
    in.gyro_sensor_dps[2] = 0.0f;
    in.accel_sensor_m_s2[0] = 0.0f;
    in.accel_sensor_m_s2[1] = 0.0f;
    assert(app_imu_kinematics_step(&ctx, &in, &out));
    in.dt_s = 0.05f;
    in.gyro_sensor_dps[2] = 2.86478898f; /* 0.05 rad/s */
    in.accel_sensor_m_s2[0] = -0.05f - 0.00025f;
    in.accel_sensor_m_s2[1] = 0.10f - 0.000125f;
    assert(app_imu_kinematics_step(&ctx, &in, &out));
    assert(near(out.yaw_angular_accel_rad_s2, 1.0f));
    assert(near(out.accel_body_centre_m_s2[0], 0.0f));
    assert(near(out.accel_body_centre_m_s2[1], 0.0f));

    /* A 90-degree mounting transform rotates vectors, not angular-rate origin. */
    app_imu_kinematics_default_cfg(&cfg);
    cfg.body_from_sensor[0][0] = 0.0f;
    cfg.body_from_sensor[0][1] = -1.0f;
    cfg.body_from_sensor[1][0] = 1.0f;
    cfg.body_from_sensor[1][1] = 0.0f;
    assert(app_imu_kinematics_init(&ctx, &cfg));
    in.accel_sensor_m_s2[0] = 1.0f;
    in.accel_sensor_m_s2[1] = 0.0f;
    in.gyro_sensor_dps[2] = 10.0f;
    assert(app_imu_kinematics_step(&ctx, &in, &out));
    assert(near(out.accel_body_centre_m_s2[0], 0.0f));
    assert(near(out.accel_body_centre_m_s2[1], 1.0f));
    assert(near(out.gyro_body_dps[2], 10.0f));

    puts("IMU kinematics host tests: PASS");
    return 0;
}
