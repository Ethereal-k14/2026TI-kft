/**
 * @file    app_line_track.h
 * @brief   四路红外循迹控制接口。
 *
 * 使用方式很简单：
 * 1. 初始化阶段调用 app_line_track_init()；
 * 2. 需要运行时调用 app_line_track_start()；
 * 3. 在固定周期任务中调用 app_line_track_step()；
 * 4. 停止时调用 app_line_track_stop()。
 *
 * app_line_track_update() 仍保留为纯算法接口，方便单独测试或移植到其他任务。
 * app_line_track_step_with_feedback() 只读取红外并输出左右轮物理速度目标；
 * 电机映射、RPM 换算和编码器闭环由控制任务负责，算法层不直接访问电机。
 */
#ifndef APP_LINE_TRACK_H
#define APP_LINE_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "bsp_ir.h"
#include "app_line_follower.h"

/* 物理安装顺序：左到右对应 BSP 红外通道索引。 */
#ifndef LINE_TRACK_CH_MAP
#define LINE_TRACK_CH_MAP              { 0U, 1U, 2U, 3U }
#endif

/* 转向参数，单位为 mm/s 等效转向量。 */
#ifndef LINE_TRACK_TURN90_ANGLE
#define LINE_TRACK_TURN90_ANGLE         (70.0f)
#endif
#ifndef LINE_TRACK_TURN_MAX_ANGLE
#define LINE_TRACK_TURN_MAX_ANGLE       (45.0f)
#endif
#ifndef LINE_TRACK_TURN_MID_ANGLE
#define LINE_TRACK_TURN_MID_ANGLE       (20.0f)
#endif
#ifndef LINE_TRACK_TURN_MIN_ANGLE
#define LINE_TRACK_TURN_MIN_ANGLE       (15.0f)
#endif

/* 速度参数，仍沿用原循迹算法的 mm/s 单位。 */
#ifndef LINE_TRACK_BASE_SPEED
#define LINE_TRACK_BASE_SPEED           (420.0f)
#endif
#ifndef LINE_TRACK_MIN_SPEED
#define LINE_TRACK_MIN_SPEED            (220.0f)
#endif
#ifndef LINE_TRACK_MAX_SPEED
#define LINE_TRACK_MAX_SPEED            (520.0f)
#endif
#ifndef LINE_TRACK_STEER_KP
#define LINE_TRACK_STEER_KP             (135.0f)
#endif
#ifndef LINE_TRACK_STEER_KI
#define LINE_TRACK_STEER_KI             (0.0f)
#endif
#ifndef LINE_TRACK_STEER_KD
#define LINE_TRACK_STEER_KD             (0.12f)
#endif
#ifndef LINE_TRACK_YAW_DAMPING
#define LINE_TRACK_YAW_DAMPING          (0.8f)
#endif
#ifndef LINE_TRACK_ERROR_FILTER_ALPHA
#define LINE_TRACK_ERROR_FILTER_ALPHA   (0.55f)
#endif
#ifndef LINE_TRACK_INTEGRAL_LIMIT
#define LINE_TRACK_INTEGRAL_LIMIT       (0.8f)
#endif
#ifndef LINE_TRACK_ACCEL_LIMIT_MM_S2
#define LINE_TRACK_ACCEL_LIMIT_MM_S2    (1200.0f)
#endif
#ifndef LINE_TRACK_STEER_LIMIT_MM_S
#define LINE_TRACK_STEER_LIMIT_MM_S     (300.0f)
#endif

/*
 * 速度到电机业务命令的换算系数。
 * 默认 1.0：1 mm/s 等效为 1 个电机业务命令单位。
 * 如果更换驱动器或希望整体调速，只需修改这个宏，或运行时修改
 * line_track_params_t.command_scale，不需要改循迹状态机。
 */
#ifndef LINE_TRACK_COMMAND_SCALE
#define LINE_TRACK_COMMAND_SCALE        (1.0f)
#endif

/* 电机业务命令的默认最大绝对值；实际工程会优先使用 PRJ_MOTOR_COMMAND_MAX。 */
#ifndef LINE_TRACK_COMMAND_MAX
#define LINE_TRACK_COMMAND_MAX          (500)
#endif

/* 直角弯记忆周期数，取决于 app_line_track_update() 的调用频率。 */
#ifndef LINE_TRACK_TURN90_HOLD_CYCLES
#define LINE_TRACK_TURN90_HOLD_CYCLES   (9U)
#endif
#ifndef LINE_TRACK_TURN90_MAX_CYCLES
#define LINE_TRACK_TURN90_MAX_CYCLES    (200U)
#endif

/*
 * 一圈识别参数。标准赛道中心线周长约为：2×1.5m + 2×π×0.5m = 6.14m。
 * 启动后先确认离开起点横线，再累计里程；达到消隐里程后才允许识别终点横线，
 * 防止车辆刚启动时把脚下的 A 点横线误判为终点。
 */
#ifndef LINE_TRACK_LAP_ARM_DISTANCE_M
#define LINE_TRACK_LAP_ARM_DISTANCE_M        (4.50f)
#endif
#ifndef LINE_TRACK_START_LEAVE_CONFIRM_MS
#define LINE_TRACK_START_LEAVE_CONFIRM_MS    (50U)
#endif
#ifndef LINE_TRACK_FINISH_CONFIRM_MS
#define LINE_TRACK_FINISH_CONFIRM_MS         (20U)
#endif
#ifndef LINE_TRACK_LOST_STOP_MS
#define LINE_TRACK_LOST_STOP_MS              (300U)
#endif
#ifndef LINE_TRACK_MAX_RUN_MS
#define LINE_TRACK_MAX_RUN_MS                (35000U)
#endif
/*
 * 检测到终点横线后继续前进的补偿距离，单位 m。
 * 默认 0 表示立即刹车；实车根据“传感器到停车基准点距离－制动滑行距离”标定。
 */
#ifndef LINE_TRACK_FINISH_ADVANCE_M
#define LINE_TRACK_FINISH_ADVANCE_M          (0.0f)
#endif

/** 红外组合状态，bit3=最左，bit0=最右；0=黑线，1=白色。 */
typedef enum {
    LINE_TRACK_STATE_CROSS       = 0,
    LINE_TRACK_STATE_LEFT_90_A   = 1,
    LINE_TRACK_STATE_LEFT_90_B   = 3,
    LINE_TRACK_STATE_RIGHT_90_A  = 8,
    LINE_TRACK_STATE_RIGHT_90_B  = 12,
    LINE_TRACK_STATE_LEFT_BIG    = 7,
    LINE_TRACK_STATE_RIGHT_BIG   = 14,
    LINE_TRACK_STATE_LEFT_SMALL  = 11,
    LINE_TRACK_STATE_RIGHT_SMALL = 13,
    LINE_TRACK_STATE_STRAIGHT    = 9,
    LINE_TRACK_STATE_LOST        = 15,
} line_track_state_t;

/** 一次算法更新的结果。 */
typedef struct {
    float left_target_speed;    /* 左轮建议速度，单位 m/s。 */
    float right_target_speed;   /* 右轮建议速度，单位 m/s。 */
    float turn_diff;            /* 转向量：正值左转，负值右转。 */
    float base_speed_mm;        /* 基础速度，单位 mm/s。 */
    int32_t left_command;       /* 左轮统一电机命令，正值表示车体前进。 */
    int32_t right_command;      /* 右轮统一电机命令，正值表示车体前进。 */
    uint8_t sensor_bits;        /* 算法状态字，bit3=最左，bit0=最右。 */
    uint8_t ir_raw[BSP_IR_CHANNEL_COUNT]; /* 1=黑线，0=白色。 */
    int current_state;          /* 当前 line_track_state_t 状态。 */
    bool turn90_active;         /* 直角弯记忆是否有效。 */
    float line_error;           /* 归一化横向误差，左偏为正，范围约 -1..1。 */
    float steering_mm_s;        /* 左右轮差速量，单位 mm/s。 */
    uint32_t lost_ms;           /* 连续丢线时间，单位 ms。 */
} line_track_output_t;

/** 与硬件无关的循迹算法输入，便于主机单测和跨平台移植。 */
typedef struct {
    uint8_t ir_raw[BSP_IR_CHANNEL_COUNT]; /* 1=黑线，0=白色。 */
    float yaw_rate_dps;                  /* IMU 偏航角速度；无效时填 0。 */
    float dt_s;                          /* 调用周期，建议 0.005 s。 */
} line_track_input_t;

/** 初始化参数和内部状态；上电默认不运行、不输出电机命令。 */
void app_line_track_init(void);

/** 启动循迹；启动时会清除上一次方向和直角弯记忆。 */
void app_line_track_start(void);

/** 停止循迹算法；控制任务应同时确保电机安全停止。 */
void app_line_track_stop(void);

/** 查询循迹算法是否处于运行状态。 */
bool app_line_track_is_running(void);

/**
 * 执行一次循迹控制周期。
 *
 * 函数内部完成：读取红外 → 算法计算 → 调用现有电机接口输出。
 * 返回 true 表示本周期确实输出了循迹命令，false 表示当前未启动。
 */
bool app_line_track_step(void);

/** 读取红外并结合 IMU 偏航角速度更新物理速度目标，不直接访问电机。 */
bool app_line_track_step_with_feedback(float yaw_rate_dps, float dt_s);

/** 清除上一次方向和直角弯记忆。 */
void app_line_track_reset(void);

/** 执行一次读取和计算；out 不能为 NULL。该接口不直接驱动电机。 */
void app_line_track_update(line_track_output_t *out);

/** 纯算法入口：不访问 BSP，可在主机环境直接回归测试。 */
bool app_line_track_update_sample(const line_track_input_t *input,
                                  line_track_output_t *out);

/** 获取最近一次输出，返回静态对象，不需要释放。 */
const line_track_output_t *app_line_track_get_output(void);

/** 通过现有 BSP 接口停止四路电机。 */
void app_line_track_stop_motors(void);

/** 原子切换一组已验证参数：安全、精确、均衡、快速。 */
bool app_line_track_set_profile(app_line_profile_t profile);

/** 验证并整体替换配置，避免运行时部分参数更新造成瞬态冲突。 */
bool app_line_track_configure(const app_line_follower_cfg_t *cfg,
                              float command_scale);

/** 只读访问当前配置。 */
const app_line_follower_cfg_t *app_line_track_get_config(void);

/** 恢复头文件中的默认参数。 */
void app_line_track_restore_default_params(void);

/** 获取状态名称，返回静态字符串，不需要释放。 */
const char *app_line_track_state_name(int state);

#ifdef __cplusplus
}
#endif

#endif /* APP_LINE_TRACK_H */
