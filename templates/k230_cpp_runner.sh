#!/bin/sh
# K230 Linux 板端 C++ 推理可执行文件启动包装脚本
# 自动设置动态库加载路径 (LD_LIBRARY_PATH)

SCRIPT_DIR=$(cd $(dirname $0); pwd)
export LD_LIBRARY_PATH=$SCRIPT_DIR/lib:$LD_LIBRARY_PATH

echo "[K230] 正在启动板端 C++ Vision AI 推理程序..."
if [ -f "$SCRIPT_DIR/yolo_k230.elf" ]; then
    chmod +x "$SCRIPT_DIR/yolo_k230.elf"
    "$SCRIPT_DIR/yolo_k230.elf" "$@"
else
    echo "[ERROR] 未搜寻到 yolo_k230.elf 部署文件"
    exit 1
fi
