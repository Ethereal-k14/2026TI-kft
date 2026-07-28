#!/usr/bin/env python3
"""nncase: ONNX -> .kmodel（K230 部署转换）

前置依赖（详见 requirements-convert.txt / docs/k230_deploy.md）：
    pip install nncase==<与K230镜像一致的版本>
    # nncase-kpu 插件不在 PyPI，需从 GitHub Release 手动下载对应版本的
    #   nncase_kpu-<版本>-py2.py3-none-win_amd64.whl 离线安装（Windows 原生路径）
    nncase / nncase-kpu 版本必须与 K230 烧录镜像严格一致，否则 kmodel 无法运行。

运行平台：
    * Linux / WSL2 / Docker (ghcr.io/kendryte/k230_sdk) 均可。
    * Windows 原生也可：nncase 官方要求 .NET 7，但本机已装 .NET 8.0，
      本脚本会自动设置 DOTNET_ROLL_FORWARD=Major 借 .NET 8 向前兼容运行，
      无需额外安装 .NET 7（见下方环境注入）。

示例：
    uv run python tools/to_kmodel.py \
        --model weights/best.onnx \
        --dataset datasets/calib \
        --input-size 320 320 \
        --output weights/best.kmodel

校准集（--dataset）放 20~100 张代表性图片即可，需覆盖真实场景分布。
"""
import argparse
import os
import sys

# ---------------------------------------------------------------------------
# Windows 原生转换支持（免装 .NET 7）：
# 本机装有 .NET 8.0.x；nncase 官方要求 .NET 7，但 .NET 支持向前兼容——
# 设置 DOTNET_ROLL_FORWARD=Major 即可让 net7.0 的 nncase 跑在 .NET 8 运行时上。
# 同时把 PATH/DOTNET_ROOT 指向 x64 的 dotnet（PATH 里可能先命中坏的 x86 桩）。
# 必须在 import nncase 之前完成。
# ---------------------------------------------------------------------------
if sys.platform == "win32":
    _dotnet_x64 = r"C:\Program Files\dotnet"
    _in_path = "dotnet" in os.environ.get("PATH", "").lower()
    _in_root = "DOTNET_ROOT" in os.environ
    if not _in_path and not _in_root:
        if os.path.isdir(_dotnet_x64):
            if _dotnet_x64 not in os.environ.get("PATH", ""):
                os.environ["PATH"] = _dotnet_x64 + os.pathsep + os.environ.get("PATH", "")
            os.environ.setdefault("DOTNET_ROOT", _dotnet_x64)
    # 本机装有 .NET 8.0.x；nncase 官方要求 .NET 7，但 .NET 向前兼容——
    # 设置 DOTNET_ROLL_FORWARD=Major 即可让 net7.0 的 nncase 跑在 .NET 8 运行时上。
    os.environ["DOTNET_ROLL_FORWARD"] = "Major"
    # 自动定位 nncase kpu 插件目录（nncase/modules/kpu），注入 NNCASE_PLUGIN_PATH，
    # 否则 nncase 找不到 kpu 后端，compile target='k230' 会失败。必须在 import nncase 前完成。
    try:
        import importlib.util as _ilu
        _spec = _ilu.find_spec("nncase")
        if _spec is not None and getattr(_spec, "submodule_search_locations", None):
            _nncase_dir = _spec.submodule_search_locations[0]
            _kpu_dir = os.path.join(_nncase_dir, "modules", "kpu")
            if os.path.isdir(_kpu_dir):
                os.environ["NNCASE_PLUGIN_PATH"] = _kpu_dir
    except Exception:
        pass

import numpy as np
from PIL import Image

try:
    import nncase
except ImportError:
    nncase = None
    # 当未安装 nncase 时，提示友情指南
    if "--help" not in sys.argv and "-h" not in sys.argv:
        print("[WARNING] 未检测到 nncase 模块。请先安装：")
        print("  uv pip install nncase==<与K230镜像一致版本>")
        print("  并手动下载同版本 nncase_kpu 的 win_amd64 wheel 离线安装。")
        print("  Windows 原生需 .NET 运行时（本机 .NET 8 经向前兼容即可，无需装 7）。")


def read_model_file(model_file: str) -> bytes:
    with open(model_file, "rb") as f:
        return f.read()


def preprocess(image_path: str, width: int, height: int, mode: str):
    """与 K230 KPU AI2D 硬件与 Ultralytics 推理一致的预处理。

    mode:
      norm255 -> AI2D 预处理模式：返回 NCHW (1, 3, H, W) np.uint8 (0~255)
      norm01  -> 纯软件前处理模式：返回 NCHW (1, 3, H, W) np.float32 (0.0~1.0)
    """
    img = Image.open(image_path).convert("RGB").resize((width, height), Image.BILINEAR)
    arr = np.asarray(img)  # HWC uint8 (0~255)

    if mode == "norm255":
        # 统一转为 NCHW (1, 3, H, W) uint8
        return arr.transpose(2, 0, 1)[None, ...].astype(np.uint8)
    else:
        # HWC -> CHW -> NCHW (1, 3, H, W) float32, 归一化 0~1
        arr_f32 = arr.astype(np.float32) / 255.0
        return arr_f32.transpose(2, 0, 1)[None, ...]


def collect_calib_images(dataset_dir: str, limit: int = 100):
    if not os.path.exists(dataset_dir) or not os.path.isdir(dataset_dir):
        return []
    exts = (".jpg", ".jpeg", ".png", ".bmp")
    files = []
    for root, _, fs in os.walk(dataset_dir):
        for f in fs:
            if f.lower().endswith(exts):
                files.append(os.path.join(root, f))
                if len(files) >= limit:
                    break
        if len(files) >= limit:
            break
    return files


def compile_kmodel(args):
    if nncase is None:
        print("[ERR] 当前环境缺失 nncase 库。请参考 requirements-convert.txt 在 Linux/WSL2/Docker 环境中运行本转换脚本。")
        return 1

    w, h = args.input_size
    mode = args.preprocess
    mean = [0.0, 0.0, 0.0]
    std = [255.0, 255.0, 255.0] if mode == "norm255" else [1.0, 1.0, 1.0]

    # 1) 编译选项 (严格对齐 K230 KPU 官方 AI2D 规范)
    compile_options = nncase.CompileOptions()
    compile_options.target = "k230"
    compile_options.quant_type = args.quant_type          # uint8 / int8
    
    if mode == "norm255":
        compile_options.preprocess = True
        compile_options.input_type = "uint8"
        compile_options.input_shape = [1, 3, h, w]
        compile_options.input_range = [0, 255]
        compile_options.input_layout = "NCHW"
        if hasattr(compile_options, "mean"):
            compile_options.mean = mean
        if hasattr(compile_options, "std"):
            compile_options.std = std
    else:
        compile_options.preprocess = False
        compile_options.input_type = "float32"

    compile_options.output_type = "float32"

    if args.dump_dir:
        compile_options.dump_ir = True
        compile_options.dump_asm = True
        compile_options.dump_dir = args.dump_dir
    else:
        compile_options.dump_ir = False
        compile_options.dump_asm = False

    compiler = nncase.Compiler(compile_options)

    # 2) 导入 ONNX
    import_options = nncase.ImportOptions()
    model_data = read_model_file(args.model)
    compiler.import_onnx(model_data, import_options)

    # 3) 准备校准数据
    calib_files = collect_calib_images(args.dataset, limit=args.calib_count)
    if not calib_files:
        raise SystemExit(f"[ERR] 校准集为空：{args.dataset}")
    calib_data = [preprocess(f, w, h, mode) for f in calib_files]

    # 4) PTQ 量化配置（严格遵循 nncase 2.x 官方 API 规范）
    ptq_options = nncase.PTQTensorOptions()
    ptq_options.samples_count = len(calib_data)
    if hasattr(ptq_options, "input_mean"):
        ptq_options.input_mean = mean
    if hasattr(ptq_options, "input_std"):
        ptq_options.input_std = std
    ptq_options.quant_type = args.quant_type          # "uint8" / "int8"
    if hasattr(ptq_options, "w_quant_type"):
        ptq_options.w_quant_type = args.quant_type
    if hasattr(ptq_options, "a_quant_type"):
        ptq_options.a_quant_type = args.quant_type
    ptq_options.calibrate_method = args.calib_method  # 字符串: "Kld" / "NoClip"
    if hasattr(ptq_options, "finetune_weights_method"):
        ptq_options.finetune_weights_method = "NoFineTuneWeights"
    
    cali_dtype = np.uint8 if mode == "norm255" else np.float32
    formatted_data = [d.astype(cali_dtype) for d in calib_data]

    if hasattr(ptq_options, "set_tensor_data"):
        try:
            ptq_options.set_tensor_data([[d] for d in formatted_data])
        except Exception:
            ptq_options.cali_data = [nncase.RuntimeTensor.from_numpy(d) for d in formatted_data]
    else:
        ptq_options.cali_data = [nncase.RuntimeTensor.from_numpy(d) for d in formatted_data]

    compiler.use_ptq(ptq_options)

    # 5) 编译并写出 kmodel
    compiler.compile()
    kmodel = compiler.gencode_tobytes()
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(kmodel)
    print(f"[OK] kmodel 已生成：{args.output}  ({len(kmodel)/1024:.1f} KB)")


def parse_args():
    p = argparse.ArgumentParser(description="ONNX -> kmodel via nncase (K230)")
    p.add_argument("--model", required=True, help="输入 ONNX 路径")
    p.add_argument("--dataset", required=True, help="校准图片目录")
    p.add_argument("--input-size", type=int, nargs=2, default=[320, 320],
                   metavar=("W", "H"), help="模型输入尺寸，需与导出 ONNX 一致")
    p.add_argument("--output", default="weights/best.kmodel")
    p.add_argument("--quant-type", default="uint8", choices=["uint8", "int8"])
    p.add_argument("--preprocess", default="norm255", choices=["norm255", "norm01"])
    p.add_argument("--calib-count", type=int, default=100, help="最多使用的校准图片数")
    p.add_argument("--calib-method", default="Kld",
                   choices=["Kld", "NoClip"],
                   help="Kld=KL散度(默认,推荐); NoClip")
    p.add_argument("--dump-dir", default=None, help="IR/asm 调试信息保存目录（默认不生成）")
    return p.parse_args()


if __name__ == "__main__":
    sys.exit(compile_kmodel(parse_args()))
