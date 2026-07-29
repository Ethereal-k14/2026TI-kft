/**
 * @file    app_scheduler.h
 * @brief   基于 TIM6 1 kHz 的任务调度器接口（规范 §3、§6）
 *
 *  TIM6 ISR 设置 sched_flags 位域（volatile），主循环轮询并清零后执行。
 *  各任务分频：
 *    1    kHz : BSP_Key_Process、App_Safety_Check（内环 STEP 更新由 TMC 硬件定时）
 *    500   Hz : App_Controller_InnerLoop
 *    100   Hz : App_Estimator_Update、App_Chassis_Process、App_Vision_Process
 *    50    Hz : App_Controller_OuterLoop
 *    20    Hz : BSP_OledSpi_Flush
 *    10    Hz : App_Chassis_SendHeartbeat
 */
#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 调度标志位定义
 * ---------------------------------------------------------------------- */
#define SCHED_FLAG_1KHZ    (1U << 0U)
#define SCHED_FLAG_500HZ   (1U << 1U)
#define SCHED_FLAG_100HZ   (1U << 2U)
#define SCHED_FLAG_50HZ    (1U << 3U)
#define SCHED_FLAG_20HZ    (1U << 4U)
#define SCHED_FLAG_10HZ    (1U << 5U)

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/** @brief 初始化调度器（启动 TIM6 中断） */
void App_Scheduler_Init(void);

/**
 * @brief TIM6 1 kHz 中断入口（在 TIM6_DAC_IRQHandler USER CODE 中调用）
 *        仅设置 sched_flags，不执行任务逻辑
 */
void App_Scheduler_Isr(void);

/**
 * @brief 主循环轮询调度（在 while(1) 中持续调用）
 *        检测 sched_flags，按优先级顺序分发任务
 */
void App_Scheduler_Run(void);

/** @brief 读取上一个 1 kHz 周期的最大执行时间（µs，用于验收） */
uint32_t App_Scheduler_GetMaxLoopUs(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_SCHEDULER_H */
