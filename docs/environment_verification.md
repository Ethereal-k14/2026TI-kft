# K230 视觉开发工程·开发环境检验与快速开发指南

本文档详细说明面向 **嘉楠 K230 (KPU)** 视觉 AI 项目的环境规范、自动检验方法及全流程开发 Standard Operating Procedure (SOP)。

---

## 1. 环境规范要求

| 维度 | 标准配置 / 规范 | 必须遵守的原因 |
| :--- | :--- | :--- |
| **Python 版本** | `Python 3.10.x` (`>=3.10, <3.11`) | 官方 `nncase` / `nncase-kpu` 转换工具链只支持 Python 3.6~3.10 窗口。 |
| **框架支持** | `ultralytics >= 8.3.0` | 支持原生 YOLO11n / YOLOv8n / YOLOv5 模型训练与 ONNX 导出。 |
| **导出规范** | `dynamic=False` | K230 的 NPU (KPU) 硬件推理要求输入 Shape 为**完全静态**（非动态 Shape）。 |
| **ONNX Opset** | `opset=13` | `nncase` 模型算子解析器对 opset 11~13 的算子兼容性最佳。 |
| **模型简化** | `simplify=True` (`onnxsim`) | 折叠冗余常数节点，避免不支持的算子下发导致量化失败。 |

---

## 2. 一键自动检验工具 (`verify_env.py`)

工程内置了全自动环境与链路检测脚本 `tools/verify_env.py`。该工具会自动执行以下 5 个关键阶段的覆盖性测试：

1. **依赖与版本核对**：自动检查 Python 3.10 及 Torch, Ultralytics, ONNX, ONNXSim, OpenCV, NumPy 等关键依赖库。
2. **测试数据集生成**：动态构建最小测试图像与标准 YOLO 格式配置文件。
3. **训练管线验证**：使用 YOLO11n 执行极短 1-epoch 微调训练，确保数据流与模型梯度更新正常。
4. **推理与可视化**：验证模型的预测推理逻辑与可视结果保存。
5. **K230 规范导出与模型结构校验**：
   - 导出静态输入 ONNX 模型。
   - 验证模型无 `dynamic axis`（固定 `1x3x320x320` 或 `1x3x640x640`）。
   - 验证 `onnx.checker.check_model` 与 `onnxsim.simplify` 均能正常化简模型。

### 运行方式

```bash
# 在项目根目录下运行环境全项检验
uv run python tools/verify_env.py

# 或直接使用 .venv 中的 Python
.\.venv\Scripts\python.exe tools/verify_env.py

# （可选）若希望保留测试导出的示例模型与推理图片供检查：
.\.venv\Scripts\python.exe tools/verify_env.py --keep-artifacts
```

---

## 2.1 工作区完整性审计工具 (`audit_workspace.py`)

除了管线功能检验，工程还内置了 7 维工作区完整性审计脚本 `tools/audit_workspace.py`，覆盖：

1. Python 版本与虚拟环境
2. 核心依赖包版本（13 个）
3. 项目文件完整性（36 个关键文件）
4. 全部源码 AST 编译检查
5. Git 仓库与远程配置
6. YAML 配置文件语法校验
7. 核心库运行时 Smoke Test

```bash
uv run python tools/audit_workspace.py
```

---

## 3. K230 视觉 AI 快速开发 S.O.P

标准开发与部署分为 **Windows 侧训练** 与 **Linux 侧模型转换/上板** 两大部分：

```
[Windows 侧]
数据标注 (YOLO 格式) ➔ 训练 YOLO11n (PyTorch) ➔ 验证推理 ➔ 导出 ONNX (opset=13, dynamic=False)
                                                                 │
                                                                 ▼
[Linux / WSL2 / Docker 侧]
将 ONNX 拷至 Linux ➔ 安装对应版本的 nncase ➔ 运行 tools/to_kmodel.py (PTQ量化) ➔ 生成 .kmodel
                                                                 │
                                                                 ▼
[K230 板卡侧]
拷贝 .kmodel 到 SD 卡 sharefs ➔ 执行 ./yolo.elf 启动摄像头实时推理
```

### Step 1: 训练模型 (Windows)
```bash
# 使用工程自带的示例配置开始训练（自定义数据集请参考 configs/ 下的 YAML 模板新建配置文件）
uv run python scripts/train_detect.py --data configs/coco128.yaml --model yolo11n.pt --epochs 100 --imgsz 320
```

### Step 2: 导出部署用 ONNX (Windows)
```bash
uv run python scripts/export_onnx.py --weights weights/detect/yolo11n/weights/best.pt --imgsz 320 --out weights/best_320.onnx
```

### Step 3: 转为 K230 专用的 .kmodel (Linux / WSL2)
```bash
python tools/to_kmodel.py --model weights/best_320.onnx --dataset datasets/calib --input-size 320 320 --output best.kmodel
```

---

## 4. 常见问题排查

- **Q: 为什么不能在 Windows 上直接转 `.kmodel`？**
  - A: 官方 `nncase-kpu` 量化编译插件依赖 dotnet 7.0 及 Linux 平台原生 C++ 动态库，在 Windows 上极易产生依赖缺失，强烈推荐在 **Linux / WSL2** 或官方 Docker (`ghcr.io/kendryte/k230_sdk`) 中转换。
- **Q: K230 上板推理报 Shape mismatch 错误？**
  - A: 导出 ONNX 时的 `--imgsz`、`to_kmodel.py` 的 `--input-size` 与开发板运行软件 `./yolo.elf` 的 `-ai_frame_width / -ai_frame_height` 三者必须完全相等。推荐 320×320（低开销首选）或 640×640（高精度）。
