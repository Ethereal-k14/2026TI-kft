/**
 * @file app_estimator.c
 * @brief Camera-anchored, latency-compensated alpha-beta estimator.
 */
#include "app_estimator.h"
#include "app_vision.h"
#include "bsp_encoder.h"
#include "bsp_lidar.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    estimator_cfg_t cfg;
    estimator_state_t state;
    float pos_um;
    float vel_um_s;
    uint32_t last_update_us;
    uint32_t last_vision_ts;
    uint32_t last_lidar_ts;
    uint32_t last_vision_accept_us;
    uint32_t last_vision_update_us;
} est_ctx_t;

static est_ctx_t s_ctx;

static float clampf(float v, float lo, float hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

static bool cfg_valid(const estimator_cfg_t *c)
{
    return (c != NULL) && (c->alpha_vision > 0.0f) &&
           (c->alpha_vision <= 1.0f) && (c->beta_vision > 0.0f) &&
           (c->beta_vision <= 1.0f) && (c->outlier_gate_um > 0) &&
           (c->max_velocity_um_s > 0) && (c->max_prediction_us > 0U) &&
           (c->alpha_lidar >= 0.0f) && (c->alpha_lidar <= 1.0f) &&
           (c->mrad_per_count > 0.0f) &&
           (!c->lidar_enabled ||
            (c->lidar_calibrated && (c->lidar_scale_um_per_mm != 0.0f)));
}

static estimator_cfg_t default_cfg(void)
{
    estimator_cfg_t c = {
        0.65f, 0.18f, 40000, 800000, 200000U,
        false, false, 1000.0f, 0, 0.15f, 1.570796f
    };
    return c;
}

void App_Estimator_Init(const estimator_cfg_t *cfg)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.cfg = cfg_valid(cfg) ? *cfg : default_cfg();
    s_ctx.last_update_us = BSP_GetTimestampUs();
}

bool App_Estimator_Configure(const estimator_cfg_t *cfg)
{
    if (!cfg_valid(cfg)) { return false; }
    s_ctx.cfg = *cfg;
    App_Estimator_Reset();
    return true;
}

void App_Estimator_Update(void)
{
    const uint32_t now_us = BSP_GetTimestampUs();
    uint32_t dt_us = now_us - s_ctx.last_update_us;
    float dt_s;
    encoder_state_t enc;
    vision_pose_t vp;
    bool accepted = false;
    s_ctx.last_update_us = now_us;
    if ((dt_us == 0U) || (dt_us > 100000U)) { dt_us = 10000U; }
    dt_s = (float)dt_us * 1.0e-6f;

    BSP_Encoder_GetState(&enc);
    s_ctx.state.angle_mrad =
        (int32_t)((float)enc.position_count * s_ctx.cfg.mrad_per_count);
    s_ctx.state.ang_vel_mrad_s =
        (int32_t)((float)enc.velocity_count_s * s_ctx.cfg.mrad_per_count);
    s_ctx.state.source_flags = (enc.index_valid || enc.pwm_valid) ?
        EST_SOURCE_ENCODER : 0U;

    if (s_ctx.state.vision_locked) {
        s_ctx.pos_um += s_ctx.vel_um_s * dt_s;
    }

    App_Vision_GetPose(&vp);
    if (vp.valid && vp.usable && (vp.timestamp_us != s_ctx.last_vision_ts)) {
        float measured_pos;
        float confidence_weight;
        float residual;
        float dynamic_gate;
        float vision_dt_s;
        s_ctx.last_vision_ts = vp.timestamp_us;
        measured_pos = (float)vp.position_um +
            (float)vp.velocity_um_s * (float)vp.frame_age_us * 1.0e-6f;
        confidence_weight = clampf((float)vp.confidence * 0.001f, 0.3f, 1.0f);
        vision_dt_s = s_ctx.last_vision_update_us == 0U ? dt_s :
            clampf((float)(now_us - s_ctx.last_vision_update_us) * 1.0e-6f,
                   0.005f, 0.1f);
        s_ctx.last_vision_update_us = now_us;
        if (!s_ctx.state.vision_locked) {
            s_ctx.pos_um = measured_pos;
            s_ctx.vel_um_s = clampf((float)vp.velocity_um_s,
                (float)-s_ctx.cfg.max_velocity_um_s,
                (float)s_ctx.cfg.max_velocity_um_s);
            accepted = true;
        } else {
            residual = measured_pos - s_ctx.pos_um;
            dynamic_gate = (float)s_ctx.cfg.outlier_gate_um +
                fabsf(s_ctx.vel_um_s) * (float)vp.frame_age_us * 1.0e-6f;
            if (fabsf(residual) <= dynamic_gate) {
                s_ctx.pos_um += s_ctx.cfg.alpha_vision * confidence_weight * residual;
                s_ctx.vel_um_s += s_ctx.cfg.beta_vision * confidence_weight *
                    residual / vision_dt_s;
                s_ctx.vel_um_s = 0.75f * s_ctx.vel_um_s +
                    0.25f * (float)vp.velocity_um_s;
                s_ctx.vel_um_s = clampf(s_ctx.vel_um_s,
                    (float)-s_ctx.cfg.max_velocity_um_s,
                    (float)s_ctx.cfg.max_velocity_um_s);
                accepted = true;
            }
        }
        if (accepted) {
            s_ctx.state.vision_locked = true;
            s_ctx.last_vision_accept_us = now_us;
            s_ctx.state.accepted_vision_count++;
            s_ctx.state.source_flags |= EST_SOURCE_VISION;
        } else {
            s_ctx.state.rejected_vision_count++;
        }
    }

    if (s_ctx.cfg.lidar_enabled && s_ctx.cfg.lidar_calibrated &&
        s_ctx.state.vision_locked) {
        sensor_sample_t lidar;
        BSP_Lidar_GetSample(&lidar);
        if (lidar.valid && (lidar.timestamp_us != s_ctx.last_lidar_ts)) {
            float lidar_pos;
            float residual;
            s_ctx.last_lidar_ts = lidar.timestamp_us;
            lidar_pos = (float)lidar.value * s_ctx.cfg.lidar_scale_um_per_mm +
                        (float)s_ctx.cfg.lidar_offset_um;
            residual = lidar_pos - s_ctx.pos_um;
            if (fabsf(residual) <= (float)s_ctx.cfg.outlier_gate_um) {
                s_ctx.pos_um += s_ctx.cfg.alpha_lidar * residual;
                s_ctx.state.source_flags |= EST_SOURCE_LIDAR;
            }
        }
    }

    s_ctx.state.vision_age_us = s_ctx.state.vision_locked ?
        (now_us - s_ctx.last_vision_accept_us) : UINT32_MAX;
    s_ctx.state.predicting = s_ctx.state.vision_locked && !accepted;
    s_ctx.state.valid = s_ctx.state.vision_locked &&
        (s_ctx.state.vision_age_us <= s_ctx.cfg.max_prediction_us);
    s_ctx.state.pos_um = (int32_t)s_ctx.pos_um;
    s_ctx.state.vel_um_s = (int32_t)s_ctx.vel_um_s;
    s_ctx.state.timestamp_us = now_us;
}

void App_Estimator_GetState(estimator_state_t *out)
{
    if (out != NULL) { *out = s_ctx.state; }
}

void App_Estimator_Reset(void)
{
    estimator_cfg_t cfg = s_ctx.cfg;
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.cfg = cfg;
    s_ctx.last_update_us = BSP_GetTimestampUs();
}
