# K230 视觉工程 · k230-project

> 基于 `uv` 管理的完整 Python 环境，面向 **嘉楠 K230** 开发板的视觉 AI 开发：
> 目标检测 / 实例分割 / 姿态估计 / 多目标追踪，主打 **YOLO11n** 等高精度、低开销模型，
> 并提供从自训练到 `nncase` 转 `.kmodel` 上板部署的全流程。

---

## 1. 目录结构

```
k230-project/
├── pyproject.toml          # uv 工程定义（训练/推理依赖）
├── .python-version         # 固定 Python 3.10
├── requirements-convert.txt# nncase 转换依赖（在 Linux/WSL2 中安装）
├── configs/                # 数据集 / 训练超参配置
├── scripts/                # 5 大基础 AI 视觉任务训练 / 推理 / 导出脚本
│   ├── train_detect.py     # YOLO11n 目标检测训练
│   ├── train_classify.py   # YOLO11n-cls 图像分类训练
│   ├── train_segment.py    # YOLO11n-seg 实例分割训练
│   ├── train_pose.py       # YOLO11n-pose 关键点姿态估算训练
│   ├── train_obb.py        # YOLO11n-obb 旋转框检测训练
│   ├── infer.py            # 图片 / 视频推理与可视化
│   ├── track.py            # 多目标追踪（ByteTrack / BoT-SORT）
│   └── export_onnx.py      # 导出 K230 规范 ONNX 模型
├── templates/              # 板端运行代码例程模板
│   ├── canmv_k230_demo.py  # CanMV (MicroPython) 上板推理运行代码模板
│   ├── canmv_k230_web_streamer.py # Web 端口局域网实时画框视频流服务模板
│   └── k230_cpp_runner.sh  # Linux C++ (yolo.elf) 命令行启动 Shell
├── tools/
│   ├── verify_env.py       # 一键全流程环境与开发链自动化校验工具
│   ├── generate_deploy_pack.py # 一键构建 K230 板端部署包 (deploy_pack)
│   └── to_kmodel.py        # nncase 将 ONNX 转 .kmodel（在 Linux/WSL2 运行）
├── data/                   # 标签、类别名等 (含 coco_labels.txt)
├── datasets/               # 你的训练 / 校验数据集
├── weights/                # 训练产出的权重与导出模型
└── docs/
    ├── official_sources_and_config_basis.md # 全工程配置与参数官方权威依据对照表
    ├── base_tasks_training_and_deploy.md # 5 大基础 AI 视觉任务自训练与 K230 部署指南
    ├── dotnet_and_web_streaming.md # .NET 机制探索与 Web 端口实时画框图像串流指南
    ├── canaan_k230_official_guide.md # 嘉楠 K230 官方资料深度整合与开发全景指南
    ├── environment_verification.md # 开发环境规范与检验指南
    ├── self_training.md    # 自训练方法（数据标注 → 训练 → 调优）
    ├── k230_deploy.md      # K230 上板部署（nncase → kmodel → 烧录）
    └── models_overview.md  # 官方最佳模型清单与选型建议
```

## 2. 快速开始

```bash
# 进入工程目录（本机实际位于 D 盘桌面）
cd /d/Destop/k230-project

# 用 uv 创建虚拟环境并安装依赖（首次会自动下载 Python 3.10 与依赖）
uv sync

# 1. 验证环境（基础包验证 & 一键全流程功能全项检验）
uv run python -c "import ultralytics, torch; print('ultralytics', ultralytics.__version__, '| torch', torch.__version__, '| cuda', torch.cuda.is_available())"
uv run python tools/verify_env.py

# 用 COCO128 小数据集快速体验 YOLO11n 检测训练
uv run python scripts/train_detect.py --data configs/coco128.yaml --epochs 10 --imgsz 640

# 推理
uv run python scripts/infer.py --source test.jpg --weights weights/best.pt

# 多目标追踪
uv run python scripts/track.py --source video.mp4 --tracker botsort
```

> **关于算力（从 CUDA 开始降级）**：本机已确认配备 **NVIDIA RTX 4060 Laptop GPU + CUDA 12.1**，
> `pyproject.toml` 默认从 PyTorch **cu121** 索引安装 CUDA 版 `torch`，训练/推理自动走 GPU。
> 所有脚本（`scripts/_device.py` 的 `resolve_device`）会先探测 `torch.cuda.is_available()`：
> 有 GPU 用 `cuda:0`，无 GPU 自动降级到 `cpu` 并在日志中提示——换到无显卡机器也能直接跑。
> 若要在一台纯 CPU 机器上复用本工程，把 `pyproject.toml` 的索引改回
> `https://download.pytorch.org/whl/cpu` 即可。

## 3. 为什么选 YOLO11n（以及 YOLOv8n）

| 模型 | 参数量 | 输入 | COCO mAP | K230 适用性 |
|------|--------|------|----------|-------------|
| YOLO11n | ~2.6M | 640 | ~39.5 | ✅ 官方支持，精度/速度均衡 |
| YOLOv8n | ~3.2M | 640 | ~37.3 | ✅ 官方支持，生态成熟 |
| YOLO11s | ~9.4M | 640 | ~47   | ⚠️ 精度更高，开销略大 |

K230 的 NPU（KPU）算力约 4 TOPS，YOLO11n/640 或 320 输入均可实时运行；
**320×320 输入更省算力、更适合低开销场景**，640×640 精度更高。详见 `docs/models_overview.md`。

## 4. 上板部署（要点）

1. 在 Windows 上用本工程训练并 `export_onnx.py` 导出 ONNX（固定输入尺寸、关闭 dynamic）。
2. 在 **Linux / WSL2 + Docker** 中用 `tools/to_kmodel.py` + `nncase` 做 PTQ 量化，生成 `.kmodel`。
   > nncase 与 K230 烧录镜像的版本必须严格一致（见 `docs/k230_deploy.md`）。
3. 将 `.kmodel` 拷贝到 SD 卡的 `sharefs`（双系统）或 `sdcard`（单系统）目录，上板运行。

详细步骤见 `docs/k230_deploy.md` 与 `docs/self_training.md`。
