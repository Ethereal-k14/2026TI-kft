#include "bsp_lsm6dsr.h"
#include "lsm6dsr.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#define DEG2RAD (M_PI / 180.0f)
#define RAD2DEG (180.0f / M_PI)

/* ======================== LSM6DSR 灵敏度 ======================== */
#define ACC_SENSITIVITY_4G  (0.122f / 1000.0f)   // mg/LSB -> g/LSB
#define GYRO_SENSITIVITY_250DPS (8.75f / 1000.0f) // mdps/LSB -> dps/LSB

/* ======================== I2C 桥接函数 ======================== */
static int8_t lsm6dsr_i2c_read(void *ctx, uint8_t reg, uint8_t *buf, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)ctx;
    return (HAL_I2C_Mem_Read(hi2c, LSM6DSR_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100) == HAL_OK) ? 0 : -1;
}

static int8_t lsm6dsr_i2c_write(void *ctx, uint8_t reg, const uint8_t *buf, uint16_t len)
{
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)ctx;
    return (HAL_I2C_Mem_Write(hi2c, LSM6DSR_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, len, 100) == HAL_OK) ? 0 : -1;
}

/* ======================== DWT 微秒计时 ======================== */
static void dwt_init(void)
{
    static int dwt_done = 0;
    if (!dwt_done) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        dwt_done = 1;
    }
}

static uint32_t dwt_get_us(void)
{
    return DWT->CYCCNT / (HAL_RCC_GetHCLKFreq() / 1000000);
}

/* ======================== 读取传感器原始数据 ======================== */
static int read_sensor(bsp_lsm6dsr_ctx_t *ctx, int16_t *acc, int16_t *gyro, int16_t *temp)
{
    uint8_t buf[14];
    if (lsm6dsr_read_multi(&ctx->io, LSM6DSR_REG_OUT_TEMP_L, buf, 14) != LSM6DSR_OK)
        return -1;

    temp[0] = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    gyro[0] = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    gyro[1] = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);
    gyro[2] = (int16_t)((uint16_t)buf[7] << 8 | buf[6]);
    acc[0]  = (int16_t)((uint16_t)buf[9]  << 8 | buf[8]);
    acc[1]  = (int16_t)((uint16_t)buf[11] << 8 | buf[10]);
    acc[2]  = (int16_t)((uint16_t)buf[13] << 8 | buf[12]);
    return 0;
}

/* ======================== 互补滤波 ======================== */
static void complementary_filter(bsp_lsm6dsr_ctx_t *ctx,
                                  float ax, float ay, float az,
                                  float gx, float gy, float gz,
                                  float dt)
{
    if (dt <= 0.0f || dt > 1.0f) return;

    // 由加速度计计算 pitch/roll
    float acc_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD2DEG;
    float acc_roll  = atan2f(ay, az) * RAD2DEG;

    float alpha = 0.98f;  // 互补系数
    float gyro_pitch = ctx->pitch + gx * dt;
    float gyro_roll  = ctx->roll  + gy * dt;

    // 融合
    ctx->pitch = alpha * gyro_pitch + (1.0f - alpha) * acc_pitch;
    ctx->roll  = alpha * gyro_roll  + (1.0f - alpha) * acc_roll;
    ctx->yaw  += gz * dt;  // 偏航角仅由陀螺积分
    while (ctx->yaw > 180.0f)  ctx->yaw -= 360.0f;
    while (ctx->yaw < -180.0f) ctx->yaw += 360.0f;
}

/* ======================== API 实现 ======================== */

int bsp_lsm6dsr_init(bsp_lsm6dsr_ctx_t *ctx, I2C_HandleTypeDef *hi2c)
{
    if (!ctx || !hi2c) return -1;

    ctx->hi2c = hi2c;
    ctx->io.read = lsm6dsr_i2c_read;
    ctx->io.write = lsm6dsr_i2c_write;
    ctx->io.ctx = (void *)hi2c;

    // 验证 WHO_AM_I
    if (lsm6dsr_verify_id(&ctx->io) != LSM6DSR_OK) return -1;

    // 软复位
    lsm6dsr_reset(&ctx->io);
    HAL_Delay(10);

    // 配置：ACC 104Hz/4G, GYRO 104Hz/250dps, BDU+IF_INC
    uint8_t val;
    lsm6dsr_read_reg(&ctx->io, LSM6DSR_REG_CTRL3_C, &val);
    val |= (CTRL3_C_BDU | CTRL3_C_IF_INC);
    lsm6dsr_write_reg(&ctx->io, LSM6DSR_REG_CTRL3_C, val);

    lsm6dsr_write_reg(&ctx->io, LSM6DSR_REG_CTRL1_XL, 0x40); // ACC 104Hz, 4G
    lsm6dsr_write_reg(&ctx->io, LSM6DSR_REG_CTRL2_G,  0x40); // GYRO 104Hz, 250dps

    // 禁用 I3C
    lsm6dsr_read_reg(&ctx->io, LSM6DSR_REG_CTRL9_XL, &val);
    val |= CTRL9_XL_I3C_DISABLE;
    lsm6dsr_write_reg(&ctx->io, LSM6DSR_REG_CTRL9_XL, val);

    // 初始化 DWT
    dwt_init();
    ctx->last_tick_us = dwt_get_us();

    ctx->pitch = 0.0f; ctx->roll = 0.0f; ctx->yaw = 0.0f;
    ctx->bgx = 0.0f; ctx->bgy = 0.0f; ctx->bgz = 0.0f;
    ctx->cal_ok = 0;
    ctx->initialized = 1;

    return 0;
}

int bsp_lsm6dsr_update(bsp_lsm6dsr_ctx_t *ctx, bsp_lsm6dsr_data_t *data)
{
    if (!ctx || !ctx->initialized || !data) return -1;

    int16_t acc_raw[3], gyro_raw[3], temp_raw;
    if (read_sensor(ctx, acc_raw, gyro_raw, &temp_raw) != 0) return -1;

    // 计算时间间隔
    uint32_t now_us = dwt_get_us();
    float dt = (float)(now_us - ctx->last_tick_us) / 1000000.0f;
    if (dt <= 0.0f) dt = 0.01f;
    if (dt > 0.1f) dt = 0.01f; // 防止长时间停顿后跳跃
    ctx->last_tick_us = now_us;

    // 转换为物理值
    float ax = (float)acc_raw[0] * ACC_SENSITIVITY_4G;
    float ay = (float)acc_raw[1] * ACC_SENSITIVITY_4G;
    float az = (float)acc_raw[2] * ACC_SENSITIVITY_4G;
    float gx = (float)gyro_raw[0] * GYRO_SENSITIVITY_250DPS - ctx->bgx;
    float gy = (float)gyro_raw[1] * GYRO_SENSITIVITY_250DPS - ctx->bgy;
    float gz = (float)gyro_raw[2] * GYRO_SENSITIVITY_250DPS - ctx->bgz;
    float temp = (float)temp_raw / 256.0f + 25.0f;

    // 互补滤波姿态估计
    complementary_filter(ctx, ax, ay, az, gx, gy, gz, dt);

    // 填充输出
    data->ax = ax; data->ay = ay; data->az = az;
    data->gx = gx; data->gy = gy; data->gz = gz;
    data->pitch = ctx->pitch; data->roll = ctx->roll; data->yaw = ctx->yaw;
    data->temperature = temp;

    return 0;
}

int bsp_lsm6dsr_calibrate(bsp_lsm6dsr_ctx_t *ctx, int samples)
{
    if (!ctx || !ctx->initialized) return -1;

    double sum_gx = 0, sum_gy = 0, sum_gz = 0;
    int count = 0;

    for (int i = 0; i < samples; i++) {
        int16_t acc_raw[3], gyro_raw[3], temp_raw;
        if (read_sensor(ctx, acc_raw, gyro_raw, &temp_raw) == 0) {
            sum_gx += (double)gyro_raw[0];
            sum_gy += (double)gyro_raw[1];
            sum_gz += (double)gyro_raw[2];
            count++;
        }
        HAL_Delay(5); // ~200Hz 采样率
    }

    if (count == 0) return -1;

    ctx->bgx = (float)(sum_gx / count) * GYRO_SENSITIVITY_250DPS;
    ctx->bgy = (float)(sum_gy / count) * GYRO_SENSITIVITY_250DPS;
    ctx->bgz = (float)(sum_gz / count) * GYRO_SENSITIVITY_250DPS;
    ctx->cal_ok = 1;

    // 重置姿态角
    ctx->pitch = 0.0f; ctx->roll = 0.0f; ctx->yaw = 0.0f;
    return 0;
}
