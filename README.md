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
├── .gitignore              # Git 忽略规则（过滤 .venv / 权重 / 缓存）
├── .gitattributes          # 换行符规范（强制 LF，防止 K230 板端脚本报错）
├── requirements-convert.txt# nncase 转换依赖（在 Linux/WSL2 中安装）
├── configs/                # 数据集 / 训练超参配置
│   ├── coco128.yaml        # COCO128 检测示例（80 类）
│   ├── coco_pose.yaml      # 姿态估计示例（17 关键点）
│   ├── coco_seg.yaml       # 实例分割示例
│   ├── classify_sample.yaml# 图像分类示例
│   └── obb_sample.yaml     # 旋转框检测示例
├── scripts/                # 5 大基础 AI 视觉任务训练 / 推理 / 导出
│   ├── _device.py          # GPU/CPU 自动探测公共模块
│   ├── train_detect.py     # YOLO11n 目标检测训练
│   ├── train_classify.py   # YOLO11n-cls 图像分类训练
│   ├── train_segment.py    # YOLO11n-seg 实例分割训练
│   ├── train_pose.py       # YOLO11n-pose 关键点姿态训练
│   ├── train_obb.py        # YOLO11n-obb 旋转框检测训练
│   ├── infer.py            # 图片 / 视频推理与可视化
│   ├── track.py            # 多目标追踪（ByteTrack / BoT-SORT）
│   └── export_onnx.py      # 导出 K230 规范 ONNX（含静态 Shape 校验）
├── templates/              # K230 板端运行代码例程模板
│   ├── canmv_k230_demo.py  # CanMV (MicroPython) 上板推理模板
│   ├── canmv_k230_web_streamer.py # Web 端口局域网实时画框视频流
│   └── k230_cpp_runner.sh  # Linux C++ (yolo.elf) 启动脚本
├── tools/                  # 开发工具链
│   ├── verify_env.py       # 一键全流程环境校验（训练→推理→导出）
│   ├── audit_workspace.py  # 工作区 7 维完整性审计
│   ├── generate_deploy_pack.py # 一键构建 K230 板端部署包
│   └── to_kmodel.py        # nncase ONNX→.kmodel（Linux/WSL2 运行）
├── data/                   # 标签文件（含 coco_labels.txt）
├── datasets/               # 训练 / 校验数据集
├── weights/                # 训练产出的权重与导出模型
└── docs/                   # 开发文档（8 篇）
    ├── official_sources_and_config_basis.md
    ├── base_tasks_training_and_deploy.md
    ├── dotnet_and_web_streaming.md
    ├── canaan_k230_official_guide.md
    ├── environment_verification.md
    ├── self_training.md
    ├── k230_deploy.md
    └── models_overview.md
```

## 2. 快速开始

```bash
# 进入工程目录（本机实际位于 D 盘桌面）
cd /d/Destop/k230-project

# 用 uv 创建虚拟环境并安装依赖（首次会自动下载 Python 3.10 与依赖）
uv sync

# 1. 验证环境（基础包验证 & 一键全流程功能全项检验 & 7 维工作区完整性审计）
uv run python -c "import ultralytics, torch; print('ultralytics', ultralytics.__version__, '| torch', torch.__version__, '| cuda', torch.cuda.is_available())"
uv run python tools/verify_env.py
uv run python tools/audit_workspace.py

# 用 COCO128 小数据集快速体验 YOLO11n 检测训练
uv run python scripts/train_detect.py --data configs/coco128.yaml --epochs 10 --imgsz 640

# 推理
uv run python scripts/infer.py --source test.jpg --weights weights/best.pt

# 多目标追踪
uv run python scripts/track.py --source video.mp4 --weights weights/best.pt --tracker botsort
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
