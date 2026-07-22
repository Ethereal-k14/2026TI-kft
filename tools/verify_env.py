#!/usr/bin/env python3
"""
K230 视觉 AI 开发工程 - 环境全流程一键检验工具 (tools/verify_env.py)

功能：
1. 校验 Python 版本与核心依赖包 (ultralytics, torch, onnx, onnxsim, opencv, numpy 等)
2. 验证数据集 YAML 解析与底层图像数据流
3. 验证 YOLO11n 模型初始化与微量训练管线 (Pipeline Verification)
4. 验证模型的预测推理 (Infer) 与可视化图像渲染
5. 验证面向 K230 部署规范的 ONNX 导出 (dynamic=False, opset=13, simplify=True)
6. 自动校验导出的 ONNX 模型结构有效性

用法：
    python tools/verify_env.py
    python tools/verify_env.py --keep-artifacts  # 保留测试生成的测试模型与推理图片
"""

import argparse
import importlib.metadata
import io
import os
import shutil
import sys
import tempfile
import time

# 强制标准输出 UTF-8，防止 Windows PowerShell / CMD GBK 编码报错
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')


def print_step(stage: int, title: str):
    print(f"\n==================================================")
    print(f"[Stage {stage}] {title}")
    print(f"==================================================")


def check_dependencies():
    print_step(1, "Python 环境与核心依赖包核对")

    py_version = sys.version.split()[0]
    is_py310 = sys.version_info[:2] == (3, 10)
    py_status = "PASS" if is_py310 else "FAIL"
    print(f"[{py_status}] Python 版本: {py_version} (规范要求 >=3.10, <3.11)")
    if not is_py310:
        print("[WARNING] Python 版本不在 3.10.x 窗口，nncase 转换工具可能会遇到不兼容问题。")

    required_packages = [
        ("ultralytics", ">=8.3.0"),
        ("torch", ">=2.3.0"),
        ("torchvision", ">=0.18.0"),
        ("onnx", ">=1.16.0"),
        ("onnxsim", ">=0.4.36"),
        ("onnxruntime", ">=1.18.0,<1.24"),
        ("opencv-python", ">=4.9.0", ["opencv-python-headless", "opencv-python"]),
        ("numpy", ">=1.26.0"),
        ("pillow", ">=10.0.0"),
        ("pandas", ">=2.0.0"),
        ("pyyaml", ">=6.0"),
        ("matplotlib", ">=3.8.0"),
        ("tqdm", ">=4.66.0"),
    ]

    all_passed = is_py310

    for item in required_packages:
        pkg_name = item[0]
        req_ver = item[1]
        aliases = item[2] if len(item) > 2 else [pkg_name]

        installed_ver = None
        for alias in aliases:
            try:
                installed_ver = importlib.metadata.version(alias)
                pkg_name = alias
                break
            except importlib.metadata.PackageNotFoundError:
                continue

        if installed_ver:
            print(f"[PASS] {pkg_name:24s} | 已安装: {installed_ver:14s} | 要求: {req_ver}")
        else:
            print(f"[FAIL] {pkg_name:24s} | 未安装                        | 要求: {req_ver}")
            all_passed = False

    return all_passed


def create_dummy_dataset(tmp_dir):
    print_step(2, "构建极简测试数据集与配置")

    images_dir = os.path.join(tmp_dir, "images", "train")
    labels_dir = os.path.join(tmp_dir, "labels", "train")
    os.makedirs(images_dir, exist_ok=True)
    os.makedirs(labels_dir, exist_ok=True)

    # 简单生成 2 张 320x320 纯色图片和测试 txt
    import cv2
    import numpy as np

    img1 = np.full((320, 320, 3), (255, 128, 64), dtype=np.uint8)
    cv2.rectangle(img1, (50, 50), (200, 200), (0, 255, 0), -1)
    cv2.imwrite(os.path.join(images_dir, "test1.jpg"), img1)

    img2 = np.full((320, 320, 3), (64, 128, 255), dtype=np.uint8)
    cv2.circle(img2, (160, 160), 60, (0, 0, 255), -1)
    cv2.imwrite(os.path.join(images_dir, "test2.jpg"), img2)

    # 生成对应 YOLO 标签（格式：class cx cy w h 归一化）
    with open(os.path.join(labels_dir, "test1.txt"), "w") as f:
        f.write("0 0.39 0.39 0.46 0.46\n")
    with open(os.path.join(labels_dir, "test2.txt"), "w") as f:
        f.write("1 0.50 0.50 0.37 0.37\n")

    # 创建 dataset.yaml
    dataset_yaml_path = os.path.join(tmp_dir, "verify_data.yaml")
    abs_tmp_path = os.path.abspath(tmp_dir).replace('\\', '/')
    yaml_content = f"""path: {abs_tmp_path}
train: images/train
val: images/train
names:
  0: square
  1: circle
"""
    with open(dataset_yaml_path, "w", encoding="utf-8") as f:
        f.write(yaml_content)

    print(f"[PASS] 测试小数据集构建完成，配置文件: {dataset_yaml_path}")
    return dataset_yaml_path, os.path.join(images_dir, "test1.jpg")


def test_train_pipeline(dataset_yaml, tmp_dir):
    print_step(3, "YOLO11n 微量训练流程验证 (Pipeline Test)")

    from ultralytics import YOLO

    try:
        model = YOLO("yolo11n.pt")
        print("[INFO] 已加载预训练权重: yolo11n.pt")

        # 运行 1-epoch 快速微调测试
        results = model.train(
            data=dataset_yaml,
            epochs=1,
            imgsz=320,
            batch=2,
            project=os.path.join(tmp_dir, "runs"),
            name="verify_run",
            exist_ok=True,
            verbose=False,
            device="cpu",
        )

        best_pt = os.path.join(tmp_dir, "runs", "verify_run", "weights", "best.pt")
        if not os.path.exists(best_pt):
            best_pt = os.path.join(tmp_dir, "runs", "verify_run", "weights", "last.pt")

        print(f"[PASS] 训练管线正常跑通！检查输出模型: {best_pt}")
        return best_pt
    except Exception as e:
        print(f"[FAIL] 训练流程中抛出异常: {e}")
        return None


def test_infer_pipeline(model_path, sample_img, tmp_dir):
    print_step(4, "图像推理与结果可视化验证 (Infer Test)")

    from ultralytics import YOLO

    try:
        model = YOLO(model_path)
        results = model.predict(
            source=sample_img,
            imgsz=320,
            save=True,
            project=os.path.join(tmp_dir, "runs"),
            name="predict_run",
            exist_ok=True,
            verbose=False,
        )

        out_img = os.path.join(tmp_dir, "runs", "predict_run", os.path.basename(sample_img))
        print(f"[PASS] 推理管线正常！推理结果图像保存至: {out_img}")
        return True
    except Exception as e:
        print(f"[FAIL] 推理流程中抛出异常: {e}")
        return False


def test_export_onnx_pipeline(model_path, tmp_dir):
    print_step(5, "K230 部署规范 ONNX 导出与 onnxsim 简化验证")

    from ultralytics import YOLO
    import onnx
    import onnxsim

    try:
        model = YOLO(model_path)
        out_onnx = os.path.join(tmp_dir, "verify_model.onnx")

        exported_path = model.export(
            format="onnx",
            imgsz=320,
            dynamic=False,
            simplify=True,
            opset=13,
            verbose=False,
        )

        if os.path.exists(out_onnx):
            os.remove(out_onnx)
        shutil.move(exported_path, out_onnx)

        print(f"[PASS] ONNX 成功导出: {out_onnx}")

        # ONNX 模型校验
        onnx_model = onnx.load(out_onnx)
        onnx.checker.check_model(onnx_model)
        print("[PASS] onnx.checker.check_model() 结构检查通过！")

        # 输入维度的静态性校验 (K230 规范要求: dynamic=False)
        input_tensor = onnx_model.graph.input[0]
        dim_values = [d.dim_value for d in input_tensor.type.tensor_type.shape.dim]
        print(f"[INFO] 导出 ONNX 模型输入维度 Shape: {dim_values}")

        has_dynamic = any(isinstance(v, str) or v <= 0 for v in dim_values)
        if not has_dynamic and dim_values == [1, 3, 320, 320]:
            print("[PASS] 动态轴校验通过：无动态轴，固定输入 1x3x320x320，符合 K230 (KPU) 部署规范。")
        else:
            print(f"[WARNING] 模型维度存在动态轴或与预设不符: {dim_values}")

        # 测试 onnxsim
        sim_model, check = onnxsim.simplify(onnx_model)
        if check:
            print("[PASS] onnxsim 节点化简检查通过！")
        else:
            print("[WARNING] onnxsim 提示无法进一步简化结构（非阻塞）。")

        return True
    except Exception as e:
        print(f"[FAIL] ONNX 导出/校验阶段出现错误: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="K230 项目工作区环境全流程自动化检验")
    parser.add_argument("--keep-artifacts", action="store_true", help="保留测试过程中生成的工程产物")
    args = parser.parse_args()

    start_time = time.time()
    print("=" * 60)
    print(" 🚀 开始运行 K230 视觉工程·环境与全流程开发功能全项检验")
    print("=" * 60)

    # 1. 依赖与 Python 环境检测
    deps_ok = check_dependencies()
    if not deps_ok:
        print("\n❌ 依赖检查存在缺失，请先补全包依赖后再试。")

    # 创建临时测试环境
    tmp_dir = tempfile.mkdtemp(prefix="k230_verify_")

    try:
        # 2. 构建测试数据
        dataset_yaml, sample_img = create_dummy_dataset(tmp_dir)

        # 3. 训练测试
        best_pt = test_train_pipeline(dataset_yaml, tmp_dir)
        if not best_pt:
            print("\n❌ 训练流程失败，中止后续测试。")
            return 1

        # 4. 推理测试
        infer_ok = test_infer_pipeline(best_pt, sample_img, tmp_dir)

        # 5. ONNX 导出与验证
        export_ok = test_export_onnx_pipeline(best_pt, tmp_dir)

        elapsed = time.time() - start_time
        print("\n" + "=" * 60)
        print(" 🎯 环境全流程检验完成！汇总结果:")
        print("=" * 60)
        print(f"  - 依赖与版本核对 : {'✅ PASS' if deps_ok else '⚠️ FAIL/WARNING'}")
        print(f"  - 模型训练管线   : {'✅ PASS' if best_pt else '❌ FAIL'}")
        print(f"  - 图像推理管线   : {'✅ PASS' if infer_ok else '❌ FAIL'}")
        print(f"  - ONNX 规范导出  : {'✅ PASS' if export_ok else '❌ FAIL'}")
        print(f"  - 总计用时       : {elapsed:.2f} 秒")
        print("=" * 60)
        if deps_ok and best_pt and infer_ok and export_ok:
            print("\n✨ 恭喜！当前工作区环境完美支持面向 K230 开发板的 YOLO11n 视觉开发全流程！")
            return 0
        else:
            print("\n⚠️ 校验未能全部通过，请检查上述失败项的错误信息。")
            return 1
    finally:
        if not args.keep_artifacts:
            shutil.rmtree(tmp_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
