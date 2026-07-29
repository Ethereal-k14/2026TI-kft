# 嘉楠 K230 (Kendryte) 官方资料深度整合与开发全景指南

本指南深度整合了 **嘉楠勘智 (Canaan Kendryte) K230 官方文档**、`K230_sdk`、`nncase` 工具链与 `CanMV` 平台的最新部署实操与避坑经验。

---

## 1. K230 硬件与软件架构一览

### 1.1 芯片与 NPU (KPU) 特性
- **处理器架构**：双核 64-bit RISC-V 处理器（大核 1.6GHz 运行 Linux，小核 800MHz 运行 RT-Smart RTOS）。
- **KPU (NPU) 算力**：高效率 4.0 TOPS 算力（INT8），专为轻量级 YOLO 系列（YOLOv5/v8/11/26）硬件加速优化。
- **共享文件系统 (`sharefs`)**：大小核共享内存文件系统。部署模型、标签与可执行程序放置在 SD 卡 `sharefs` 路径即可实现双核高效协同访问。

### 1.2 开发双轨路线对比

| 开发路线 | 语言/框架 | 图像采集与显示 | 适用场景 | 代表例程 |
| :--- | :--- | :--- | :--- | :--- |
| **CanMV 模式** | MicroPython | `media.camera` / `media.display` | 快速原型开发、教学科研、Python 习惯开发者 | `templates/canmv_k230_demo.py` |
| **Linux C++ 模式** | C++ (`yolo.elf`) | `mpp` (Media Process Platform) | 极致性能要求、高帧率工业落地场景 | `templates/k230_cpp_runner.sh` |

---

## 2. 官方推荐模型选型与训练规范

### 2.1 模型选型推荐矩阵
K230 KPU 对 YOLO 系列模型的 Backbone 和 Head 算子有极其出色的融合优化：
- **首选模型**：**YOLO11n**（参数量 ~2.6M，精度/速度比达到最佳）。
- **轻量替换**：YOLOv8n（生态最成熟）、YOLOv5n（兼容性强）。
- **姿态与分割**：`yolo11n-pose`（人体关键点）及 `yolo11n-seg`（实例分割）。

### 2.2 输入分辨率设定（关键性能点）
K230 的视频输入前端 (VI) 与 AI 推理通道 (AI Frame) 存在最佳匹配关系：
- **低开销/实时性优先**：推荐设为 **320×320**，帧率高，NPU 占用低。
- **高精度优先**：推荐设为 **640×640**。
- **匹配原则**：模型训练 `imgsz`、导出 ONNX `imgsz`、`to_kmodel.py` `--input-size` 与板端 `yolo.elf` 的 `-ai_frame_width/-ai_frame_height` **四者必须严格一致**！

---

## 3. ONNX 导出与 nncase PTQ 量化权威指南

### 3.1 导出的三大铁律 (`scripts/export_onnx.py`)
1. **静态输入** (`dynamic=False`)：KPU 硬件算子调度器要求输入维度固定。
2. **算子集选择** (`opset=13`)：`nncase` 解析器对 opset 11~13 的转换与量化折叠支持最稳定。
3. **结构简化** (`simplify=True`)：必须通过 `onnxsim` 消除冗余节点。

### 3.2 nncase 核心 API 与预处理选项

`nncase` 支持在编译阶段将图像预处理逻辑编译进 `.kmodel` 内部：

```python
import nncase

compile_options = nncase.CompileOptions()
compile_options.target = "k230"
compile_options.preprocess = True
compile_options.input_type = "uint8"       # 摄像头硬件直接输入 uint8
compile_options.input_shape = [1, 3, 320, 320]
compile_options.input_range = [0, 255]
compile_options.output_type = "float32"
```

### 3.3 nncase PTQ (Post-Training Quantization) 参数详解 (`tools/to_kmodel.py`)

在 **Linux / WSL2 / Docker** 环境中转换 `.kmodel` 时：

```bash
python tools/to_kmodel.py \
    --model weights/best_320.onnx \
    --dataset datasets/calib \
    --input-size 320 320 \
    --quant-type uint8 \
    --preprocess norm255 \
    --calib-method Kld \
    --output weights/best_320.kmodel
```

#### 参数避坑说明：
- `--preprocess norm255`：图像输入像素保持 `0~255`（`input_mean=0, input_std=255`）。在此模式下，K230 摄像头 HW/Sensor 传入的 uint8 图像数据可以直接交给 KPU 处理，**省略 CPU 前处理开销**。
- `--calib-method Kld`：使用 KL 散度进行激活值量化阈值选择；若发现量化后精度有掉点，可切换为 `--calib-method NoClip` 进行对比实测。注意 nncase 2.x 的量化方法仅支持 `Kld` 和 `NoClip` 两个字符串（大小写敏感）。
- `--dataset`：提供 20 ~ 100 张覆盖真实部署场景（不同光照、背景）的校验图片。

---

## 4. 常见报错与排错指南 (Troubleshooting)

### 4.1 `KeyNotFoundException: The given key 'K230' was not present`
- **原因**：环境中只安装了 `nncase` 基础包，缺失了 `nncase-kpu` 扩展插件。
- **解法**：必须同时安装同版本的扩展插件：
  ```bash
  pip install nncase==2.9.0 nncase-kpu==2.9.0
  ```

### 4.2 `RuntimeError: Failed to get hostfxr path` 或 `.NET 7.0 Error`
- **原因**：`nncase` 编译后端依赖 .NET SDK 7.0 运行时环境。
- **解法**：在 Linux 系统中安装 .NET 7.0 运行时：
  ```bash
  sudo apt-get update && sudo apt-get install -y dotnet-sdk-7.0
  ```

### 4.3 板端报错 `Invalid kmodel` 或 `Handle is not initialized`
- **原因**：编译模型时使用的 `nncase` 版本与 K230 烧录固件/SDK 中内置的 `nncase_runtime` 版本不一致。
- **解法**：使用 `pip list | grep nncase` 检查 Linux 侧转换使用的版本，确保其与系统固件版本完全对应。

---

## 5. 板端部署一键打包流程

训练与导出完成后，可通过本工程的打包工具一键生成完整部署包：

```bash
uv run python tools/generate_deploy_pack.py \
    --model weights/best_320.kmodel \
    --data configs/coco128.yaml \
    --imgsz 320 \
    --output deploy_pack
```

产出的 `deploy_pack/` 目录中包含：
- `best.kmodel`：目标模型
- `labels.txt`：规范格式的类别文本
- `main.py`：CanMV MicroPython 运行代码
- `run.sh`：Linux C++ 命令启动 Shell

将该文件夹内所有文件拷贝至 SD 卡 `sharefs` 目录，即可快速启动验证！

---

## 6. 官方资料与社区资源索引

1. **嘉楠 K230 官方文档主页**：<https://www.kendryte.com/ai_docs/zh/main/>
2. **K230 应用开发指南 (YOLO 专题)**：<https://www.kendryte.com/k230_rtos/zh/main/app_develop_guide/ai/yolo.html>
3. **Kendryte K230 SDK GitHub 仓库**：<https://github.com/kendryte/k230_sdk>
4. **nncase 官方编译器仓库**：<https://github.com/kendryte/nncase>
5. **嘉楠勘智开发者社区**：<https://developer.canaan-creative.com/>
