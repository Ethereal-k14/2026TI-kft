/**
 * @file    app_controller.h
 * @brief   双环 PID 控制器接口（规范 §6）
 *
 *  内环（角度/角速度 → 步频），≥500 Hz，带宽 20–40 Hz
 *  外环（位置/速度 → 目标角），50–100 Hz，带宽 = 内环 1/5 以内
 *
 *  所有控制系数集中在配置结构体，禁止在算法中散布魔数。
 *  输出包含速度、加速度、角度、步频和机械行程限制。
 *  积分项使用 clamping 法抗饱和。
 */
#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 单轴 PID 参数
 * ---------------------------------------------------------------------- */
typedef struct
{
    float kp;
    float ki;
    float kd;
    float integral_limit;   /* 积分项输出限幅 */
    float output_limit;     /* 总输出限幅 */
    float dt_s;             /* 调用周期（秒），用于积分/微分 */
} pid_cfg_t;

/* -------------------------------------------------------------------------
 * 控制器配置（所有系数集中于此）
 * ---------------------------------------------------------------------- */
typedef struct
{
    pid_cfg_t inner_angle;    /* 内环：角度误差 → 角速度 */
    pid_cfg_t inner_rate;     /* 内环：角速度误差 → 步频增量 */
    pid_cfg_t outer_pos;      /* 外环：位置误差 → 目标角 */
    pid_cfg_t outer_vel;      /* 外环：速度误差 → 目标角速度 */

    /* 机械限制 */
    int32_t  max_step_freq_hz;   /* 步频上限（Hz） */
    int32_t  max_angle_mrad;     /* 摆杆最大偏角（mrad，正负对称） */
    int32_t  max_pos_um;         /* 最大位置偏移（µm） */

    /* 加速度前馈增益（初始 0，验证后开启） */
    float    accel_ff_gain;
    float    mrad_per_count;     /* 编码器计数到摆杆角度的标定比例 */
} ctrl_cfg_t;

/* -------------------------------------------------------------------------
 * 控制器输出
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  target_step_freq_hz;  /* 步进脉冲频率（Hz），正值 */
    bool     dir_fwd;              /* 方向 */
    int32_t  target_angle_mrad;    /* 外环输出的目标摆杆角（供内环参考） */
} ctrl_output_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/** @brief 初始化控制器（加载配置，清零积分状态） */
void App_Controller_Init(const ctrl_cfg_t *cfg);

/**
 * @brief 内环计算（500–1000 Hz 调用）
 *        - 读取磁编码器角度/角速度
 *        - 计算 PID，输出步频和方向
 *        - 写入 BSP_Stepper_SetFreq / SetDir
 */
void App_Controller_InnerLoop(void);

/**
 * @brief 外环计算（50–100 Hz 调用）
 *        - 读取估计器位置/速度
 *        - 计算 PID，输出目标角（供内环使用）
 *        - 加速度前馈（增益初始为 0）
 */
void App_Controller_OuterLoop(void);

/** @brief 读取控制器最新输出 */
void App_Controller_GetOutput(ctrl_output_t *out);

/**
 * @brief 重置控制器（清零所有积分状态）
 *        在急停、启动、故障恢复时调用
 */
void App_Controller_Reset(void);

/** @brief 更新外环目标位置（µm） */
void App_Controller_SetTargetPos(int32_t target_um);

#ifdef __cplusplus
}
#endif
#endif /* APP_CONTROLLER_H */
