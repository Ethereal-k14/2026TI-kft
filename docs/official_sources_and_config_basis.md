# 嘉楠 K230 视觉工程·全部配置与参数权威依据对照表

本文件汇总了本工程中所有配置（`configs/`）、脚本参数（`scripts/`）、模型导出（`export_onnx.py`）、nncase 编译器参数（`to_kmodel.py`）以及板端接口的**官方权威出处与理论依据**。

---

## 1. 模型导出与转换参数权威依据

| 参数 / 配置项 | 工程设定值 | 官方 / 权威出处 | 理论与硬件设计依据 |
| :--- | :--- | :--- | :--- |
| **ONNX 动态轴** | `dynamic=False` | [嘉楠 k230_sdk 开发指南](https://github.com/kendryte/k230_sdk) | K230 KPU 硬件推理引擎采用静态内存分配与算子调度，带动态 Shape (`dynamic_axes`) 会导致 `nncase` 图分析失败。 |
| **ONNX opset** | `opset=13` | [nncase 官方文档](https://github.com/kendryte/nncase) | `nncase` 2.x 对 ONNX opset 11~13 的算子解析支持度最佳（兼容 YOLO11/v8 的 Conv, C3k2, Concat, Sigmoid, Softmax）。 |
| **ONNX 简化** | `simplify=True` | Ultralytics & 嘉楠官方 YOLO 部署教程 | 通过 `onnxsim` 折叠常量节点与冗余 Shape 操作，防止无法硬件化的中间节点下发到 KPU 触发 fallback。 |
| **nncase target** | `compile_options.target = "k230"` | `nncase` API 规范 | 必须显式声明 `k230` 以激活特定于嘉楠 KPU (NC200 架构) 的汇编生成器。 |
| **量化数据类型** | `quant_type = "uint8"` | K230 KPU 硬件规范 | K230 KPU 矩阵乘法器原生针对 `uint8` / `int8` 8位定点优化，`uint8` 能发挥 4.0 TOPS 峰值算力。 |
| **预处理参数** | `preprocess = "norm255"` | `nncase` 预处理模块设计规范 | 设定 `input_mean=[0,0,0], input_std=[255,255,255]` 后，归一化被编译进 `.kmodel` 硬件算子中，**省略 CPU 前处理耗时**。 |
| **PTQ 校准算法** | `calib-method = "Kld"` / `"NoClip"` | `nncase` PTQ 最佳实践 | KL 散度 (Kld) 能最大程度维持激活值直方图分布；NoClip 保留完整动态范围。**注意 nncase 2.x 仅支持 `Kld` 和 `NoClip` 两个字符串（大小写敏感）。** |

---

## 2. 训练与模型选型权威依据

| 参数 / 配置项 | 工程设定值 | 官方 / 权威出处 | 理论与硬件设计依据 |
| :--- | :--- | :--- | :--- |
| **首选模型** | **YOLO11n** (`yolo11n.pt`) | 嘉楠官方 [K230_training_scripts](https://github.com/kendryte/K230_training_scripts) | 参数量仅 ~2.6M，在 K230 4 TOPS KPU 上可轻松实现 30~50 FPS 实时推理，且精度优于 YOLOv8n。 |
| **模型尺寸** | `imgsz = 320` 或 `640` | K230 YOLO 应用开发指南 | 320×320 可大幅降低 KPU 算力与内存带宽占用（低开销首选）；640×640 适用于小目标识别。 |
| **预训练权重** | 迁移学习启动 | Ultralytics 训练最佳实践 | 从预训练 `.pt` 权重微调能大幅加速收敛，并规避小数据集上的梯度过拟合。 |
| **多任务拓展** | detect/classify/segment/pose/obb | 嘉楠 C++ `yolo.elf` 应用程序源码 | 官方 `yolo.elf` 内部集成了这 5 类任务的 Output Head 解析与 NMS 后处理。 |

---

## 3. 板端接口与网络参数权威依据

| 参数 / 配置项 | 工程设定值 | 官方 / 权威出处 | 理论与硬件设计依据 |
| :--- | :--- | :--- | :--- |
| **USB 虚拟网卡** | `network.USB_RNDIS()` / `192.168.42.1` | CanMV K230 官方 SDK 接口 | K230 USB OTG 默认被板端固件映射为 RNDIS 虚拟网卡，用 Type-C 线插电脑即可直连免线配置。 |
| **以太网接口** | `network.LAN()` | K230 芯片硬件 MAC 规范 | 物理 RJ45 百兆/千兆网口，局域网极低延迟 (< 5ms)。 |
| **置信度 / NMS 阈值** | `conf_thres=0.35, nms_thres=0.65` | K230 官方 `yolo.elf` 默认参数 | 经官方实测平衡召回率 (Recall) 与精确度 (Precision) 的推荐上板阈值。 |
| **Web 视频流协议** | `multipart/x-mixed-replace` | HTTP/1.0 协议标准与 CanMV 示例 | 硬件 JPEG 压缩输出直接通过 HTTP Socket 推送，无需任何第三方浏览器插件即可原生播放。 |

---

## 4. 官方资料与源码索引链接

1. **嘉楠 K230 官方文档**：<https://www.kendryte.com/ai_docs/zh/main/>
2. **K230 YOLO 官方训练仓库**：<https://github.com/kendryte/K230_training_scripts>
3. **K230 SDK 官方主仓库**：<https://github.com/kendryte/k230_sdk>
4. **nncase 官方编译器仓库**：<https://github.com/kendryte/nncase>
5. **CanMV K230 开发仓库**：<https://github.com/kendryte/canmv_k230>
