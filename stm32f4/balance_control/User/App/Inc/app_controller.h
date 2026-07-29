/**
 * @file app_controller.h
 * @brief Hardware adapter for the portable robust cascade controller.
 */
#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "bsp_common.h"
#include "ball_control_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ball_ctrl_core_cfg_t core;
    float mrad_per_count;
    int32_t max_pos_um;
} ctrl_cfg_t;

typedef struct {
    int32_t target_step_freq_hz;
    bool dir_fwd;
    int32_t target_angle_mrad;
    int32_t reference_pos_um;
    int32_t reference_vel_um_s;
    bool measurement_valid;
    bool saturated;
} ctrl_output_t;

typedef enum {
    CTRL_TARGET_OWNER_OPERATOR = 0,
    CTRL_TARGET_OWNER_IDENTIFICATION,
    CTRL_TARGET_OWNER_COMPETITION
} ctrl_target_owner_t;

void App_Controller_Init(const ctrl_cfg_t *cfg);
bool App_Controller_Configure(const ctrl_cfg_t *cfg);
bool App_Controller_SetProfile(ball_ctrl_profile_t profile);
void App_Controller_InnerLoop(void);
void App_Controller_OuterLoop(void);
void App_Controller_GetOutput(ctrl_output_t *out);
void App_Controller_Reset(void);
void App_Controller_SetTargetPos(int32_t target_um);
bool App_Controller_SetTargetOwned(ctrl_target_owner_t owner,
                                   int32_t target_um);
void App_Controller_ReleaseTarget(ctrl_target_owner_t owner);
ctrl_target_owner_t App_Controller_GetTargetOwner(void);

#ifdef __cplusplus
}
#endif
#endif
