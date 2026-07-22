# 官方最佳模型清单与选型（K230）

K230 官方 YOLO 封装统一支持 **YOLOv5 / YOLOv8 / YOLO11 / YOLO26**，覆盖
**detect（检测）/ segment（分割）/ obb（旋转框）/ pose（姿态）/ classify（分类）** 五类任务。
本工程主打 **YOLO11n**——官方支持、精度/开销均衡最佳。

---

## 1. 推荐模型（高精度 + 低开销优先）

| 模型 | 任务 | 参数量 | COCO mAP | K230 适用性 | 备注 |
|------|------|--------|----------|-------------|------|
| **YOLO11n** | detect | ~2.6M | 39.5 | ✅ 首选 | 最新架构，快且准 |
| YOLO11s | detect | ~9.4M | 47.0 | ✅ | 精度更高，开销略大 |
| **YOLOv8n** | detect | ~3.2M | 37.3 | ✅ | 生态最成熟，文档多 |
| YOLOv8n-pose | pose | ~3.3M | 62.5(POSE) | ✅ | 人体 17 关键点 |
| **YOLO11n-pose** | pose | ~2.9M | ~63 | ✅ | 更轻的姿态模型 |
| YOLOv8n-seg | segment | ~3.4M | 32.2 | ✅ | 实例分割 |
| YOLO11n-obb | obb | ~2.7M | — | ✅ | 旋转目标（航拍/文本） |
| YOLO11n-cls | classify | ~1.6M | 78.5 | ✅ | 图像分类 |

> 参数量/精度为公开发布参考值；实际精度随数据集与训练策略变化。
> 选用 `n`（nano）系列即可在 K230 4 TOPS NPU 上实时运行。

---

## 2. 任务 → 脚本 对照

| 任务 | 训练脚本 | 起始权重 | 导出 task |
|------|----------|----------|-----------|
| 目标检测 | `train_detect.py` | `yolo11n.pt` | detect |
| 实例分割 | `train_segment.py` | `yolo11n-seg.pt` | segment |
| 关键点/姿态 | `train_pose.py` | `yolo11n-pose.pt` | pose |
| 旋转目标 | 同 detect（obb 数据） | `yolo11n-obb.pt` | obb |
| 分类 | `yolo task=classify` | `yolo11n-cls.pt` | classify |

多目标追踪无需单独训练：在检测权重上用 `track.py`（ByteTrack / BoT-SORT）。

---

## 3. 输入分辨率与性能权衡

| 输入 | 精度 | 速度 | 适用 |
|------|------|------|------|
| 320×320 | 中 | 快（最省算力） | 低开销/高帧率首选 |
| 640×640 | 高 | 中 | 精度优先 |
| 与 AI 帧同分辨率 | — | 最优 | 上板调优经验值 |

K230 官方默认值：AI 帧 `640×360`。把模型输入分辨率设成与 AI 帧一致可进一步提速。

---

## 4. 官方资源入口

- K230 开发基础 / SDK / nncase：<https://www.kendryte.com/ai_docs/zh/main/开发基础.html>
- YOLO 应用指南（含 yolo11 示例）：<https://www.kendryte.com/k230_rtos/zh/main/app_develop_guide/ai/yolo.html>
- 训练脚本合集：<https://github.com/kendryte/K230_training_scripts>
- 勘智开发者社区（镜像/工具下载）：<https://developer.canaan-creative.com/>
- “YOLO 大作战” 模型转换教程（ONNX→kmodel 权威流程）

---

## 5. 选型建议（一句话）

> **要高精度低开销：先用 YOLO11n @ 320 起步，精度不够再上 640 或 YOLO11s；
> 追踪直接复用检测权重 + ByteTrack/BoT-SORT；分割/姿态用对应 -seg/-pose 变体。**
