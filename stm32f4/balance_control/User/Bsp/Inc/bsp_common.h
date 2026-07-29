/**
 * @file    bsp_common.h
 * @brief   公共数据类型、错误码和宏定义（规范 §5）
 *          所有 BSP/App 模块共用；不依赖任何具体外设头文件。
 */
#ifndef BSP_COMMON_H
#define BSP_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "stm32f4xx_hal.h"
#include "user_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 错误码
 * ---------------------------------------------------------------------- */
typedef enum
{
    BSP_OK              = 0,
    BSP_ERR_TIMEOUT     = 1,
    BSP_ERR_RANGE       = 2,
    BSP_ERR_CRC         = 3,
    BSP_ERR_INVALID     = 4,
    BSP_ERR_BUSY        = 5,
    BSP_ERR_OVERFLOW    = 6,
} bsp_err_t;

/* -------------------------------------------------------------------------
 * 通用传感器采样（规范 §5）
 * value        : 原始计数或缩放后物理量（单位由各模块注释说明）
 * timestamp_us : 采样时刻，HAL_GetTick()*1000 + TIM 亚毫秒补偿，允许回绕
 * age_ms       : 自上次有效更新经过的毫秒数
 * quality      : 0=无效 / 1–255 = 信号质量（越大越好）
 * valid        : false = 数据陈旧或未初始化
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  value;
    uint32_t timestamp_us;
    uint16_t age_ms;
    uint8_t  quality;
    bool     valid;
} sensor_sample_t;

/* -------------------------------------------------------------------------
 * 磁编码器状态（规范 §5）
 * position_count   : TIM2 32 位计数值（相对于 Z 脉冲索引）
 * velocity_count_s : 每秒计数变化量（带符号，由差分计算）
 * index_valid      : Z 脉冲已捕获过，position_count 为绝对值
 * pwm_valid        : TIM5 PWM 捕获角度有效（备用通道）
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  position_count;
    int32_t  velocity_count_s;
    bool     index_valid;
    bool     pwm_valid;
    uint16_t pwm_duty_permille; /* PWM 角度输出的占空比（0–1000） */
} encoder_state_t;

/* -------------------------------------------------------------------------
 * 步进电机状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    uint32_t freq_hz;       /* 当前 STEP 脉冲频率 */
    bool     enabled;       /* ENN 已使能（低有效） */
    bool     dir_fwd;       /* 方向：true=正向 */
    bool     diag_fault;    /* TMC2209 DIAG 引脚触发过故障 */
} stepper_state_t;

/* -------------------------------------------------------------------------
 * 宏工具
 * ---------------------------------------------------------------------- */
#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0U]))
#define CLAMP(v, lo, hi)    (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))

/* 时间戳辅助：假设 SysTick 已配置为 1 ms 节拍 */
#define BSP_GetTimestampUs()   (HAL_GetTick() * 1000U)

#ifdef __cplusplus
}
#endif
#endif /* BSP_COMMON_H */
