#ifndef __TIMER_H
#define __TIMER_H
#include <stdint.h>
extern volatile uint32_t sys_tick_ms;
void Timer_Init(void);
uint32_t Timer_GetTick(void);
#endif