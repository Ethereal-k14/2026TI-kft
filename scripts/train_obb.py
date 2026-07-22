#!/usr/bin/env python3
"""YOLO11n 旋转框检测 (Oriented Bounding Box, OBB) 自训练脚本

适用于 K230 部署的旋转目标/角度识别场景：
    PCB 零件方向识别、倾斜文本行检测、航拍与旋转物体识别。

示例用法：
    uv run python scripts/train_obb.py --data configs/obb_sample.yaml --epochs 100 --imgsz 320
    uv run python scripts/train_obb.py --data configs/obb_sample.yaml --model yolo11n-obb.pt --batch 16
"""
import argparse
import sys
from ultralytics import YOLO
from _device import resolve_device


def parse_args():
    p = argparse.ArgumentParser(description="YOLO11n-obb 旋转目标检测自训练 (K230)")
    p.add_argument("--data", required=True, help="旋转框数据集 yaml 路径")
    p.add_argument("--model", default="yolo11n-obb.pt", help="预训练模型，默认 yolo11n-obb.pt")
    p.add_argument("--epochs", type=int, default=100)
    p.add_argument("--imgsz", type=int, default=320, help="推荐 320 或 640")
    p.add_argument("--batch", type=int, default=16)
    p.add_argument("--workers", type=int, default=4)
    p.add_argument("--device", default="", help="cuda 编号 (如 0) 或 cpu")
    p.add_argument("--project", default="weights/obb", help="训练产出目录")
    p.add_argument("--name", default="yolo11n_obb", help="实验名称")
    return p.parse_args()


def main():
    a = parse_args()
    model = YOLO(a.model)

    kwargs = dict(
        data=a.data,
        epochs=a.epochs,
        imgsz=a.imgsz,
        batch=a.batch,
        workers=a.workers,
        project=a.project,
        name=a.name,
        exist_ok=True,
    )
    kwargs["device"] = resolve_device(a.device)

    print(f"[INFO] 开始旋转框 (OBB) 目标检测模型训练...")
    print(f"[INFO] 预训练模型: {a.model} | 配置: {a.data} | imgsz: {a.imgsz}")

    results = model.train(**kwargs)

    best_path = f"{a.project}/{a.name}/weights/best.pt"
    print("\n" + "=" * 50)
    print(f"🎉 训练完成！最佳模型参数已保存至: {best_path}")
    print(f"👉 下一步请运行 ONNX 导出命令:")
    print(f"   uv run python scripts/export_onnx.py --weights {best_path} --imgsz {a.imgsz} --task obb")
    print("=" * 50)
    return 0


if __name__ == "__main__":
    sys.exit(main())
