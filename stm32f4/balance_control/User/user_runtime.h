/**
 * @file user_runtime.h
 * @brief 单一用户业务入口，隔离 CubeMX 生成代码与 User 模块。
 *
 * Core 只需要在外设初始化完成后调用 User_Runtime_Init()，并在主循环
 * 调用 User_Runtime_Run()。所有业务实现均位于 User/，无动态内存。
 */
#ifndef USER_RUNTIME_H
#define USER_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化所有用户 BSP、协议、控制和调度模块。 */
void User_Runtime_Init(void);

/** @brief 非阻塞主循环入口；没有调度事件时立即返回。 */
void User_Runtime_Run(void);

/** @brief 查询运行时是否完成初始化。 */
uint8_t User_Runtime_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_RUNTIME_H */
