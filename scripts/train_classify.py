#!/usr/bin/env python3
"""YOLO11n 图像分类 (Classification) 自训练脚本

针对 K230 部署优化：
    1. 默认推荐轻量级 yolo11n-cls.pt（参数量仅 ~1.6M，算力开销极小）
    2. 输入尺寸推荐 imgsz=224 或 320（需与后续 ONNX 导出及 kmodel 转换一致）

数据集结构要求：
    datasets/my_cls/
    ├── train/
    │   ├── class1/  img01.jpg ...
    │   └── class2/  img02.jpg ...
    └── val/
        ├── class1/  ...
        └── class2/  ...

示例用法：
    uv run python scripts/train_classify.py --data datasets/my_cls --epochs 50 --imgsz 224
    uv run python scripts/train_classify.py --data datasets/my_cls --model yolo11n-cls.pt --batch 32
"""
import argparse
import sys
from ultralytics import YOLO
from _device import resolve_device


def parse_args():
    p = argparse.ArgumentParser(description="YOLO11n-cls 图像分类自训练 (K230)")
    p.add_argument("--data", required=True, help="分类数据集根目录路径 (含 train/val 文件夹)")
    p.add_argument("--model", default="yolo11n-cls.pt", help="预训练模型，默认 yolo11n-cls.pt")
    p.add_argument("--epochs", type=int, default=50)
    p.add_argument("--imgsz", type=int, default=224, help="推荐 224 或 320")
    p.add_argument("--batch", type=int, default=32)
    p.add_argument("--workers", type=int, default=4)
    p.add_argument("--device", default="", help="cuda 编号 (如 0) 或 cpu")
    p.add_argument("--project", default="weights/classify", help="训练产出目录")
    p.add_argument("--name", default="yolo11n_cls", help="实验名称")
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

    print(f"[INFO] 开始图像分类模型训练...")
    print(f"[INFO] 预训练模型: {a.model} | 数据集: {a.data} | imgsz: {a.imgsz}")

    results = model.train(**kwargs)

    best_path = f"{a.project}/{a.name}/weights/best.pt"
    print("\n" + "=" * 50)
    print(f"🎉 训练完成！最佳模型参数已保存至: {best_path}")
    print(f"[INFO] 尝试自动导出 ONNX: {best_path}")
    try:
        m = YOLO(best_path)
        m.export(format="onnx", imgsz=a.imgsz, dynamic=False, simplify=True, opset=13)
    except Exception as e:  # noqa: BLE001
        print(f"[WARN] 自动导出 ONNX 失败，可稍后手动执行 scripts/export_onnx.py：{e}")
    print("=" * 50)
    return 0


if __name__ == "__main__":
    sys.exit(main())
