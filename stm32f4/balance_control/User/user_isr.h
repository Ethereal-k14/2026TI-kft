/**
 * @file user_isr.h
 * @brief CubeMX ISR 到用户模块的窄适配接口。
 *
 * 这些函数只做事件转发，不执行协议解析、滤波或控制计算。生成的
 * stm32f4xx_it.c 可在 USER CODE 区域调用它们，避免直接依赖业务头文件。
 */
#ifndef USER_ISR_H
#define USER_ISR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void User_Isr_OnSchedulerTick(void);
#define USER_ISR_PORT_LIDAR   (0U)
#define USER_ISR_PORT_CHASSIS (1U)
#define USER_ISR_PORT_VISION  (2U)
#define USER_ISR_PORT_DEBUG   (3U)

void User_Isr_OnUartIdle(uint8_t port);
void User_Isr_OnUartTxComplete(uint8_t port);
void User_Isr_OnStartKey(void);
void User_Isr_OnLimitMin(void);
void User_Isr_OnLimitMax(void);
void User_Isr_OnStepperDiag(void);
void User_Isr_OnEncoderIndex(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_ISR_H */
