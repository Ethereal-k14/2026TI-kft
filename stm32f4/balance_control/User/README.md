# User 用户实现目录

本目录是工程中所有非 CubeMX 生成代码的唯一实现来源。`Core/`、`.ioc` 和
`MDK-ARM/` 保持由 CubeMX 管理；根目录的 `Bsp/Inc`、`App/Inc` 只提供旧
include 路径兼容，不应再添加实现文件。目录和 Keil 工程可用
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
