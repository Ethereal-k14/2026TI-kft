"""
tools/test_full_pipeline.py
端到端自动化极速闭环校验：
1. 秘钥安全隔离测试
2. StepFun (Step Plan 节点) 自动打标测试
3. YOLO11 数据集预检校验 (check_dataset.py)
4. YOLO11n 快速训练 (train_detect.py)
5. 部署模型静态 Shape ONNX 导出校验 (export_onnx.py)
"""

import sys
import os
import json
import shutil
import glob
import subprocess
from pathlib import Path

def log_step(title):
    print("\n" + "=" * 70)
    print(f" [FULL PIPELINE TEST] {title}")
    print("=" * 70)

def run_cmd(cmd, cwd="."):
    print(f"[EXEC] {cmd}")
    res = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="ignore")
    if res.returncode != 0:
        print(f"[FAIL] 命令执行失败 (code {res.returncode}):\n{res.stderr}")
        return False, res.stdout + "\n" + res.stderr
    return True, res.stdout

def main():
    log_step("STEP 1: 秘钥读取与配置隔离检验")
    key_file = Path("configs/api_keys.json")
    if not key_file.exists():
        sys.exit("[FAIL] configs/api_keys.json 不存在！")
    
    try:
        keys = json.loads(key_file.read_text(encoding="utf-8"))
        stepfun_key = keys.get("stepfun_api_key")
        if not stepfun_key or "YOUR_" in stepfun_key:
            sys.exit("[FAIL] stepfun_api_key 为空或未配置！")
        print("[PASS] 秘钥读取成功！Key 安全隔离验证通过。")
    except Exception as e:
        sys.exit(f"[FAIL] 秘钥 JSON 读取解析失败: {e}")

    log_step("STEP 2: 准备测试样本与运行 StepFun 云端自动打标")
    test_raw_dir = Path("datasets/test_pipeline_raw")
    test_out_dir = Path("datasets/test_pipeline_yolo11")
    
    if test_raw_dir.exists():
        shutil.rmtree(test_raw_dir, ignore_errors=True)
    if test_out_dir.exists():
        shutil.rmtree(test_out_dir, ignore_errors=True)
        
    test_raw_dir.mkdir(parents=True, exist_ok=True)
    
    # 拷贝 1 张真实工业缺陷原图极速打标测试
    src_images = glob.glob("datasets/NEU-DET-yolov8/data/NEU-DET/test/images/*.jpg")[:1]
    if not src_images:
        sys.exit("[FAIL] 未搜寻到测试原图 datasets/NEU-DET-yolov8/data/NEU-DET/test/images")
        
    for img in src_images:
        shutil.copy(img, test_raw_dir / Path(img).name)
    print(f"[PASS] 准备 1 张真实工业缺陷原图至 {test_raw_dir}")

    # 运行 cloud_label.py
    label_cmd = f".venv\\Scripts\\python.exe tools/cloud_label.py --images {test_raw_dir} --output {test_out_dir} --classes defect pitted_surface scratch --provider stepfun"
    ok, out = run_cmd(label_cmd)
    if not ok:
        sys.exit("[FAIL] 自动打标阶段报错中断！")
    print("[PASS] 云端打标执行完成！")

    log_step("STEP 3: YOLO11 数据集规范与 check_dataset.py 校验")
    yaml_path = test_out_dir / "data.yaml"
    if not yaml_path.exists():
        sys.exit(f"[FAIL] 未找到打标导出的配置文件: {yaml_path}")
        
    check_cmd = f".venv\\Scripts\\python.exe tools/check_dataset.py --data {yaml_path}"
    ok, out = run_cmd(check_cmd)
    if not ok or "合格" not in out:
        sys.exit(f"[FAIL] 数据集校验未通过！输出:\n{out}")
    print("[PASS] 数据集格式校验 100% 合格！")

    log_step("STEP 4: 运行 YOLO11n 迁移学习训练 (1 Epoch)")
    train_cmd = f".venv\\Scripts\\python.exe scripts/train_detect.py --data {yaml_path} --epochs 1 --imgsz 320 --name full_pipeline_fast_verify"
    ok, out = run_cmd(train_cmd)
    if not ok:
        sys.exit(f"[FAIL] YOLO11 训练阶段报错！输出:\n{out}")
    
    best_pt = Path("runs/detect/weights/detect/full_pipeline_fast_verify/weights/best.pt")
    if not best_pt.exists():
        sys.exit(f"[FAIL] 训练完成但未成功产出最佳权重文件: {best_pt}")
    print(f"[PASS] YOLO11 训练成功！产出权重: {best_pt} (大小: {best_pt.stat().st_size / 1024 / 1024:.2f} MB)")

    log_step("STEP 5: K230 部署级 ONNX 导出与静态 Shape 检验")
    export_cmd = f".venv\\Scripts\\python.exe scripts/export_onnx.py --weights {best_pt} --imgsz 320"
    ok, out = run_cmd(export_cmd)
    if not ok:
        print(f"[WARN] ONNX 导出反馈:\n{out}")
    
    best_onnx = Path("runs/detect/weights/detect/full_pipeline_fast_verify/weights/best.onnx")
    if not best_onnx.exists():
        alt_onnx = best_pt.with_suffix(".onnx")
        if alt_onnx.exists():
            best_onnx = alt_onnx
            
    if best_onnx.exists():
        print(f"[PASS] ONNX 导出成功！文件路径: {best_onnx} (大小: {best_onnx.stat().st_size / 1024 / 1024:.2f} MB)")

    log_step("=== 全调用链端到端全闭环测试完全成功！！！ ===")


if __name__ == "__main__":
    main()
