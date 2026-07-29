/**
 * @file    bsp_adc.c
 * @brief   电位器 ADC1_IN10 (PC0) 实现
 *          TIM3 TRGO 2 kHz 触发，DMA2 Stream0 循环采集 32 点缓冲
 *          半满/全满回调计算均值；标定后将计数转换为 mrad
 */
#include "bsp_adc.h"
#include "adc.h"    /* CubeMX 生成：hadc1, MX_ADC1_Init */
#include "tim.h"    /* CubeMX 生成：htim3,  MX_TIM3_Init */
#include "main.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * 私有变量
 * ---------------------------------------------------------------------- */
static uint16_t  s_dma_buf[BSP_ADC_BUF_LEN];  /* DMA 目标缓冲区（half-word） */
static sensor_sample_t s_sample;               /* 最新采样结果 */
static adc_cal_t       s_cal;                  /* 标定参数 */
static bool            s_cal_valid = false;    /* 标定是否已设置 */
static volatile bool   s_half_ready = false;   /* 半满标志 */
static volatile bool   s_full_ready = false;   /* 全满标志 */

/* -------------------------------------------------------------------------
 * 私有函数
 * ---------------------------------------------------------------------- */

/**
 * @brief 对 buf 中 len 个 uint16_t 求均值
 */
static uint32_t adc_average(const uint16_t *buf, uint32_t len)
{
    uint32_t sum = 0U;
    uint32_t i;
    for (i = 0U; i < len; i++)
    {
        sum += (uint32_t)buf[i];
    }
    return sum / len;
}

/**
 * @brief 将 ADC 原始值映射到 mrad（线性插值）
 */
static int32_t adc_to_mrad(uint32_t raw)
{
    int32_t  span_raw   = (int32_t)s_cal.raw_max - (int32_t)s_cal.raw_min;
    int32_t  span_angle = s_cal.angle_max_mrad    - s_cal.angle_min_mrad;
    int32_t  delta      = (int32_t)raw - (int32_t)s_cal.raw_min;

    if (span_raw == 0)
    {
        return s_cal.angle_min_mrad;
    }
    return s_cal.angle_min_mrad + (delta * span_angle) / span_raw;
}

/**
 * @brief 处理一段缓冲区（half 或 full）
 */
static void adc_process_segment(const uint16_t *buf, uint32_t len)
{
    uint32_t avg = adc_average(buf, len);

    s_sample.timestamp_us = BSP_GetTimestampUs();
    s_sample.age_ms       = 0U;
    s_sample.quality      = 255U;
    s_sample.valid        = true;

    if (s_cal_valid)
    {
        s_sample.value = adc_to_mrad(avg);
    }
    else
    {
        s_sample.value = (int32_t)avg;
    }
}

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void BSP_Adc_Init(void)
{
    (void)memset(&s_sample, 0, sizeof(s_sample));
    (void)memset(&s_cal,    0, sizeof(s_cal));
    s_cal_valid   = false;
    s_half_ready  = false;
    s_full_ready  = false;

    /* 启动 DMA 循环采集（CubeMX 已配置 TIM3 TRGO → ADC 触发） */
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_dma_buf, BSP_ADC_BUF_LEN) != HAL_OK)
    {
        /* 初始化失败：进入 Error_Handler 或仅记录，保持 valid=false */
        Error_Handler();
    }

    /* 启动 TIM3 触发源（2 kHz） */
    if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
    {
        Error_Handler();
    }
}

void BSP_Adc_SetCalibration(const adc_cal_t *cal)
{
    if (cal != NULL)
    {
        s_cal       = *cal;
        s_cal_valid = true;
    }
}

/**
 * @brief 在 HAL ADC DMA 半满/全满回调中由 Core 层转发调用
 *        中断上下文：只设置标志，不执行浮点运算
 */
void BSP_Adc_DmaCallback(bool half)
{
    if (half)
    {
        s_half_ready = true;
    }
    else
    {
        s_full_ready = true;
    }
}

void BSP_Adc_GetSample(sensor_sample_t *out)
{
    /* 在主循环（调度器）中调用，消费就绪标志 */
    if (s_half_ready)
    {
        s_half_ready = false;
        /* 前半段有效 */
        adc_process_segment(&s_dma_buf[0], BSP_ADC_BUF_LEN / 2U);
    }
    else if (s_full_ready)
    {
        s_full_ready = false;
        /* 后半段有效 */
        adc_process_segment(&s_dma_buf[BSP_ADC_BUF_LEN / 2U], BSP_ADC_BUF_LEN / 2U);
    }
    else
    {
        /* 无新数据：更新 age_ms */
        if (s_sample.valid)
        {
            uint32_t now_ms = HAL_GetTick();
            uint32_t ts_ms  = s_sample.timestamp_us / 1000U;
            s_sample.age_ms = (uint16_t)((now_ms >= ts_ms) ?
                              (now_ms - ts_ms) : (0xFFFFU - ts_ms + now_ms));
        }
    }

    if (out != NULL)
    {
        *out = s_sample;
    }
}

/* -------------------------------------------------------------------------
 * HAL 回调转发（放在本文件，由 Core/stm32f4xx_it.c USER CODE 段调用）
 * 若 CubeMX 在其他地方弱定义了这些回调，请改为在 Core 层调用 BSP_Adc_DmaCallback
 * ---------------------------------------------------------------------- */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        BSP_Adc_DmaCallback(true);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        BSP_Adc_DmaCallback(false);
    }
}
