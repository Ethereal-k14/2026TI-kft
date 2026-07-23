# K230 视觉工程 · 快速上手参考卡

> 一张卡搞定开发环境 → 训练 → 导出 → 上板全流程

---

## ① 首次安装（只需一次）

```bash
# 进入工程目录
cd d:/Destop/k230-project     # Windows
# cd /mnt/d/Destop/k230-project  # WSL2

# 安装依赖（自动下载 Python 3.10 + 全部包）
uv sync

# 验证环境（约 20s，全部 PASS 才继续）
uv run python tools/audit_workspace.py
uv run python tools/verify_env.py
```

---

## ② 训练（5 种任务）

```bash
# 目标检测（最常用）
uv run python scripts/train_detect.py --data configs/coco128.yaml --epochs 10 --imgsz 320

# 图像分类
uv run python scripts/train_classify.py --data datasets/my_cls --epochs 50

# 实例分割
uv run python scripts/train_segment.py --data configs/coco_seg.yaml --epochs 10

# 关键点姿态
uv run python scripts/train_pose.py --data configs/coco_pose.yaml --epochs 10

# 旋转框检测
uv run python scripts/train_obb.py --data configs/obb_sample.yaml --epochs 10
```

训练权重自动保存到 `weights/<任务>/<name>/weights/best.pt`

---

## ③ 推理与追踪

```bash
# 图片推理
uv run python scripts/infer.py --source test.jpg --weights weights/detect/yolo11n/weights/best.pt

# 视频推理
uv run python scripts/infer.py --source video.mp4 --weights weights/detect/yolo11n/weights/best.pt

# 摄像头实时推理
uv run python scripts/infer.py --source 0 --weights weights/detect/yolo11n/weights/best.pt

# 多目标追踪
uv run python scripts/track.py --source video.mp4 --weights weights/detect/yolo11n/weights/best.pt
```

---

## ④ 导出 ONNX（上板前置步骤）

```bash
uv run python scripts/export_onnx.py \
    --weights weights/detect/yolo11n/weights/best.pt \
    --imgsz 320 \
    --out weights/best_320.onnx
```

> 自动校验：静态 Shape、onnxsim 化简、opset=13

---

## ⑤ 转换 .kmodel（Windows 原生 或 Linux/WSL2）

```bash
# Windows 原生（需已 uv sync 安装 nncase + nncase-kpu）
uv run python tools/to_kmodel.py \
    --model weights/best_320.onnx \
    --dataset datasets/calib \
    --input-size 320 320 \
    --output weights/best_320.kmodel

# Linux/WSL2（把 ONNX 和 calib 目录拷过去再执行）
python tools/to_kmodel.py \
    --model best_320.onnx \
    --dataset calib \
    --input-size 320 320 \
    --output best_320.kmodel
```

---

## ⑥ 打包部署包

```bash
uv run python tools/generate_deploy_pack.py \
    --model weights/best_320.kmodel \
    --data configs/coco128.yaml \
    --task detect \
    --imgsz 320
# 生成 deploy_pack/ 目录，直接拷贝到 K230 SD 卡 /sharefs/
```

---

## ⑦ 板端运行（K230）

**MicroPython (CanMV)**：拷贝 `templates/canmv_k230_demo.py` → 修改 `MODEL_PATH` → 重命名为 `main.py` → 放 SD 卡

**Linux C++**：
```bash
# 在 K230 串口终端执行
chmod +x run.sh && ./run.sh
```

**Web 视频流**（用 Type-C 或 RJ45 直连）：
```
http://192.168.42.1:8080    # Type-C USB RNDIS
http://<板子IP>:8080        # 网线 RJ45
```

---

## 常用诊断命令

```bash
# 检查 GPU
uv run python -c "import torch; print('CUDA:', torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'N/A')"

# 检查包版本
uv run python -c "import ultralytics, onnx; print('ultralytics', ultralytics.__version__, '| onnx', onnx.__version__)"

# 完整工作区审计
uv run python tools/audit_workspace.py

# 全流程功能验证
uv run python tools/verify_env.py

# 查看所有训练参数
uv run python scripts/train_detect.py --help
```

---

## 关键约束速记

| 约束 | 值 | 原因 |
| :--- | :--- | :--- |
| Python 版本 | `3.10.x` | nncase 兼容窗口 |
| ONNX opset | `13` | nncase 2.x 最佳兼容 |
| 导出模式 | `dynamic=False` | KPU 静态 Shape 推理 |
| 量化类型 | `uint8` | K230 KPU 原生精度 |
| 校准方法 | `Kld`（大小写敏感） | nncase 2.x 唯一合法值 |
| 预处理 | `norm255` | 省去板端 CPU 前处理 |
| nncase 版本 | **必须与 K230 镜像一致** | 不匹配=上板 Invalid kmodel |
