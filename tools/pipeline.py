#!/usr/bin/env python3
"""
K230 视觉 AI 开发工程 - 一键全流程闭环管线 (tools/pipeline.py)

自动协同 4 阶段：
  1. [Train]   运行指定 YOLO 任务训练，提取最终权重 (best.pt)
  2. [Export]  将 best.pt 导出为符合 K230 (KPU) 规范的静态 Shape ONNX
  3. [KModel]  调用 nncase 完成 ONNX -> .kmodel 量化编译 (有 nncase 时编译，无则打印跳过说明)
  4. [Pack]    一键打包至板端部署包 (deploy_pack)

用法：
    python tools/pipeline.py --data configs/coco128.yaml --epochs 10 --imgsz 320
"""

import argparse
import io
import os
import sys
import subprocess
import yaml

# 强制 UTF-8 输出
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def print_banner(step: int, title: str):
    print(f"\n==================================================")
    print(f" 🚀 [Pipeline Stage {step}/4] {title}")
    print(f"==================================================")


def run_cmd(cmd_list):
    full_cmd = [sys.executable] + cmd_list
    res = subprocess.run(full_cmd, cwd=PROJECT_ROOT)
    if res.returncode != 0:
        print(f"[ERR] 步骤执行失败: {' '.join(cmd_list)}")
        sys.exit(res.returncode)


def find_calib_dir(data_yaml: str):
    """从 yaml 文件中尝试寻找图片路径作为 kmodel 校准集目录"""
    if os.path.exists(data_yaml):
        try:
            with open(data_yaml, "r", encoding="utf-8") as f:
                cfg = yaml.safe_load(f)
                val_path = cfg.get("val") or cfg.get("train")
                if val_path and os.path.exists(val_path):
                    return val_path
        except Exception:
            pass
    return None


def main():
    parser = argparse.ArgumentParser(description="一键全流程闭环管线 (train -> export -> kmodel -> pack)")
    parser.add_argument("--data", required=True, help="数据集 yaml 配置文件路径")
    parser.add_argument("--task", default="detect", choices=["detect", "classify", "segment", "pose", "obb"])
    parser.add_argument("--epochs", type=int, default=10)
    parser.add_argument("--imgsz", type=int, default=320)
    parser.add_argument("--device", default="", help="训练设备")
    parser.add_argument("--output", default="deploy_pack", help="输出部署包目录")

    args = parser.parse_args()

    # Stage 1: Train
    print_banner(1, f"YOLO11n {args.task.upper()} 模型微调/训练")
    task_scripts = {
        "detect": "scripts/train_detect.py",
        "classify": "scripts/train_classify.py",
        "segment": "scripts/train_segment.py",
        "pose": "scripts/train_pose.py",
        "obb": "scripts/train_obb.py",
    }
    train_script = task_scripts[args.task]
    train_cmd = [train_script, "--data", args.data, "--epochs", str(args.epochs), "--imgsz", str(args.imgsz)]
    if args.device:
        train_cmd.extend(["--device", args.device])
    run_cmd(train_cmd)

    # 寻找生成的 best.pt 路径
    pt_path = os.path.join(PROJECT_ROOT, "weights", args.task, "yolo11n", "weights", "best.pt")
    if not os.path.exists(pt_path):
        # 兼容备选路径
        pt_path = os.path.join(PROJECT_ROOT, "weights", "best.pt")

    if not os.path.exists(pt_path):
        print(f"[ERR] 训练未找到产出权重文件: {pt_path}")
        sys.exit(1)

    print(f"[✓] 阶段 1 成功，得到产出权重: {pt_path}")

    # Stage 2: Export ONNX
    print_banner(2, "导出符合 K230 规范的静态 Shape ONNX 模型")
    onnx_out = os.path.join(PROJECT_ROOT, "weights", f"best_{args.imgsz}.onnx")
    export_cmd = [
        "scripts/export_onnx.py",
        "--weights", pt_path,
        "--imgsz", str(args.imgsz),
        "--out", onnx_out,
        "--task", args.task,
    ]
    run_cmd(export_cmd)

    # Stage 3: Convert to KModel (if nncase installed)
    print_banner(3, "调用 nncase 尝试量化编译 .kmodel")
    has_nncase = False
    try:
        import nncase
        has_nncase = True
    except ImportError:
        pass

    kmodel_out = os.path.join(PROJECT_ROOT, "weights", f"best_{args.imgsz}.kmodel")

    if has_nncase:
        calib_dir = find_calib_dir(args.data)
        if not calib_dir or not os.path.exists(calib_dir):
            calib_dir = os.path.join(PROJECT_ROOT, "datasets")
        kmodel_cmd = [
            "tools/to_kmodel.py",
            "--model", onnx_out,
            "--dataset", calib_dir,
            "--input-size", str(args.imgsz), str(args.imgsz),
            "--output", kmodel_out,
        ]
        run_cmd(kmodel_cmd)
        target_model = kmodel_out
    else:
        print("[NOTE] 本机未安装 nncase 包，自动跳过 kmodel 量化编译，直接使用 ONNX 封装部署包。")
        print("       (如需编译 .kmodel，请参考 requirements-convert.txt 在 WSL2 / Linux 中安装 nncase)")
        target_model = onnx_out

    # Stage 4: Generate Deploy Pack
    print_banner(4, "一键生成 K230 板端部署包 (deploy_pack)")
    pack_cmd = [
        "tools/generate_deploy_pack.py",
        "--model", target_model,
        "--data", args.data,
        "--task", args.task,
        "--imgsz", str(args.imgsz),
        "--output", args.output,
    ]
    run_cmd(pack_cmd)

    print("\n==================================================")
    print(" 🎉 [Pipeline SUCCESS] 全闭环自动化管线全部完成！")
    print(f" 📂 板端部署包位置: {os.path.abspath(args.output)}")
    print("==================================================")


if __name__ == "__main__":
    main()
