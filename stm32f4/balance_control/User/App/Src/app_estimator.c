/**
 * @file    app_estimator.c
 * @brief   位置/速度状态估计器实现
 *
 *  采用互补滤波思路：
 *    预测：pos += vel * dt（基于磁编码器速度）
 *    更新：若视觉有效且可用，用加权补偿误差
 *          若激光有效且已配置映射，同理
 *  异常值拒绝：残差绝对值 > outlier_gate_um 时拒绝该测量
 */
#include "app_estimator.h"
#include "app_vision.h"
#include "bsp_encoder.h"
#include "bsp_lidar.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 私有状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    estimator_cfg_t  cfg;
    estimator_state_t state;

    uint32_t last_update_us;

    /* 上一次处理过的视觉时间戳（去重） */
    uint32_t last_vision_ts;
    /* 上一次处理过的激光时间戳 */
    uint32_t last_lidar_ts;
} est_ctx_t;

static est_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void App_Estimator_Init(const estimator_cfg_t *cfg)
{
    (void)memset(&s_ctx, 0, sizeof(s_ctx));
    if (cfg != NULL)
    {
        s_ctx.cfg = *cfg;
        if (s_ctx.cfg.mrad_per_count <= 0.0f)
        {
            s_ctx.cfg.mrad_per_count = 1.570796f;
        }
    }
    else
    {
        /* 默认配置：仅视觉，激光禁用 */
        s_ctx.cfg.alpha_vision    = 0.8f;
        s_ctx.cfg.alpha_lidar     = 0.0f;
        s_ctx.cfg.outlier_gate_um = 50000; /* 50 mm */
        s_ctx.cfg.lidar_enabled   = false;
        s_ctx.cfg.mrad_per_count  = 1.570796f; /* 4000 count/rev 的初值 */
    }
    s_ctx.last_update_us = BSP_GetTimestampUs();
}

void App_Estimator_Update(void)
{
    uint32_t now_us = BSP_GetTimestampUs();
    uint32_t dt_us  = now_us - s_ctx.last_update_us;
    s_ctx.last_update_us = now_us;

    /* ---- 预测步骤（基于编码器角度/速度）---- */
    encoder_state_t enc;
    BSP_Encoder_GetState(&enc);

    /* 更新角度/角速度：计数先经过机构标定比例转换为 mrad。 */
    s_ctx.state.angle_mrad =
        (int32_t)((float)enc.position_count * s_ctx.cfg.mrad_per_count);
    s_ctx.state.ang_vel_mrad_s =
        (int32_t)((float)enc.velocity_count_s * s_ctx.cfg.mrad_per_count);

    /* 位置预测：pos += vel * dt（速度以 µm/s 计） */
    if (dt_us > 0U && dt_us < 100000U) /* 限制 dt < 100 ms 防异常 */
    {
        int64_t delta = ((int64_t)s_ctx.state.vel_um_s * (int64_t)dt_us) / 1000000;
        s_ctx.state.pos_um    = (int32_t)((int64_t)s_ctx.state.pos_um + delta);
    }
    s_ctx.state.timestamp_us = now_us;
    s_ctx.state.valid        = true;

    /* ---- 视觉测量更新 ---- */
    vision_pose_t vp;
    App_Vision_GetPose(&vp);

    if (vp.valid && vp.usable && vp.timestamp_us != s_ctx.last_vision_ts)
    {
        s_ctx.last_vision_ts = vp.timestamp_us;

        int32_t residual = vp.position_um - s_ctx.state.pos_um;
        /* 异常值拒绝 */
        if ((residual > -s_ctx.cfg.outlier_gate_um) &&
            (residual <  s_ctx.cfg.outlier_gate_um))
        {
            /* 互补更新 */
            s_ctx.state.pos_um    = s_ctx.state.pos_um +
                                    (int32_t)((float)residual * s_ctx.cfg.alpha_vision);
            /* 速度软更新 */
            int32_t vel_residual  = vp.velocity_um_s - s_ctx.state.vel_um_s;
            s_ctx.state.vel_um_s  = s_ctx.state.vel_um_s +
                                    (int32_t)((float)vel_residual * s_ctx.cfg.alpha_vision * 0.5f);
        }
    }

    /* ---- 激光测量更新（仅当 lidar_enabled 且几何已标定） ---- */
    if (s_ctx.cfg.lidar_enabled)
    {
        sensor_sample_t lidar;
        BSP_Lidar_GetSample(&lidar);

        if (lidar.valid && lidar.timestamp_us != s_ctx.last_lidar_ts)
        {
            s_ctx.last_lidar_ts = lidar.timestamp_us;
            /* 激光距离 → 被控位置需要外部提供映射函数，
               此处直接用距离作为位置近似（需用户标定） */
            int32_t lidar_pos_um = lidar.value * 1000; /* mm → µm */
            int32_t residual     = lidar_pos_um - s_ctx.state.pos_um;
            if ((residual > -s_ctx.cfg.outlier_gate_um) &&
                (residual <  s_ctx.cfg.outlier_gate_um))
            {
                s_ctx.state.pos_um = s_ctx.state.pos_um +
                                     (int32_t)((float)residual * s_ctx.cfg.alpha_lidar);
            }
        }
    }
}

void App_Estimator_GetState(estimator_state_t *out)
{
    if (out != NULL) { *out = s_ctx.state; }
}

void App_Estimator_Reset(void)
{
    s_ctx.state.pos_um       = 0;
    s_ctx.state.vel_um_s     = 0;
    s_ctx.state.valid        = false;
    s_ctx.last_update_us     = BSP_GetTimestampUs();
}
