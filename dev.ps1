#!/usr/bin/env pwsh
<#
.SYNOPSIS
    K230 工程开发快捷命令集

.DESCRIPTION
    封装所有常用开发操作为简短命令，避免记忆长命令行。
    使用工程内 .venv 虚拟环境，无需激活虚拟环境即可运行。

.EXAMPLE
    .\dev.ps1 check
    .\dev.ps1 train-detect configs/coco128.yaml 10 320
    .\dev.ps1 infer test.jpg weights/detect/yolo11n/weights/best.pt
    .\dev.ps1 export weights/detect/yolo11n/weights/best.pt 320
    .\dev.ps1 gpu
#>

param(
    [Parameter(Position=0)]
    [string]$Command = "help",
    [Parameter(Position=1)]
    [string]$Arg1 = "",
    [Parameter(Position=2)]
    [string]$Arg2 = "",
    [Parameter(Position=3)]
    [string]$Arg3 = ""
)

Set-Location $PSScriptRoot

# 定位 Python：优先用工程 .venv，回退到系统 python
$PY = ".\.venv\Scripts\python.exe"
if (-not (Test-Path $PY)) {
    Write-Warning ".venv 未找到，请先运行: uv sync"
    $PY = "python"
}

function Run-Py {
    param([string[]]$Args)
    & $PY @Args
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Show-Help {
    Write-Host ""
    Write-Host "K230 视觉工程 · 快捷命令集 (dev.ps1)" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  环境检查" -ForegroundColor White
    Write-Host "  .\dev.ps1 check              # 完整工作区审计 + 全流程验证" -ForegroundColor Green
    Write-Host "  .\dev.ps1 audit              # 仅 7 维工作区完整性审计" -ForegroundColor Green
    Write-Host "  .\dev.ps1 verify             # 仅全流程功能验证（~20s）" -ForegroundColor Green
    Write-Host "  .\dev.ps1 gpu                # 检查 GPU / CUDA 状态" -ForegroundColor Green
    Write-Host ""
    Write-Host "  训练（5 种任务）" -ForegroundColor White
    Write-Host "  .\dev.ps1 train-detect  <data.yaml> [epochs=100] [imgsz=640]" -ForegroundColor Yellow
    Write-Host "  .\dev.ps1 train-cls     <data_dir>  [epochs=50]  [imgsz=224]" -ForegroundColor Yellow
    Write-Host "  .\dev.ps1 train-seg     <data.yaml> [epochs=100] [imgsz=640]" -ForegroundColor Yellow
    Write-Host "  .\dev.ps1 train-pose    <data.yaml> [epochs=100] [imgsz=640]" -ForegroundColor Yellow
    Write-Host "  .\dev.ps1 train-obb     <data.yaml> [epochs=100] [imgsz=320]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  推理与追踪" -ForegroundColor White
    Write-Host "  .\dev.ps1 infer   <source> <weights.pt>          # 推理可视化" -ForegroundColor Magenta
    Write-Host "  .\dev.ps1 track   <source> <weights.pt>          # 多目标追踪" -ForegroundColor Magenta
    Write-Host ""
    Write-Host "  导出与部署" -ForegroundColor White
    Write-Host "  .\dev.ps1 export  <weights.pt>  [imgsz=320]      # 导出 ONNX" -ForegroundColor Blue
    Write-Host "  .\dev.ps1 kmodel  <model.onnx>  [imgsz=320]      # 转换 .kmodel" -ForegroundColor Blue
    Write-Host "  .\dev.ps1 pack    <model.kmodel> <data.yaml>     # 打包部署包" -ForegroundColor Blue
    Write-Host ""
    Write-Host "  .\dev.ps1 clean              # 清理临时文件夹 (runs, dump, __pycache__)" -ForegroundColor DarkCyan
    Write-Host "  .\dev.ps1 help               # 显示此帮助" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "提示：使用 Tab 补全命令名；所有路径支持相对路径。" -ForegroundColor DarkGray
    Write-Host ""
}

switch ($Command.ToLower()) {
    "help" { Show-Help }

    "check" {
        Write-Host "[1/2] 运行工作区完整性审计..." -ForegroundColor Cyan
        Run-Py "tools/audit_workspace.py"
        Write-Host "[2/2] 运行全流程功能验证..." -ForegroundColor Cyan
        Run-Py "tools/verify_env.py"
    }

    "audit" { Run-Py "tools/audit_workspace.py" }

    "verify" { Run-Py "tools/verify_env.py" }

    "gpu" {
        Run-Py "-c", "import torch; cuda=torch.cuda.is_available(); print('CUDA可用:', cuda); print('设备:', torch.cuda.get_device_name(0) if cuda else 'CPU only'); print('PyTorch:', torch.__version__)"
    }

    "train-detect" {
        if (-not $Arg1) { Write-Error "用法: .\dev.ps1 train-detect <data.yaml> [epochs] [imgsz]"; exit 1 }
        $epochs = if ($Arg2) { $Arg2 } else { "100" }
        $imgsz  = if ($Arg3) { $Arg3 } else { "640" }
        Write-Host "检测训练: data=$Arg1 epochs=$epochs imgsz=$imgsz" -ForegroundColor Yellow
        Run-Py "scripts/train_detect.py", "--data", $Arg1, "--epochs", $epochs, "--imgsz", $imgsz
    }

    "train-cls" {
        if (-not $Arg1) { Write-Error "用法: .\dev.ps1 train-cls <data_dir> [epochs] [imgsz]"; exit 1 }
        $epochs = if ($Arg2) { $Arg2 } else { "50" }
        $imgsz  = if ($Arg3) { $Arg3 } else { "224" }
        Write-Host "分类训练: data=$Arg1 epochs=$epochs imgsz=$imgsz" -ForegroundColor Yellow
        Run-Py "scripts/train_classify.py", "--data", $Arg1, "--epochs", $epochs, "--imgsz", $imgsz
    }

    "train-seg" {
        if (-not $Arg1) { Write-Error "用法: .\dev.ps1 train-seg <data.yaml> [epochs] [imgsz]"; exit 1 }
        $epochs = if ($Arg2) { $Arg2 } else { "100" }
        $imgsz  = if ($Arg3) { $Arg3 } else { "640" }
        Write-Host "分割训练: data=$Arg1 epochs=$epochs imgsz=$imgsz" -ForegroundColor Yellow
        Run-Py "scripts/train_segment.py", "--data", $Arg1, "--epochs", $epochs, "--imgsz", $imgsz
    }

    "train-pose" {
        if (-not $Arg1) { Write-Error "用法: .\dev.ps1 train-pose <data.yaml> [epochs] [imgsz]"; exit 1 }
        $epochs = if ($Arg2) { $Arg2 } else { "100" }
        $imgsz  = if ($Arg3) { $Arg3 } else { "640" }
        Write-Host "姿态训练: data=$Arg1 epochs=$epochs imgsz=$imgsz" -ForegroundColor Yellow
        Run-Py "scripts/train_pose.py", "--data", $Arg1, "--epochs", $epochs, "--imgsz", $imgsz
    }

    "train-obb" {
        if (-not $Arg1) { Write-Error "用法: .\dev.ps1 train-obb <data.yaml> [epochs] [imgsz]"; exit 1 }
        $epochs = if ($Arg2) { $Arg2 } else { "100" }
        $imgsz  = if ($Arg3) { $Arg3 } else { "320" }
        Write-Host "旋转框训练: data=$Arg1 epochs=$epochs imgsz=$imgsz" -ForegroundColor Yellow
        Run-Py "scripts/train_obb.py", "--data", $Arg1, "--epochs", $epochs, "--imgsz", $imgsz
    }

    "infer" {
        if (-not $Arg1 -or -not $Arg2) { Write-Error "用法: .\dev.ps1 infer <source> <weights.pt>"; exit 1 }
        Write-Host "推理: source=$Arg1 weights=$Arg2" -ForegroundColor Magenta
        Run-Py "scripts/infer.py", "--source", $Arg1, "--weights", $Arg2
    }

    "track" {
        if (-not $Arg1 -or -not $Arg2) { Write-Error "用法: .\dev.ps1 track <source> <weights.pt>"; exit 1 }
        Write-Host "追踪: source=$Arg1 weights=$Arg2" -ForegroundColor Magenta
        Run-Py "scripts/track.py", "--source", $Arg1, "--weights", $Arg2, "--tracker", "botsort"
    }

    "export" {
        if (-not $Arg1) { Write-Error "用法: .\dev.ps1 export <weights.pt> [imgsz=320]"; exit 1 }
        $imgsz = if ($Arg2) { $Arg2 } else { "320" }
        $stem  = [System.IO.Path]::GetFileNameWithoutExtension($Arg1)
        $out   = "weights/${stem}_${imgsz}.onnx"
        Write-Host "导出 ONNX: $Arg1 -> $out (imgsz=$imgsz)" -ForegroundColor Blue
        Run-Py "scripts/export_onnx.py", "--weights", $Arg1, "--imgsz", $imgsz, "--out", $out
    }

    "kmodel" {
        if (-not $Arg1) { Write-Error "用法: .\dev.ps1 kmodel <model.onnx> [imgsz=320]"; exit 1 }
        $imgsz = if ($Arg2) { $Arg2 } else { "320" }
        $out   = $Arg1 -replace "\.onnx$", ".kmodel"
        Write-Host "转换 kmodel: $Arg1 -> $out (imgsz=$imgsz)" -ForegroundColor Blue
        Run-Py "tools/to_kmodel.py", "--model", $Arg1, "--dataset", "datasets/calib", "--input-size", $imgsz, $imgsz, "--output", $out
    }

    "clean" {
        Write-Host "清理临时运行与缓存目录..." -ForegroundColor Cyan
        Remove-Item -Recurse -Force "runs", "dump", "deploy_pack" -ErrorAction SilentlyContinue
        Get-ChildItem -Recurse -Filter "__pycache__" -Directory -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "[OK] 清理完毕，工作区恢复干净状态。" -ForegroundColor Green
    }

    "pack" {
        if (-not $Arg1 -or -not $Arg2) { Write-Error "用法: .\dev.ps1 pack <model.kmodel> <data.yaml>"; exit 1 }
        Write-Host "打包部署包: model=$Arg1 data=$Arg2" -ForegroundColor Blue
        Run-Py "tools/generate_deploy_pack.py", "--model", $Arg1, "--data", $Arg2, "--task", "detect", "--imgsz", "320"
    }

    default {
        Write-Host "未知命令: $Command" -ForegroundColor Red
        Show-Help
        exit 1
    }
}
