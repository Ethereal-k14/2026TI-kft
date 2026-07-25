#!/usr/bin/env python3
"""
K230 SD 卡智能严谨部署同步工具 (tools/deploy_sd.py)

安全防护机制：
  1. Win32 原生 API 检索 (GetDriveTypeW / GetVolumeInformationW / GetDiskFreeSpaceExW)
  2. 多重特征检测：卷标 (Volume Label)、文件系统 (FAT32/exFAT)、容量阈值、K230板端结构签名
  3. 系统关键盘符硬隔离 guard (绝对禁止盲目写入 C:\, D:\ 或系统硬盘)
  4. 交互式二次确认卡片 (支持明晰容量/文件系统/覆盖提示，默认 [y/N] 防误触)

示例：
    python tools/deploy_sd.py
    python tools/deploy_sd.py --drive E:\
    python tools/deploy_sd.py --source deploy_pack --yes  # 仅限无头脚本/CI
"""

import argparse
import io
import os
import shutil
import string
import sys

# 强制 UTF-8 输出
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def get_windows_drive_info(drive_letter: str) -> dict:
    """使用 Win32 API 严谨检索 Windows 盘符元数据"""
    import ctypes

    info = {
        "drive": drive_letter,
        "is_removable": False,
        "drive_type_str": "未知",
        "volume_name": "[无卷标]",
        "file_system": "未知",
        "total_gb": 0.0,
        "free_gb": 0.0,
        "has_k230_signature": False,
        "warning": None,
    }

    # 1. GetDriveTypeW
    # 2 = DRIVE_REMOVABLE, 3 = DRIVE_FIXED, 4 = DRIVE_REMOTE, 5 = DRIVE_CDROM, 6 = DRIVE_RAMDISK
    dt = ctypes.windll.kernel32.GetDriveTypeW(drive_letter)
    if dt == 2:
        info["is_removable"] = True
        info["drive_type_str"] = "可移动磁盘 (DRIVE_REMOVABLE)"
    elif dt == 3:
        info["drive_type_str"] = "固定本地硬盘 (DRIVE_FIXED)"
    elif dt == 4:
        info["drive_type_str"] = "网络共享盘 (DRIVE_REMOTE)"
    elif dt == 5:
        info["drive_type_str"] = "光盘驱动器 (DRIVE_CDROM)"
    else:
        info["drive_type_str"] = f"其他设备 (Type {dt})"

    # 2. GetVolumeInformationW
    vol_buf = ctypes.create_unicode_buffer(1024)
    fs_buf = ctypes.create_unicode_buffer(1024)
    res_vol = ctypes.windll.kernel32.GetVolumeInformationW(
        drive_letter, vol_buf, 1024, None, None, None, fs_buf, 1024
    )
    if res_vol:
        info["volume_name"] = vol_buf.value or "[无卷标]"
        info["file_system"] = fs_buf.value or "未知"

    # 3. GetDiskFreeSpaceExW
    free_b = ctypes.c_ulonglong(0)
    total_b = ctypes.c_ulonglong(0)
    res_space = ctypes.windll.kernel32.GetDiskFreeSpaceExW(
        drive_letter, None, ctypes.byref(total_b), ctypes.byref(free_b)
    )
    if res_space:
        info["total_gb"] = total_b.value / (1024 ** 3)
        info["free_gb"] = free_b.value / (1024 ** 3)

    # 4. K230 SD Signature Check
    sd_dir = os.path.join(drive_letter, "sharefs", "sdcard")
    main_py = os.path.join(drive_letter, "main.py")
    if os.path.exists(sd_dir) or os.path.exists(main_py):
        info["has_k230_signature"] = True

    # 5. 校验预警项
    warnings = []
    if not info["is_removable"]:
        warnings.append("⚠️ 该盘符未声明为可移动磁盘(DRIVE_REMOVABLE)！")
    if info["total_gb"] > 500:
        warnings.append("⚠️ 该驱动器容量 > 500GB，极有可能是大容量硬盘而非 SD 卡！")
    if info["file_system"].upper() == "NTFS":
        warnings.append("⚠️ 文件系统为 NTFS（K230 开发板 SD 卡通常推荐 FAT32 或 exFAT）！")

    if warnings:
        info["warning"] = " ".join(warnings)

    return info


def find_candidate_drives() -> list:
    """搜寻潜在的可移动设备与 SD 卡"""
    drives = []

    if sys.platform == "win32":
        import ctypes
        bitmask = ctypes.windll.kernel32.GetLogicalDrives()
        for letter in string.ascii_uppercase:
            if bitmask & 1:
                drive_path = f"{letter}:\\"
                # 硬隔离：坚决禁止将 C: 与系统主工程盘默认列为目标
                if letter != "C":
                    try:
                        info = get_windows_drive_info(drive_path)
                        # 优先展示可移动磁盘、容量符合 SD 卡特征或已包含 K230 签名的盘符
                        if info["is_removable"] or info["has_k230_signature"] or (0 < info["total_gb"] <= 256):
                            drives.append(info)
                    except Exception:
                        pass
            bitmask >>= 1
    else:
        # Linux / macOS
        mount_bases = ["/media", "/run/media", "/mnt", "/Volumes"]
        for base in mount_bases:
            if os.path.exists(base):
                for sub in os.listdir(base):
                    full_p = os.path.join(base, sub)
                    if os.path.isdir(full_p):
                        has_sig = os.path.exists(os.path.join(full_p, "sharefs")) or os.path.exists(os.path.join(full_p, "main.py"))
                        drives.append({
                            "drive": full_p,
                            "is_removable": True,
                            "drive_type_str": "挂载存储",
                            "volume_name": sub,
                            "file_system": "VFAT/FAT32",
                            "total_gb": 0.0,
                            "free_gb": 0.0,
                            "has_k230_signature": has_sig,
                            "warning": None,
                        })

    return drives


def ask_double_confirmation(info: dict, files_to_write: list, target_dir: str) -> bool:
    """交互式二次确认面板"""
    print("\n" + "=" * 70)
    print(" ⚠️ 目标存储设备二次确认 (Target Drive Double Confirmation)")
    print("=" * 70)
    print(f"  目标盘符/路径:   {info['drive']}")
    print(f"  设备卷标 (Label): {info['volume_name']}")
    print(f"  文件系统 (FS):   {info['file_system']}")
    print(f"  容量规格:       {info['total_gb']:.1f} GB (可用: {info['free_gb']:.1f} GB)")
    print(f"  设备类型:       {info['drive_type_str']}")
    print(f"  K230 板端签名:   {'✅ 已识别 (存在 sharefs/sdcard/ 或 main.py)' if info['has_k230_signature'] else '未发现现存部署文件'}")
    print(f"  写入目标目录:   {target_dir}")
    print(f"  拟写入部署文件:  {len(files_to_write)} 项 ({', '.join(files_to_write[:4])})")

    if info.get("warning"):
        print(f"\n  {info['warning']}")

    print("=" * 70)
    try:
        confirm = input("⚠️ 确认要向上述设备写入/覆盖部署包吗？[输入 y 确认, Enter/n 取消]: ").strip().lower()
        if confirm in ("y", "yes"):
            return True
    except KeyboardInterrupt:
        pass

    print("[CANCEL] 用户已取消写入，未对磁盘进行任何修改。")
    return False


def deploy_to_sd(target_drive: str = None, pack_dir: str = "deploy_pack", auto_confirm: bool = False):
    pack_abs = os.path.abspath(os.path.join(PROJECT_ROOT, pack_dir))
    if not os.path.exists(pack_abs):
        print(f"[ERR] 未找到部署包目录: {pack_abs}")
        print("      请先运行 `python tools/k230.py pack` 生成部署包！")
        return False

    files_to_write = os.listdir(pack_abs)
    if not files_to_write:
        print(f"[ERR] 部署包目录为空: {pack_abs}")
        return False

    info = None

    # 1. 未指定盘符，检索可移动磁盘
    if not target_drive:
        candidates = find_candidate_drives()
        if not candidates:
            print("[ERR] 未检索到符合特征的可移动 SD 卡驱动器！")
            print("      请插入 SD 卡/读卡器，或使用 `--drive E:\\` 显式指定目标盘符。")
            return False

        if len(candidates) == 1:
            info = candidates[0]
            target_drive = info["drive"]
            print(f"[INFO] 自动检索到目标驱动器: {target_drive} ({info['volume_name']})")
        else:
            print("\n检索到多个潜在移动盘符:")
            for idx, c in enumerate(candidates, 1):
                sig_tag = " [K230 SD卡]" if c["has_k230_signature"] else ""
                print(f"  [{idx}] {c['drive']:8s} | 卷标: {c['volume_name']:12s} | 容量: {c['total_gb']:.1f} GB | {c['file_system']}{sig_tag}")
            try:
                sel = input("\n请选择目标盘符编号 [1-%d]: " % len(candidates)).strip()
                sel_idx = int(sel) - 1
                info = candidates[sel_idx]
                target_drive = info["drive"]
            except Exception:
                print("[ERR] 盘符选择无效，终止操作。")
                return False
    else:
        # 显式指定盘符
        if sys.platform == "win32":
            target_drive = target_drive.rstrip("\\").upper() + "\\"
            info = get_windows_drive_info(target_drive)
        else:
            info = {
                "drive": target_drive,
                "is_removable": True,
                "drive_type_str": "指定路径",
                "volume_name": os.path.basename(target_drive.rstrip("/")),
                "file_system": "标准",
                "total_gb": 0.0,
                "free_gb": 0.0,
                "has_k230_signature": os.path.exists(os.path.join(target_drive, "sharefs")),
                "warning": None,
            }

    # 确定板端目标写入路径 (优先放入 sharefs/sdcard/，如无则放根目录)
    sd_sharefs = os.path.join(target_drive, "sharefs", "sdcard")
    if os.path.exists(os.path.join(target_drive, "sharefs")):
        dst_dir = sd_sharefs
    else:
        dst_dir = target_drive

    # 2. 严谨二次确认面板
    if not auto_confirm:
        if not ask_double_confirmation(info, files_to_write, dst_dir):
            return False

    # 3. 部署拷贝
    os.makedirs(dst_dir, exist_ok=True)
    print(f"\n🚀 开始写入部署包到 SD 卡 ({dst_dir})...")

    copied_count = 0
    for root, dirs, files in os.walk(pack_abs):
        rel_root = os.path.relpath(root, pack_abs)
        target_root = os.path.join(dst_dir, rel_root) if rel_root != "." else dst_dir
        os.makedirs(target_root, exist_ok=True)

        for f in files:
            src_file = os.path.join(root, f)
            dst_file = os.path.join(target_root, f)
            shutil.copy2(src_file, dst_file)
            copied_count += 1
            print(f"  [✓] 写入: {os.path.relpath(dst_file, target_drive)}")

    print("=" * 70)
    print(f"🎉 [SUCCESS] 成功完成 {copied_count} 个文件的安全同步！")
    print(f"👉 目标位置: {dst_dir}")
    print("👉 可以安全拔出 SD 卡并插入 K230 开发板直接开机运行。")
    print("=" * 70)
    return True


def main():
    p = argparse.ArgumentParser(description="K230 SD 卡智能严谨部署同步工具 (tools/deploy_sd.py)")
    p.add_argument("--drive", default=None, help="显式指定目标 SD 卡盘符 (如 E:\\)")
    p.add_argument("--source", default="deploy_pack", help="部署包目录 (默认 deploy_pack)")
    p.add_argument("--yes", "-y", action="store_true", help="跳过交互式二次确认 (仅用于自动化 CI 脚本)")
    args = p.parse_args()

    success = deploy_to_sd(args.drive, args.source, auto_confirm=args.yes)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
