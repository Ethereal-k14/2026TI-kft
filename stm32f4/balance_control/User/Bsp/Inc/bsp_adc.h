/**
 * @file    bsp_adc.h
 * @brief   电位器 ADC1_IN10 (PC0) BSP 接口
 *          TIM3 2 kHz 触发，DMA2 Stream0 循环采集，32 点滑动均值
 *          输出 sensor_sample_t，value 单位：ADC 原始计数（0–4095）
 *          调用 BSP_Adc_SetCalibration() 后 value 转换为 mrad（毫弧度）
 */
#ifndef BSP_ADC_H
#define BSP_ADC_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 配置常量（调用 BSP_Adc_SetCalibration 修改）
 * ---------------------------------------------------------------------- */
#define BSP_ADC_BUF_LEN        32U   /* DMA 缓冲区长度（规范要求偶数且≥32） */
#define BSP_ADC_VREF_MV        3300U /* 参考电压 mV */
#define BSP_ADC_RESOLUTION     4095U /* 12 位 ADC */

/* -------------------------------------------------------------------------
 * 标定参数
 * ---------------------------------------------------------------------- */
typedef struct
{
    uint16_t raw_min;     /* 对应机械最小角度的 ADC 值 */
    uint16_t raw_max;     /* 对应机械最大角度的 ADC 值 */
    int32_t  angle_min_mrad; /* 最小角度（毫弧度） */
    int32_t  angle_max_mrad; /* 最大角度（毫弧度） */
} adc_cal_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 初始化 ADC BSP（启动 TIM3 触发 + DMA 循环采集）
 *        必须在 MX_ADC1_Init()、MX_TIM3_Init()、MX_DMA_Init() 之后调用
 */
void BSP_Adc_Init(void);

/**
 * @brief 设置标定参数（上电后调用，在零点校准流程完成后更新）
 */
void BSP_Adc_SetCalibration(const adc_cal_t *cal);

/**
 * @brief 在 DMA 半满/全满回调中调用（由 HAL_ADC_ConvHalfCpltCallback /
 *        HAL_ADC_ConvCpltCallback 转发过来）
 * @param half  true = 半满回调，false = 全满回调
 */
void BSP_Adc_DmaCallback(bool half);

/**
 * @brief 读取最新均值采样
 * @param out  输出结构体指针（非 NULL）
 *             valid=false 表示 DMA 尚未完成首次采集
 *             value: 无标定时为 ADC 计数；标定后为 mrad
 */
void BSP_Adc_GetSample(sensor_sample_t *out);

/** true only after a physical angle calibration has been installed. */
bool BSP_Adc_IsCalibrated(void);

#ifdef __cplusplus
}
#endif
#endif /* BSP_ADC_H */
