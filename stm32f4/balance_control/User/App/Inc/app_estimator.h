/**
 * @file    app_estimator.h
 * @brief   位置/速度状态估计器接口（规范 §6）
 *
 *  输入来源：视觉（USART3）+ SDM18 激光（UART4）+ 磁编码器（TIM2）
 *  策略：
 *  - 视觉与激光不直接平均（规范 §6 明确禁止）
 *  - 激光只有在安装几何能唯一映射为被控位置时才参与状态更新
 *  - 使用简化预测-更新结构（低复杂度一阶互补滤波 + 马氏距离门控）
 *  - 预测频率 100–200 Hz
 */
#ifndef APP_ESTIMATOR_H
#define APP_ESTIMATOR_H

#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * 估计器输出状态
 * ---------------------------------------------------------------------- */
typedef struct
{
    int32_t  pos_um;         /* 估计位置（µm） */
    int32_t  vel_um_s;       /* 估计速度（µm/s） */
    int32_t  angle_mrad;     /* 摆杆角度（mrad，来自磁编码器） */
    int32_t  ang_vel_mrad_s; /* 摆杆角速度（mrad/s） */
    uint32_t timestamp_us;
    bool     valid;
} estimator_state_t;

/* -------------------------------------------------------------------------
 * 估计器配置
 * ---------------------------------------------------------------------- */
typedef struct
{
    float    alpha_vision;    /* 视觉测量融合权重（0–1）*/
    float    alpha_lidar;     /* 激光融合权重（0=不参与） */
    int32_t  outlier_gate_um; /* 马氏距离门控阈值（µm），超出则拒绝 */
    bool     lidar_enabled;   /* 激光是否参与位置更新 */
    float    mrad_per_count;   /* 编码器计数到摆杆毫弧度的标定比例 */
} estimator_cfg_t;

/* -------------------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------------- */

/** @brief 初始化估计器 */
void App_Estimator_Init(const estimator_cfg_t *cfg);

/**
 * @brief 更新估计器（100 Hz 调用）
 *        - 从各 BSP/App 模块读取最新传感器数据
 *        - 执行预测步骤（基于上一步速度外推）
 *        - 若有新视觉/激光测量且可用，执行更新步骤
 */
void App_Estimator_Update(void);

/** @brief 读取当前估计状态 */
void App_Estimator_GetState(estimator_state_t *out);

/** @brief 重置估计器（急停或重新启动时调用） */
void App_Estimator_Reset(void);

#ifdef __cplusplus
}
#endif
#endif /* APP_ESTIMATOR_H */
