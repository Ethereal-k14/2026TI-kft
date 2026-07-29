# 文档导航

本目录只维护当前 H 题三端系统的权威文档。芯片厂商、传感器厂商和上游仓库自带
文档保留在各自目录，仅作参考；当内容冲突时，以本目录、实际配置文件和自动检查
结果为准。

| 文档 | 用途 |
|---|---|
| [H题系统集成与验收.md](H题系统集成与验收.md) | 赛题映射、控制架构、接线、协议、标定、调参与分级验收 |
| [目录与交付规范.md](目录与交付规范.md) | 有效工程入口、目录所有权、生成代码边界、Git 和发布规则 |
| [../README.md](../README.md) | 仓库首页和一键软件验收入口 |
| [../H题_车载平衡滚球运动控制系统.pdf](../H题_车载平衡滚球运动控制系统.pdf) | 原始赛题文件，只读基准 |

组件级详细资料：

- MSPM0 上游来源：[../mspm0/UPSTREAM.md](../mspm0/UPSTREAM.md)
- MSPM0 固件文档索引：[../mspm0/MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Docs/README.md](../mspm0/MSPM0G3507_Project/MSPM0G3507_FreeRTOS/Docs/README.md)
- STM32 固件规范：[../stm32f4/balance_control/Docs/firmware_specification.md](../stm32f4/balance_control/Docs/firmware_specification.md)
- STM32 用户代码结构：[../stm32f4/balance_control/User/README.md](../stm32f4/balance_control/User/README.md)
- K230 快速开始：[../k230/QUICKSTART.md](../k230/QUICKSTART.md)
- K230 部署说明：[../k230/docs/k230_deploy.md](../k230/docs/k230_deploy.md)

所有提交前检查统一从仓库根目录执行：

```powershell
powershell -File tools\verify_all.ps1
```

该命令验证权威路径和跨端协议常量，并依次运行 K230、MSPM0、STM32 的主机测试、
CubeMX/工程结构检查和 ARM 源文件语法检查。它不替代 Keil 完整链接、烧录和实车验收。
