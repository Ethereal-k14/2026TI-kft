#!/usr/bin/env python3
"""统一设备解析：从 CUDA 开始降级。

训练 / 推理 / 追踪脚本统一调用 resolve_device()，实现：
  - 用户显式指定（如 "0" / "cpu"）时直接使用；
  - 未指定时优先使用 GPU，无可用 GPU 时自动降级到 CPU 并打印提示。

这样本机有 RTX 4060 时自动走 CUDA 发挥性能，换到无 GPU 机器也能跑。
"""
import torch


def resolve_device(requested: str = "") -> str:
    """解析目标 device。

    Args:
        requested: 用户通过 --device 传入的值（"0" / "cpu" / "" 表示自动）。
    Returns:
        传给 Ultralytics 的 device 字符串。
    """
    if requested:
        return requested
    if torch.cuda.is_available():
        name = torch.cuda.get_device_name(0)
        props = torch.cuda.get_device_properties(0)
        vram_gb = props.total_memory / 1e9
        compute = props.major * 10 + props.minor
        print(f"[device] 自动选择 GPU: {name} "
              f"({vram_gb:.1f} GB, compute capability {compute / 10:.1f})")
        return "cuda:0"
    print("[device] 未检测到可用 GPU，降级到 CPU")
    return "cpu"


if __name__ == "__main__":
    print("resolved:", resolve_device())
