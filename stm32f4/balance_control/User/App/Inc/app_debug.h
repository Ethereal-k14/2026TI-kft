/**
 * @file    app_debug.h
 * @brief   调试日志和 OLED 显示接口（规范 §3、§7）
 *
 *  USART1（115200 bps），非阻塞 DMA TX，不参与实时控制环
 *  OLED 显示：安全状态、估计位置、角度、步频（20 Hz 刷新）
 */
#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化调试模块（USART1 DMA TX） */
void App_Debug_Init(void);

/**
 * @brief 发送格式化日志（非阻塞，放入 TX 队列）
 *        格式：[ms] MODULE: message\r\n
 *        禁止在控制周期（TIM6 ISR 上下文）调用
 */
void App_Debug_Log(const char *module, const char *msg);

/**
 * @brief 更新 OLED 帧缓冲内容（20 Hz 调用，由调度器触发）
 *        Flush 由调度器在此后调用
 */
void App_Debug_UpdateDisplay(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_DEBUG_H */
