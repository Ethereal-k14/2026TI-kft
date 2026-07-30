#ifndef BSP_LSM6DSR_H
#define BSP_LSM6DSR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* IMU 姿态数据 */
typedef struct {
    float ax, ay, az;       // 加速度 (g)
    float gx, gy, gz;       // 角速度 (dps, 已补偿零偏)
    float pitch, roll, yaw; // 姿态角 (deg)
    float temperature;      // 温度 (degC)
} bsp_lsm6dsr_data_t;

/* IMU 上下文（可多实例） */
typedef struct {
    I2C_HandleTypeDef *hi2c;        // I2C 句柄
    lsm6dsr_io_t io;                // 寄存器层 I/O 回调
    float bgx, bgy, bgz;           // 陀螺零偏 (dps)
    int cal_ok;                     // 校准完成标志
    float pitch, roll, yaw;        // 姿态角 (deg)
    uint32_t last_tick_us;         // 上次时间戳 (us)
    int initialized;               // 初始化标志
} bsp_lsm6dsr_ctx_t;

/* API */
int  bsp_lsm6dsr_init(bsp_lsm6dsr_ctx_t *ctx, I2C_HandleTypeDef *hi2c);
int  bsp_lsm6dsr_update(bsp_lsm6dsr_ctx_t *ctx, bsp_lsm6dsr_data_t *data);
int  bsp_lsm6dsr_calibrate(bsp_lsm6dsr_ctx_t *ctx, int samples);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LSM6DSR_H */
