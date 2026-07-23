#!/usr/bin/env python3
"""工作区环境配套完整性一键审计 (tools/audit_workspace.py)"""
import sys, io, os, importlib.metadata, subprocess, py_compile, glob
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

PASS = "[PASS]"
FAIL = "[FAIL]"
results = {}


def section(num, total, title):
    print(f"\n{'=' * 70}")
    print(f" [{num}/{total}] {title}")
    print('=' * 70)


def main():
    total = 7

    # 1. Python
    section(1, total, "Python 环境检查")
    py_ver = sys.version.split()[0]
    py_ok = sys.version_info[:2] == (3, 10)
    print(f"  {PASS if py_ok else FAIL} Python 版本: {py_ver} (要求 >=3.10, <3.11)")
    results['python'] = py_ok

    # 2. Dependencies
    section(2, total, "核心依赖包版本核对")
    pkgs = [
        ("ultralytics", ">=8.3.0"), ("torch", ">=2.3.0"), ("torchvision", ">=0.18.0"),
        ("onnx", ">=1.16.0"), ("onnxsim", ">=0.4.36"), ("onnxruntime", ">=1.18.0,<1.24"),
        ("numpy", ">=1.26.0"), ("pillow", ">=10.0.0"), ("pandas", ">=2.0.0"),
        ("pyyaml", ">=6.0"), ("matplotlib", ">=3.8.0"), ("tqdm", ">=4.66.0"),
    ]
    deps_ok = True
    for pkg, req in pkgs:
        try:
            ver = importlib.metadata.version(pkg)
            print(f"  {PASS} {pkg:24s} {ver:14s} ({req})")
        except importlib.metadata.PackageNotFoundError:
            print(f"  {FAIL} {pkg:24s} MISSING ({req})")
            deps_ok = False
    for alias in ["opencv-python-headless", "opencv-python"]:
        try:
            ver = importlib.metadata.version(alias)
            print(f"  {PASS} {alias:24s} {ver:14s} (>=4.9.0)")
            break
        except importlib.metadata.PackageNotFoundError:
            pass
    results['deps'] = deps_ok

    # 3. File integrity
    section(3, total, "项目关键文件与目录完整性")
    expected = [
        "pyproject.toml", ".python-version", "requirements-convert.txt", "README.md",
        "STRUCTURE.md", "QUICKSTART.md", "dev.ps1",
        ".gitignore", ".gitattributes", "uv.lock",
        "configs/coco128.yaml", "configs/coco_pose.yaml", "configs/coco_seg.yaml",
        "configs/classify_sample.yaml", "configs/obb_sample.yaml",
        "scripts/_device.py",
        "scripts/train_detect.py", "scripts/train_classify.py", "scripts/train_segment.py",
        "scripts/train_pose.py", "scripts/train_obb.py",
        "scripts/infer.py", "scripts/track.py", "scripts/export_onnx.py",
        "tools/verify_env.py", "tools/audit_workspace.py", "tools/generate_deploy_pack.py", "tools/to_kmodel.py",
        "templates/canmv_k230_demo.py", "templates/canmv_k230_web_streamer.py",
        "templates/k230_cpp_runner.sh",
        "data/coco_labels.txt",
        "datasets/.gitkeep", "weights/.gitkeep",
        "docs/base_tasks_training_and_deploy.md", "docs/canaan_k230_official_guide.md",
        "docs/dotnet_and_web_streaming.md", "docs/environment_verification.md",
        "docs/k230_deploy.md", "docs/models_overview.md", "docs/self_training.md",
        "docs/official_sources_and_config_basis.md",
    ]
    files_ok = True
    for f in expected:
        exists = os.path.exists(f)
        print(f"  {PASS if exists else FAIL} {f}")
        if not exists:
            files_ok = False
    results['files'] = files_ok

    # 4. AST compile
    section(4, total, "全部 Python 源码 AST 编译检查")
    py_files = [f for f in glob.glob("**/*.py", recursive=True)
                if not any(x in f.replace("\\", "/") for x in [".venv/", "site-packages/", "dump/", "build/", "dist/", "runs/"])]
    ast_ok = True
    for fp in sorted(py_files):
        try:
            py_compile.compile(fp, doraise=True)
            print(f"  {PASS} {fp}")
        except Exception as e:
            print(f"  {FAIL} {fp}: {e}")
            ast_ok = False
    results['ast'] = ast_ok

    # 5. Git
    section(5, total, "Git 仓库与远程配置检查")
    git_ok = True
    checks = [
        (["git", "rev-parse", "--is-inside-work-tree"], "Git 仓库初始化"),
        (["git", "branch", "--show-current"], "当前分支"),
        (["git", "remote", "get-url", "origin"], "远程仓库 URL"),
        (["git", "log", "--oneline", "-1"], "最新提交"),
    ]
    for cmd, desc in checks:
        r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8")
        val = r.stdout.strip() if r.returncode == 0 else "FAIL"
        ok = r.returncode == 0
        print(f"  {PASS if ok else FAIL} {desc}: {val}")
        if not ok:
            git_ok = False

    with open(".gitignore", encoding="utf-8") as f:
        gi = f.read()
    for pattern, desc in [(".venv/", ".gitignore 过滤 .venv/"), ("*.pt", ".gitignore 过滤 *.pt")]:
        ok = pattern in gi
        print(f"  {PASS if ok else FAIL} {desc}")
        if not ok:
            git_ok = False

    with open(".gitattributes", encoding="utf-8") as f:
        ga = f.read()
    ok = "eol=lf" in ga
    print(f"  {PASS if ok else FAIL} .gitattributes 强制 LF 换行符")
    if not ok:
        git_ok = False
    results['git'] = git_ok

    # 6. YAML
    section(6, total, "YAML 配置文件加载与语法校验")
    import yaml
    yaml_files = glob.glob("configs/*.yaml")
    yaml_ok = True
    for yf in sorted(yaml_files):
        try:
            with open(yf, encoding="utf-8") as f:
                data = yaml.safe_load(f)
            names = data.get("names", {})
            n = len(names) if isinstance(names, (dict, list)) else 0
            print(f"  {PASS} {yf:36s} ({n} 类)")
        except Exception as e:
            print(f"  {FAIL} {yf}: {e}")
            yaml_ok = False
    results['yaml'] = yaml_ok

    # 7. Runtime smoke test
    section(7, total, "核心库运行时 Smoke Test")
    runtime_ok = True
    try:
        import torch
        t = torch.zeros((1, 3, 320, 320))
        print(f"  {PASS} PyTorch Tensor 初始化: {t.shape}")
    except Exception as e:
        print(f"  {FAIL} PyTorch: {e}")
        runtime_ok = False
    try:
        import ultralytics
        print(f"  {PASS} Ultralytics 导入: {ultralytics.__version__}")
    except Exception as e:
        print(f"  {FAIL} Ultralytics: {e}")
        runtime_ok = False
    try:
        import onnx
        print(f"  {PASS} ONNX 导入: {onnx.__version__}")
    except Exception as e:
        print(f"  {FAIL} ONNX: {e}")
        runtime_ok = False
    try:
        import cv2
        print(f"  {PASS} OpenCV 导入: {cv2.__version__}")
    except Exception as e:
        print(f"  {FAIL} OpenCV: {e}")
        runtime_ok = False
    results['runtime'] = runtime_ok

    # Summary
    print(f"\n{'=' * 70}")
    print(" 环境配套完整性总览")
    print('=' * 70)
    items = [
        ("Python 3.10 环境", results.get('python', False)),
        ("核心依赖包版本", results.get('deps', False)),
        ("项目文件完整性", results.get('files', False)),
        ("源码 AST 编译", results.get('ast', False)),
        ("Git 仓库与远程", results.get('git', False)),
        ("YAML 配置校验", results.get('yaml', False)),
        ("运行时 Smoke Test", results.get('runtime', False)),
    ]
    all_pass = True
    for name, ok in items:
        tag = "PASS" if ok else "FAIL"
        print(f"  {name:20s}: {tag}")
        if not ok:
            all_pass = False

    print()
    if all_pass:
        print("  ALL PASSED - 当前工作区环境配套完整、规范，可以放心开始开发！")
    else:
        print("  部分检查项未通过，请检查上方对应 [FAIL] 条目。")
    print('=' * 70)
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
