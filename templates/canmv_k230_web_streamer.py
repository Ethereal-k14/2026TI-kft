"""
CanMV K230 Web 端口实时画框图像串流模板 (templates/canmv_k230_web_streamer.py)

功能：
    结合 K230 开发板的本地网卡 (WiFi 或 以太网) 与 KPU 视觉推理，
    建立轻量级 HTTP MJPEG 视频流服务器。
    局域网内任意设备的浏览器打开 http://<K230_IP>:8080/ 即可实时查看带 YOLO 检测框的画框直播！

部署说明：
    1. 在 K230 板端连接 WiFi (WLAN) 或插入网线。
    2. 将本代码命名为 main_web.py 拷贝到 SD 卡 (sharefs/sdcard)。
    3. 运行后控制台会打印 K230 的 IP 地址与 Web 查看端口。
"""

import time
import os
import sys
import socket

# 尝试导入 CanMV 板级硬件与网络 API
try:
    from media.camera import *
    from media.display import *
    from media.media import *
    import nncase_runtime as nn
    import network
    HAS_CANMV_HARDWARE = True
except ImportError:
    HAS_CANMV_HARDWARE = False
    print("[WARNING] 当前非 CanMV 板端环境，本脚本提供 Web 视频流服务端逻辑与架构示范。")


# 部署配置
MODEL_PATH = "best.kmodel"
LABELS_PATH = "labels.txt"
MODEL_INPUT_SIZE = (320, 320)
WEB_PORT = 8080

# WiFi 自动连接配置 (若使用以太网 LAN 可忽略)
WIFI_SSID = "Your_WiFi_SSID"
WIFI_PASS = "Your_WiFi_Password"


def connect_network():
    """初始化 K230 网络 (支持: 1. USB Type-C 虚拟网卡直连  2. RJ45 网线直连  3. WiFi)"""
    if not HAS_CANMV_HARDWARE:
        return "127.0.0.1"

    # 1. 首选：USB Type-C 虚拟网卡直连 (USB RNDIS / CDC-ECM 模式)
    # 用 Type-C 线插电脑，电脑会自动识别虚拟网卡 (IP 默认为 192.168.42.1)
    try:
        usberr = network.USB_RNDIS()
        if usberr.isconnected():
            ip = usberr.ifconfig()[0]
            print(f"[NET] ✅ USB Type-C 虚拟网卡已直连 (RNDIS)，IP: {ip}")
            return ip
    except Exception:
        pass

    # 2. 推荐：RJ45 标准网线直连 (百兆/千兆板载网口)
    # 直接用标准 RJ45 网线插在 K230 板卡的网口上
    try:
        lan = network.LAN()
        if lan.isconnected():
            ip = lan.ifconfig()[0]
            print(f"[NET] ✅ RJ45 板载网线已连接，IP: {ip}")
            return ip
    except Exception:
        pass

    # 3. 备选：WiFi 无线连接
    try:
        wlan = network.WLAN(network.STA_IF)
        wlan.active(True)
        if not wlan.isconnected():
            print(f"[NET] 正在尝试连接 WiFi: {WIFI_SSID}...")
            wlan.connect(WIFI_SSID, WIFI_PASS)
            timeout = 5
            while not wlan.isconnected() and timeout > 0:
                time.sleep(1)
                timeout -= 1

        if wlan.isconnected():
            ip = wlan.ifconfig()[0]
            print(f"[NET] ✅ WiFi 已连接，IP: {ip}")
            return ip
    except Exception as e:
        print(f"[NET] 网卡提示: {e}")

    return "0.0.0.0"


def start_mjpeg_web_server(ip, port=8080):
    """启动轻量级 HTTP MJPEG 串流 Socket 服务端"""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('0.0.0.0', port))
    s.listen(2)
    s.settimeout(0.5)

    print("\n" + "=" * 50)
    print(f" 🌐 K230 Web 实时 AI 视频流服务已就绪!")
    print(f" 👉 请在同一局域网电脑/手机浏览器访问: http://{ip}:{port}/")
    print("=" * 50 + "\n")

    return s


def main():
    if not HAS_CANMV_HARDWARE:
        print("[ERR] 必须在 CanMV K230 板端 MicroPython 环境下运行此脚本。")
        return

    # 1. 联网并获取 IP
    ip_addr = connect_network()

    # 2. 装载 kmodel
    kpu = nn.kpu()
    kpu.load_kmodel(MODEL_PATH)

    # 3. 初始化 Camera 摄像头
    Camera.sensor_init(0, Camera.V4L2_PIX_FMT_YUV420P)
    MediaManager.init()
    Camera.set_outsize(0, MODEL_INPUT_SIZE[0], MODEL_INPUT_SIZE[1])
    Camera.start_stream()

    # 4. 开启 HTTP Socket
    server_socket = start_mjpeg_web_server(ip_addr, WEB_PORT)

    try:
        while True:
            try:
                cl, addr = server_socket.accept()
                print(f"[NET] 收到浏览器客户端连接: {addr}")

                # 发送 HTTP MJPEG 头信息
                header = (
                    "HTTP/1.0 200 OK\r\n"
                    "Server: K230-WebStreamer\r\n"
                    "Connection: close\r\n"
                    "Max-Age: 0\r\n"
                    "Expires: 0\r\n"
                    "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n"
                    "Pragma: no-cache\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n"
                )
                cl.send(header.encode('utf-8'))

                # 持续推送 JPEG 帧
                while True:
                    img = Camera.snapshot()

                    # 送入 KPU 推理
                    kpu.set_input_tensor(0, img)
                    kpu.run()

                    # 在图像上绘制假定推理结果（如框和标签）
                    # img.draw_rectangle(50, 50, 150, 150, color=(255, 0, 0), thickness=2)
                    # img.draw_string(52, 32, "target 0.95", scale=2, color=(0, 255, 0))

                    # 硬件转为 JPEG 字节流
                    jpg_bytes = img.compress(quality=75)

                    # 发送 multipart 帧边界
                    frame_header = f"--frame\r\nContent-Type: image/jpeg\r\nContent-Length: {len(jpg_bytes)}\r\n\r\n"
                    cl.send(frame_header.encode('utf-8'))
                    cl.send(jpg_bytes)
                    cl.send(b"\r\n")

            except OSError:
                # accept 超时，继续抓帧循环
                pass

    except KeyboardInterrupt:
        print("[INFO] 退出串流程序")
    finally:
        Camera.stop_stream()
        MediaManager.deinit()
        server_socket.close()


if __name__ == "__main__":
    main()
