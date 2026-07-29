# 2026 TI H题：车载平衡滚球控制工程

本仓库给出可直接联调的三端实现：MSPM0 两轮差速底盘、STM32F407
滚球平衡主控和 K230 视觉位置前端。设计以“单一控制权、物理量接口、纯算法内核”
为边界，避免巡线、辨识、串口和安全逻辑同时写执行器。

| 目录 | 职责 | 主入口 |
|---|---|---|
| `mspm0/MSPM0G3507_Project/MSPM0G3507_FreeRTOS` | M1/M4 两轮差速、四路红外、轮速编码器、IMU、单圈状态机 | `Application/app_main.c` |
| `stm32f4/balance_control` | 视觉—物理估计、滚球—摆杆鲁棒串级控制、步进执行与安全 | `User/user_runtime.c` |
| `k230/runtime` | 像素—物理位置标定、跳变门控、速度估计和视觉协议帧 | `ball_balance_link.py` |

详细架构、接线、参数、标定和逐项验收见
[`docs/H题系统集成与验收.md`](docs/H题系统集成与验收.md)。

快速软件验收：

```powershell
python k230\tests\test_ball_balance_link.py
powershell -File mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Test\run_line_follower_host.ps1
powershell -File mspm0\MSPM0G3507_Project\MSPM0G3507_FreeRTOS\Test\check_arm_syntax.ps1
powershell -File stm32f4\balance_control\Tools\run_ball_control_tests.ps1
powershell -File stm32f4\balance_control\Tools\check_ioc.ps1
powershell -File stm32f4\balance_control\Tools\check_user_layout.ps1
powershell -File stm32f4\balance_control\Tools\check_arm_syntax.ps1
```

主机测试和语法检查不能替代实车方向确认、传感器标定、负载阶跃与赛道验收；
未完成标定前应使用 `SAFE` 档位并架空驱动轮。
