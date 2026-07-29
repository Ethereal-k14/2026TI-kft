/**
 * @file app_line_track.c
 * @brief BSP adapter for the portable line-follower core.
 */
#include "app_line_track.h"
#include "bsp_motor.h"
#include "project_config.h"
#include <stddef.h>
#include <string.h>

#if defined(PRJ_DRV8870_FACTORY_TEST_ENABLE) && \
    (PRJ_DRV8870_FACTORY_TEST_ENABLE != 0)
#define APP_LINE_TRACK_BUILD_ENABLE 0
#else
#define APP_LINE_TRACK_BUILD_ENABLE 1
#endif

#if defined(PRJ_MOTOR_COMMAND_MAX)
#define APP_LINE_TRACK_COMMAND_MAX ((int32_t)(PRJ_MOTOR_COMMAND_MAX))
#else
#define APP_LINE_TRACK_COMMAND_MAX ((int32_t)(LINE_TRACK_COMMAND_MAX))
#endif

static volatile bool s_running;
static line_track_output_t s_last_output;

#if APP_LINE_TRACK_BUILD_ENABLE
static const uint8_t s_ch_map[BSP_IR_CHANNEL_COUNT] = LINE_TRACK_CH_MAP;
static app_line_follower_t s_follower;
static float s_command_scale = LINE_TRACK_COMMAND_SCALE;

static int32_t speed_to_command(float speed_mm_s)
{
    const float command = speed_mm_s * s_command_scale;
    if (command >= (float)APP_LINE_TRACK_COMMAND_MAX) {
        return APP_LINE_TRACK_COMMAND_MAX;
    }
    if (command <= -(float)APP_LINE_TRACK_COMMAND_MAX) {
        return -APP_LINE_TRACK_COMMAND_MAX;
    }
    return (int32_t)command;
}

static void map_output(const uint8_t raw[BSP_IR_CHANNEL_COUNT],
                       const app_line_follower_output_t *core,
                       line_track_output_t *out)
{
    (void)memcpy(out->ir_raw, raw, sizeof(out->ir_raw));
    out->left_target_speed = core->left_speed_mm_s * 0.001f;
    out->right_target_speed = core->right_speed_mm_s * 0.001f;
    out->left_command = speed_to_command(core->left_speed_mm_s);
    out->right_command = speed_to_command(core->right_speed_mm_s);
    out->turn_diff = core->steering_mm_s;
    out->base_speed_mm = core->base_speed_mm_s;
    out->current_state = (int)core->sensor_bits;
    out->turn90_active = false;
    out->line_error = core->line_error;
    out->steering_mm_s = core->steering_mm_s;
    out->left_accel_mm_s2 = core->left_accel_mm_s2;
    out->right_accel_mm_s2 = core->right_accel_mm_s2;
    out->curve_speed_limit_mm_s = core->curve_speed_limit_mm_s;
    out->target_yaw_rate_dps = core->target_yaw_rate_dps;
    out->lost_ms = core->lost_ms;
    out->sensor_bits = core->sensor_bits;
    out->planner_limited = core->planner_limited;
}

void app_line_track_init(void)
{
    app_line_follower_init(&s_follower, NULL);
    (void)app_line_track_set_profile(
        (app_line_profile_t)PRJ_LINE_TRACK_DEFAULT_PROFILE);
    (void)memset(&s_last_output, 0, sizeof(s_last_output));
    s_running = false;
}

void app_line_track_start(void)
{
    app_line_follower_reset(&s_follower);
    (void)memset(&s_last_output, 0, sizeof(s_last_output));
    s_running = true;
}

void app_line_track_stop(void)
{
    s_running = false;
    app_line_follower_reset(&s_follower);
    (void)memset(&s_last_output, 0, sizeof(s_last_output));
}

bool app_line_track_is_running(void) { return s_running; }

bool app_line_track_step(void)
{
    return app_line_track_step_with_feedback(0.0f, 0.005f);
}

bool app_line_track_step_with_feedback(float yaw_rate_dps, float dt_s)
{
    line_track_input_t input;
    if (!s_running) { return false; }
    BSP_IR_Read(input.ir_raw);
    input.yaw_rate_dps = yaw_rate_dps;
    input.dt_s = dt_s;
    return app_line_track_update_sample(&input, &s_last_output);
}

void app_line_track_reset(void)
{
    app_line_follower_reset(&s_follower);
    (void)memset(&s_last_output, 0, sizeof(s_last_output));
}

void app_line_track_update(line_track_output_t *out)
{
    line_track_input_t input;
    if (out == NULL) { return; }
    BSP_IR_Read(input.ir_raw);
    input.yaw_rate_dps = 0.0f;
    input.dt_s = 0.005f;
    (void)app_line_track_update_sample(&input, out);
}

bool app_line_track_update_sample(const line_track_input_t *input,
                                  line_track_output_t *out)
{
    app_line_follower_input_t core_input;
    app_line_follower_output_t core_output;
    if ((input == NULL) || (out == NULL)) { return false; }
    for (uint32_t i = 0U; i < BSP_IR_CHANNEL_COUNT; i++) {
        core_input.black[i] = input->ir_raw[s_ch_map[i]] != 0U ? 1U : 0U;
    }
    core_input.yaw_rate_dps = input->yaw_rate_dps;
    core_input.dt_s = input->dt_s;
    if (!app_line_follower_step(&s_follower, &core_input, &core_output)) {
        return false;
    }
    map_output(input->ir_raw, &core_output, out);
    return true;
}

bool app_line_track_set_profile(app_line_profile_t profile)
{
    app_line_follower_cfg_t cfg;
    s_command_scale = LINE_TRACK_COMMAND_SCALE;
    if (!app_line_follower_set_profile(&s_follower, profile)) { return false; }
    cfg = s_follower.cfg;
    cfg.track_width_mm = PRJ_CF_WHEEL_BASE_M * 1000.0f;
    return app_line_follower_configure(&s_follower, &cfg);
}

bool app_line_track_configure(const app_line_follower_cfg_t *cfg,
                              float command_scale)
{
    if (command_scale <= 0.0f) { return false; }
    if (!app_line_follower_configure(&s_follower, cfg)) { return false; }
    s_command_scale = command_scale;
    return true;
}

const app_line_follower_cfg_t *app_line_track_get_config(void)
{
    return &s_follower.cfg;
}

void app_line_track_restore_default_params(void)
{
    (void)app_line_track_set_profile(APP_LINE_PROFILE_BALANCED);
}

const line_track_output_t *app_line_track_get_output(void)
{
    return &s_last_output;
}

void app_line_track_stop_motors(void) { bsp_motor_stop_all(); }

const char *app_line_track_state_name(int state)
{
    switch (state) {
    case LINE_TRACK_STATE_CROSS:       return "CROSS";
    case LINE_TRACK_STATE_LEFT_90_A:   return "L90A";
    case LINE_TRACK_STATE_LEFT_90_B:   return "L90B";
    case LINE_TRACK_STATE_RIGHT_90_A:  return "R90A";
    case LINE_TRACK_STATE_RIGHT_90_B:  return "R90B";
    case LINE_TRACK_STATE_LEFT_BIG:    return "L_BIG";
    case LINE_TRACK_STATE_RIGHT_BIG:   return "R_BIG";
    case LINE_TRACK_STATE_LEFT_SMALL:  return "L_MIN";
    case LINE_TRACK_STATE_RIGHT_SMALL: return "R_MIN";
    case LINE_TRACK_STATE_STRAIGHT:    return "STR";
    case LINE_TRACK_STATE_LOST:        return "LOST";
    default:                           return "UNK";
    }
}

#else
void app_line_track_init(void) { memset(&s_last_output, 0, sizeof(s_last_output)); s_running = false; }
void app_line_track_start(void) { s_running = false; }
void app_line_track_stop(void) { s_running = false; }
bool app_line_track_is_running(void) { return false; }
bool app_line_track_step(void) { return false; }
bool app_line_track_step_with_feedback(float yaw_rate_dps, float dt_s) { (void)yaw_rate_dps; (void)dt_s; return false; }
void app_line_track_reset(void) { memset(&s_last_output, 0, sizeof(s_last_output)); }
void app_line_track_update(line_track_output_t *out) { if (out != NULL) { memset(out, 0, sizeof(*out)); } }
bool app_line_track_update_sample(const line_track_input_t *input, line_track_output_t *out) { (void)input; if (out != NULL) { memset(out, 0, sizeof(*out)); } return false; }
bool app_line_track_set_profile(app_line_profile_t profile) { (void)profile; return false; }
bool app_line_track_configure(const app_line_follower_cfg_t *cfg, float command_scale) { (void)cfg; (void)command_scale; return false; }
const app_line_follower_cfg_t *app_line_track_get_config(void) { return NULL; }
void app_line_track_restore_default_params(void) {}
const line_track_output_t *app_line_track_get_output(void) { return &s_last_output; }
void app_line_track_stop_motors(void) {}
const char *app_line_track_state_name(int state) { (void)state; return "OFF"; }
#endif
