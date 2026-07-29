/**
 * @file app_identification.h
 * @brief Bounded +5 cm/-5 cm step acceptance and identification sequence.
 */
#ifndef APP_IDENTIFICATION_H
#define APP_IDENTIFICATION_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_ID_IDLE = 0,
    APP_ID_RUNNING,
    APP_ID_COMPLETE,
    APP_ID_ABORTED
} app_id_state_t;

typedef struct {
    app_id_state_t state;
    uint8_t stage;
    uint32_t elapsed_ms;
    uint32_t first_settle_ms;
    uint32_t second_settle_ms;
    int32_t peak_abs_error_um;
    int32_t final_abs_error_um;
    bool passed_time;
    bool passed_error;
} app_id_result_t;

void App_Identification_Init(void);
bool App_Identification_Start(void);
void App_Identification_Process(void);
void App_Identification_Abort(void);
void App_Identification_GetResult(app_id_result_t *out);
bool App_Identification_IsRunning(void);

#ifdef __cplusplus
}
#endif
#endif
