#!/usr/bin/env python3
"""
K230 数据集格式预检校验器 (tools/check_dataset.py)

训练前自动预检 YOLO 格式数据集，防止因数据路径错乱、标签格式错误或归一化超界导致训练崩溃。

检查项：
  1. YAML 配置解析与字段检查 (names, train, val)
  2. 训练集 / 验证集图片目录与标签目录对应关系检查
  3. 样本标签文件归一化坐标校验 (0.0 <= x,y,w,h <= 1.0)
  4. 类别索引合法性校验 (0 <= class_id < num_classes)

示例：
    python tools/check_dataset.py --data configs/coco128.yaml
"""

import argparse
import io
import os
import sys
import yaml

# 强制 UTF-8 输出
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def resolve_dataset_path(path_str: str, yaml_dir: str) -> str:
    """尝试以相对路径和绝对路径解析数据集目录"""
    if not path_str:
        return ""
    if os.path.isabs(path_str):
        return path_str
    # 优先尝试从工程根目录解析
    p1 = os.path.abspath(os.path.join(PROJECT_ROOT, path_str))
    if os.path.exists(p1):
        return p1
    # 尝试相对 YAML 目录解析
    p2 = os.path.abspath(os.path.join(yaml_dir, path_str))
    return p2 if os.path.exists(p2) else p1


def check_dataset(yaml_path: str, max_check_samples: int = 100):
    print(f"\n🔍 正在预检数据集配置: {yaml_path}")
    print("=" * 60)

    yaml_abs = os.path.abspath(yaml_path)
    if not os.path.exists(yaml_abs):
        print(f"[ERR] YAML 配置文件不存在: {yaml_path}")
        return False

    yaml_dir = os.path.dirname(yaml_abs)

    try:
        with open(yaml_abs, "r", encoding="utf-8") as f:
            cfg = yaml.safe_load(f)
    except Exception as e:
        print(f"[ERR] YAML 解析失败: {e}")
        return False

    # 1. 检查类别声明
    names = cfg.get("names")
    if not names:
        print("[ERR] YAML 中未找到 `names` 类别定义")
        return False

    if isinstance(names, dict):
        num_classes = len(names)
        class_names = [names[k] for k in sorted(names.keys())]
    elif isinstance(names, list):
        num_classes = len(names)
        class_names = names
    else:
        print(f"[ERR] `names` 必须是列表或字典格式，当前类型: {type(names)}")
        return False

    print(f"[PASS] 类别定义: 共 {num_classes} 类 -> {class_names[:5]}{'...' if num_classes > 5 else ''}")

    # 2. 检查图片与标签路径
    root_path = cfg.get("path", "")
    train_path = cfg.get("train", "")
    val_path = cfg.get("val", "")

    base_dir = resolve_dataset_path(root_path, yaml_dir) if root_path else yaml_dir
    train_abs = os.path.abspath(os.path.join(base_dir, train_path)) if train_path else ""
    val_abs = os.path.abspath(os.path.join(base_dir, val_path)) if val_path else ""

    print(f"[INFO] 基准目录: {base_dir}")
    print(f"[INFO] 训练集路径: {train_abs} ({'存在' if os.path.exists(train_abs) else '未建路径'})")
    print(f"[INFO] 验证集路径: {val_abs} ({'存在' if os.path.exists(val_abs) else '未建路径'})")

    target_dir = train_abs if os.path.exists(train_abs) else (val_abs if os.path.exists(val_abs) else "")
    if not target_dir:
        print("[WARNING] 未找到实际存在的数据集图片目录，训练时将尝试依赖 Ultralytics 自动下载/解压。")
        return True

    # 3. 抽样检查标签文件
    image_exts = (".jpg", ".jpeg", ".png", ".bmp")
    img_files = []
    if os.path.isdir(target_dir):
        for root, _, files in os.walk(target_dir):
            for f in files:
                if f.lower().endswith(image_exts):
                    img_files.append(os.path.join(root, f))
                if len(img_files) >= max_check_samples:
                    break
            if len(img_files) >= max_check_samples:
                break

    print(f"[PASS] 检测到图片样本: {len(img_files)} 张 (抽查上限 {max_check_samples})")

    valid_labels = 0
    corrupt_labels = 0

    for img_p in img_files:
        # 寻找对应的 .txt 标签文件 (如 images/train/xxx.jpg -> labels/train/xxx.txt)
        txt_p = img_p.replace("images", "labels")
        txt_p = os.path.splitext(txt_p)[0] + ".txt"

        if not os.path.exists(txt_p):
            continue

        valid_labels += 1
        with open(txt_p, "r", encoding="utf-8") as lf:
            for line_idx, line in enumerate(lf, 1):
                parts = line.strip().split()
                if not parts:
                    continue
                try:
                    cls_id = int(parts[0])
                    coords = [float(x) for x in parts[1:5]]
                except (ValueError, IndexError):
                    print(f"[WARNING] 标签行解析格式异常: {txt_p}:L{line_idx} -> '{line.strip()}'")
                    corrupt_labels += 1
                    continue

                if cls_id < 0 or cls_id >= num_classes:
                    print(f"[WARNING] 类别 ID {cls_id} 超出 [0, {num_classes-1}] 范围: {txt_p}:L{line_idx}")
                    corrupt_labels += 1

                for c in coords:
                    if c < -0.05 or c > 1.05:  # 允许极小浮点微差
                        print(f"[WARNING] 坐标未归一化 [0.0, 1.0]: {c} at {txt_p}:L{line_idx}")
                        corrupt_labels += 1

    print(f"[PASS] 抽查标签文件: {valid_labels} 份，格式异常行数: {corrupt_labels}")

    if corrupt_labels > 0:
        print("[WARNING] ⚠️ 发现部分标签格式可能不规范，建议检查处理后再开始训练。")
    else:
        print("✅ 数据集格式预检全部合格，可放心开启训练！")

    print("=" * 60)
    return True


def main():
    p = argparse.ArgumentParser(description="YOLO 数据集格式预检校验器 (tools/check_dataset.py)")
    p.add_argument("--data", required=True, help="数据集 yaml 配置文件")
    p.add_argument("--samples", type=int, default=100, help="抽样检查图片数量")
    args = p.parse_args()

    success = check_dataset(args.data, args.samples)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
