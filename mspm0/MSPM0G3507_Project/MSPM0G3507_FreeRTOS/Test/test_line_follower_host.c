#include "app_line_follower.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static app_line_follower_output_t step(app_line_follower_t *ctx,
                                       uint8_t a, uint8_t b,
                                       uint8_t c, uint8_t d,
                                       float yaw)
{
    app_line_follower_input_t in = {{a, b, c, d}, yaw, 0.005f};
    app_line_follower_output_t out;
    assert(app_line_follower_step(ctx, &in, &out));
    return out;
}

int main(void)
{
    app_line_follower_t ctx;
    app_line_follower_output_t out;
    app_line_follower_cfg_t bad = {0};

    app_line_follower_init(&ctx, NULL);
    assert(ctx.initialized);
    assert(!app_line_follower_configure(&ctx, &bad));
    bad = ctx.cfg;
    bad.jerk_limit_mm_s3 = NAN;
    assert(!app_line_follower_configure(&ctx, &bad));
    assert(!app_line_follower_set_profile(&ctx, (app_line_profile_t)99));

    out = step(&ctx, 0, 1, 1, 0, 0.0f);
    assert(out.line_detected && !out.cross_detected);
    assert(fabsf(out.line_error) < 1.0e-6f);
    assert(fabsf(out.left_speed_mm_s - out.right_speed_mm_s) < 1.0e-6f);
    assert(out.left_speed_mm_s <= 0.126f); /* jerk-limited start: 5000*dt^2 */
    assert(fabsf(out.left_accel_mm_s2) <= 25.001f);
    {
        const float previous_accel = out.left_accel_mm_s2;
        out = step(&ctx, 0, 1, 1, 0, 0.0f);
        assert(fabsf(out.left_accel_mm_s2 - previous_accel) <= 25.001f);
    }

    app_line_follower_reset(&ctx);
    out = step(&ctx, 1, 0, 0, 0, 0.0f);
    assert(out.line_error > 0.0f);
    assert(fabsf(out.left_speed_mm_s) <= 0.126f);
    assert(fabsf(out.right_speed_mm_s) <= 0.126f);
    for (int i = 0; i < 60; i++) {
        out = step(&ctx, 1, 0, 0, 0, 0.0f);
    }
    assert(out.right_speed_mm_s > out.left_speed_mm_s);

    app_line_follower_reset(&ctx);
    for (int i = 0; i < 60; i++) {
        out = step(&ctx, 0, 0, 0, 1, 0.0f);
    }
    assert(out.line_error < 0.0f);
    assert(out.left_speed_mm_s > out.right_speed_mm_s);

    app_line_follower_reset(&ctx);
    out = step(&ctx, 0, 1, 1, 0, 20.0f);
    assert(out.steering_mm_s < 0.0f); /* IMU yaw damping */

    app_line_follower_reset(&ctx);
    out = step(&ctx, 0, 0, 0, 0, 0.0f);
    assert(!out.line_detected && !out.cross_detected);
    assert(out.lost_ms == 5U);

    out = step(&ctx, 1, 1, 1, 1, 0.0f);
    assert(out.line_detected && out.cross_detected);
    assert(out.sensor_bits == 0U);

    assert(app_line_follower_set_profile(&ctx, APP_LINE_PROFILE_FAST));
    assert(ctx.cfg.max_speed_mm_s == 620.0f);

    {
        app_line_follower_cfg_t limited = ctx.cfg;
        limited.base_speed_mm_s = 500.0f;
        limited.min_speed_mm_s = 400.0f;
        limited.max_speed_mm_s = 600.0f;
        limited.max_lateral_accel_mm_s2 = 100.0f;
        limited.max_yaw_rate_dps = 90.0f;
        assert(app_line_follower_configure(&ctx, &limited));
        out = step(&ctx, 1, 0, 0, 0, 0.0f);
        assert(fabsf(out.target_yaw_rate_dps) <= 90.001f);
        assert(out.base_speed_mm_s * fabsf(out.target_yaw_rate_dps) *
               0.01745329252f <= 100.01f);
        assert(out.planner_limited);
    }
    puts("line follower host tests: PASS");
    return 0;
}
