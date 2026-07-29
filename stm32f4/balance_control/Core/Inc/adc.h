/* USER CODE BEGIN Header */
/**
  * @file    adc.h
  * @brief   ADC1 handle and initialization prototype.
  *
  * This file is the CubeMX-generated peripheral layer for PC0/ADC1_IN10.
  * User application code belongs under User/.
  */
/* USER CODE END Header */
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;

void MX_ADC1_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */
