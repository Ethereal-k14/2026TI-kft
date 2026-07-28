#!/usr/bin/env python3
"""
K230 视觉 AI 开发工程 - 跨平台统一 CLI (tools/k230.py)

跨平台支持：Windows / Linux / WSL2 / macOS。
可用 `python tools/k230.py <command>` 或 `uv run python tools/k230.py <command>`。
无参数直接运行将启动零记忆交互式控制台 (TUI Menu)。

功能清单：
  - check       : 7维工作区完整性审计 + 全流程功能检验
  - audit       : 7维工作区完整性审计
  - verify      : 全流程功能检验 (~20s)
  - gpu         : CUDA / GPU 显卡状态检查
  - check-data  : 数据集格式预检校验 (防止bad label崩溃)
  - train       : YOLO 5大任务训练 (detect / classify / segment / pose / obb)
  - infer       : 目标检测/实例分割/姿态估计推理与可视化
  - track       : 多目标追踪 (ByteTrack / BoT-SORT)
  - export      : 导出 K230 规范静态 Shape ONNX
  - kmodel      : nncase ONNX -> .kmodel 转换
  - pack        : 构建 K230 SD 卡板端部署包 (deploy_pack)
  - sd-deploy   : 💳 智能盘符识别并一键同步到 SD 卡
  - pipeline    : 🚀 一键全流程闭环训练到打包 (train -> export -> kmodel -> pack)
  - clean       : 🧹 一键清理临时与构建目录
"""

import argparse
import io
import os
import shutil
import sys
import subprocess

# 强制 UTF-8 输出
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run_cmd(args):
    """优先使用原生 Python 模块动态导入调起，避免子进程二次加载重型包的开销"""
    script_path = args[0]
    script_args = args[1:]
    
    # 模块名转换 (e.g. tools/audit_workspace.py -> tools.audit_workspace)
    mod_name = script_path.replace("\\", "/").replace("/", ".").removesuffix(".py")
    try:
        import importlib
        old_argv = sys.argv
        sys.argv = [script_path] + script_args
        try:
            mod = importlib.import_module(mod_name)
            if hasattr(mod, "main"):
                res = mod.main()
                if isinstance(res, int) and res != 0:
                    sys.exit(res)
                return
        finally:
            sys.argv = old_argv
    except Exception:
        pass

    # 回退到子进程安全执行
    cmd = [sys.executable] + args
    res = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if res.returncode != 0:
        sys.exit(res.returncode)


def cmd_check():
    print("==================================================")
    print(" 🔍 [1/2] 运行工作区完整性审计...")
    print("==================================================")
    run_cmd(["tools/audit_workspace.py"])
    print("\n==================================================")
    print(" ⚡ [2/2] 运行全流程功能检验...")
    print("==================================================")
    run_cmd(["tools/verify_env.py"])


def cmd_audit():
    run_cmd(["tools/audit_workspace.py"])


def cmd_verify():
    run_cmd(["tools/verify_env.py"])


def cmd_gpu():
    code = (
        "import torch; "
        "cuda = torch.cuda.is_available(); "
        "print('CUDA 可用:', cuda); "
        "print('设备:', torch.cuda.get_device_name(0) if cuda else 'CPU only'); "
        "print('PyTorch 版本:', torch.__version__)"
    )
    run_cmd(["-c", code])


def cmd_check_data(args):
    cmd = ["tools/check_dataset.py", "--data", args.data]
    if getattr(args, "strict", False):
        cmd.append("--strict")
    run_cmd(cmd)


def cmd_label(args):
    cmd = ["tools/cloud_label.py", "--images", args.images, "--output", args.output, "--provider", args.provider]
    if args.classes:
        cmd.extend(["--classes"] + args.classes)
    if args.api_key:
        cmd.extend(["--api-key", args.api_key])
    run_cmd(cmd)


def cmd_test_pipeline():
    run_cmd(["tools/test_full_pipeline.py"])


def cmd_strict_check():
    run_cmd(["tools/strict_high_precision_check.py"])


def cmd_sd_deploy(args):
    cmd = ["tools/deploy_sd.py"]
    if args.drive:
        cmd.extend(["--drive", args.drive])
    if args.source:
        cmd.extend(["--source", args.source])
    run_cmd(cmd)


def cmd_train(args):
    task_map = {
        "detect": "scripts/train_detect.py",
        "classify": "scripts/train_classify.py",
        "segment": "scripts/train_segment.py",
        "pose": "scripts/train_pose.py",
        "obb": "scripts/train_obb.py",
    }
    script = task_map.get(args.task, "scripts/train_detect.py")
    cmd = [script, "--data", args.data, "--epochs", str(args.epochs), "--imgsz", str(args.imgsz)]
    if args.device:
        cmd.extend(["--device", args.device])
    run_cmd(cmd)


def cmd_infer(args):
    cmd = [
        "scripts/infer.py",
        "--source", args.source,
        "--weights", args.weights,
        "--imgsz", str(args.imgsz),
        "--conf", str(args.conf),
        "--task", args.task,
    ]
    if args.device:
        cmd.extend(["--device", args.device])
    run_cmd(cmd)


def cmd_track(args):
    cmd = [
        "scripts/track.py",
        "--source", args.source,
        "--weights", args.weights,
        "--imgsz", str(args.imgsz),
        "--tracker", args.tracker,
    ]
    if args.device:
        cmd.extend(["--device", args.device])
    run_cmd(cmd)


def cmd_export(args):
    cmd = ["scripts/export_onnx.py", "--weights", args.weights, "--imgsz", str(args.imgsz), "--opset", str(args.opset)]
    if args.out:
        cmd.extend(["--out", args.out])
    if args.task:
        cmd.extend(["--task", args.task])
    run_cmd(cmd)


def cmd_kmodel(args):
    cmd = [
        "tools/to_kmodel.py",
        "--model", args.model,
        "--dataset", args.dataset,
        "--input-size", str(args.imgsz), str(args.imgsz),
    ]
    if args.output:
        cmd.extend(["--output", args.output])
    run_cmd(cmd)


def cmd_pack(args):
    cmd = [
        "tools/generate_deploy_pack.py",
        "--model", args.model,
        "--task", args.task,
        "--imgsz", str(args.imgsz),
        "--output", args.output,
    ]
    if args.data:
        cmd.extend(["--data", args.data])
    if args.labels:
        cmd.extend(["--labels", args.labels])
    run_cmd(cmd)


def cmd_pipeline(args):
    cmd = [
        "tools/pipeline.py",
        "--data", args.data,
        "--epochs", str(args.epochs),
        "--imgsz", str(args.imgsz),
        "--task", args.task,
        "--output", args.output,
    ]
    if args.device:
        cmd.extend(["--device", args.device])
    run_cmd(cmd)


def cmd_clean():
    targets = ["runs", "dump", "tmp", "__pycache__", ".pytest_cache"]
    for root, dirs, files in os.walk(PROJECT_ROOT):
        for d in dirs:
            if d in ("__pycache__", ".pytest_cache"):
                shutil.rmtree(os.path.join(root, d), ignore_errors=True)
    for t in targets:
        p = os.path.join(PROJECT_ROOT, t)
        if os.path.exists(p) and os.path.isdir(p):
            shutil.rmtree(p, ignore_errors=True)
            print(f"[CLEAN] 已清理: {t}/")
    os.makedirs(os.path.join(PROJECT_ROOT, "tmp"), exist_ok=True)
    print("[CLEAN] 清理完成！")


def cmd_menu():
    print("\n" + "=" * 60)
    print(" 🚀 K230 视觉 AI 工程 · 零记忆交互式控制台")
    print("=" * 60)
    print("  [1] 🚀 一键全流程闭环训练到打包 (Pipeline)")
    print("  [2] 🔍 环境全项审计与功能检验 (Check)")
    print("  [3] ⚡ 检查 GPU / CUDA 显卡状态 (GPU)")
    print("  [4] 📋 数据集格式预检校验 (Check Dataset)")
    print("  [5] 📦 导出 K230 规范静态 Shape ONNX")
    print("  [6] 📂 构建 K230 板端部署包 (deploy_pack)")
    print("  [7] 💳 智能识别 SD 卡并一键同步部署 (SD Deploy)")
    print("  [8] 🧹 清理工作区临时与构建缓存 (Clean)")
    print("  [0] 退出 (Exit)")
    print("=" * 60)
    try:
        choice = input("请选择操作编号 [0-8]: ").strip()
        if choice == "1":
            data = input("请输入数据集配置文件路径 [默认: configs/coco128.yaml]: ").strip() or "configs/coco128.yaml"
            run_cmd(["tools/pipeline.py", "--data", data, "--epochs", "10", "--imgsz", "320"])
        elif choice == "2":
            cmd_check()
        elif choice == "3":
            cmd_gpu()
        elif choice == "4":
            data = input("请输入数据集配置文件路径 [默认: configs/coco128.yaml]: ").strip() or "configs/coco128.yaml"
            run_cmd(["tools/check_dataset.py", "--data", data])
        elif choice == "5":
            weights = input("请输入 .pt 权重文件路径 [默认: weights/yolo11n.pt]: ").strip() or "weights/yolo11n.pt"
            run_cmd(["scripts/export_onnx.py", "--weights", weights, "--imgsz", "320"])
        elif choice == "6":
            model = input("请输入模型路径 [默认: weights/yolo11n_320.onnx]: ").strip() or "weights/yolo11n_320.onnx"
            data = input("请输入数据集配置文件路径 [默认: configs/coco128.yaml]: ").strip() or "configs/coco128.yaml"
            run_cmd(["tools/generate_deploy_pack.py", "--model", model, "--data", data])
        elif choice == "7":
            run_cmd(["tools/deploy_sd.py"])
        elif choice == "8":
            cmd_clean()
        elif choice == "0":
            print("已退出控制台。")
        else:
            print("未知选项，请输入 0-8 之间的数字。")
    except KeyboardInterrupt:
        print("\n[INFO] 已取消操作。")


def main():
    parser = argparse.ArgumentParser(
        description="K230 视觉 AI 工程 unified CLI 工具 (tools/k230.py)",
        formatter_class=argparse.RawTextHelpFormatter
    )
    sub = parser.add_subparsers(dest="subcommand", help="子命令")

    # check / audit / verify / gpu / clean
    sub.add_parser("check", help="完整工作区审计 + 全流程验证")
    sub.add_parser("audit", help="7 维工作区完整性审计")
    sub.add_parser("verify", help="全流程功能验证")
    sub.add_parser("gpu", help="检查 CUDA / GPU 状态")
    sub.add_parser("clean", help="清理构建与临时文件")

    # check-data
    p_check_data = sub.add_parser("check-data", help="数据集格式预检校验 (tools/check_dataset.py)")
    p_check_data.add_argument("--data", required=True, help="数据集 yaml 配置文件路径")
    p_check_data.add_argument("--strict", action="store_true", help="开启严格模式")

    # cloud-label
    p_label = sub.add_parser("cloud-label", help="多模态大模型云端自动打标 (tools/cloud_label.py)")
    p_label.add_argument("--images", required=True, help="原始图片目录")
    p_label.add_argument("--output", default="datasets/auto_labeled", help="输出目录")
    p_label.add_argument("--classes", required=True, nargs="+", help="类别列表")
    p_label.add_argument("--provider", choices=["stepfun", "qwen", "gemini"], default="stepfun")
    p_label.add_argument("--api-key", default=None, help="API Key (默认从 configs/api_keys.json 或环境变量读取)")

    # test-pipeline
    sub.add_parser("test-pipeline", help="端到端打标+校验+训练全流程一键集成测试 (tools/test_full_pipeline.py)")

    # strict-check
    sub.add_parser("strict-check", help="🔬 工业级高精度、高标准严密综合校验 (tools/strict_high_precision_check.py)")

    # sd-deploy
    p_sd = sub.add_parser("sd-deploy", help="智能识别盘符并一键部署到 SD 卡")
    p_sd.add_argument("--drive", default=None, help="显式指定目标盘符 (如 E:\\)")
    p_sd.add_argument("--source", default="deploy_pack", help="部署包来源")

    # train
    p_train = sub.add_parser("train", help="YOLO 任务训练")
    p_train.add_argument("--data", required=True, help="数据集 yaml 配置文件路径")
    p_train.add_argument("--task", default="detect", choices=["detect", "classify", "segment", "pose", "obb"])
    p_train.add_argument("--epochs", type=int, default=10)
    p_train.add_argument("--imgsz", type=int, default=640)
    p_train.add_argument("--device", default="", help="设备 (如 0 或 cpu)")

    # infer
    p_infer = sub.add_parser("infer", help="模型推理与可视化")
    p_infer.add_argument("--source", required=True, help="图片/视频路径或 0 (摄像头)")
    p_infer.add_argument("--weights", required=True, help="best.pt 或 .onnx 权重路径")
    p_infer.add_argument("--task", default="detect", choices=["detect", "classify", "segment", "pose", "obb"])
    p_infer.add_argument("--imgsz", type=int, default=640)
    p_infer.add_argument("--conf", type=float, default=0.35)
    p_infer.add_argument("--device", default="")

    # track
    p_track = sub.add_parser("track", help="多目标追踪")
    p_track.add_argument("--source", required=True)
    p_track.add_argument("--weights", required=True)
    p_track.add_argument("--imgsz", type=int, default=640)
    p_track.add_argument("--tracker", default="botsort", choices=["bytetrack", "botsort"])
    p_track.add_argument("--device", default="")

    # export
    p_exp = sub.add_parser("export", help="导出静态 Shape ONNX 模型")
    p_exp.add_argument("--weights", required=True)
    p_exp.add_argument("--imgsz", type=int, default=640)
    p_exp.add_argument("--opset", type=int, default=13)
    p_exp.add_argument("--out", default=None)
    p_exp.add_argument("--task", default=None)

    # kmodel
    p_km = sub.add_parser("kmodel", help="nncase ONNX -> .kmodel 转换")
    p_km.add_argument("--model", required=True, help="ONNX 模型路径")
    p_km.add_argument("--dataset", required=True, help="校准集图片目录")
    p_km.add_argument("--imgsz", type=int, default=320)
    p_km.add_argument("--output", default=None)

    # pack
    p_pk = sub.add_parser("pack", help="构建 K230 板端部署包")
    p_pk.add_argument("--model", required=True)
    p_pk.add_argument("--data", default=None)
    p_pk.add_argument("--labels", default=None)
    p_pk.add_argument("--task", default="detect")
    p_pk.add_argument("--imgsz", type=int, default=320)
    p_pk.add_argument("--output", default="deploy_pack")

    # pipeline
    p_pipe = sub.add_parser("pipeline", help="一键全流程闭环管线 (train -> export -> kmodel -> pack)")
    p_pipe.add_argument("--data", required=True, help="数据集 yaml 配置")
    p_pipe.add_argument("--epochs", type=int, default=10)
    p_pipe.add_argument("--imgsz", type=int, default=320)
    p_pipe.add_argument("--task", default="detect")
    p_pipe.add_argument("--output", default="deploy_pack")
    p_pipe.add_argument("--device", default="")

    args = parser.parse_args()

    if not args.subcommand:
        cmd_menu()
        return

    cmd_map = {
        "check": cmd_check,
        "audit": cmd_audit,
        "verify": cmd_verify,
        "gpu": cmd_gpu,
        "check-data": lambda: cmd_check_data(args),
        "cloud-label": lambda: cmd_label(args),
        "test-pipeline": cmd_test_pipeline,
        "strict-check": cmd_strict_check,
        "sd-deploy": lambda: cmd_sd_deploy(args),
        "train": lambda: cmd_train(args),
        "infer": lambda: cmd_infer(args),
        "track": lambda: cmd_track(args),
        "export": lambda: cmd_export(args),
        "kmodel": lambda: cmd_kmodel(args),
        "pack": lambda: cmd_pack(args),
        "pipeline": lambda: cmd_pipeline(args),
        "clean": cmd_clean,
    }
    action = cmd_map.get(args.subcommand)
    if action:
        action()


if __name__ == "__main__":
    main()
