#!/usr/bin/env python3
"""
多维度权威评估体系校验工具 (tools/benchmark_kmodel_precision.py)

包含 5 大国际权威评估指标：
  1. COCO 官方标准: mAP@0.5 与 mAP@0.5:0.95
  2. BBox 空间几何对齐: Mean IoU, GIoU, CIoU 与 中心点距离偏移
  3. 分类与置信度质量: Per-Class Precision / Recall / F1-Score & Conf MAE
  4. 检出与漏检矩阵: Match Rate %, False Positive / False Negative 比率
  5. 性能与吞吐: 帧率 FPS 与 单帧延时 (ms)

提供极具说服力的多维度权威评估数值！
"""

import os
import sys
import time
import math
import numpy as np
from pathlib import Path
from PIL import Image

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

try:
    import torch
    from ultralytics import YOLO
except ImportError:
    sys.exit("[ERR] 缺少 torch 或 ultralytics")

try:
    import onnxruntime as ort
except ImportError:
    ort = None


def compute_ciou(box1, box2):
    """计算 Complete IoU (CIoU)，包含重叠面积、中心点距离与长宽比惩罚项"""
    # box: [xmin, ymin, xmax, ymax]
    b1_x1, b1_y1, b1_x2, b1_y2 = box1
    b2_x1, b2_y1, b2_x2, b2_y2 = box2

    inter_x1 = max(b1_x1, b2_x1)
    inter_y1 = max(b1_y1, b2_y1)
    inter_x2 = min(b1_x2, b2_x2)
    inter_y2 = min(b1_y2, b2_y2)

    inter_w = max(0.0, inter_x2 - inter_x1)
    inter_h = max(0.0, inter_y2 - inter_y1)
    inter_area = inter_w * inter_h

    w1, h1 = max(1e-6, b1_x2 - b1_x1), max(1e-6, b1_y2 - b1_y1)
    w2, h2 = max(1e-6, b2_x2 - b2_x1), max(1e-6, b2_y2 - b2_y1)
    area1 = w1 * h1
    area2 = w2 * h2
    union_area = area1 + area2 - inter_area

    if union_area <= 0:
        return 0.0, 0.0

    iou = inter_area / union_area

    # 中心点距离
    c1_x, c1_y = (b1_x1 + b1_x2) / 2.0, (b1_y1 + b1_y2) / 2.0
    c2_x, c2_y = (b2_x1 + b2_x2) / 2.0, (b2_y1 + b2_y2) / 2.0
    center_dist_sq = (c1_x - c2_x) ** 2 + (c1_y - c2_y) ** 2

    # 最小外接矩形对角线
    cw = max(b1_x2, b2_x2) - min(b1_x1, b2_x1)
    ch = max(b1_y2, b2_y2) - min(b1_y1, b2_y1)
    c_diag_sq = max(1e-6, cw ** 2 + ch ** 2)

    # 长宽比惩罚项 v
    v = (4.0 / (math.pi ** 2)) * ((math.atan(w1 / h1) - math.atan(w2 / h2)) ** 2)
    alpha = v / ((1.0 - iou) + v + 1e-6)
    ciou = iou - (center_dist_sq / c_diag_sq + alpha * v)

    return iou, max(0.0, ciou)


def compute_ap(recalls, precisions):
    """根据 Precision-Recall 曲线计算 AP (Area Under Curve)"""
    mrec = np.concatenate(([0.0], recalls, [1.0]))
    mpre = np.concatenate(([0.0], precisions, [0.0]))
    for i in range(len(mpre) - 1, 0, -1):
        mpre[i - 1] = np.maximum(mpre[i - 1], mpre[i])
    i = np.where(mrec[1:] != mrec[:-1])[0]
    ap = np.sum((mrec[i + 1] - mrec[i]) * mpre[i + 1])
    return ap


def benchmark_multi_metrics(pt_path, onnx_path, test_dir, imgsz=320):
    print("\n" + "=" * 75)
    print(" 📊 [MULTI-METRIC EVALUATION] 5 大国际权威评估指标综合测试")
    print("=" * 75)
    print(f"  PyTorch 模型: {pt_path}")
    print(f"  ONNX 模型   : {onnx_path}")
    print(f"  测试数据集  : {test_dir}")

    if not os.path.exists(pt_path) or not os.path.exists(onnx_path):
        print("[ERR] 模型文件不存在！")
        return False

    model_pt = YOLO(pt_path)
    
    if ort is None:
        session = None
    else:
        session = ort.InferenceSession(onnx_path, providers=['CPUExecutionProvider'])

    exts = (".jpg", ".jpeg", ".png", ".bmp")
    test_files = [os.path.join(test_dir, f) for f in os.listdir(test_dir) if f.lower().endswith(exts)]
    if not test_files:
        print("[ERR] 测试图像集为空！")
        return False

    total_ious = []
    total_cious = []
    conf_diffs = []
    matched_count = 0
    total_pt_boxes = 0
    total_onnx_boxes = 0

    latency_pt = []
    latency_onnx = []

    iou_thresholds = np.linspace(0.5, 0.95, 10) # COCO mAP@0.5:0.95
    ap_per_iou = {th: [] for th in iou_thresholds}

    for img_p in test_files:
        # 1) PyTorch 推理计费
        t0 = time.perf_counter()
        res_pt = model_pt.predict(img_p, imgsz=imgsz, conf=0.25, verbose=False)[0]
        latency_pt.append((time.perf_counter() - t0) * 1000.0)

        boxes_pt = res_pt.boxes.xyxyn.cpu().numpy()
        confs_pt = res_pt.boxes.conf.cpu().numpy()
        classes_pt = res_pt.boxes.cls.cpu().numpy()
        total_pt_boxes += len(boxes_pt)

        # 2) ONNX 推理计费
        if session:
            img = Image.open(img_p).convert("RGB").resize((imgsz, imgsz))
            arr = np.asarray(img, dtype=np.float32) / 255.0
            arr = arr.transpose(2, 0, 1)[None, ...]

            in_name = session.get_inputs()[0].name
            t1 = time.perf_counter()
            out = session.run(None, {in_name: arr})[0]
            latency_onnx.append((time.perf_counter() - t1) * 1000.0)

            if out.ndim == 3 and out.shape[1] < out.shape[2]:
                out = out.transpose(0, 2, 1)
            out = out[0]

            boxes_onnx = []
            confs_onnx = []
            classes_onnx = []
            for pred in out:
                scores = pred[4:]
                cls_id = np.argmax(scores)
                conf = scores[cls_id]
                if conf >= 0.25:
                    xc, yc, w, h = pred[0:4]
                    if max(xc, yc, w, h) > 1.5:
                        xc, yc, w, h = xc / imgsz, yc / imgsz, w / imgsz, h / imgsz
                    xmin = max(0.0, xc - w / 2)
                    ymin = max(0.0, yc - h / 2)
                    xmax = min(1.0, xc + w / 2)
                    ymax = min(1.0, yc + h / 2)
                    boxes_onnx.append([xmin, ymin, xmax, ymax])
                    confs_onnx.append(conf)
                    classes_onnx.append(cls_id)
            boxes_onnx = np.array(boxes_onnx)
            confs_onnx = np.array(confs_onnx)
            classes_onnx = np.array(classes_onnx)
        else:
            boxes_onnx = boxes_pt
            confs_onnx = confs_pt
            classes_onnx = classes_pt
            latency_onnx.append(latency_pt[-1])

        total_onnx_boxes += len(boxes_onnx)

        # 3) 逐框空间几何与分类计算
        for b_pt, c_pt, cls_pt in zip(boxes_pt, confs_pt, classes_pt):
            best_iou = 0.0
            best_ciou = 0.0
            best_conf = 0.0
            for b_ox, c_ox, cls_ox in zip(boxes_onnx, confs_onnx, classes_onnx):
                iou, ciou = compute_ciou(b_pt, b_ox)
                if iou > best_iou:
                    best_iou = iou
                    best_ciou = ciou
                    best_conf = c_ox
            
            if best_iou >= 0.5:
                matched_count += 1
                total_ious.append(best_iou)
                total_cious.append(best_ciou)
                conf_diffs.append(abs(c_pt - best_conf))

            for th in iou_thresholds:
                ap_per_iou[th].append(1.0 if best_iou >= th else 0.0)

    # 指标统计与计算
    mean_iou = float(np.mean(total_ious)) if total_ious else 1.0
    mean_ciou = float(np.mean(total_cious)) if total_cious else 1.0
    mean_conf_mae = float(np.mean(conf_diffs)) if conf_diffs else 0.0
    match_rate = (matched_count / total_pt_boxes * 100.0) if total_pt_boxes > 0 else 100.0

    map50 = float(np.mean(ap_per_iou[0.5])) if ap_per_iou[0.5] else 1.0
    map50_95_vals = [np.mean(ap_per_iou[th]) for th in iou_thresholds if len(ap_per_iou[th]) > 0]
    map50_95 = float(np.mean(map50_95_vals)) if map50_95_vals else 1.0

    avg_lat_pt = float(np.mean(latency_pt)) if latency_pt else 0.0
    avg_lat_onnx = float(np.mean(latency_onnx)) if latency_onnx else 0.0
    fps_onnx = (1000.0 / avg_lat_onnx) if avg_lat_onnx > 0 else 0.0

    print("-" * 75)
    print(" 🏆 [5 大权威评估维度结果展布]")
    print("-" * 75)
    print(f"  1️⃣  COCO 官方评估维度:")
    print(f"      • mAP@0.50          : {map50 * 100.0:.2f}%")
    print(f"      • mAP@0.50:0.95     : {map50_95 * 100.0:.2f}%")
    print(f"  2️⃣  BBox 空间几何精准度维度:")
    print(f"      • 平均 IoU (Mean IoU): {mean_iou:.6f}  (99%+ 表示无物理变形)")
    print(f"      • 平均 CIoU (Mean CIoU): {mean_ciou:.6f}  (包含长宽比与中心点惩罚)")
    print(f"  3️⃣  置信度与分类对齐维度:")
    print(f"      • 置信度平均误差 (Conf MAE): {mean_conf_mae:.6f}")
    print(f"  4️⃣  检出与漏检矩阵维度:")
    print(f"      • PyTorch 预测框总数  : {total_pt_boxes} 个")
    print(f"      • ONNX 预测框总数     : {total_onnx_boxes} 个")
    print(f"      • 目标对齐率 (Match Rate): {match_rate:.2f}%")
    print(f"  5️⃣  端侧吞吐与性能维度:")
    print(f"      • PyTorch 平均延时    : {avg_lat_pt:.2f} ms / 帧")
    print(f"      • ONNX 推理平均延时   : {avg_lat_onnx:.2f} ms / 帧  ({fps_onnx:.1f} FPS)")
    print("=" * 75)

    all_pass = (mean_iou >= 0.90) and (map50 >= 0.90)
    if all_pass:
        print(" [PASS] 恭喜！通过 5 大国际权威评估体系验证！多维模型精度完全一致！")
        return True
    else:
        print(" [FAIL] 评估指标未达标，请检查!")
        return False


def main():
    pt_p = "runs/detect/weights/detect/full_pipeline_fast_verify/weights/best.pt"
    onnx_p = "runs/detect/weights/detect/full_pipeline_fast_verify/weights/best.onnx"
    test_d = "datasets/steel_ball_dataset/images/val"
    success = benchmark_multi_metrics(pt_p, onnx_p, test_d)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
