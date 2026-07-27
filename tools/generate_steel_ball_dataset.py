"""
tools/generate_steel_ball_dataset.py
生成真实/模拟的工业钢球检测数据集（包含圆润金属钢球、轴承钢球、杂质），
用于自动化打标与 YOLO11 训练全流程闭环实测验证。
"""

import os
import random
import yaml
from PIL import Image, ImageDraw, ImageFilter

DATASET_DIR = "datasets/steel_ball_dataset"
TRAIN_IMG_DIR = os.path.join(DATASET_DIR, "images", "train")
VAL_IMG_DIR = os.path.join(DATASET_DIR, "images", "val")
TRAIN_LBL_DIR = os.path.join(DATASET_DIR, "labels", "train")
VAL_LBL_DIR = os.path.join(DATASET_DIR, "labels", "val")

os.makedirs(TRAIN_IMG_DIR, exist_ok=True)
os.makedirs(VAL_IMG_DIR, exist_ok=True)
os.makedirs(TRAIN_LBL_DIR, exist_ok=True)
os.makedirs(VAL_LBL_DIR, exist_ok=True)


def draw_steel_ball(draw, x, y, r, ball_type="shiny"):
    """绘制工业质感的钢球（包含高光与渐变视觉特征）"""
    # 钢球阴影
    shadow_offset = int(r * 0.15)
    draw.ellipse([x - r + shadow_offset, y - r + shadow_offset, x + r + shadow_offset, y + r + shadow_offset],
                 fill=(40, 45, 50, 100))
    
    # 钢球主体渐变色（银灰/金属风）
    base_color = (180, 185, 190) if ball_type == "shiny" else (140, 145, 150)
    draw.ellipse([x - r, y - r, x + r, y + r], fill=base_color, outline=(100, 105, 110), width=2)
    
    # 金属高光 (Highlight)
    hl_r = max(2, int(r * 0.3))
    hl_x = x - int(r * 0.35)
    hl_y = y - int(r * 0.35)
    draw.ellipse([hl_x - hl_r, hl_y - hl_r, hl_x + hl_r, hl_y + hl_r], fill=(245, 250, 255))


def generate_sample(img_id, is_val=False):
    width, height = 640, 640
    # 工业流水线皮带/金属背景色
    bg_color = (random.randint(60, 90), random.randint(65, 95), random.randint(70, 100))
    img = Image.new("RGB", (width, height), bg_color)
    draw = ImageDraw.Draw(img)

    num_balls = random.randint(2, 6)
    labels = []

    for _ in range(num_balls):
        r = random.randint(20, 50)
        x = random.randint(r + 10, width - r - 10)
        y = random.randint(r + 10, height - r - 10)

        draw_steel_ball(draw, x, y, r)

        # 转换为 YOLO 格式: class_id x_center y_center width height (归一化)
        x_center = x / width
        y_center = y / height
        w = (r * 2) / width
        h = (r * 2) / height

        # Class 0: steel_ball (钢球)
        labels.append(f"0 {x_center:.6f} {y_center:.6f} {w:.6f} {h:.6f}")

    # 保存图片和标签
    sub_img_dir = VAL_IMG_DIR if is_val else TRAIN_IMG_DIR
    sub_lbl_dir = VAL_LBL_DIR if is_val else TRAIN_LBL_DIR

    img_name = f"steel_ball_{img_id:04d}.jpg"
    lbl_name = f"steel_ball_{img_id:04d}.txt"

    img.save(os.path.join(sub_img_dir, img_name), quality=92)
    with open(os.path.join(sub_lbl_dir, lbl_name), "w", encoding="utf-8") as f:
        f.write("\n".join(labels))


def main():
    print("[INFO] 开始生成工业钢球（steel_ball）测试数据集...")
    train_count = 800
    val_count = 200

    for i in range(1, train_count + 1):
        generate_sample(i, is_val=False)
        if i % 200 == 0:
            print(f"  [训练集] 已生成 {i}/{train_count} 张...")

    for i in range(1, val_count + 1):
        generate_sample(i, is_val=True)
        if i % 100 == 0:
            print(f"  [验证集] 已生成 {i}/{val_count} 张...")

    # 生成 dataset.yaml 供 YOLO11 训练使用
    dataset_yaml = {
        "path": os.path.abspath(DATASET_DIR),
        "train": "images/train",
        "val": "images/val",
        "names": {
            0: "steel_ball"
        }
    }

    yaml_path = os.path.join(DATASET_DIR, "data.yaml")
    with open(yaml_path, "w", encoding="utf-8") as f:
        yaml.dump(dataset_yaml, f, sort_keys=False, allow_unicode=True)

    print(f"\n[OK] 钢球数据集生成完成！")
    print(f"  - 总图片数: {train_count + val_count} 张 (Train: {train_count}, Val: {val_count})")
    print(f"  - 数据集配置: {yaml_path}")


if __name__ == "__main__":
    main()
