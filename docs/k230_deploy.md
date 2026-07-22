# K230 上板部署（nncase → kmodel）

K230 的 NPU（KPU）只认 `.kmodel`。部署链路：**PyTorch(.pt) → ONNX → nncase → .kmodel**。

> ⚠️ **版本铁律**：`nncase` / `nncase-kpu` 版本必须与 K230 **烧录镜像**的版本严格一致，
> 否则生成的 kmodel 板子跑不起来。先确认镜像版本，再装对应 nncase。

---

## 1. 转换能在哪跑？（重要更正）

**结论：Windows 原生就能跑转换，不必非得 Linux/WSL。**

- `nncase` 主程序可在 Windows 直接 `pip install`（PyPI 有 win 可用 wheel，已纳入本工程 `uv` 依赖）。
- `nncase-kpu` 插件 **不在 PyPI**，需从 GitHub Release 手动下载对应版本的
  `nncase_kpu-<版本>-py2.py3-none-win_amd64.whl` 离线 `pip install`。
- 官方文档写 nncase 依赖 **dotnet-7.0**，但实测本机装有 **.NET 8.0.29** 即可通过
  **向前兼容（roll-forward）** 运行 net7.0 的 nncase —— **无需额外安装 .NET 7**。
  本工程的 `tools/to_kmodel.py` 已自动设置 `DOTNET_ROLL_FORWARD=Major` 并指向 x64 dotnet，
  直接 `uv run python tools/to_kmodel.py ...` 即可在 Windows 上转换。

> ✅ **实测通过（2026-07-22）**：本机 .NET 8.0.29 + nncase 2.11.0 + nncase-kpu 2.11.0(win_amd64)，
> 已成功将 `weights/yolo11n_320.onnx`（YOLO11n, 320×320, uint8 PTQ, Kld 校准）转为
> `weights/yolo11n_320.kmodel`（约 3.0MB，文件头 `LDMK` 魔术字校验通过）。
> 校准集用 20 张合成图仅验证流程；**真实部署请替换为你的真实场景图**以保证量化精度。

可选的三条路径（任选其一）：
1. **Windows 原生（推荐，零额外安装）**：用本机 .NET 8 + roll-forward，装 nncase + 手动 kpu wheel。
2. **WSL2 + Ubuntu / Docker**：官方推荐，需另装 dotnet-7（`sudo apt install dotnet-sdk-7.0`）。
3. **官方 k230_sdk 镜像**：`ghcr.io/kendryte/k230_sdk`（内置 Ubuntu 20.04 + Python 3.8 + dotnet-7）。

```bash
# 路径 3 示例
docker pull ghcr.io/kendryte/k230_sdk
docker run -it --rm -v "$PWD":/mnt -w /mnt ghcr.io/kendryte/k230_sdk /bin/bash
```

---

## 2. 转换依赖安装

### Windows 原生
```bash
# nncase 已由 uv 管理（uv sync 时已装好）。若需手动：
uv pip install nncase==<与K230镜像一致版本>
# nncase-kpu 不在 PyPI，去 GitHub Release 下载同版本 win_amd64 wheel 后离线安装：
# 注：本工程已在 tools/ 下预置了 nncase_kpu-2.11.0 的 Windows wheel，可直接安装：
uv pip install tools/nncase_kpu-2.11.0-py2.py3-none-win_amd64.whl
# .NET：本机已装 8.0，经 to_kmodel.py 的 roll-forward 自动兼容，无需装 7
```

### Linux / WSL2 / Docker
```bash
sudo apt-get update && sudo apt-get install -y dotnet-sdk-7.0
python -m pip install --upgrade pip
# 版本以你 K230 镜像为准，示例 2.9.0
python -m pip install nncase==2.9.0 nncase-kpu==2.9.0
```

---

## 3. ONNX → kmodel

### Windows 原生（本工程默认）
```bash
uv run python tools/to_kmodel.py \
    --model weights/best.onnx \
    --dataset datasets/calib \
    --input-size 320 320 \
    --output weights/best.kmodel
```

### Linux / WSL2（若走该路径）
把 Windows 导出的 `best.onnx` 与校准图片目录拷到 Linux 环境，执行同样命令：

```bash
python tools/to_kmodel.py \
    --model best.onnx \
    --dataset datasets/calib \
    --input-size 320 320 \
    --output best.kmodel
```

参数说明：
- `--dataset`：20~100 张代表性图片（覆盖真实场景，影响量化精度）。
- `--input-size`：必须与导出 ONNX 时 `imgsz` 一致。
- `--quant-type`：`uint8`（默认，推荐）/ `int8`（精度更高但更易掉点）。
- `--preprocess`：
  - `norm255`（默认）：输入 0~255，kmodel 直接吃 uint8 摄像头帧，省前处理。
  - `norm01`：输入已归一化到 0~1。
- `--calib-method`：`Kld`（默认，KL散度，推荐）/ `NoClip`。注意 nncase 2.x 的 `use_ptq` 仅支持这两项**字符串**（大小写敏感，勿写 `KLD`/`ACIQ`/`Random`，否则报 Unsupported Calibrate Method）。

生成 `best.kmodel` 后，可用 nncase 自带模拟器先在 PC 验证推理结果再上板。

---

## 4. 上板运行（以 K230 YOLO 示例为例）

官方 YOLO 示例支持 `yolov5 / yolov8 / yolo11 / yolo26` 与
`detect / segment / obb / pose / classify`，运行参数（节选）：

| 参数 | 默认 | 说明 |
|------|------|------|
| `-model_type` | yolov8 | 改为 `yolo11` |
| `-task_type` | detect | detect/segment/obb/pose/classify |
| `-ai_frame_width` | 640 | AI 帧宽 |
| `-ai_frame_height` | 360 | AI 帧高 |
| `-kmodel_path` | yolov8n.kmodel | 你的 `best.kmodel` |
| `-labels_txt_filepath` | coco_labels.txt | 类别名（每行一个） |
| `-conf_thres` | 0.35 | 置信度阈值 |
| `-nms_thres` | 0.65 | NMS 阈值 |

视频推理：
```bash
./yolo.elf -model_type yolo11 -task_type detect -task_mode video \
    -kmodel_path best.kmodel -labels_txt_filepath coco_labels.txt \
    -conf_thres 0.35 -nms_thres 0.65
```

> **调优提示**：把 AI 帧分辨率与模型输入分辨率设为相同值，推理速度更优。
> 例：模型 320×320 输入时，设置 `-ai_frame_width 320 -ai_frame_height 320`。

---

## 5. 烧录与文件摆放

- 镜像烧录用 **Rufus**（Windows）或 `dd`（Linux）写入 SD 卡。
- 双系统镜像：把 `best.kmodel`、标签文件、`yolo.elf` 放到 SD 卡 **`sharefs`**（大小核共享）。
- 单系统镜像：放到 **`sdcard`** 目录。
- 串口调试用 MobaXterm（115200），小核 `root` 登录，大核 `q` 退出自启程序。

---

## 6. 校验清单

- [ ] nncase 版本 == K230 镜像版本
- [ ] ONNX `dynamic=False`、固定输入、opset 13
- [ ] 校准集覆盖真实场景
- [ ] PC 模拟器推理结果与 PyTorch 一致
- [ ] kmodel 输入尺寸 == AI 帧尺寸（或按需设置）
- [ ] 类别标签文件行数与模型输出一致
