"""
CanMV K230 MicroPython 板端部署示例模板 (templates/canmv_k230_demo.py)

说明：
    适用于 嘉楠 CanMV K230 开发板 (MicroPython 固件)。
    用于将编译好的 .kmodel 文件放到 SD 卡 (sharefs/sdcard) 上直接运行摄像头实时推理。

部署流程：
    1. 将本文件重命名为 main.py 放入 SD 卡 /sdcard/ 或 /sharefs/
    2. 同级目录下放置 best.kmodel 与 labels.txt
    3. 上电或在 CanMV IDE 中连接运行
"""

import time
import os
import sys

# 尝试导入 CanMV 板级专属硬件 API
try:
    from media.camera import *
    from media.display import *
    from media.media import *
    import nncase_runtime as nn
    import ulab.numpy as np
    HAS_CANMV_HARDWARE = True
except ImportError:
    HAS_CANMV_HARDWARE = False
    print("[WARNING] 当前为非 CanMV 硬件环境，本代码仅供模板参考与打包说明。")


# 参数定义 (必须与训练导出时的 imgsz 完全一致)
MODEL_PATH = "best.kmodel"
LABELS_PATH = "labels.txt"
MODEL_INPUT_SIZE = (320, 320)  # (W, H)
CONF_THRES = 0.35
NMS_THRES = 0.65


def load_labels(path):
    if not os.path.exists(path):
        return ["object"]
    with open(path, "r", encoding="utf-8") as f:
        return [line.strip() for line in f if line.strip()]


def main():
    if not HAS_CANMV_HARDWARE:
        print("[ERR] 必须在 CanMV K230 开发板的 MicroPython 环境中运行此脚本。")
        return

    labels = load_labels(LABELS_PATH)
    print(f"[INFO] 成功加载 {len(labels)} 个类别: {labels[:5]}...")

    # 1. 初始化 KPU 引擎并加载 kmodel
    kpu = nn.kpu()
    kpu.load_kmodel(MODEL_PATH)
    print(f"[INFO] 已装载 kmodel: {MODEL_PATH}")

    # 2. 初始化 Display 显示屏 (VO) 与 Camera 摄像头 (VI)
    Display.init(Display.LT9611_1920X1080)
    Camera.sensor_init(0, Camera.V4L2_PIX_FMT_YUV420P)
    MediaManager.init()

    # 开启摄像头通道
    Camera.set_outsize(0, MODEL_INPUT_SIZE[0], MODEL_INPUT_SIZE[1])
    Camera.start_stream()

    print("[INFO] 开始实时 AI 推理... (按 Ctrl+C 退出)")

    try:
        while True:
            # 获取摄像头帧
            img = Camera.snapshot()
            
            # 将帧数据灌入 KPU
            kpu.set_input_tensor(0, img)
            kpu.run()
            
            # 获取模型输出 Tensor
            outputs = [kpu.get_output_tensor(i) for i in range(kpu.inputs_size())]

            # 在此接入后处理 (YOLO11/v8 后处理)
            # CanMV 常用官方 C 扩展库或 ulab 简化计算

            # 显示结果帧
            Display.show_image(img)
            time.sleep_ms(1)

    except KeyboardInterrupt:
        print("[INFO] 接收到退出信号")
    finally:
        Camera.stop_stream()
        Display.deinit()
        MediaManager.deinit()
        print("[INFO] 硬件资源已正常释放")


if __name__ == "__main__":
    main()
