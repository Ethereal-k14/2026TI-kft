# 2026 TI H题：车载平衡滚球控制工程

本仓库给出可直接联调的三端实现：MSPM0 两轮差速底盘、STM32F407
滚球平衡主控和 K230 视觉位置前端。设计以“单一控制权、物理量接口、纯算法内核”
为边界，避免巡线、辨识、串口和安全逻辑同时写执行器。

| 目录 | 职责 | 主入口 |
|---|---|---|
| `mspm0/MSPM0G3507_Project/MSPM0G3507_FreeRTOS` | M1/M4 两轮差速、四路红外、轮速编码器、IMU 安装/偏心补偿、S 曲线速度规划、单圈状态机 | `Application/app_main.c` |
| `stm32f4/balance_control` | 视觉—物理估计、滚球—摆杆鲁棒串级控制、步进执行与安全 | `User/user_runtime.c` |
| `k230/runtime` | 像素—物理位置标定、跳变门控、速度估计和视觉协议帧 | `ball_balance_link.py` |

`SDM18/`、`WHEELTEC MS42CG…/` 和 `mspm0/MSPM0G3507_M0_Base/` 是供应商/上游
只读参考，不参与当前比赛构建。完整文档入口见 [`docs/README.md`](docs/README.md)，
目录所有权和交付规则见 [`docs/目录与交付规范.md`](docs/目录与交付规范.md)。

一键软件验收：

```powershell
powershell -File tools\verify_all.ps1
```

该命令同时检查权威目录、禁止的生成物和三端接口常量漂移。主机测试和语法检查
不能替代完整链接、实车方向确认、传感器标定、负载阶跃与赛道验收；未完成标定前
应使用 `SAFE` 档位并架空驱动轮。详细流程见
[`docs/H题系统集成与验收.md`](docs/H题系统集成与验收.md)。
