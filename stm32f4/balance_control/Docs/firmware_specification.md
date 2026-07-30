# 平衡控制器固件与接口规范

## 1. 适用范围

当前权威目标为 `STM32F407VET6` (LQFP100) 和 8 MHz 无源外部晶振；
`balance_control.ioc` 与 Keil 工程均以该器件为准。`STM32F407ZGT6` (LQFP144)
仅作为同系列扩展封装的可移植参考，切换器件必须重新检查封装引脚、电源脚、链接脚本
和工程 Device，不能把“同系列”视为已经验证的比赛目标。固件采用 STM32 HAL、无
RTOS、MISRA C:2012 风格；中断只搬运数据和更新时间戳，不在中断中解析协议或执行
浮点控制。

时钟使用 PH0/PH1 的 HSE Crystal/Ceramic Resonator 模式：8 MHz ÷ PLLM 4 × PLLN 168 ÷ PLLP 2 = 168 MHz。AHB=168 MHz，APB1=42 MHz（定时器84 MHz），APB2=84 MHz（定时器168 MHz），PLLQ=7得到48 MHz。晶振必须按所选器件的负载电容和PCB寄生重新计算两侧电容，不能把有源时钟的 Bypass 模式用于无源晶振。

## 2. 外设与接线

| 模块 | MCU 外设 | 引脚 | 说明 |
|---|---|---|---|
| 5 kΩ 角度电位器 | ADC1_IN10 | PC0 | 两端接 3.3 V/GND，滑臂接 PC0；滑臂增加 1 kΩ 串联和 100 nF 对地滤波 |
| TMC2209 STEP | TIM8_CH1 | PC6 | 1 MHz 计数基准，初始 1 kHz、2 µs 高脉冲，运行时更新 ARR/CCR |
| TMC2209 DIR/ENN | GPIO | PC7/PC8 | ENN 低有效；上电默认高电平禁用驱动 |
| TMC2209 DIAG | EXTI9 | PC9 | 上拉输入，仅作故障/堵转辅助，不能替代磁编码器闭环 |
| TMC2209 MS1/MS2 | GPIO | PE2/PE3 | 默认均为高，独立模式选择 16 微步；仅在驱动禁用时改变 |
| TMC2209 SPREAD | GPIO | PE4 | 默认高，选择 SpreadCycle；静音优先时可在停机状态切到低电平 |
| 磁编码器 AB | TIM2 Encoder | PA15/PB3 | 32 位正交计数器，数字滤波值 6 |
| 磁编码器 Z | EXTI4 | PB4 | 上升沿建立机械零位；必须校验 Z 与绝对机械位置 |
| 磁编码器 PWM | TIM5 PWM Input (CH1/CH2) | PA0 | CH1 捕获周期、CH2 间接捕获高电平；备用角度通道 |
| YDLIDAR SDM18 | UART4 | PC10/PC11 | 921600 bit/s，3.3 V TTL，RX/TX 均使用 DMA |
| 下位机 | USART2 | PD5/PD6 | 115200 bit/s，与 MSPM0 UART1 同速，接收 IMU 并发送启停命令 |
| 视觉模块 | USART3 | PD8/PD9 | 921600 bit/s，接收目标位置、速度、置信度和采集时间 |
| 调试参数口 | USART1 | PA9/PA10 | 115200 bit/s，不参与实时闭环 |
| I²C OLED | I2C1 | PB8/PB9 | 400 kHz，外部 4.7 kΩ 上拉到 3.3 V |
| SPI OLED | SPI1 | PA5/PA7 | SCK/MOSI；PE7=CS、PE8=DC、PE9=RST，SPI 5.25 Mbit/s |
| 启动按键 | EXTI0 | PE0 | 低有效，上拉；软件去抖后向下位机发送启动事件 |
| 两端限位 | EXTI5/6 | PE5/PE6 | 常闭到地：低电平正常，高电平表示触发或断线 |
| 8 MHz 无源晶振 | HSE | PH0/PH1 | Crystal/Ceramic Resonator；短走线、对称负载电容、远离STEP和电机功率回路 |

I²C 和 SPI OLED 驱动可以同时编译，但只实例化一个显示后端；当前运行时选择
SPI，I²C 后端位于 `User/Bsp`，切换时替换调度器显示调用。I²C 全屏刷新为
低速阻塞操作，只能放在低频诊断任务，不能放入 1 kHz 控制路径。SPI 的 PA6/MISO
被 CubeMX 保留但 OLED 不连接该脚。

EXTI 边沿固定为：START_KEY下降沿，磁编Z和TMC DIAG上升沿，LIMIT_MIN/MAX双边沿。限位ISR只记录事件，安全任务必须再次读取引脚稳定电平；这样既能识别触发，也能识别常闭线路断线和恢复。

SDM18 裸模块使用 3.3 V，并将 UART_INT 拉低选择 UART 模式；激光电源脚使用独立 LC/π 型滤波和就近退耦。带转接板版本的供电与逻辑电平必须以实物手册为准，不得把 5 V UART 直接接入 MCU。

## 3. DMA 与实时调度

| 数据流 | DMA | 模式 | 优先级 |
|---|---|---|---|
| 电位器 ADC | DMA2 Stream0 | Circular, half-word | High |
| OLED SPI TX | DMA2 Stream3 | Normal | Low |
| 调试 USART1 RX/TX | DMA2 Stream2/7 | Circular/Normal | Low |
| 下位机 USART2 RX/TX | DMA1 Stream5/6 | Circular/Normal | High/Medium |
| 视觉 USART3 RX/TX | DMA1 Stream1/3 | Circular/Normal | High/Medium |
| SDM18 UART4 RX/TX | DMA1 Stream2/4 | Circular/Normal | High/Medium |

- TIM3 以 2 kHz 触发 ADC；DMA 使用至少 32 点的偶数长度缓冲区，在半满/全满回调中只记录区段完成标志。
- TIM6 以 1 kHz 触发控制调度；控制 ISR 只设置调度标志或执行有界的内环计算，禁止 HAL_Delay、串口轮询和显示刷新。
- 所有 UART RX 采用环形 DMA 加 UART IDLE/位置差分取数；解析器消费环形缓冲区，不停止 DMA。
- TX 使用每端口独立的静态队列；DMA 完成回调推进队列。高优先级控制帧不能被遥测帧长期阻塞。
- OLED 限制为 10–20 Hz 刷新，并在 SPI/I²C 完成前禁止覆盖帧缓冲。

## 4. 通用二进制协议

所有自定义串口使用小端序和同一帧结构；SDM18 使用厂家协议，不套通用帧。

| 字段 | 长度 | 说明 |
|---|---:|---|
| SOF | 2 | 固定 `0xA5 0x5A` |
| Version | 1 | 初始为 1 |
| Message ID | 1 | 消息类型 |
| Payload Length | 2 | 0–256 字节 |
| Sequence | 2 | 每端口独立递增 |
| Timestamp | 4 | 发送端采样时间，单位 µs，允许回绕 |
| Payload | N | 只使用定宽整数；线上的物理量使用明确缩放 |
| CRC16 | 2 | CRC-16/CCITT-FALSE，覆盖 Version 至 Payload |

解析器必须执行：SOF 重同步、长度上限、版本、CRC、序号跳变和超时检查。错误帧只增加诊断计数，不修改控制状态。

### 4.1 下位机消息

| ID | 方向 | 内容 |
|---:|---|---|
| `0x10` | 主控→下位机 | `command:u8`、`reason:u8`；0=停止、1=启动、2=急停 |
| `0x11` | 下位机→主控 | `state:u8`、`fault:u16`、`last_command_seq:u16` |
| `0x12` | 下位机→主控 | `ax_mm_s2:i32`、`ay_mm_s2:i32`、`az_mm_s2:i32`、`quality:u8` |
| `0x13` | 双向 | 心跳；10–50 Hz |

START_KEY 稳定按下 20 ms 后只产生一次边沿事件；动态模式只有在底盘心跳和无故障
状态均新鲜时才接受启动。主控发送启动命令后等待最多 300 ms 且序号匹配的运行状态
确认，不把
按键电平连续刷到串口。底盘端对远程启动会话设置 250 ms 合法帧看门狗，失联会停止
双轮并以状态故障位 `0x8000` 报告。下位机 IMU 超过 50 ms 未更新时，加速度前馈
权重平滑衰减为零；超过 200 ms 进入通信降级。

安全机制按现场动作直接编排，不再经过独立事件分类层：START 发送/ACK 失败、
持续视觉丢失、持续编码器失效或传感器不一致均使用普通 STOP、撤销 STEP 并回到 IDLE；
底盘链路或底盘自身异常由 MSPM0 保护双轮，STM32 仅记录 WARN、补发一次 STOP 并继续
静态持球；只有限位和 DIAG 进入锁存 FAULT。OLED 独立显示 `WARN` 与 `FAULT`，
现场人员只需记住“FAULT 检查机械，WARN 检查链路/反馈”。

### 4.2 视觉消息

| ID | 方向 | 内容 |
|---:|---|---|
| `0x20` | 视觉→主控 | `position_um:i32`、`velocity_um_s:i32`、`confidence:u16`、`frame_age_us:u32` |
| `0x21` | 视觉→主控 | `status:u16`、`frame_counter:u32`、`exposure_us:u32` |
| `0x22` | 主控→视觉 | 目标位置、运行状态和时间同步请求 |

视觉必须发送采集时刻而非串口发送时刻。`confidence` 范围 0–1000；低于配置阈值或帧龄超限时，只允许状态预测，不直接用该测量修正控制量。

## 5. 驱动和模块边界

建议目录保持三层，不允许控制器直接访问 HAL 句柄。工程已将非生成代码全部收口归档到 `User/` 唯一主目录，消除了根目录历史重叠目录：

```text
Core/                 CubeMX 生成代码与启动入口（由 CubeMX 管理）
User/Bsp/             ADC、encoder、stepper、UART DMA、OLED、key/limit 驱动
User/App/             estimator、controller、safety、scheduler、业务模块
User/Protocol/        通用协议编解码及帧定义
User/Config/          用户编译期配置 (user_config.h)
User/user_runtime.*   业务组合根（Core 只依赖此入口）
User/user_isr.*       中断事件窄适配（只转发，不做业务计算）
```

编译器必须加入 `User`、`User/Bsp/Inc`、`User/App/Inc`、`User/Protocol/Inc` 和
`User/Config` 五个包含路径；源文件只加入 `User/Bsp/Src`、`User/App/Src`、
`User/Protocol/Src` 以及 `User/user_runtime.c`、`User/user_isr.c`。
CubeMX 重新生成时只允许覆盖 `Core/` 与 `Drivers/`，不得把
`User/` 设置为生成目录。

启用 ADC1 后，Keil 工程还必须包含 HAL 驱动源
`stm32f4xx_hal_adc.c` 和 `stm32f4xx_hal_adc_ex.c`；这两项已加入当前
`MDK-ARM/balance_control.uvprojx`。

`User_Runtime_Init()` 应在所有 `MX_*_Init()` 完成后调用一次，主循环只调用
`User_Runtime_Run()`。生成的中断文件在 USER CODE 区域调用
`User_Isr_OnSchedulerTick()`、`User_Isr_OnUartIdle()` 等窄接口即可。这样
业务模块不会散落在 `main.c`，也避免 ISR 直接调用协议解析或控制器。

公开接口仅传递值对象和状态，不暴露可修改的全局变量：

```c
typedef struct
{
    int32_t value;
    uint32_t timestamp_us;
    uint16_t age_ms;
    uint8_t quality;
    bool valid;
} sensor_sample_t;

typedef struct
{
    int32_t position_count;
    int32_t velocity_count_s;
    bool index_valid;
    bool pwm_valid;
} encoder_state_t;
```

- 每个模块提供 `Init`、周期 `Process`、只读 `GetState` 和故障清除接口。
- 编码器状态同时保留 ABZ 计数和 PWM 占空比千分比；PWM 只在 ABZ 无效或维护
  模式下作为降级测量，必须经过独立标定后才能进入控制量。
- HAL 回调只调用对应 BSP 的 ISR 入口，不调用 App 层业务函数。
- 配置常量集中到模块配置结构，禁止在控制算法中散布引脚、计数频率、量程和魔数。
- 编码器计数到摆杆角度的 `mrad_per_count` 必须在机构标定后写入估计器和控制器配置；默认值只用于上电调试，不能作为最终精度参数。
- MISRA C 要求：固定宽度整数、显式转换、枚举状态机、单一所有权缓冲区、有界循环、无动态内存、无递归、检查所有 HAL 返回值。

## 6. 闭环结构与带宽

控制状态推荐为摆杆角度、角速度、目标位置和目标速度。磁编码器 AB 为快速角度/速度主反馈，电位器用于绝对零点和交叉校验，PWM 编码输出只作降级后备。

```text
视觉位置 + SDM18距离 -> 时间对齐与异常值拒绝 -> 位置/速度估计器
底盘 ax -------------------------------------> 加速度前馈
位置/速度误差 -> 外环 -> 目标摆杆角/目标角速度
磁编角度/速度 -> 内环 -> 目标步频和方向 -> TMC2209 STEP/DIR
```

- 调度基准：1 kHz；TIM8 由硬件持续产生 STEP，软件只更新 ARR/CCR/DIR。
- 当前角度/速度内环：500 Hz，目标带宽先设 20–40 Hz，并根据机构共振降低。
- 当前位置外环：50 Hz，带宽保持为内环的 1/5 至 1/10。
- 视觉与激光融合：按新测量到达事件更新，预测频率 100–200 Hz。
- 加速度前馈先从零增益开始，仅在坐标方向、零偏和延迟验证后增加。
- 所有控制输出包含速度、加速度、角度、步频和机械行程限制，并实现积分抗饱和。

SDM18 是单点距离传感器，只有在安装几何能把距离唯一映射为被控位置时才参与状态更新；否则只作为边界/障碍信息。不要把激光距离与视觉位置直接平均。

## 7. CubeMX 生成后的实施顺序

1. 在工程根目录运行 `powershell -ExecutionPolicy Bypass -File Tools/check_ioc.ps1` 和 `Tools/check_user_layout.ps1`，再打开 `.ioc`，确认 ADC1、TIM2 Encoder、TIM5 PWM Input、DMA 和引脚均在生成清单中，没有黄色时钟或引脚冲突标记，然后生成 MDK-ARM 工程；保留所有 USER CODE 区域。当前仓库已提供 ADC1/TIM2/TIM5 外设层快照；重新生成后应再次运行两项检查。
2. 首先验证 168 MHz 时钟、四路 UART 波特率、DMA IRQ 和 TIM6 1 kHz 周期。
3. 验证 PC8 在复位及初始化全过程保持高，随后以低速 STEP 测试方向、限位和急停。
4. 分别记录电位器 ADC、ABZ 计数和 PWM 角度，完成零点、方向、每转计数及非线性标定。
5. 实现 UART 环形 DMA 和协议解析器，使用丢字节、粘包、拆包、错误 CRC 和计数器回绕测试。
6. 先闭合步进电机位置/速度环，再加入摆杆角度环，最后启用视觉位置外环。
7. SDM18 与 IMU 前馈先只记录日志；验证时间戳、符号、比例和延迟后逐项加入融合。
8. 最后启用 OLED，确保显示刷新不会改变控制周期最大执行时间。

## 8. 验收条件

- CubeMX 重新打开 `.ioc` 后所有外设配置保持不丢失且无引脚/DMA 冲突。
- TIM6 控制周期抖动不超过周期的 5%；待处理 tick 使用饱和计数而非位标志，任何串口持续满速接收时 `dropped_tick_count` 必须为零。
- TMC2209 复位默认禁用；视觉/摆杆反馈持续超时时受控停止 STEP，限位或 DIAG 则在
  一个控制周期内锁存急停。单独的底盘 IMU 超时只撤销加速度前馈；底盘异常由
  底盘自身停止双轮，不同时关闭滚球执行器。
- ABZ 与电位器换算角度在标定工作区内保持允许误差；不一致持续超限进入降级或故障，而不是平均掩盖错误。
- 视觉、激光、IMU 任意一路超时不会留下陈旧控制量；恢复时权重平滑恢复。
- MSPM0 栈溢出或动态内存失败钩子在停留现场前先调用底层电机断使能，不依赖已失效的调度任务。
- 协议解析通过正常帧、拆包、粘包、乱序、超长、CRC 错误和时间戳回绕测试。
