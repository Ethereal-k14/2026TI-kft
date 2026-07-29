# MSPM0 上游来源

- 仓库：`https://github.com/Nanami7-7/AxiomTrace.git`
- 分支：`CXY`
- 导入基线：`a05ed9f`
- 导入用途：`MSPM0G3507_Project/MSPM0G3507_FreeRTOS` 为比赛底盘工程；
  `MSPM0G3507_M0_Base` 与其余示例保留作硬件和协议参考，不参与当前比赛运行拓扑。

本目录以源码快照纳入本仓库，而非 Git submodule，确保主仓库一次克隆即可复现。
后续同步上游时应先在独立分支比较，再选择性移植，不能覆盖本项目的两轮配置、
STM32 专用 UART1 链路和安全改动。
