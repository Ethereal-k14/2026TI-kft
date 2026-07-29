/**
 * @file app_identification.c
 * @brief Competition static step sequence with measured acceptance metrics.
 */
#include "app_identification.h"
#include "app_controller.h"
#include "app_estimator.h"
#include "app_safety.h"
#include <stdlib.h>
#include <string.h>

#define ID_STEP_UM (50000)
#define ID_SETTLE_BAND_UM (10000)
#define ID_SETTLE_HOLD_MS (250U)
#define ID_TOTAL_LIMIT_MS (5000U)

typedef struct {
    app_id_result_t result;
    uint32_t start_ms;
    uint32_t stage_start_ms;
    uint32_t in_band_since_ms;
    int32_t target_um;
} app_id_ctx_t;

static app_id_ctx_t s_ctx;

void App_Identification_Init(void)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
}

bool App_Identification_Start(void)
{
    if (!App_Safety_IsRunning() ||
        (s_ctx.result.state == APP_ID_RUNNING) ||
        !App_Controller_SetTargetOwned(CTRL_TARGET_OWNER_IDENTIFICATION,
                                       ID_STEP_UM)) {
        return false;
    }
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.result.state = APP_ID_RUNNING;
    s_ctx.result.stage = 1U;
    s_ctx.start_ms = HAL_GetTick();
    s_ctx.stage_start_ms = s_ctx.start_ms;
    s_ctx.target_um = ID_STEP_UM;
    return true;
}

void App_Identification_Process(void)
{
    estimator_state_t est;
    uint32_t now;
    int32_t abs_error;
    if (s_ctx.result.state != APP_ID_RUNNING) { return; }
    if (!App_Safety_IsRunning()) { App_Identification_Abort(); return; }
    now = HAL_GetTick();
    App_Estimator_GetState(&est);
    if (!est.valid) { return; }
    abs_error = abs(s_ctx.target_um - est.pos_um);
    s_ctx.result.final_abs_error_um = abs_error;
    if (abs_error > s_ctx.result.peak_abs_error_um) {
        s_ctx.result.peak_abs_error_um = abs_error;
    }
    if (abs_error <= ID_SETTLE_BAND_UM) {
        if (s_ctx.in_band_since_ms == 0U) { s_ctx.in_band_since_ms = now; }
        if ((now - s_ctx.in_band_since_ms) >= ID_SETTLE_HOLD_MS) {
            const uint32_t settle = now - s_ctx.stage_start_ms;
            if (s_ctx.result.stage == 1U) {
                s_ctx.result.first_settle_ms = settle;
                s_ctx.result.stage = 2U;
                s_ctx.target_um = -ID_STEP_UM;
                s_ctx.stage_start_ms = now;
                s_ctx.in_band_since_ms = 0U;
                (void)App_Controller_SetTargetOwned(
                    CTRL_TARGET_OWNER_IDENTIFICATION, s_ctx.target_um);
            } else {
                s_ctx.result.second_settle_ms = settle;
                s_ctx.result.elapsed_ms = now - s_ctx.start_ms;
                s_ctx.result.passed_time =
                    s_ctx.result.elapsed_ms <= ID_TOTAL_LIMIT_MS;
                s_ctx.result.passed_error = abs_error <= ID_SETTLE_BAND_UM;
                s_ctx.result.state = APP_ID_COMPLETE;
                App_Controller_ReleaseTarget(CTRL_TARGET_OWNER_IDENTIFICATION);
            }
        }
    } else {
        s_ctx.in_band_since_ms = 0U;
    }
    s_ctx.result.elapsed_ms = now - s_ctx.start_ms;
    if (s_ctx.result.elapsed_ms > ID_TOTAL_LIMIT_MS) {
        s_ctx.result.passed_time = false;
        s_ctx.result.passed_error = false;
        s_ctx.result.state = APP_ID_COMPLETE;
        App_Controller_ReleaseTarget(CTRL_TARGET_OWNER_IDENTIFICATION);
    }
}

void App_Identification_Abort(void)
{
    if (s_ctx.result.state == APP_ID_RUNNING) {
        s_ctx.result.state = APP_ID_ABORTED;
        App_Controller_ReleaseTarget(CTRL_TARGET_OWNER_IDENTIFICATION);
    }
}

void App_Identification_GetResult(app_id_result_t *out)
{
    if (out != NULL) { *out = s_ctx.result; }
}

bool App_Identification_IsRunning(void)
{
    return s_ctx.result.state == APP_ID_RUNNING;
}
