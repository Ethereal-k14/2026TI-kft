# STM32F4 移植模块 - TB6612 + PID + LSM6DSR 陀螺仪

从 [Nanami7-7/AxiomTrace](https://github.com/Nanami7-7/AxiomTrace/tree/CXY) (CXY分支) 移植。
原平台：MSPM0G3507 (FreeRTOS) -> 目标：**STM32F4 (STM32 HAL)**。

## 目录结构

`
stm32f4/
├── Core/Inc/
│   ├── app_pid.h          # PID 控制器（纯算法，平台无关）
│   ├── lsm6dsr.h          # LSM6DSR 寄存器层（平台无关）
│   ├── bsp_tb6612.h       # TB6612 电机驱动（STM32 HAL）
│   ├── bsp_motor.h        # 电机统一接口
│   └── bsp_lsm6dsr.h      # IMU 业务层（STM32 HAL I2C）
└── Core/Src/
    ├── app_pid.c           # PID 实现
    ├── lsm6dsr.c           # LSM6DSR 寄存器级驱动
    ├── bsp_tb6612.c        # TB6612 驱动实现
    ├── bsp_motor.c         # 电机统一接口
    └── bsp_lsm6dsr.c       # IMU 姿态估计（互补滤波）
`

## 使用方法

将这些文件直接加入 STM32CubeIDE / Keil 工程即可。

### 1. PID 控制器
`c
#include "app_pid.h"

app_pid_t pid;
app_pid_init(&pid, 1.5f, 0.1f, 0.05f, APP_PID_MODE_POSITION, -1000, 1000);
app_pid_set_setpoint(&pid, 200.0f);  // 目标值

// 每个控制周期调用
float output = app_pid_compute(&pid, feedback_value, dt_s);
`

### 2. TB6612 电机驱动
`c
#include "bsp_tb6612.h"

bsp_tb6612_config_t motor_cfgs[BSP_TB6612_COUNT] = {
    { &htim1, TIM_CHANNEL_1, GPIOA, GPIO_PIN_0, GPIOA, GPIO_PIN_1, 1 },
    { &htim1, TIM_CHANNEL_2, GPIOA, GPIO_PIN_2, GPIOA, GPIO_PIN_3, 1 },
    { &htim1, TIM_CHANNEL_3, GPIOB, GPIO_PIN_0, GPIOB, GPIO_PIN_1, 1 },
    { &htim1, TIM_CHANNEL_4, GPIOB, GPIO_PIN_2, GPIOB, GPIO_PIN_3, 1 },
};

bsp_tb6612_init(motor_cfgs, 4, 1000, 1000);
bsp_tb6612_power_enable();
bsp_tb6612_set_speed(BSP_TB6612_A, 500);  // 50% 正转
`

### 3. LSM6DSR 陀螺仪/加速度计
`c
#include "bsp_lsm6dsr.h"

bsp_lsm6dsr_init(&hi2c1);

bsp_lsm6dsr_data_t imu_data;
while (1) {
    bsp_lsm6dsr_update(&imu_data);
    printf("pitch=%.1f roll=%.1f yaw=%.1f\\r\\n",
           imu_data.pitch, imu_data.roll, imu_data.yaw);
}
`

## 移植要点

- **PID**: 纯 C 算法，无硬件依赖，直接复制使用
- **TB6612**: 将原 MSPM0 HAL 替换为 STM32 HAL (HAL_GPIO_WritePin / __HAL_TIM_SET_COMPARE)
- **LSM6DSR**: 寄存器层 (lsm6dsr.c) 通过 lsm6dsr_io_t 回调分离平台代码；BSP 层使用 STM32 HAL I2C + DWT 计时
- **滤波器**: 互补滤波（轻量级，适合电赛）、Madgwick/Mahony（更精确的姿态融合）

## 注意事项

1. 需根据实际 PCB 接线修改 bsp_tb6612.c 中的 GPIO/PWM 配置
2. LSM6DSR I2C 地址默认为 0x6A（AD0=1），若 AD0=0 需改为 0x68
3. STM32F4 的 TIM 预分频和周期需根据系统时钟计算
4. 倾角计算仅含互补滤波（Madgwick/Mahony 见原项目 Filter/ 目录）
