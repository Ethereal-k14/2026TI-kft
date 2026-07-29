/**
 * @file    app_controller.c
 * @brief   双环 PID 控制器实现
 *
 *  内环：磁编角度误差 → PID → 步频（直接驱动 TMC2209）
 *  外环：位置误差   → PID → 目标摆杆角（内环设定点）
 *
 *  积分抗饱和（clamping）：
 *    当总输出已饱和且误差同号时，停止积分累积
 *
 *  加速度前馈：
 *    step_ff = accel_ff_gain × ax_mm_s2 × feedfwd_weight
 *    初始 gain = 0，验证延迟/符号后才增加
 */
#include "app_controller.h"
#include "app_estimator.h"
#include "app_chassis.h"
#include "bsp_encoder.h"
#include "bsp_stepper.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 单轴 PID 运行状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    float integral;
    float prev_error;
    bool  initialized;
} pid_state_t;

/* -------------------------------------------------------------------------
 * 私有上下文
 * ---------------------------------------------------------------------- */
typedef struct
{
    ctrl_cfg_t    cfg;
    ctrl_output_t output;

    pid_state_t   inner_angle_state;
    pid_state_t   inner_rate_state;
    pid_state_t   outer_pos_state;
    pid_state_t   outer_vel_state;

    int32_t       target_pos_um;   /* 外环目标位置 */
    int32_t       target_angle_mrad; /* 外环输出的目标摆杆角 */
} ctrl_ctx_t;

static ctrl_ctx_t s_ctx;

/* -------------------------------------------------------------------------
 * 私有：单轴 PID 计算（带 clamping 抗饱和）
 * ---------------------------------------------------------------------- */
static float pid_compute(pid_state_t    *st,
                          const pid_cfg_t *cfg,
                          float           error)
{
    /* 比例项 */
    float p_term = cfg->kp * error;

    /* 积分项（梯形法，clamping 抗饱和） */
    st->integral += cfg->ki * error * cfg->dt_s;
    /* 限幅积分 */
    if (st->integral >  cfg->integral_limit) { st->integral =  cfg->integral_limit; }
    if (st->integral < -cfg->integral_limit) { st->integral = -cfg->integral_limit; }
    float i_term = st->integral;

    /* 微分项（后向差分） */
    float d_term = 0.0f;
    if (st->initialized)
    {
        d_term = cfg->kd * (error - st->prev_error) / cfg->dt_s;
    }
    st->prev_error   = error;
    st->initialized  = true;

    float output = p_term + i_term + d_term;

    /* 输出限幅，clamping：若已饱和且误差同号，停止积分 */
    bool saturated = false;
    if (output >  cfg->output_limit) { output =  cfg->output_limit; saturated = true; }
    if (output < -cfg->output_limit) { output = -cfg->output_limit; saturated = true; }

    if (saturated)
    {
        /* 若误差与积分同号（同向饱和），停止积分增长 */
        if ((error > 0.0f && st->integral > 0.0f) ||
            (error < 0.0f && st->integral < 0.0f))
        {
            st->integral -= cfg->ki * error * cfg->dt_s;
        }
    }

    return output;
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void App_Controller_Init(const ctrl_cfg_t *cfg)
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
        /* 保守默认值（上电后不运动，需调参） */
        s_ctx.cfg.inner_angle.kp            = 1.0f;
        s_ctx.cfg.inner_angle.ki            = 0.0f;
        s_ctx.cfg.inner_angle.kd            = 0.05f;
        s_ctx.cfg.inner_angle.integral_limit = 200.0f;
        s_ctx.cfg.inner_angle.output_limit   = 500.0f;
        s_ctx.cfg.inner_angle.dt_s           = 0.001f; /* 1 kHz */

        s_ctx.cfg.inner_rate.kp             = 0.5f;
        s_ctx.cfg.inner_rate.ki             = 0.0f;
        s_ctx.cfg.inner_rate.kd             = 0.0f;
        s_ctx.cfg.inner_rate.integral_limit = 100.0f;
        s_ctx.cfg.inner_rate.output_limit   = 5000.0f;
        s_ctx.cfg.inner_rate.dt_s           = 0.001f;

        s_ctx.cfg.outer_pos.kp              = 0.5f;
        s_ctx.cfg.outer_pos.ki              = 0.0f;
        s_ctx.cfg.outer_pos.kd              = 0.0f;
        s_ctx.cfg.outer_pos.integral_limit  = 200.0f;
        s_ctx.cfg.outer_pos.output_limit    = 300.0f;  /* mrad */
        s_ctx.cfg.outer_pos.dt_s            = 0.02f;   /* 50 Hz */

        s_ctx.cfg.outer_vel.kp              = 0.2f;
        s_ctx.cfg.outer_vel.ki              = 0.0f;
        s_ctx.cfg.outer_vel.kd              = 0.0f;
        s_ctx.cfg.outer_vel.integral_limit  = 100.0f;
        s_ctx.cfg.outer_vel.output_limit    = 200.0f;
        s_ctx.cfg.outer_vel.dt_s            = 0.02f;

        s_ctx.cfg.max_step_freq_hz  = 20000;
        s_ctx.cfg.max_angle_mrad    = 500;
        s_ctx.cfg.max_pos_um        = 200000; /* 200 mm */
        s_ctx.cfg.accel_ff_gain     = 0.0f;  /* 初始禁用 */
        s_ctx.cfg.mrad_per_count    = 1.570796f; /* 4000 count/rev 的初值 */
    }
}

void App_Controller_InnerLoop(void)
{
    /* 读取磁编码器状态 */
    encoder_state_t enc;
    BSP_Encoder_GetState(&enc);

    /* 角度误差（内环设定点和反馈均为 mrad）。 */
    float angle_mrad = (float)enc.position_count * s_ctx.cfg.mrad_per_count;
    float rate_mrad_s = (float)enc.velocity_count_s * s_ctx.cfg.mrad_per_count;
    float angle_error = (float)s_ctx.target_angle_mrad - angle_mrad;
    float rate_sp     = pid_compute(&s_ctx.inner_angle_state,
                                     &s_ctx.cfg.inner_angle,
                                     angle_error);

    /* 角速度误差 */
    float rate_error  = rate_sp - rate_mrad_s;
    float freq_delta  = pid_compute(&s_ctx.inner_rate_state,
                                     &s_ctx.cfg.inner_rate,
                                     rate_error);

    /* 加速度前馈 */
    chassis_imu_t imu;
    App_Chassis_GetImu(&imu);
    float ff = 0.0f;
    if (imu.valid)
    {
        ff = s_ctx.cfg.accel_ff_gain * (float)imu.ax_mm_s2 * imu.feedfwd_weight;
    }

    float freq_out = freq_delta + ff;

    /* 方向和频率分离 */
    bool dir_fwd = (freq_out >= 0.0f);
    if (!dir_fwd) { freq_out = -freq_out; }

    /* 限幅 */
    int32_t freq_hz = (int32_t)freq_out;
    if (freq_hz > s_ctx.cfg.max_step_freq_hz)
    {
        freq_hz = s_ctx.cfg.max_step_freq_hz;
    }
    if (freq_hz < BSP_STEPPER_FREQ_MIN)
    {
        freq_hz = 0;
    }

    /* 写入步进驱动 */
    BSP_Stepper_SetDir(dir_fwd);
    BSP_Stepper_SetFreq((uint32_t)freq_hz);

    /* 更新输出状态 */
    s_ctx.output.target_step_freq_hz = freq_hz;
    s_ctx.output.dir_fwd             = dir_fwd;
}

void App_Controller_OuterLoop(void)
{
    estimator_state_t est;
    App_Estimator_GetState(&est);

    if (!est.valid)
    {
        /* 估计器无效，不更新外环 */
        return;
    }

    /* 位置误差 */
    float pos_error = (float)(s_ctx.target_pos_um - est.pos_um);
    /* 限幅目标角 */
    pos_error = CLAMP(pos_error,
                      (float)(-s_ctx.cfg.max_pos_um),
                      (float)( s_ctx.cfg.max_pos_um));

    float target_angle = pid_compute(&s_ctx.outer_pos_state,
                                      &s_ctx.cfg.outer_pos,
                                      pos_error);

    /* 速度前馈（速度 PID） */
    float vel_error    = -(float)est.vel_um_s; /* 期望速度为 0（保持位置） */
    target_angle      += pid_compute(&s_ctx.outer_vel_state,
                                      &s_ctx.cfg.outer_vel,
                                      vel_error);

    /* 机械角度限制 */
    target_angle = CLAMP(target_angle,
                         (float)(-s_ctx.cfg.max_angle_mrad),
                         (float)( s_ctx.cfg.max_angle_mrad));

    s_ctx.target_angle_mrad      = (int32_t)target_angle;
    s_ctx.output.target_angle_mrad = s_ctx.target_angle_mrad;
}

void App_Controller_GetOutput(ctrl_output_t *out)
{
    if (out != NULL) { *out = s_ctx.output; }
}

void App_Controller_Reset(void)
{
    (void)memset(&s_ctx.inner_angle_state, 0, sizeof(s_ctx.inner_angle_state));
    (void)memset(&s_ctx.inner_rate_state,  0, sizeof(s_ctx.inner_rate_state));
    (void)memset(&s_ctx.outer_pos_state,   0, sizeof(s_ctx.outer_pos_state));
    (void)memset(&s_ctx.outer_vel_state,   0, sizeof(s_ctx.outer_vel_state));
    s_ctx.target_angle_mrad = 0;
    s_ctx.output.target_step_freq_hz = 0;
}

void App_Controller_SetTargetPos(int32_t target_um)
{
    s_ctx.target_pos_um = target_um;
}
