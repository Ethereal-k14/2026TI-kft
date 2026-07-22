#!/usr/bin/env python3
"""多目标追踪（MOT）：在检测基础上做跨帧关联。

Ultralytics 内置两种追踪器：
    - botsort  （默认，精度高，支持 ReID 外观特征）
    - bytetrack （轻量、速度快，适合算力受限场景）

示例：
    uv run python scripts/track.py --source video.mp4 --weights best.pt --tracker botsort
    uv run python scripts/track.py --source 0 --weights best.pt --tracker bytetrack
"""
import argparse
import sys

from ultralytics import YOLO
from _device import resolve_device


def parse_args():
    p = argparse.ArgumentParser(description="Multi-object tracking on K230-trained model")
    p.add_argument("--source", required=True, help="视频路径或摄像头索引(0)")
    p.add_argument("--weights", required=True, help="best.pt（追踪基于检测权重）")
    p.add_argument("--tracker", default="botsort",
                   choices=["botsort", "bytetrack"], help="追踪器")
    p.add_argument("--imgsz", type=int, default=640)
    p.add_argument("--conf", type=float, default=0.35)
    p.add_argument("--device", default="")
    p.add_argument("--save", action="store_true", default=True)
    p.add_argument("--project", default="weights/track")
    p.add_argument("--name", default="exp")
    return p.parse_args()


def main():
    a = parse_args()
    model = YOLO(a.weights)
    model.track(
        source=a.source, tracker=f"{a.tracker}.yaml", imgsz=a.imgsz,
        conf=a.conf, device=resolve_device(a.device), save=a.save,
        project=a.project, name=a.name, verbose=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
