#!/bin/sh
# K230 Linux 侧 C++ yolo.elf 命令行运行脚本模板 (templates/k230_cpp_runner.sh)
# 部署路径：将本脚本与 best.kmodel、labels.txt、yolo.elf 一起放入 SD 卡 /sharefs/ 目录

# 进入执行目录
cd "$(dirname "$0")" || exit 1

# 检查依赖文件
if [ ! -f "yolo.elf" ]; then
    echo "[ERR] 未找到 yolo.elf 可执行文件，请确保将其与脚本放在同一目录！"
    exit 1
fi

if [ ! -f "best.kmodel" ]; then
    echo "[ERR] 未找到 best.kmodel 模型文件！"
    exit 1
fi

if [ ! -f "labels.txt" ]; then
    echo "[ERR] 未找到 labels.txt 标签文件！"
    exit 1
fi

# 参数设定（需与模型导出时的 imgsz 严格一致）
MODEL_TYPE="yolo11"        # 可选: yolov5 / yolov8 / yolo11 / yolo26
TASK_TYPE="detect"        # 可选: detect / segment / pose / obb / classify
TASK_MODE="video"         # 可选: video (摄像头实时) / image (单图)
INPUT_W=320               # AI 帧宽 (必须与 kmodel 尺寸相同)
INPUT_H=320               # AI 帧高 (必须与 kmodel 尺寸相同)
CONF_THRES=0.35           # 置信度阈值
NMS_THRES=0.65            # NMS 阈值

echo "[INFO] 启动 K230 C++ YOLO 实时推理..."
echo "[INFO] 模型类型: $MODEL_TYPE | 任务: $TASK_TYPE | 输入尺寸: ${INPUT_W}x${INPUT_H}"

./yolo.elf \
    -model_type "$MODEL_TYPE" \
    -task_type "$TASK_TYPE" \
    -task_mode "$TASK_MODE" \
    -ai_frame_width "$INPUT_W" \
    -ai_frame_height "$INPUT_H" \
    -kmodel_path "best.kmodel" \
    -labels_txt_filepath "labels.txt" \
    -conf_thres "$CONF_THRES" \
    -nms_thres "$NMS_THRES"
