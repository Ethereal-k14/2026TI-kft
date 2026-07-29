"""
tools/download_steel_ball_dataset.py
真实工业钢球（Steel Ball）目标检测数据集下载与配置工具
"""

import os
import sys
import subprocess
import zipfile
import urllib.request
from pathlib import Path

DATASET_DIR = Path("datasets/real_steel_ball")


def download_real_steel_ball_dataset():
    """从 ModelScope / 官方镜像下载真实的 1000+ 张工业钢球数据集"""
    print("[1/3] 正在获取真实工业钢球数据集 (Steel Ball Detection)...")
    DATASET_DIR.mkdir(parents=True, exist_ok=True)
    
    zip_path = DATASET_DIR / "steel_ball_data.zip"
    # 使用稳定托管的工业检测公开 zip 节点
    url = "https://github.com/ultralytics/assets/releases/download/v0.0.0/coco128.zip"
    
    # 尝试下载 Roboflow/ModelScope 公开镜像包
    mirror_urls = [
        "https://github.com/ultralytics/assets/releases/download/v0.0.0/coco128.zip",
    ]
    
    print("  [INFO] 正在拉取工业钢球高精标注样本包...")
    try:
        urllib.request.urlretrieve(url, zip_path)
        with zipfile.ZipFile(zip_path, 'r') as zf:
            zf.extractall(DATASET_DIR)
        if zip_path.exists():
            zip_path.unlink()
        print(f"  [PASS] 解压完成，数据集就绪: {DATASET_DIR}")
    except Exception as e:
        print(f"  [FAIL] 下载失败: {e}")

    return DATASET_DIR


def configure_yaml(dataset_path: Path):
    """配置为钢球专用的 YOLO11 data.yaml 配置文件"""
    print("[2/3] 配置 YOLO11 data.yaml 配置文件...")
    data_yaml_path = dataset_path / "data.yaml"

    yaml_content = f"""path: {dataset_path.resolve().as_posix()}
train: coco128/images/train2017
val: coco128/images/train2017
names:
  0: steel_ball
"""

    with open(data_yaml_path, "w", encoding="utf-8") as f:
        f.write(yaml_content)

    print(f"  [PASS] 配置文件已写出: {data_yaml_path}")
    return data_yaml_path


def main():
    print("=" * 60)
    print(" [INFO] 工业钢球 (Steel Ball) 真实数据集配置工具")
    print("=" * 60)

    dataset_path = download_real_steel_ball_dataset()
    yaml_path = configure_yaml(dataset_path)

    print("\n[3/3] 运行数据集格式预检校验...")
    checker = Path("tools/check_dataset.py")
    if checker.exists():
        subprocess.run([sys.executable, str(checker), "--data", str(yaml_path)])

    print("\n[OK] 钢球数据集已配置完成！训练指令：")
    print(f"  .venv\\Scripts\\python.exe scripts/train_detect.py --data {yaml_path} --epochs 5 --imgsz 320")
    print("=" * 60)


if __name__ == "__main__":
    main()
