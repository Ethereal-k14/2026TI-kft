# 自训练方法（Self-Training）

面向 K230 的视觉模型自训练全流程。**目标**：用你自己的数据，训练出高精度、低开销的
YOLO11n（或 YOLOv8n）模型，并导出为 K230 可部署的 `.kmodel`。

---

## 0. 环境准备

```bash
cd /d/Destop/k230-project
uv sync                      # 创建 venv + 安装训练/推理依赖（默认 CUDA cu121）
```

**GPU 训练（本机默认）**：`pyproject.toml` 已默认从 PyTorch **cu121** 索引安装 CUDA 版
`torch`，所有脚本通过 `scripts/_device.py` 自动探测 `torch.cuda.is_available()`，
有 GPU 用 `cuda:0`，无 GPU 自动降级到 `cpu`。

如需在**纯 CPU 机器**上复用本工程：把 `pyproject.toml` 中的索引
`https://download.pytorch.org/whl/cu121` 改为 `https://download.pytorch.org/whl/cpu`，
并将 `torch`/`torchvision` 的 source 指向 `pytorch-cpu`，再 `uv sync`。
CUDA 驱动需自行安装，与 PyTorch 版本匹配。

---

## 1. 数据采集与标注

### 数量建议（经验值）
| 场景 | 每类最少 | 推荐 |
|------|----------|------|
| 简单目标（形态固定） | 200 张 | 500+ |
| 复杂目标（姿态/光照多变） | 500 张 | 1000+ |
| 小目标检测 | 800 张 | 2000+ |

- 覆盖真实部署场景的**光照、角度、距离、遮挡、背景**。
- 标注格式用 **YOLO txt**（每行：`class_id cx cy w h`，均归一化 0~1）。
- 标注工具：[Label Studio](https://labelstud.io/)、[CVAT](https://www.cvat.ai/)、
  [makesense.ai](https://www.makesense.ai/)（免费网页版）。

### 目录结构
```
datasets/mydata/
├── images/
│   ├── train/  train01.jpg  train02.jpg ...
│   └── val/    val01.jpg ...
└── labels/
    ├── train/  train01.txt ...   # 与图片同名
    └── val/    val01.txt ...
```

### 数据集 YAML（configs/mydata.yaml）
```yaml
path: ../datasets/mydata
train: images/train
val: images/val
names:
  0: cat
  1: dog
```

---

## 2. 训练

```bash
# 检测（最常用）
uv run python scripts/train_detect.py --data configs/mydata.yaml \
    --model yolo11n.pt --epochs 100 --imgsz 640 --batch 16

# 姿态（关键点）
uv run python scripts/train_pose.py --data configs/mypose.yaml --model yolo11n-pose.pt

# 实例分割
uv run python scripts/train_segment.py --data configs/myseg.yaml --model yolo11n-seg.pt
```

### 输入尺寸怎么选？
- **imgsz=640**：精度高，K230 仍可实时（YOLO11n 约 20~30 FPS）。
- **imgsz=320**：精度略降，速度更快、更省算力，**低开销首选**。
- 训练 / 导出 ONNX / K230 推理三者 **imgsz 必须一致**。

### 提升精度的实用技巧
1. **预训练权重**：始终从 `yolo11n.pt` 做迁移学习，别从头训。
2. **学习率**：`cos_lr=True`（脚本默认），`optimizer=auto`（默认 SGD/AdamW 自动）。
3. **数据增强**：小数据集适当降低 mosaic（`--close-mosaic 10`），避免过拟合。
4. **早停**：`--patience 30`，验证指标不再提升即停。
5. **难例挖掘**：训练后用 `infer.py` 跑测试集，把漏检/误检样本加回训练集再训一轮。

---

## 3. 评估与推理

```bash
# 跑验证集指标（mAP 等）
uv run yolo val model=weights/detect/yolo11n/weights/best.pt data=configs/mydata.yaml

# 单图 / 视频 / 摄像头推理
uv run python scripts/infer.py --source test.jpg --weights weights/detect/yolo11n/weights/best.pt
uv run python scripts/infer.py --source 0 --weights best.pt        # 摄像头
```

---

## 4. 多目标追踪（复用检测权重）

```bash
uv run python scripts/track.py --source video.mp4 --weights best.pt --tracker botsort
# bytetrack 更轻量，算力受限场景优先
```

---

## 5. 导出 ONNX（进入部署环节）

```bash
uv run python scripts/export_onnx.py --weights weights/detect/yolo11n/weights/best.pt --imgsz 640
```

导出后得到 `best.onnx`，下一步到 **Linux/WSL2** 用 `tools/to_kmodel.py` 转 `.kmodel`
（见 `docs/k230_deploy.md`）。

---

## 6. 常见问题

- **显存不足**：调小 `--batch`，或 `--imgsz 320`。
- **过拟合**：增加数据、增强、降低 epochs、提高 `--close-mosaic`。
- **导出 ONNX 后节点含动态轴**：确认 `dynamic=False`（脚本已默认）。
- **nncase 量化掉点严重**：增加校准集多样性（20~100 张），换 `--calib-method KLD/ACIQ`。
