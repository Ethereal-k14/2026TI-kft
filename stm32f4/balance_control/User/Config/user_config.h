/**
 * @file user_config.h
 * @brief 用户实现的集中配置（CubeMX 生成文件之外）。
 *
 * 只放编译期常量和功能开关；引脚、DMA、时钟仍以 .ioc/生成头文件为
 * 唯一事实来源。所有数值均带无符号后缀，便于 MISRA-C 类型检查。
 */
#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include <stdint.h>

#define USER_UART_RX_BUF_SIZE       (256U)
#define USER_UART_TX_BUF_SIZE       (320U) /* >= 270-byte max protocol frame */
#define USER_PROTO_MAX_PAYLOAD      (256U)
#define USER_CONTROL_TICK_HZ        (1000U)
#define USER_INNER_LOOP_HZ          (500U)
#define USER_OUTER_LOOP_HZ          (50U)
#define USER_VISION_TIMEOUT_MS      (200U)
#define USER_LIDAR_TIMEOUT_MS       (100U)
#define USER_CHASSIS_LINK_TIMEOUT_MS (250U)
#define USER_CHASSIS_START_ACK_TIMEOUT_MS (300U)

/* 0: 仅使用磁编码器/电位器；1: 允许融合激光位置。 */
#define USER_LIDAR_FUSION_ENABLE    (0U)

/* Start-key action: 0=dynamic lap/center hold, 1=+5 cm/-5 cm static test. */
#define USER_START_ACTION_DYNAMIC   (0U)
#define USER_START_ACTION_STATIC_STEP (1U)
#define USER_START_ACTION           USER_START_ACTION_DYNAMIC

/* ball_ctrl_profile_t: 0=safe, 1=precision, 2=balanced, 3=fast. */
#define USER_DEFAULT_BALL_PROFILE   (2U)

#endif /* USER_CONFIG_H */
