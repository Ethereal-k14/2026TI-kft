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
import select
import gc

# 尝试导入 CanMV 硬件 API 模块
try:
    from media.camera import *
    from media.display import *
    from media.media import *
    import nncase_runtime as nn
    import network
    HAS_CANMV_HARDWARE = True
except ImportError:
    HAS_CANMV_HARDWARE = False
    print("[WARNING] 当前处于 PC 非硬件环境，网络视频流代码模板加载完成。")
    class MockCamera:
        @classmethod
        def sensor_init(cls, *args, **kwargs): pass
        @classmethod
        def set_outsize(cls, *args, **kwargs): pass
        @classmethod
        def start_stream(cls): pass
        @classmethod
        def stop_stream(cls): pass
        @classmethod
        def snapshot(cls): return None
    class MockMediaManager:
        @classmethod
        def init(cls): pass
        @classmethod
        def deinit(cls): pass
    Camera = MockCamera
    MediaManager = MockMediaManager

# 兼容 MicroPython time.sleep_ms
import time
if not hasattr(time, "sleep_ms"):
    time.sleep_ms = lambda ms: time.sleep(ms / 1000.0)


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
    s.listen(5)
    s.setblocking(False) # 设置为非阻塞模式，支持多客户端

    print("\n" + "=" * 50)
    print(f" 🌐 K230 Web 实时 AI 视频流服务已就绪!")
    print(f" 👉 请在同一局域网电脑/手机浏览器访问: http://{ip}:{port}/")
    print("=" * 50 + "\n")

    return s


HTML_DASHBOARD = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>K230 AI 实时视觉控制台</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: #0f172a; color: #f8fafc; font-family: system-ui, -apple-system, sans-serif; display: flex; flex-direction: column; align-items: center; min-height: 100vh; padding: 20px; }
  .header { display: flex; align-items: center; justify-content: space-between; width: 100%; max-width: 800px; margin-bottom: 20px; padding: 16px 24px; background: #1e293b; border-radius: 12px; border: 1px solid #334155; }
  .title { font-size: 1.25rem; font-weight: 700; color: #38bdf8; display: flex; align-items: center; gap: 8px; }
  .badge { background: #166534; color: #4ade80; font-size: 0.75rem; padding: 4px 10px; border-radius: 9999px; font-weight: 600; display: flex; align-items: center; gap: 6px; }
  .pulse { width: 8px; height: 8px; background: #4ade80; border-radius: 50%; animation: blink 1.5s infinite; }
  @keyframes blink { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }
  .stream-card { background: #1e293b; border-radius: 16px; padding: 12px; border: 1px solid #334155; width: 100%; max-width: 800px; display: flex; flex-direction: column; align-items: center; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5); }
  .stream-img { width: 100%; height: auto; max-height: 520px; border-radius: 8px; object-fit: contain; background: #000; }
  .info-bar { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 12px; width: 100%; max-width: 800px; margin-top: 20px; }
  .info-card { background: #1e293b; padding: 14px 18px; border-radius: 10px; border: 1px solid #334155; }
  .info-label { font-size: 0.75rem; color: #94a3b8; text-transform: uppercase; margin-bottom: 4px; }
  .info-val { font-size: 1rem; font-weight: 600; color: #f1f5f9; }
</style>
</head>
<body>
  <div class="header">
    <div class="title">⚡ K230 CanMV AI 视觉直方控制台</div>
    <div class="badge"><div class="pulse"></div> LIVE ONLINE</div>
  </div>
  <div class="stream-card">
    <img src="/stream" class="stream-img" alt="K230 Live Stream">
  </div>
  <div class="info-bar">
    <div class="info-card"><div class="info-label">硬件平台</div><div class="info-val">Canaan K230 KPU</div></div>
    <div class="info-card"><div class="info-label">当前模型</div><div class="info-val">best.kmodel (YOLO)</div></div>
    <div class="info-card"><div class="info-label">输入分辨率</div><div class="info-val">320 x 320</div></div>
    <div class="info-card"><div class="info-label">推流模式</div><div class="info-val">HTTP MJPEG</div></div>
  </div>
</body>
</html>"""


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
    
    # 使用 select.poll 实现非阻塞多客户端并发
    poller = select.poll()
    poller.register(server_socket, select.POLLIN)
    
    clients = []

    try:
        while True:
            # 捕获一帧图像并推理
            img = Camera.snapshot()
            kpu.set_input_tensor(0, img)
            kpu.run()
            
            # 检查网络事件，超时 5ms
            events = poller.poll(5)
            for fd, event in events:
                if fd == server_socket.fileno():
                    try:
                        cl, addr = server_socket.accept()
                        cl.setblocking(False)
                        print(f"[NET] 收到浏览器客户端连接: {addr}")
                        clients.append({'socket': cl, 'addr': addr, 'stream': False})
                        poller.register(cl, select.POLLIN)
                    except Exception:
                        pass
                else:
                    # 查找对应客户端
                    client = next((c for c in clients if c['socket'].fileno() == fd), None)
                    if client and (event & select.POLLIN):
                        try:
                            req = client['socket'].recv(512).decode('utf-8', 'ignore')
                            if not req:
                                raise Exception("Client disconnected")
                            if "GET /stream" in req:
                                header = (
                                    "HTTP/1.0 200 OK\r\n"
                                    "Server: K230-WebStreamer\r\n"
                                    "Connection: keep-alive\r\n"
                                    "Max-Age: 0\r\n"
                                    "Expires: 0\r\n"
                                    "Cache-Control: no-store, no-cache, must-revalidate\r\n"
                                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n"
                                )
                                client['socket'].send(header.encode('utf-8'))
                                client['stream'] = True
                            else:
                                html_bytes = HTML_DASHBOARD.encode('utf-8')
                                response_header = (
                                    "HTTP/1.0 200 OK\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    f"Content-Length: {len(html_bytes)}\r\n"
                                    "Connection: close\r\n\r\n"
                                )
                                client['socket'].send(response_header.encode('utf-8'))
                                client['socket'].send(html_bytes)
                                raise Exception("Dashboard sent")
                        except Exception:
                            poller.unregister(client['socket'])
                            client['socket'].close()
                            clients.remove(client)
                            print(f"[NET] 客户端连接断开: {client['addr']}")
                    elif client and (event & (select.POLLERR | select.POLLHUP)):
                        poller.unregister(client['socket'])
                        client['socket'].close()
                        clients.remove(client)
                        print(f"[NET] 客户端连接异常断开: {client['addr']}")
            
            # 向所有订阅视频流的客户端推送最新帧
            if any(c['stream'] for c in clients):
                try:
                    jpg_bytes = img.compress(quality=85)
                    frame_header = f"--frame\r\nContent-Type: image/jpeg\r\nContent-Length: {len(jpg_bytes)}\r\n\r\n".encode('utf-8')
                    
                    for client in list(clients):
                        if client['stream']:
                            try:
                                client['socket'].send(frame_header)
                                client['socket'].send(jpg_bytes)
                                client['socket'].send(b"\r\n")
                            except Exception:
                                poller.unregister(client['socket'])
                                client['socket'].close()
                                clients.remove(client)
                                print(f"[NET] 客户端流推送失败，断开: {client['addr']}")
                except Exception:
                    pass
            
            # 显式回收内存，解决堆碎片化与废弃引用积累
            gc.collect()

    except KeyboardInterrupt:
        print("[INFO] 退出串流程序")
    finally:
        Camera.stop_stream()
        MediaManager.deinit()
        server_socket.close()
        for client in clients:
            try:
                client['socket'].close()
            except:
                pass


if __name__ == "__main__":
    main()
