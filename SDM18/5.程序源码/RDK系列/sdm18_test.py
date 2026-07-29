import serial
import time

class SMD18:
    #串口初始化 uart init
    def __init__(self, port='/dev/ttyUSB0', baudrate=921600):
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1
        )
        # 初始化接收缓冲区 uart init rx buffer
        self.rx_buffer = bytearray()
        # 初始化运行状态  Initialize the running state
        self.running = False

    #位反转  Bit reversal
    @staticmethod
    def reverse_bits(data, width):
        result = 0
        for i in range(width):
            if data & (1 << i):
                result |= (1 << (width - 1 - i))
        return result

    # CRC16计算  CRC16 calculation
    '''
    @param data_bytes: 数据字节数组  data byte array
    @return: CRC16值  CRC16 value
    '''
    def calculate_crc16(self, data_bytes):
        poly = 0x8005
        init = 0xFFFF
        xor_out = 0x0000
        ref_in = True
        ref_out = True
        
        crc = init
        for byte in data_bytes:
            if ref_in:
                current = self.reverse_bits(byte, 8) 
            else:
                current = byte
            crc ^= (current << 8)
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ poly
                else:
                    crc = (crc << 1)
                crc &= 0xFFFF
                
        if ref_out:
            crc = self.reverse_bits(crc, 16)
        return crc ^ xor_out

    # 发送命令  Send command
    '''
    @param cmd: 命令字节数组  command byte array
    '''
    def send_command(self, cmd):
        self.ser.write(cmd)

    # 停止扫描  Stop scan
    def stop_scan(self):
        cmd = bytearray([0xA5, 0x03, 0x20, 0x02, 0x00, 0x00, 0x00])
        crc = self.calculate_crc16(cmd)
        cmd.append((crc >> 8) & 0xFF)   # CRC高字节  CRC high byte
        cmd.append(crc & 0xFF)          # CRC低字节  CRC low byte
        self.send_command(cmd)
    
    # 开始扫描  Start scan
    def start_scan(self):
        cmd = bytearray([0xA5, 0x03, 0x20, 0x01, 0x00, 0x00, 0x00])
        crc = self.calculate_crc16(cmd) 
        cmd.append((crc >> 8) & 0xFF)   # CRC高字节  CRC high byte
        cmd.append(crc & 0xFF)          # CRC低字节  CRC low byte
        self.send_command(cmd)
        self.running = True
    
    # 解析数据  Parse data
    def decode_data(self):
        """解码接收到的数据"""
        # 读取串口数据
        while self.ser.in_waiting > 0:
            data = self.ser.read(self.ser.in_waiting)
            self.rx_buffer += data
        
        # 解析数据  Parse data
        # 处理缓冲区数据  Handle buffer data
        while len(self.rx_buffer) >= 7:  # 最小帧头长度  Minimum frame header length
            # 检查帧头(A5 03 20 01 00)
            if self.rx_buffer[0] == 0xA5 and self.rx_buffer[1] == 0x03 and self.rx_buffer[2] == 0x20 and self.rx_buffer[3] == 0x01 and self.rx_buffer[4] == 0x00:
                
                # 提取数据长度 Extract the data length
                data_len = (self.rx_buffer[5] << 8) | self.rx_buffer[6]
                total_len = 7 + data_len + 2  # 帧头7字节 + 数据长度 + CRC2字节
                
                # 检查完整帧 Check complete frame
                #没接受完退出继续接收  Exit and continue receiving if not received yet
                if len(self.rx_buffer) < total_len:
                    return None, None  # 等待更多数据  Wait for more data
                
                # 收到完整数据就先放到缓冲区  Received complete data, put it in the buffer first
                frame = self.rx_buffer[:total_len]
                self.rx_buffer = self.rx_buffer[total_len:]  # 移除已处理数据
                
                # CRC校验 (不包括最后的2字节CRC)  CRC check (excluding the last 2 bytes CRC)
                crc_calc = self.calculate_crc16(frame[:-2])
                crc_recv = (frame[-2] << 8) | frame[-1]  
                
                # CRC校验不通过  CRC check failed
                if crc_calc != crc_recv:
                    print(f"CRC Error: Calc 0x{crc_calc:04X} != Recv 0x{crc_recv:04X}")
                    continue
                    
                # 帧头7字节后是数据部分，距离在数据部分的偏移6字节  The data part starts after the 7-byte frame header, and the distance is at an offset of 6 bytes in the data part
                if len(frame) >= 17:  # 确保有足够字节数  Ensure there are enough bytes
                    dist_idx = 7 + 6  # 7字节帧头 + 6字节偏移  7-byte frame header + 6-byte offset
                    # 距离是2字节  Distance is 2 bytes
                    distance = (frame[dist_idx +1 ] << 8) | frame[dist_idx]
                    # 强度是2字节  Strength is 2 bytes
                    str_idx = 7 + 10  
                    strength = (frame[str_idx + 1] << 8) | frame[str_idx]
                else:
                    distance = strength = 0
                
                print(f"Distance: {distance} mm, Strength: {strength}")
                return distance, strength
            else:
                # 如果不是有效帧头，移除第一个字节
                self.rx_buffer = self.rx_buffer[1:]
        
        return None, None

    def run(self):
        try:
            self.stop_scan()
            time.sleep(0.2)
            self.start_scan()
            print("Scan started")
            
            while self.running:
                dist, strength = self.decode_data()
                # 数据没有接收完成继续接收  Data not received yet, continue receiving
                if dist is not None:
                    pass
                # 降低CPU占用
                time.sleep(0.01)
                
        except KeyboardInterrupt:
            self.stop_scan()
            self.running = False
        finally:
            if self.ser.is_open:
                self.ser.close()
            print("EXIT")

if __name__ == "__main__":
    lidar = SMD18()
    lidar.run()
