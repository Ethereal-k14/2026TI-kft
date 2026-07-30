#include "ball_control_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    ball_ctrl_core_t ctx;
    ball_ctrl_outer_input_t outer = {0.0f, 0.0f, 0.02f, true};
    ball_ctrl_inner_input_t inner = {0.0f, 0.0f, 0.0f, 0.0f, 0.002f, true};
    ball_ctrl_core_output_t out;
    ball_ctrl_core_cfg_t bad = {0};
    float angle_before_dropout;

    BallCtrlCore_Init(&ctx, NULL);
    assert(ctx.initialized);
    assert(!BallCtrlCore_Configure(&ctx, &bad));
    assert(!BallCtrlCore_SetProfile(&ctx, (ball_ctrl_profile_t)99));
    BallCtrlCore_Reset(&ctx, 0.0f);
    BallCtrlCore_SetTarget(&ctx, 50.0f);

    assert(BallCtrlCore_StepOuter(&ctx, &outer));
    BallCtrlCore_GetOutput(&ctx, &out);
    assert(out.reference_accel_mm_s2 > 0.0f);
    assert(out.reference_accel_mm_s2 <= 36.001f); /* jerk * 20 ms */
    assert(out.target_angle_mrad > 0.0f);
    assert(fabsf(out.target_angle_mrad) <= ctx.cfg.max_angle_mrad);

    assert(BallCtrlCore_StepInner(&ctx, &inner));
    BallCtrlCore_GetOutput(&ctx, &out);
    assert(out.signed_step_freq_hz > 0.0f);
    assert(fabsf(out.signed_step_freq_hz) <= ctx.cfg.max_step_freq_hz);

    angle_before_dropout = fabsf(out.target_angle_mrad);
    outer.valid = false;
    assert(!BallCtrlCore_StepOuter(&ctx, &outer));
    BallCtrlCore_GetOutput(&ctx, &out);
    assert(!out.measurement_valid);
    assert(fabsf(out.target_angle_mrad) <= angle_before_dropout);

    outer.valid = true;
    assert(BallCtrlCore_StepOuter(&ctx, &outer));
    BallCtrlCore_GetOutput(&ctx, &out);
    assert(out.measurement_valid);

    inner.enabled = false;
    assert(!BallCtrlCore_StepInner(&ctx, &inner));
    BallCtrlCore_GetOutput(&ctx, &out);
    assert(out.signed_step_freq_hz == 0.0f);

    assert(BallCtrlCore_SetProfile(&ctx, BALL_CTRL_PROFILE_FAST));
    assert(ctx.cfg.max_ref_vel_mm_s == 140.0f);
    puts("ball control core tests: PASS");
    return 0;
}
