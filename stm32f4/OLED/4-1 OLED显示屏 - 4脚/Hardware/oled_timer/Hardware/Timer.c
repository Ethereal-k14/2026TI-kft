#include "stm32f10x.h"
#include "Timer.h"

volatile uint32_t sys_tick_ms = 0;

void Timer_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseInitTypeDef tim;
    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = 72 - 1;
    tim.TIM_Period    = 1000 - 1;
    TIM_TimeBaseInit(TIM2, &tim);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM_Cmd(TIM2, ENABLE);
}

uint32_t Timer_GetTick(void)
{
    uint32_t tick;
    __disable_irq();
    tick = sys_tick_ms;
    __enable_irq();
    return tick;
}
