/**
 * @file user_runtime.c
 * @brief 用户模块组合根；不修改 CubeMX 生成文件。
 */
#include "user_runtime.h"

#include "usart.h"
#include "bsp_adc.h"
#include "bsp_encoder.h"
#include "bsp_key.h"
#include "bsp_lidar.h"
#include "bsp_oled_spi.h"
#include "bsp_stepper.h"
#include "bsp_uart_dma.h"
#include "app_chassis.h"
#include "app_controller.h"
#include "app_debug.h"
#include "app_estimator.h"
#include "app_identification.h"
#include "app_protocol.h"
#include "app_safety.h"
#include "app_scheduler.h"
#include "app_vision.h"
#include "user_config.h"

static proto_port_ctx_t s_chassis_protocol;
static proto_port_ctx_t s_vision_protocol;
static uint8_t s_initialized;

void User_Runtime_Init(void)
{
    BSP_UartDma_Init(UART_PORT_LIDAR, &huart4);
    BSP_UartDma_Init(UART_PORT_CHASSIS, &huart2);
    BSP_UartDma_Init(UART_PORT_VISION, &huart3);
    BSP_UartDma_Init(UART_PORT_DEBUG, &huart1);

    BSP_Adc_Init();
    BSP_Encoder_Init();
    BSP_Stepper_Init();
    BSP_Lidar_Init();
    BSP_OledSpi_Init();
    BSP_Key_Init();

    App_Chassis_Init(&s_chassis_protocol);
    App_Vision_Init(&s_vision_protocol);
    App_Estimator_Init(NULL);
    App_Controller_Init(NULL);
    (void)App_Controller_SetProfile(
        (ball_ctrl_profile_t)USER_DEFAULT_BALL_PROFILE);
    App_Identification_Init();
    App_Safety_Init(NULL);
    App_Debug_Init();
    App_Scheduler_Init();

    s_initialized = 1U;
}

void User_Runtime_Run(void)
{
    if (s_initialized != 0U)
    {
        App_Scheduler_Run();
    }
}

uint8_t User_Runtime_IsInitialized(void)
{
    return s_initialized;
}
