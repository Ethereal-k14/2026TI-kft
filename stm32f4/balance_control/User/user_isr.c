/**
 * @file user_isr.c
 * @brief 低开销 ISR 事件转发实现。
 */
#include "user_isr.h"

#include "app_scheduler.h"
#include "bsp_encoder.h"
#include "bsp_key.h"
#include "bsp_stepper.h"
#include "bsp_uart_dma.h"
#include "main.h"
#include "tim.h"

void User_Isr_OnSchedulerTick(void)
{
    App_Scheduler_Isr();
}

void User_Isr_OnUartIdle(uint8_t port)
{
    if (port < (uint8_t)UART_PORT_COUNT)
    {
        BSP_UartDma_IdleIsr((uart_port_t)port);
    }
}

void User_Isr_OnUartTxComplete(uint8_t port)
{
    if (port < (uint8_t)UART_PORT_COUNT)
    {
        BSP_UartDma_TxCpltCallback((uart_port_t)port);
    }
}

void User_Isr_OnStartKey(void)
{
    BSP_Key_StartKeyIsr();
}

void User_Isr_OnLimitMin(void)
{
    BSP_Key_LimitMinIsr();
}

void User_Isr_OnLimitMax(void)
{
    BSP_Key_LimitMaxIsr();
}

void User_Isr_OnStepperDiag(void)
{
    BSP_Stepper_DiagIsr();
}

void User_Isr_OnEncoderIndex(void)
{
    BSP_Encoder_ZIndexIsr();
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if ((htim != NULL) && (htim->Instance == TIM6))
    {
        User_Isr_OnSchedulerTick();
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
    switch (gpio_pin)
    {
    case START_KEY_Pin:
        User_Isr_OnStartKey();
        break;
    case LIMIT_MIN_Pin:
        User_Isr_OnLimitMin();
        break;
    case LIMIT_MAX_Pin:
        User_Isr_OnLimitMax();
        break;
    case TMC_DIAG_Pin:
        User_Isr_OnStepperDiag();
        break;
    case MAG_ENC_Z_Pin:
        User_Isr_OnEncoderIndex();
        break;
    default:
        break;
    }
}
