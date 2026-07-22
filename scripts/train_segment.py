#!/usr/bin/env python3
"""YOLO11n 实例分割训练，适用于需要像素级Mask的场景（如工业缺陷、医学）。

示例：
    uv run python scripts/train_segment.py --data configs/seg.yaml --epochs 100 --imgsz 640
"""
import argparse
import sys

from ultralytics import YOLO
from _device import resolve_device


def parse_args():
    p = argparse.ArgumentParser(description="Train YOLO11n segmentation model for K230")
    p.add_argument("--data", required=True, help="segment 数据集 YAML 路径")
    p.add_argument("--model", default="yolo11n-seg.pt")
    p.add_argument("--epochs", type=int, default=100)
    p.add_argument("--imgsz", type=int, default=640)
    p.add_argument("--batch", type=int, default=16)
    p.add_argument("--device", default="")
    p.add_argument("--project", default="weights/segment", help="输出目录")
    p.add_argument("--name", default="yolo11n-seg")
    p.add_argument("--workers", type=int, default=4)
    p.add_argument("--patience", type=int, default=30)
    return p.parse_args()


def main():
    a = parse_args()
    model = YOLO(a.model)
    model.train(
        data=a.data, epochs=a.epochs, imgsz=a.imgsz, batch=a.batch,
        device=resolve_device(a.device), workers=a.workers, patience=a.patience,
        project=a.project, name=a.name, rect=False, cos_lr=True, amp=True,
    )

    best = f"{a.project}/{a.name}/weights/best.pt"
    print(f"[INFO] 训练完成，尝试导出 ONNX: {best}")
    try:
        m = YOLO(best)
        m.export(format="onnx", imgsz=a.imgsz, dynamic=False, simplify=True, opset=13)
    except Exception as e:  # noqa: BLE001
        print(f"[WARN] 自动导出 ONNX 失败，可稍后手动执行 scripts/export_onnx.py：{e}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
