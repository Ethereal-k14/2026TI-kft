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
