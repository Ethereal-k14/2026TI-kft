# 基础 AI 视觉识别任务自训练与 K230 部署权威指南

本指南全面覆盖了嘉楠 K230 (KPU) 平台支持的 **5 大基础 AI 视觉任务**：
1. **目标检测 (Detect)**
2. **图像分类 (Classify)**
3. **实例分割 (Segment)**
4. **关键点姿态估算 (Pose)**
5. **旋转框目标检测 (OBB)**

---

## 1. 5 大基础识别任务矩阵与模型选型

| 任务类型 | 描述与典型场景 | 推荐预训练模型 | 训练脚本 | 推荐输入尺寸 (`imgsz`) |
| :--- | :--- | :--- | :--- | :--- |
| **目标检测 (Detect)** | 识别物体位置与类别 (人/车/物品) | `yolo11n.pt` | `scripts/train_detect.py` | 320 或 640 |
| **图像分类 (Classify)** | 判定整张图属于什么类别/状态 | `yolo11n-cls.pt` | `scripts/train_classify.py` | 224 或 320 |
| **实例分割 (Segment)** | 检测物体并提取像素级掩膜边界 | `yolo11n-seg.pt` | `scripts/train_segment.py` | 320 或 640 |
| **关键点姿态 (Pose)** | 识别人体 17 关键点 / 手势关节点 | `yolo11n-pose.pt` | `scripts/train_pose.py` | 320 或 640 |
| **旋转框检测 (OBB)** | 检测倾斜/带角度的物体 (PCB/文本) | `yolo11n-obb.pt` | `scripts/train_obb.py` | 320 或 640 |

---

## 2. 5 大任务训练、导出与打包 SOP

### 2.1 目标检测 (Detect)
```bash
# 1. 训练
uv run python scripts/train_detect.py --data configs/coco128.yaml --epochs 100 --imgsz 320

# 2. 导出 ONNX
uv run python scripts/export_onnx.py --weights weights/detect/yolo11n/weights/best.pt --imgsz 320 --task detect

# 3. 部署打包
uv run python tools/generate_deploy_pack.py --model best.kmodel --data configs/coco128.yaml --task detect --imgsz 320
```

### 2.2 图像分类 (Classify)
```bash
# 1. 训练
uv run python scripts/train_classify.py --data datasets/my_cls --epochs 50 --imgsz 224

# 2. 导出 ONNX
uv run python scripts/export_onnx.py --weights weights/classify/yolo11n_cls/weights/best.pt --imgsz 224 --task classify

# 3. 部署打包
uv run python tools/generate_deploy_pack.py --model best.kmodel --data configs/classify_sample.yaml --task classify --imgsz 224
```

### 2.3 实例分割 (Segment)
```bash
# 1. 训练
uv run python scripts/train_segment.py --data configs/coco_seg.yaml --epochs 100 --imgsz 320

# 2. 导出 ONNX
uv run python scripts/export_onnx.py --weights weights/segment/yolo11n-seg/weights/best.pt --imgsz 320 --task segment

# 3. 部署打包
uv run python tools/generate_deploy_pack.py --model best.kmodel --data configs/coco_seg.yaml --task segment --imgsz 320
```

### 2.4 关键点姿态 (Pose)
```bash
# 1. 训练
uv run python scripts/train_pose.py --data configs/coco_pose.yaml --epochs 100 --imgsz 320

# 2. 导出 ONNX
uv run python scripts/export_onnx.py --weights weights/pose/yolo11n-pose/weights/best.pt --imgsz 320 --task pose

# 3. 部署打包
uv run python tools/generate_deploy_pack.py --model best.kmodel --data configs/coco_pose.yaml --task pose --imgsz 320
```

### 2.5 旋转框检测 (OBB)
```bash
# 1. 训练
uv run python scripts/train_obb.py --data configs/obb_sample.yaml --epochs 100 --imgsz 320

# 2. 导出 ONNX
uv run python scripts/export_onnx.py --weights weights/obb/yolo11n_obb/weights/best.pt --imgsz 320 --task obb

# 3. 部署打包
uv run python tools/generate_deploy_pack.py --model best.kmodel --data configs/obb_sample.yaml --task obb --imgsz 320
```

---

## 3. K230 板端多任务命令参数表 (`yolo.elf`)

在 K230 Linux 侧串口运行官方 C++ 可执行文件 `yolo.elf` 时，可通过 `-task_type` 参数动态切换五大任务：

| `-task_type` 参数 | 对应任务 | 板端启动命令示例 |
| :--- | :--- | :--- |
| `detect` | 目标检测 | `./yolo.elf -model_type yolo11 -task_type detect -task_mode video -kmodel_path best.kmodel -labels_txt_filepath coco_labels.txt -conf_thres 0.35 -nms_thres 0.65` |
| `classify` | 图像分类 | `./yolo.elf -model_type yolo11 -task_type classify -task_mode video -kmodel_path best.kmodel -labels_txt_filepath labels.txt` |
| `segment` | 实例分割 | `./yolo.elf -model_type yolo11 -task_type segment -task_mode video -kmodel_path best.kmodel -labels_txt_filepath coco_labels.txt` |
| `pose` | 关键点姿态 | `./yolo.elf -model_type yolo11 -task_type pose -task_mode video -kmodel_path best.kmodel -labels_txt_filepath coco_labels.txt` |
| `obb` | 旋转框识别 | `./yolo.elf -model_type yolo11 -task_type obb -task_mode video -kmodel_path best.kmodel -labels_txt_filepath labels.txt` |

> **说明**：`-task_mode video` 表示摄像头实时模式；`-labels_txt_filepath` 与 `-kmodel_path` 为必填参数。详见 `docs/k230_deploy.md`。
