import serial
import time

# 配置你的串口号和波特率
SERIAL_PORT = 'COM4'  # Windows示例: 'COM3', Mac/Linux示例: '/dev/ttyUSB0'
BAUD_RATE = 115200

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"✅ 成功打开串口 {SERIAL_PORT}，正在监听 JPEG 图像数据...")
    except Exception as e:
        print(f"❌ 打开串口失败: {e}")
        return

    buffer = bytearray()
    image_count = 0

    while True:
        try:
            # 读取串口数据
            data = ser.read(4096)
            if data:
                buffer.extend(data)
                
                # JPEG 文件的固定开头是 FF D8，结尾是 FF D9
                start_idx = buffer.find(b'\xff\xd8')
                if start_idx != -1:
                    # 寻找对应的结尾
                    end_idx = buffer.find(b'\xff\xd9', start_idx)
                    
                    if end_idx != -1:
                        # 截取完整的一张图片数据
                        jpg_data = buffer[start_idx : end_idx + 2]
                        
                        # 保存为文件
                        filename = f"capture_{image_count:04d}.jpg"
                        with open(filename, "wb") as f:
                            f.write(jpg_data)
                            
                        print(f"📸 抓拍成功: {filename} (大小: {len(jpg_data)} 字节)")
                        image_count += 1
                        
                        # 把处理过的数据从缓存中移除，准备接下一张
                        buffer = buffer[end_idx + 2 :]
        except KeyboardInterrupt:
            print("\n⏹️ 停止监听。")
            break

if __name__ == "__main__":
    main()