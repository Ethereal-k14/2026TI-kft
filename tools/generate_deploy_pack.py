#!/usr/bin/env python3
"""
K230 上板部署包一键打包工具 (tools/generate_deploy_pack.py)

功能：
    在模型训练、导出或完成 nncase 转换后，将部署到嘉楠 K230 开发板所需的：
    1. .kmodel / .onnx 模型文件
    2. 格式规范的 labels.txt 标签文件
    3. MicroPython (CanMV) 板端运行入口脚本 main.py
    4. Linux C++ (yolo.elf) 启动脚本 run.sh
    5. 部署说明文档 README.txt

    一键收集并打包发布到 deploy_pack/ 目录，直接拷贝到 SD 卡 (sharefs/sdcard) 即可在开发板运行。

示例：
    python tools/generate_deploy_pack.py --model weights/best.kmodel --data configs/coco128.yaml --output deploy_pack
    python tools/generate_deploy_pack.py --model best.onnx --labels data/coco_labels.txt --imgsz 320
"""

import argparse
import io
import os
import shutil
import sys
import yaml

# 强制标准输出 UTF-8，防止 Windows PowerShell / CMD GBK 编码报错
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')


def parse_args():
    p = argparse.ArgumentParser(description="一键构建 K230 板端部署包 (deploy_pack)")
    p.add_argument("--model", required=True, help="模型路径 (.kmodel / .onnx / .pt)")
    p.add_argument("--data", default=None, help="数据集 yaml 配置文件 (用于自动提炼类别名)")
    p.add_argument("--labels", default=None, help="已有的 labels.txt 文件")
    p.add_argument("--task", default="detect", choices=["detect", "segment", "pose", "classify", "obb"],
                   help="AI 视觉任务类型 (默认 detect)")
    p.add_argument("--imgsz", type=int, default=320, help="输入分辨率 (默认 320)")
    p.add_argument("--output", default="deploy_pack", help="生成的部署包目录路径")
    return p.parse_args()


def extract_labels(args):
    """提取并生成统一规范的 labels.txt 内容列表"""
    labels = []
    if args.labels and os.path.exists(args.labels):
        with open(args.labels, "r", encoding="utf-8") as f:
            labels = [line.strip() for line in f if line.strip()]
        print(f"[INFO] 从 labels 文件读取 {len(labels)} 个类别: {args.labels}")
    elif args.data and os.path.exists(args.data):
        with open(args.data, "r", encoding="utf-8") as f:
            data_cfg = yaml.safe_load(f)
            names = data_cfg.get("names", {})
            if isinstance(names, dict):
                labels = [str(names[k]) for k in sorted(names.keys())]
            elif isinstance(names, list):
                labels = [str(x) for x in names]
        print(f"[INFO] 从 YAML 配置文件提取 {len(labels)} 个类别: {args.data}")
    else:
        print("[WARNING] 未提供 --data 或 --labels，使用默认通用 object 类别标签")
        labels = ["object"]

    return labels


def main():
    a = parse_args()

    output_dir = os.path.abspath(a.output)
    os.makedirs(output_dir, exist_ok=True)
    print(f"\n📦 开始打包 K230 板端部署包至: {output_dir}")

    # 1. 复制模型文件
    model_src = os.path.abspath(a.model)
    model_name = os.path.basename(model_src)
    if not os.path.exists(model_src):
        print(f"[ERR] 模型文件不存在: {model_src}")
        return 1

    # 如果是 .kmodel，重命名或保留 best.kmodel
    target_model_name = "best.kmodel" if model_name.endswith(".kmodel") else model_name
    model_dst = os.path.join(output_dir, target_model_name)
    shutil.copy2(model_src, model_dst)
    print(f"  [✓] 封装模型: {target_model_name}")

    # 2. 导出 labels.txt
    labels = extract_labels(a)
    labels_path = os.path.join(output_dir, "labels.txt")
    with open(labels_path, "w", encoding="utf-8") as f:
        for lbl in labels:
            f.write(f"{lbl}\n")
    print(f"  [✓] 封装标签: labels.txt (共 {len(labels)} 类)")

    # 3. 复制并填充 CanMV (MicroPython) 示例入口脚本 main.py
    template_canmv = os.path.join("templates", "canmv_k230_demo.py")
    main_py_dst = os.path.join(output_dir, "main.py")
    if os.path.exists(template_canmv):
        shutil.copy2(template_canmv, main_py_dst)
        print(f"  [✓] 封装 CanMV MicroPython 入口: main.py")

    # 4. 复制并定制 Linux C++ 启动脚本 run.sh
    template_sh = os.path.join("templates", "k230_cpp_runner.sh")
    run_sh_dst = os.path.join(output_dir, "run.sh")
    if os.path.exists(template_sh):
        with open(template_sh, "r", encoding="utf-8") as f:
            sh_content = f.read()

        # 动态更新输入尺寸
        sh_content = sh_content.replace("INPUT_W=320", f"INPUT_W={a.imgsz}")
        sh_content = sh_content.replace("INPUT_H=320", f"INPUT_H={a.imgsz}")

        with open(run_sh_dst, "w", encoding="utf-8", newline="\n") as f:
            f.write(sh_content)
        print(f"  [✓] 封装 Linux C++ 启动脚本: run.sh (尺寸设定: {a.imgsz}x{a.imgsz})")

    # 5. 生成部署说明 README.txt
    readme_content = f"""K230 板端部署包说明 (Generated automatically)
==================================================
包内容：
  - {target_model_name}   : 模型文件 (需要编译为 .kmodel 才能上板)
  - labels.txt         : 类别名称文件 (共 {len(labels)} 类)
  - main.py            : CanMV (MicroPython) 上板运行代码
  - run.sh             : Linux 侧 yolo.elf 启动脚本

部署到 K230 板卡指南：
--------------------------------------------------
【路径 A：MicroPython (CanMV) 模式】
  1. 将全套文件拷贝至 SD 卡的 /sdcard/ 或 /sharefs/ 目录。
  2. 保证模型为 .kmodel 格式 (如果是 .onnx，请先用 tools/to_kmodel.py 转换)。
  3. 板端执行 `python main.py` 或在 CanMV IDE 中打开运行。

【路径 B：Linux C++ (yolo.elf) 模式】
  1. 将本目录所有文件及官方编译的 `yolo.elf` 拷贝至 SD 卡 /sharefs/ 目录。
  2. 在 K230 小核串口终端运行:
     chmod +x run.sh
     ./run.sh
==================================================
"""
    with open(os.path.join(output_dir, "README.txt"), "w", encoding="utf-8") as f:
        f.write(readme_content)
    print(f"  [✓] 生成板端部署说明: README.txt")

    print(f"\n✨ 打包完成！你可以将 {output_dir}/ 文件夹内的所有文件拷贝到 K230 开发板运行。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
