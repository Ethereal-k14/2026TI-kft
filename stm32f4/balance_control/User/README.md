# User 用户实现目录

本目录是工程中所有非 CubeMX 生成代码的唯一实现与头文件来源。`Core/`、`.ioc` 和
`MDK-ARM/` 保持由 CubeMX 管理。根目录已精简并移除了旧 `App/` 和 `Bsp/` 转接目录，
所有模块头文件与源文件统一收口于本 `User/` 目录中。目录与 Keil 工程可用
`Tools/check_user_layout.ps1` 做一致性检查。

## 目录约定

| 目录 | 内容 |
|---|---|
| `Bsp/Inc`, `Bsp/Src` | ADC 电位器、ABZ/PWM 编码器、TMC2209、SDM18、OLED（SPI/I²C）、按键、UART DMA |
| `App/Inc`, `App/Src` | 下位机/视觉应用、估计器、控制器、安全状态机、调度器、调试显示 |
| `Protocol/Inc`, `Protocol/Src` | `app_protocol` 通用帧、CRC 和有界解析器 |
| `Config` | 用户侧编译期常量和功能开关 |
| `user_runtime.*` | 模块初始化和主循环组合根 |
| `user_isr.*` | CubeMX ISR 的窄事件转发接口 |

## 集成步骤

1. CubeMX 生成并初始化所有外设后，调用 `User_Runtime_Init()` 一次。
2. 主循环只调用 `User_Runtime_Run()`，不直接依赖业务模块。
3. 在生成 ISR 的 USER CODE 区域转发 TIM6、UART IDLE/TX、TMC DIAG、ABZ Z
   索引事件到 `User_Isr_*`；ISR 不解析协议、不执行浮点控制。
4. 将本目录的五个 include 路径（`User`、`Config`、`Bsp/Inc`、`App/Inc`、
   `Protocol/Inc`）、三个 `Src` 路径以及根目录的 `user_runtime.c`、
   `user_isr.c` 加入构建系统。旧 `Bsp/` 和 `App/` 源目录不应加入构建。

SPI 与 I²C OLED 后端均已提供，但运行时只选择一个；当前 `User_Runtime_Init()`
使用 SPI 后端，切换 I²C 时应在调度器中替换对应的 `BSP_Oled*` 调用，避免两路
同时驱动同一块屏幕。

## 静态审查结论

- 所有用户缓冲区为静态存储；未引入堆、递归或无界循环。
- DMA 回调仅推进索引/置位标志；业务在调度器中有界运行。
- 配置与引脚职责分离：`user_config.h` 不复制 `.ioc` 引脚或 DMA 定义。
- ADC1 的 `Core/Inc/adc.h`/`Core/Src/adc.c` 已补齐为 CubeMX 外设层，且
  `main.c` 在 `MX_DMA_Init()` 后调用 `MX_ADC1_Init()`；重新用 CubeMX 生成时
  允许这两个文件被同名生成文件覆盖，`User/Bsp` 不持有 HAL 句柄。
- `bsp_uart_dma.c` 与 `bsp_oled_spi.c` 提供 HAL 完成回调；若后续将回调统一
  收口到 `user_isr.c`，必须删除旧回调定义或只保留一处，避免链接重复。

## MISRA C 基线

采用 `<stdint.h>` 固定宽度类型、显式转换、枚举状态、静态内存、有界循环和
返回值检查。对 HAL 宏和寄存器的必要例外应在静态分析配置中集中豁免，不能
通过关闭全部诊断规避问题。

## 外设配置与 STM32F407VET6 移植指南

### 1. 时钟系统配置 (Clock Tree)
* **HSE**: 8 MHz 无源晶振 (Crystal/Ceramic Resonator)
* **SYSCLK**: 168 MHz (HSE 8 MHz ÷ 4 × 168 ÷ 2 = 168 MHz)
* **AHB**: 168 MHz | **APB1**: 42 MHz (Timer 84 MHz) | **APB2**: 84 MHz (Timer 168 MHz)
* **PLLQ**: 48 MHz (USB/RNG/SDIO 专用)

### 2. 外设与引脚完整配置矩阵 (Pin & Function Matrix)

| 模块名称 | 功能 / 信号 | MCU 引脚 | 模式 / 参数配置 | DMA / 优先级 | 备注 |
|---|---|---|---|---|---|
| **磁编码器** | AB 正交信号 | PA15 (CH1) / PB3 (CH2) | TIM2 Encoder Mode (TI1/TI2), Counter=0x3FFFFFFF | - | 32 位计数器，滤波 Filter=6 |
| | Z 信号 (零位) | PB4 | EXTI4 (上升沿触发, 内部上拉) | NVIC Priority 2 | 零位建立与校验 |
| | PWM 角度 (备用) | PA0 | TIM5 PWM Input Mode (CH1 周期 / CH2 高电平) | - | 备用绝对角度通道 |
| **角度电位器**| 模拟输入 | PC0 | ADC1_IN10 (12-bit, Sampling=15 Cycles) | DMA2 Stream0 (Circular, 16-bit) | TIM3 TRGO (2 kHz) 触发采样 |
| **TMC2209** | STEP 脉冲输出 | PC6 | TIM8_CH1 PWM/Output (Prescaler=167, 1MHz基准) | NVIC Priority 2 | 初始 1 kHz、2 µs 脉冲宽度 |
| | DIR (方向) | PC7 | GPIO Output Push-Pull | - | 电机运动方向控制 |
| | ENN (使能) | PC8 | GPIO Output Push-Pull | - | **低电平有效**，默认高禁用 |
| | DIAG (堵转/故障) | PC9 | EXTI9 (上升沿触发, 上拉) | NVIC Priority 3 | 辅助检测堵转 |
| | MS1 / MS2 | PE2 / PE3 | GPIO Output Push-Pull | - | 默认全高 (16 微步) |
| | SPREAD | PE4 | GPIO Output Push-Pull | - | 默认高 (SpreadCycle 模式) |
| **激光测距** | SDM18 串口 | PC10 (TX) / PC11 (RX) | UART4 (921600 8N1) | DMA1 Stream2 (RX Circular) / Stream4 (TX Normal) | UART IDLE + DMA 环形取数 |
| **下位机 IMU**| 姿态/控制通信 | PD5 (TX) / PD6 (RX) | USART2 (115200 8N1) | DMA1 Stream5 (RX Circular) / Stream6 (TX Normal) | MSPM0 专用二进制链路 |
| **视觉模块** | 位置/速度输入 | PD8 (TX) / PD9 (RX) | USART3 (921600 8N1) | DMA1 Stream1 (RX Circular) / Stream3 (TX Normal) | 接收视觉位姿数据 |
| **调试输出** | 串口 Log/VOFA | PA9 (TX) / PA10 (RX) | USART1 (115200 8N1) | DMA2 Stream2 (RX Circular) / Stream7 (TX Normal) | 调试输出，不入控制环 |
| **OLED 显示** | SPI 模式 (主选) | PA5 (SCK), PA7 (MOSI)<br>PE7 (CS), PE8 (DC), PE9 (RST) | SPI1 TX-Only (5.25 Mbit/s, CPOL=High, CPHA=2Edge) | DMA2 Stream3 (TX Normal) | PA6 (MISO) 悬空不连 |
| | I²C 模式 (备用) | PB8 (SCL), PB9 (SDA) | I2C1 Fast Mode (400 kHz) | - | 外部 4.7 kΩ 上拉到 3.3V |
| **系统按键** | 启动按键 | PE0 | EXTI0 (下降沿触发, 上拉) | NVIC Priority 5 | 20 ms 软件防抖 |
| **限位开关** | MIN / MAX 限位 | PE5 / PE6 | EXTI5 / EXTI6 (双边沿触发, 上拉) | NVIC Priority 3 | 常闭到地：低=正常，高=触发/断线 |
| **控制定时器**| 系统 1kHz Tick | - | TIM6 (Prescaler=83, Period=999, Interrupt) | NVIC Priority 1 | 系统最高优先级控制时钟 |

### 3. STM32F407VET6 (LQFP100) 芯片适配说明

1. **`.ioc` 文件已直接切至 STM32F407VET6**：
   仓库根目录下的 [balance_control.ioc](file:///d:/Destop/test/26%E7%94%B5%E8%B5%9B/2026TI-kft/stm32f4/balance_control/balance_control.ioc) 已结合 ST 官方数据手册完成了修改，将目标芯片调整为 **STM32F407VET6 (LQFP100)**，可以直接使用 STM32CubeMX 打开并重新生成代码。
2. **引脚 100% 硬件兼容**：
   项目用到的所有 GPIO 引脚（`PA0~PA15`、`PB3~PB9`、`PC0`、`PC6~PC11`、`PD5~PD9`、`PE0~PE9`、`PH0~PH1`）在 **STM32F407VET6 (LQFP100)** 封装中全部存在且位置功能完全一致，**无需更改任何引脚分配**。
3. **Flash / RAM 容量**：
   STM32F407VET6 具备 **512 KB Flash + 192 KB SRAM**；每次正式构建应以链接器 map
   文件记录实际占用，不能用语法检查结果推断 Flash/RAM 余量。
4. **内核与时钟完全一致**：
   同属 STM32F407 系列，Cortex-M4F 主频 168 MHz (HSE 8MHz)，所有 TIM / ADC / UART 分频参数保持原样。

### 4. 打开与生成步骤

1. 直接在 **STM32CubeMX** 中打开工程根目录下的 [balance_control.ioc](file:///d:/Destop/test/26%E7%94%B5%E8%B5%9B/2026TI-kft/stm32f4/balance_control/balance_control.ioc)。
2. 确认 MCU 已自动识别为 `STM32F407VET6` (LQFP100 封装)，外设、时钟树、DMA、NVIC 参数无缝对应。
3. 点击 **Generate Code** 重新生成工程即可！
4. 在 Keil MDK-ARM 中确认 `Include Paths` 与 `User/` 源文件关联，运行 `Tools/check_user_layout.ps1` 校验全通过后直接编译烧录！
