/**
 * @file app_estimator.h
 * @brief Camera-anchored alpha-beta estimator with bounded prediction.
 */
#ifndef APP_ESTIMATOR_H
#define APP_ESTIMATOR_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EST_SOURCE_VISION (1U << 0U)
#define EST_SOURCE_LIDAR  (1U << 1U)
#define EST_SOURCE_ENCODER (1U << 2U)

typedef struct {
    int32_t pos_um;
    int32_t vel_um_s;
    int32_t angle_mrad;
    int32_t ang_vel_mrad_s;
    uint32_t timestamp_us;
    uint32_t vision_age_us;
    uint32_t accepted_vision_count;
    uint32_t rejected_vision_count;
    uint8_t source_flags;
    bool valid;
    bool vision_locked;
    bool predicting;
} estimator_state_t;

typedef struct {
    float alpha_vision;
    float beta_vision;
    int32_t outlier_gate_um;
    int32_t max_velocity_um_s;
    uint32_t max_prediction_us;
    bool lidar_enabled;
    bool lidar_calibrated;
    float lidar_scale_um_per_mm;
    int32_t lidar_offset_um;
    float alpha_lidar;
    float mrad_per_count;
} estimator_cfg_t;

void App_Estimator_Init(const estimator_cfg_t *cfg);
bool App_Estimator_Configure(const estimator_cfg_t *cfg);
void App_Estimator_Update(void);
void App_Estimator_GetState(estimator_state_t *out);
void App_Estimator_Reset(void);

#ifdef __cplusplus
}
#endif
#endif
