#!/usr/bin/env python3
"""将训练好的 .pt 导出为 K230 友好的 ONNX。

关键点（务必遵守，否则 nncase 量化会失败）：
    1. dynamic=False  -> 固定输入尺寸（K230 推理时尺寸固定）
    2. simplify=True   -> 用 onnxsim 折叠冗余节点，提升 KPU 兼容性
    3. opset=13        -> nncase 对 opset 11~13 支持良好
    4. 输入尺寸与训练 imgsz 一致（推荐 320 或 640 的正方形）

示例：
    uv run python scripts/export_onnx.py --weights weights/detect/yolo11n/weights/best.pt --imgsz 640
    uv run python scripts/export_onnx.py --weights best.pt --imgsz 320 --out weights/best_320.onnx
"""
import argparse
import sys

from ultralytics import YOLO


def parse_args():
    p = argparse.ArgumentParser(description="Export .pt -> ONNX for K230")
    p.add_argument("--weights", required=True, help="best.pt 路径")
    p.add_argument("--imgsz", type=int, default=640, help="必须与训练/推理输入一致")
    p.add_argument("--opset", type=int, default=13)
    p.add_argument("--out", default=None, help="输出 onnx 路径（默认同目录）")
    p.add_argument("--task", default=None,
                   choices=["detect", "segment", "pose", "classify", "obb"])
    return p.parse_args()


def main():
    a = parse_args()
    model = YOLO(a.weights)
    kwargs = dict(format="onnx", imgsz=a.imgsz, dynamic=False,
                  simplify=True, opset=a.opset)
    if a.task:
        kwargs["task"] = a.task

    path = model.export(**kwargs)
    print(f"[INFO] ONNX 导出成功: {path}")

    target_path = path
    if a.out and a.out != path:
        # 用 copy2 而非 move：move 会触发 workbuddy 安全删除沙箱（回收站不可用）而失败
        import shutil
        shutil.copy2(path, a.out)
        target_path = a.out
        print(f"[INFO] 已复制到目标位置: {a.out}")

    # ONNX 结构与静态 Shape 校验 (针对 K230 部署)
    try:
        import onnx
        onnx_model = onnx.load(target_path)
        onnx.checker.check_model(onnx_model)
        input_tensor = onnx_model.graph.input[0]
        dim_values = [d.dim_value for d in input_tensor.type.tensor_type.shape.dim]
        print(f"[INFO] 模型输入维度: {dim_values}")
        has_dynamic = any(isinstance(v, str) or v <= 0 for v in dim_values)
        if not has_dynamic:
            print("[PASS] ✅ 动态轴校验通过：无动态 Shape，符合 K230 (KPU) 部署规范。")
        else:
            print(f"[WARNING] ⚠️ 检出动态 Shape: {dim_values}，nncase 编译量化可能失败。")
    except Exception as e:
        print(f"[NOTE] 跳过 ONNX 详细结构校验: {e}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
