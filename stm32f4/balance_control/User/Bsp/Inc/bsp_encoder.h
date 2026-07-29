/**
 * @file    bsp_encoder.h
 * @brief   磁编码器 BSP 接口（规范 §2）
 *
 *  AB 相：TIM2 Encoder 模式，PA15/PB3，32 位正交计数器，数字滤波 6
 *  Z  相：EXTI4/PB4，上升沿建立机械零位
 *  PWM：  TIM5_CH1/PA0，输入捕获（备用角度通道）
 *
 *  速度计算：每次 BSP_Encoder_Process() 差分计算，单位 count/s；PWM
 *  备用通道同时输出占空比千分比，需经独立标定映射到角度。
 *  角度标定：提供每转计数（counts_per_rev）配置接口
 */
#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 配置
 * ---------------------------------------------------------------------- */
/** MS42CG GMR 编码器：线数 × 4（正交）= 有效计数/圈
 *  默认值 4000 = 1000 线 × 4；请根据实物手册修改 */
#define BSP_ENC_COUNTS_PER_REV     4000U

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 初始化编码器 BSP
 *        - 启动 CubeMX 配置的 TIM2 Encoder 模式
 *        - 启动 TIM5 CH1/CH2 PWM 输入捕获（备用角度）
 *        必须在 MX_TIM2_Init()/MX_TIM5_Init() 之后调用
 */
void BSP_Encoder_Init(void);

/**
 * @brief 周期性处理（建议 ≥500 Hz 调用，与内环同频）
 *        - 读取 TIM2 CNT，计算速度
 *        - 更新 PWM 捕获角度
 */
void BSP_Encoder_Process(void);

/**
 * @brief Z 脉冲 EXTI 中断入口（在 EXTI4_IRQHandler 中调用）
 *        记录当前计数值为零点，置 index_valid = true
 */
void BSP_Encoder_ZIndexIsr(void);

/**
 * @brief TIM5 捕获完成回调（在 HAL_TIM_IC_CaptureCallback 中调用）
 */
void BSP_Encoder_PwmCaptureCallback(void);

/**
 * @brief 读取最新编码器状态（只读，不修改内部状态）
 * @param out 非 NULL 输出指针
 */
void BSP_Encoder_GetState(encoder_state_t *out);

/**
 * @brief 读取编码器角度采样（同规范中的 sensor_sample_t 格式）
 *        value = position_count（相对零点计数）
 *        调用 BSP_Encoder_SetCountsPerRev 后可额外换算为 mrad
 */
void BSP_Encoder_GetAngleSample(sensor_sample_t *out);

/**
 * @brief 设置每转计数（标定后调用）
 */
void BSP_Encoder_SetCountsPerRev(uint32_t counts);

/**
 * @brief 手动复位零点（将当前位置设为 0）
 */
void BSP_Encoder_ResetIndex(void);

#ifdef __cplusplus
}
#endif
#endif /* BSP_ENCODER_H */
