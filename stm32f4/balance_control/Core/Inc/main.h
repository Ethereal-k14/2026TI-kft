/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TMC_MS1_Pin GPIO_PIN_2
#define TMC_MS1_GPIO_Port GPIOE
#define TMC_MS2_Pin GPIO_PIN_3
#define TMC_MS2_GPIO_Port GPIOE
#define TMC_SPREAD_Pin GPIO_PIN_4
#define TMC_SPREAD_GPIO_Port GPIOE
#define LIMIT_MIN_Pin GPIO_PIN_5
#define LIMIT_MIN_GPIO_Port GPIOE
#define LIMIT_MAX_Pin GPIO_PIN_6
#define LIMIT_MAX_GPIO_Port GPIOE
#define MAG_PWM_Pin GPIO_PIN_0
#define MAG_PWM_GPIO_Port GPIOA
#define OLED_SPI_SCK_Pin GPIO_PIN_5
#define OLED_SPI_SCK_GPIO_Port GPIOA
#define OLED_SPI_MISO_UNUSED_Pin GPIO_PIN_6
#define OLED_SPI_MISO_UNUSED_GPIO_Port GPIOA
#define OLED_SPI_MOSI_Pin GPIO_PIN_7
#define OLED_SPI_MOSI_GPIO_Port GPIOA
#define OLED_SPI_CS_Pin GPIO_PIN_7
#define OLED_SPI_CS_GPIO_Port GPIOE
#define OLED_DC_Pin GPIO_PIN_8
#define OLED_DC_GPIO_Port GPIOE
#define OLED_RST_Pin GPIO_PIN_9
#define OLED_RST_GPIO_Port GPIOE
#define VISION_TX_Pin GPIO_PIN_8
#define VISION_TX_GPIO_Port GPIOD
#define VISION_RX_Pin GPIO_PIN_9
#define VISION_RX_GPIO_Port GPIOD
#define TMC_STEP_Pin GPIO_PIN_6
#define TMC_STEP_GPIO_Port GPIOC
#define TMC_DIR_Pin GPIO_PIN_7
#define TMC_DIR_GPIO_Port GPIOC
#define TMC_ENN_Pin GPIO_PIN_8
#define TMC_ENN_GPIO_Port GPIOC
#define TMC_DIAG_Pin GPIO_PIN_9
#define TMC_DIAG_GPIO_Port GPIOC
#define DEBUG_TX_Pin GPIO_PIN_9
#define DEBUG_TX_GPIO_Port GPIOA
#define DEBUG_RX_Pin GPIO_PIN_10
#define DEBUG_RX_GPIO_Port GPIOA
#define LIDAR_TX_Pin GPIO_PIN_10
#define LIDAR_TX_GPIO_Port GPIOC
#define LIDAR_RX_Pin GPIO_PIN_11
#define LIDAR_RX_GPIO_Port GPIOC
#define CHASSIS_TX_Pin GPIO_PIN_5
#define CHASSIS_TX_GPIO_Port GPIOD
#define CHASSIS_RX_Pin GPIO_PIN_6
#define CHASSIS_RX_GPIO_Port GPIOD
#define MAG_ENC_A_Pin GPIO_PIN_15
#define MAG_ENC_A_GPIO_Port GPIOA
#define MAG_ENC_B_Pin GPIO_PIN_3
#define MAG_ENC_B_GPIO_Port GPIOB
#define MAG_ENC_Z_Pin GPIO_PIN_4
#define MAG_ENC_Z_GPIO_Port GPIOB
#define OLED_I2C_SCL_Pin GPIO_PIN_8
#define OLED_I2C_SCL_GPIO_Port GPIOB
#define OLED_I2C_SDA_Pin GPIO_PIN_9
#define OLED_I2C_SDA_GPIO_Port GPIOB
#define START_KEY_Pin GPIO_PIN_0
#define START_KEY_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
