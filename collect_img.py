import serial
import time
import argparse
import os

# ==========================================
# 手势类别映射表 (定义你要采集的所有手势)
# ==========================================
VALID_GESTURES = ['rock', 'paper', 'scissors', 'none']

def run_collection(port, baudrate, base_dir, target_gesture, target_count):
    """
    执行目标手势的图像采集
    """
    # 1. 自动创建多级目录结构 (例如: dataset/rock/)
    output_dir = os.path.join(base_dir, target_gesture)
    os.makedirs(output_dir, exist_ok=True)

    # 2. 连接串口
    print(f"[{time.strftime('%H:%M:%S')}] 正在连接串口 {port}...")
    try:
        # 注意：timeout=1 防止死锁
        ser = serial.Serial(port, baudrate, timeout=1) 
    except Exception as e:
        print(f"❌ 串口错误: {e}")
        print("💡 提示：请确保单片机已插上，并且关闭了 XCOM 等所有串口调试助手！")
        return

    print("\n" + "="*60)
    print(f">>> 当前录制目标手势: 【{target_gesture.upper()}】")
    print(f">>> 保存存储目录: {output_dir}")
    print(f">>> 目标采集数量: {target_count} 张")
    print(">>> 动作说明: 请在镜头前摆好【{target_gesture.upper()}】手势，可缓慢变换角度和远近。")
    print("="*60 + "\n")

    buffer = bytearray()
    saved_count = 0
    start_time = time.time()

    # 3. 开始接收并解析纯净的二进制 JPEG 数据流
    while saved_count < target_count:
        try:
            data = ser.read(4096)
            if data:
                buffer.extend(data)
                
                # 【容错机制】缓存过大说明中间丢包导致找不到帧尾，清空重来
                if len(buffer) > 102400:
                    print("⚠️ 数据流错位，正在清空缓存重新同步...")
                    buffer.clear()
                    continue

                # 寻找 JPEG 的固定帧头 FF D8
                start_idx = buffer.find(b'\xff\xd8')
                if start_idx != -1:
                    # 寻找 JPEG 的固定帧尾 FF D9
                    end_idx = buffer.find(b'\xff\xd9', start_idx)
                    
                    if end_idx != -1:
                        # 成功截取一完整帧
                        jpg_data = buffer[start_idx : end_idx + 2]
                        
                        # 生成带有时间戳的唯一文件名: 例如 rock_1680123456789.jpg
                        timestamp = int(time.time() * 1000)
                        filename = f"{target_gesture}_{timestamp}.jpg"
                        filepath = os.path.join(output_dir, filename)
                        
                        # 写入文件
                        with open(filepath, "wb") as f:
                            f.write(jpg_data)
                            
                        saved_count += 1
                        print(f"[{saved_count}/{target_count}] 📸 保存成功: {filename} (大小: {len(jpg_data)} bytes)")
                        
                        # 截断处理过的数据，准备接下一张图
                        buffer = buffer[end_idx + 2 :]
                        
        except KeyboardInterrupt:
            print("\n⏹️ 收到中断信号，提前停止采集。")
            break

    # 4. 统计与清理
    elapsed = time.time() - start_time
    print(f"\n[{time.strftime('%H:%M:%S')}] 采集结束！")
    print(f"总计耗时: {elapsed:.1f} 秒。共成功保存 {saved_count} 张【{target_gesture}】图像。")
    ser.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="OV2640 串口图像自动化切片与标注工具")
    
    # 基础通讯参数
    parser.add_argument('--port', type=str, default='COM3', help="串口号 (如 COM3 或 /dev/ttyUSB0)")
    parser.add_argument('--baud', type=int, default=115200, help="波特率 (默认 115200)")
    
    # 存储参数
    parser.add_argument('--dir', type=str, default='dataset', help="数据集根目录名称 (默认 'dataset')")
    parser.add_argument('--count', type=int, default=100, help="本次期望采集的图片数量 (默认 100张)")
    
    # 核心：手势标签
    parser.add_argument('--gesture', type=str, required=True, choices=VALID_GESTURES,
                        help="本次采集的手势类型 (将自动建立同名文件夹，并以此作为文件名前缀)")
    
    args = parser.parse_args()
    
    run_collection(args.port, args.baud, args.dir, args.gesture, args.count)