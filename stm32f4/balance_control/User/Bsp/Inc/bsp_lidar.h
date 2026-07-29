/**
 * @file    bsp_lidar.h
 * @brief   YDLIDAR SDM18 单点激光测距 BSP 接口（规范 §2、§6）
 *
 *  接口：UART4 / PC10(TX) / PC11(RX)，921600 bps，3.3 V TTL
 *  协议：厂商私有帧，不套通用协议帧（规范 §4 注明）
 *
 *  SDM18 UART 帧格式（参考 YDLIDAR SDM18 开发手册 V0.1.0）：
 *  响应帧（主动上报）：
 *    [0] 0xAA  帧头1
 *    [1] 0xFF  帧头2
 *    [2] 距离高字节（mm，大端）
 *    [3] 距离低字节
 *    [4] 信号强度高字节
 *    [5] 信号强度低字节
 *    [6] 校验字节 = ([2]+[3]+[4]+[5]) & 0xFF
 *
 *  距离范围：100–10000 mm（0.1–10 m），超出范围为无效测量
 *  信号强度：0–65535，越大信号越好
 *
 *  命令帧（主控→SDM18）：
 *    启动测量：0xA5 0x00 0x00 0x01 0x00 0x00
 *    停止测量：0xA5 0x00 0x00 0x02 0x00 0x00
 */
#ifndef BSP_LIDAR_H
#define BSP_LIDAR_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 配置
 * ---------------------------------------------------------------------- */
#define BSP_LIDAR_DIST_MIN_MM    100U    /* 最小有效距离（mm） */
#define BSP_LIDAR_DIST_MAX_MM    10000U  /* 最大有效距离（mm） */
#define BSP_LIDAR_FRAME_LEN      7U      /* 响应帧字节数 */
#define BSP_LIDAR_TIMEOUT_MS     100U    /* 无数据超时（ms） */

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/**
 * @brief 初始化 SDM18 BSP（发送启动测量命令）
 *        必须在 BSP_UartDma_Init(UART_PORT_LIDAR) 之后调用
 */
void BSP_Lidar_Init(void);

/**
 * @brief 周期性处理（建议 ≥100 Hz 调用）
 *        消费 UART4 ring buffer，解析帧，更新内部 sample
 */
void BSP_Lidar_Process(void);

/**
 * @brief 停止激光（发送停止命令）
 */
void BSP_Lidar_Stop(void);

/**
 * @brief 读取最新距离采样
 * @param out  输出结构体指针
 *             value    = 距离（mm）
 *             quality  = 信号强度（映射到 0–255）
 *             valid    = false 表示超时或帧错误
 */
void BSP_Lidar_GetSample(sensor_sample_t *out);

#ifdef __cplusplus
}
#endif
#endif /* BSP_LIDAR_H */
