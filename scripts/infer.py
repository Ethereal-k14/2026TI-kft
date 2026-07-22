#!/usr/bin/env python3
"""推理 / 识别：图片、视频、摄像头、目录，输出带标注的结果。

示例：
    uv run python scripts/infer.py --source test.jpg --weights weights/detect/yolo11n/weights/best.pt
    uv run python scripts/infer.py --source 0 --weights best.pt          # 摄像头
    uv run python scripts/infer.py --source clip.mp4 --weights best.pt --conf 0.4
"""
import argparse
import sys

from ultralytics import YOLO
from _device import resolve_device


def parse_args():
    p = argparse.ArgumentParser(description="YOLO inference / recognition")
    p.add_argument("--source", required=True, help="图片/视频路径、目录或摄像头索引(0)")
    p.add_argument("--weights", required=True, help="best.pt 或 .onnx")
    p.add_argument("--imgsz", type=int, default=640)
    p.add_argument("--conf", type=float, default=0.35, help="置信度阈值（K230 默认 0.35）")
    p.add_argument("--iou", type=float, default=0.65, help="NMS IOU 阈值（K230 默认 0.65）")
    p.add_argument("--device", default="")
    p.add_argument("--save", action="store_true", default=True)
    p.add_argument("--project", default="weights/predict")
    p.add_argument("--name", default="exp")
    p.add_argument("--task", default="detect",
                   choices=["detect", "segment", "pose", "classify", "obb"])
    return p.parse_args()


def main():
    a = parse_args()
    model = YOLO(a.weights)
    model.predict(
        source=a.source, imgsz=a.imgsz, conf=a.conf, iou=a.iou,
        device=resolve_device(a.device), save=a.save, project=a.project, name=a.name,
        task=a.task, stream=False, verbose=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
