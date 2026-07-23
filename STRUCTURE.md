# K230 视觉工程 · 目录规范

> 本文档是项目**目录结构的唯一权威说明**。
> 所有协作者、工具脚本与文档均以此为准，不得随意新增顶层目录。

---

## 顶层目录一览

```
k230-project/
│
│── STRUCTURE.md          ← 你正在读的文件：目录规范
│── README.md             ← 项目概览与快速开始
│── QUICKSTART.md         ← 最快速的上手参考卡
│
│── pyproject.toml        ← uv 工程定义（Python 版本 + 全部依赖）
│── .python-version       ← 锁定 Python 3.10（nncase 兼容窗口）
│── uv.lock               ← 依赖版本快照（不手动编辑）
│── requirements-convert.txt  ← Linux/WSL2 侧 nncase 转换辅助依赖
│
│── .gitignore            ← Git 忽略规则
│── .gitattributes        ← 换行符规范（强制 LF）
│
├── configs/              ← 数据集与训练超参 YAML 配置
├── scripts/              ← 训练 / 推理 / 导出 Python 脚本
├── tools/                ← 开发工具链（验证、转换、打包）
├── templates/            ← K230 板端代码模板
├── data/                 ← 静态标签 / 辅助文件
├── datasets/             ← 用户数据集（不入 Git）
├── weights/              ← 模型权重（不入 Git）
└── docs/                 ← 开发文档
```

---

## 各目录详细规范

### `configs/` — 数据集 & 训练配置

```
configs/
├── coco128.yaml          # COCO128 目标检测示例（80 类）
├── coco_pose.yaml        # COCO-Pose 姿态估计（17 关键点）
├── coco_seg.yaml         # 实例分割示例
├── classify_sample.yaml  # 图像分类示例（2 类）
└── obb_sample.yaml       # 旋转框检测示例（3 类）
```

**命名规范**：`<数据集名称>_<任务>.yaml`，全小写下划线。  
**自定义配置**：新建时复制最近的示例 YAML 修改，**不要修改已有示例文件**（供他人参考）。

---

### `scripts/` — 核心 Python 脚本

```
scripts/
├── _device.py            # 【公共模块】GPU/CPU 自动探测，所有训练脚本共用
├── train_detect.py       # 目标检测 → weights/detect/<name>/
├── train_classify.py     # 图像分类 → weights/classify/<name>/
├── train_segment.py      # 实例分割 → weights/segment/<name>/
├── train_pose.py         # 关键点姿态 → weights/pose/<name>/
├── train_obb.py          # 旋转框检测 → weights/obb/<name>/
├── infer.py              # 推理可视化（图片/视频/摄像头）
├── track.py              # 多目标追踪（ByteTrack / BoT-SORT）
└── export_onnx.py        # 导出 K230 规范 ONNX（静态 Shape + onnxsim）
```

**5 大训练脚本统一 CLI 接口**：

| 参数 | 含义 | 默认值 |
| :--- | :--- | :--- |
| `--data` | 数据集 YAML 路径（必填） | — |
| `--model` | 预训练权重 | `yolo11n*.pt` |
| `--epochs` | 训练轮次 | 100 |
| `--imgsz` | 输入尺寸 | 640（detect/seg/pose/obb）/ 224（classify） |
| `--batch` | Batch size | 16（classify 为 32） |
| `--project` | 输出根目录 | `weights/<task>` |
| `--name` | 实验子目录名 | `yolo11n`（detect）/ `yolo11n-seg`（segment）/ `yolo11n-pose`（pose）/ `yolo11n_cls`（classify）/ `yolo11n_obb`（obb） |
| `--device` | 设备（空=自动） | 自动探测 GPU → CPU |

**命名规范**：新增脚本以 `动词_任务.py` 格式命名，小写下划线。`_device.py` 开头下划线表示内部公共模块。

---

### `tools/` — 开发工具链

```
tools/
├── verify_env.py         # 全流程功能验证（训练→推理→ONNX导出 ~17s）
├── audit_workspace.py    # 7 维工作区完整性审计（依赖/文件/AST/Git/YAML）
├── to_kmodel.py          # nncase PTQ 量化：ONNX → .kmodel（Windows原生 / Linux / WSL2 均可）
├── generate_deploy_pack.py # 一键打包 K230 板端部署文件（kmodel+labels+脚本）
└── nncase_kpu-2.11.0-py2.py3-none-win_amd64.whl  # 预置 Windows 离线 wheel
```

**用法顺序**：开发时先跑 `audit_workspace.py` 检查，改动后跑 `verify_env.py` 验证，最后 `to_kmodel.py` + `generate_deploy_pack.py` 打包部署。

---

### `templates/` — 板端代码模板

```
templates/
├── canmv_k230_demo.py         # CanMV MicroPython 推理模板（复制改 MODEL_PATH 即用）
├── canmv_k230_web_streamer.py # MJPEG Web 视频流（Type-C / RJ45 直连局域网）
└── k230_cpp_runner.sh         # Linux C++ yolo.elf 启动脚本（改参数直接用）
```

**使用方式**：复制到 SD 卡 `/sharefs/` 目录，修改顶部常量（`MODEL_PATH` / `INPUT_W` / `INPUT_H` 等），**不在此处直接编辑**——保持模板干净可复用。

---

### `data/` — 静态标签与辅助文件

```
data/
└── coco_labels.txt       # COCO 80 类标签名（部署包 labels.txt 来源）
```

**规范**：存放随工程版本管理的**静态小文件**（标签、标注辅助、schema 等）。训练数据集放 `datasets/`，不入此目录。

---

### `datasets/` — 用户数据集（不入 Git）

```
datasets/
├── .gitkeep              # 占位（保持目录入库）
├── calib/                # PTQ 校准图片（20~100 张，供 to_kmodel.py 使用）
└── <你的数据集>/          # 自定义数据集（YOLO 格式）
    ├── images/
    │   ├── train/
    │   └── val/
    └── labels/
        ├── train/
        └── val/
```

**规范**：`*.pt / *.onnx / *.kmodel` 和数据集目录均被 `.gitignore` 排除，**不提交大文件**。  
`datasets/calib/` 已预置 20 张占位校准图，用自己的真实图片替换效果更好。

---

### `weights/` — 模型权重（不入 Git）

```
weights/
├── .gitkeep              # 占位
├── detect/<name>/weights/best.pt     # train_detect.py 输出
├── classify/<name>/weights/best.pt   # train_classify.py 输出
├── segment/<name>/weights/best.pt    # train_segment.py 输出
├── pose/<name>/weights/best.pt       # train_pose.py 输出
├── obb/<name>/weights/best.pt        # train_obb.py 输出
├── <name>.onnx           # export_onnx.py 输出
└── <name>.kmodel         # to_kmodel.py 输出
```

**规范**：训练脚本默认输出到 `weights/<任务>/` 子目录，保持任务隔离。

---

### `docs/` — 开发文档（8 篇）

| 文件 | 主题 |
| :--- | :--- |
| `official_sources_and_config_basis.md` | 全工程参数官方权威依据对照表 |
| `canaan_k230_official_guide.md` | 嘉楠 K230 官方资料深度整合 |
| `environment_verification.md` | 开发环境规范与检验指南 |
| `base_tasks_training_and_deploy.md` | 5 大基础任务自训练与部署 SOP |
| `self_training.md` | 自训练方法（标注→训练→调优→导出） |
| `k230_deploy.md` | K230 上板部署（nncase→kmodel→烧录） |
| `dotnet_and_web_streaming.md` | .NET 机制与 Web 实时视频流指南 |
| `models_overview.md` | 官方最佳模型清单与选型建议 |

**规范**：文档只在 `docs/` 内，不在根目录新增 `.md` 文件（`README.md`、`STRUCTURE.md`、`QUICKSTART.md` 除外）。

---

## 不允许的目录/文件

| 禁止项 | 原因 | 正确位置 |
| :--- | :--- | :--- |
| 根目录下的 `*.pt / *.onnx / *.kmodel` | 大文件，污染根目录 | `weights/` |
| `runs/` | Ultralytics 训练日志，已被 gitignore | 临时产物 |
| `dump/` | nncase IR/asm 调试文件 | 用 `--dump-dir` 手动指定 |
| `deploy_pack*/` | 部署包，已被 gitignore | 临时产物 |
| `__pycache__/` | Python 字节码缓存 | 已被 gitignore |
| `docs/` 外新增 `.md` | 文档碎片化 | `docs/` 内 |

---

## 新增脚本/工具 Checklist

新增文件时，按此 checklist 操作：

- [ ] 文件名符合命名规范（小写下划线）
- [ ] 放在正确目录（`scripts/` / `tools/` / `templates/`）
- [ ] 顶部有 docstring，含**用法示例**
- [ ] 若是训练脚本：使用 `_device.resolve_device` 处理设备，`--project` 指定输出目录
- [ ] 若是工具脚本：添加到 `tools/audit_workspace.py` 的 `expected` 文件列表
- [ ] 若是文档：添加到 `docs/` 并更新 `STRUCTURE.md` 文档列表
- [ ] 运行 `uv run python tools/audit_workspace.py` 验证通过
