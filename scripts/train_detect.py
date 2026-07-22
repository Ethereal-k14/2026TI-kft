#!/usr/bin/env python3
"""YOLO11n 目标检测训练（K230 友好）。

默认使用官方 YOLO11n 预训练权重做迁移学习；数据集使用 Ultralytics YAML 格式：
    path: /abs/path/to/dataset
    train: images/train
    val:   images/val
    names:
      0: person
      1: car
      ...

示例：
    uv run python scripts/train_detect.py --data configs/coco128.yaml --epochs 50 --imgsz 640
    uv run python scripts/train_detect.py --data mydata.yaml --model yolo11s.pt --imgsz 320
"""
import argparse
import sys

from ultralytics import YOLO
from _device import resolve_device


def parse_args():
    p = argparse.ArgumentParser(description="Train YOLO11n detector for K230")
    p.add_argument("--data", required=True, help="数据集 YAML 路径")
    p.add_argument("--model", default="yolo11n.pt", help="起始权重，默认 yolo11n.pt")
    p.add_argument("--epochs", type=int, default=100)
    p.add_argument("--imgsz", type=int, default=640,
                   help="K230 常用 320 / 640；与 AI 帧分辨率一致更省算力")
    p.add_argument("--batch", type=int, default=16)
    p.add_argument("--device", default="", help="cuda device, i.e. 0 or cpu")
    p.add_argument("--project", default="weights/detect", help="输出目录")
    p.add_argument("--name", default="yolo11n", help="实验名")
    p.add_argument("--workers", type=int, default=4)
    p.add_argument("--patience", type=int, default=30, help="早停耐心轮数")
    p.add_argument("--optimizer", default="auto")
    p.add_argument("--close-mosaic", type=int, default=10,
                   help="最后 N 轮关闭 mosaic，提升最终精度")
    return p.parse_args()


def main():
    a = parse_args()
    model = YOLO(a.model)
    model.train(
        data=a.data,
        epochs=a.epochs,
        imgsz=a.imgsz,
        batch=a.batch,
        device=resolve_device(a.device),
        workers=a.workers,
        optimizer=a.optimizer,
        patience=a.patience,
        close_mosaic=a.close_mosaic,
        project=a.project,
        name=a.name,
        # K230 部署建议：保持固定正方形输入，便于后续 ONNX 导出与量化
        rect=False,
        cos_lr=True,
        amp=True,
        verbose=True,
    )
    # 训练结束后自动导出 ONNX（固定输入，关闭 dynamic，便于 nncase 量化）
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
