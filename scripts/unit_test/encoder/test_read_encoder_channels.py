#!/usr/bin/env python3
"""
编码器指定通道读取脚本
读取编码器设备的指定通道位置值
"""

import sys
import time
import tango
from datetime import datetime

# 编码器通道映射：代码通道 -> (采集器IP后三位, 物理通道号, 设备说明)
# 注意：通道从1开始编号，与C++代码保持一致
CHANNEL_MAPPING = {
    # 192.168.1.199 的通道 (代码通道 1-10)
    1: ("199", 1, "六自由度轴1"),
    2: ("199", 2, "六自由度轴2"),
    3: ("199", 3, "六自由度轴3"),
    4: ("199", 4, "六自由度轴4"),
    5: ("199", 5, "六自由度轴5"),
    6: ("199", 6, "六自由度轴6"),
    7: ("199", 7, "大行程"),
    8: ("199", 8, "反射光设备1-X"),
    9: ("199", 9, "反射光设备1-Y"),
    10: ("199", 10, "反射光设备2-X"),
    # 192.168.1.198 的通道 (物理CH5在C++中被跳过)
    11: ("198", 1, "反射光设备2-Y"),
    12: ("198", 2, "反射光设备1-Z"),
    13: ("198", 3, "反射光设备2-Z"),
    14: ("198", 4, "辅助支撑设备1"),
    # 物理CH5未连接编码器，被跳过
    15: ("198", 6, "辅助支撑设备2"),
    16: ("198", 7, "辅助支撑设备3"),
    17: ("198", 8, "辅助支撑设备4"),
    18: ("198", 9, "辅助支撑设备5"),
}

def read_channel_value(device, channel):
    """读取单个通道的编码器值
    
    Args:
        device: 设备代理
        channel: 通道号
    
    Returns:
        float or None: 编码器值，失败返回None
    """
    try:
        value = device.command_inout("readEncoder", channel)
        return value
    except Exception as e:
        return None

def calculate_change(current, previous):
    """计算编码器值变化"""
    if current is None or previous is None:
        return None
    return current - previous

def get_channel_info(channel):
    """获取通道信息"""
    if channel in CHANNEL_MAPPING:
        collector_ip, physical_ch, device_desc = CHANNEL_MAPPING[channel]
        return f"192.168.1.{collector_ip}", physical_ch, device_desc
    return "未知", "?", "未配置"

def test_read_channels(device_name="sys/encoder/1", channels=None, interval=0.5, count=None, show_delta=True):
    """循环读取指定通道的编码器值
    
    Args:
        device_name: 设备名称
        channels: 通道列表，例如 [0, 1, 2] 或 [6, 7, 8]
        interval: 读取间隔（秒）
        count: 读取次数，None表示无限循环
        show_delta: 是否显示变化量
    """
    if channels is None or len(channels) == 0:
        print("错误: 必须指定至少一个通道")
        return
    
    # 验证通道号
    valid_channels = []
    for ch in channels:
        if 1 <= ch <= 18:
            valid_channels.append(ch)
        else:
            print(f"⚠ 警告: 通道 {ch} 超出范围 (1-18)，已忽略")
    
    if not valid_channels:
        print("错误: 没有有效的通道")
        return
    
    channels = sorted(valid_channels)
    
    print(f"\n{'='*100}")
    print(f"编码器指定通道读取")
    print(f"设备名称: {device_name}")
    print(f"读取通道: {channels}")
    print(f"读取间隔: {interval}秒")
    print(f"读取次数: {'无限' if count is None else count}")
    print(f"显示变化: {'是' if show_delta else '否'}")
    print(f"{'='*100}\n")
    
    # 显示通道映射信息
    print("通道映射信息:")
    print(f"{'─'*100}")
    print(f"{'代码CH':<8} {'采集器':<16} {'物理CH':<8} {'设备说明':<30}")
    print(f"{'─'*100}")
    for ch in channels:
        collector_ip, physical_ch, device_desc = get_channel_info(ch)
        print(f"{ch:<8} {collector_ip:<16} CH{physical_ch:<6} {device_desc:<30}")
    print(f"{'─'*100}\n")
    
    try:
        # 1. 连接设备
        print(f"正在连接设备: {device_name}...")
        device = tango.DeviceProxy(device_name)
        device.ping()
        print("✓ 设备连接成功\n")
        
        # 2. 检查设备状态
        state = device.state()
        print(f"设备状态: {state}")
        
        if state == tango.DevState.FAULT:
            print("⚠ 警告: 设备处于FAULT状态")
        
        print(f"\n{'='*100}")
        print("开始循环读取 (按Ctrl+C停止)...")
        print(f"{'='*100}\n")
        
        iteration = 0
        last_values = {ch: None for ch in channels}
        start_time = time.time()
        total_movement = {ch: 0.0 for ch in channels}
        
        while count is None or iteration < count:
            iteration += 1
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            
            # 读取所有指定通道的编码器值
            current_values = {}
            read_success = True
            for ch in channels:
                value = read_channel_value(device, ch)
                if value is None:
                    read_success = False
                current_values[ch] = value
            
            if not read_success:
                print(f"[{timestamp}] 读取#{iteration:04d}: ⚠ 部分通道读取失败")
            
            # 计算变化量
            deltas = {}
            has_movement = False
            if show_delta:
                for ch in channels:
                    delta = calculate_change(current_values[ch], last_values[ch])
                    deltas[ch] = delta
                    if delta is not None:
                        total_movement[ch] += abs(delta)
                        if abs(delta) > 0.01:
                            has_movement = True
            
            # 每次都输出或只在有变化时输出
            if has_movement or iteration % 10 == 1:
                print(f"\n[{timestamp}] 读取#{iteration:04d} (运行时间: {time.time()-start_time:.1f}s):")
                print(f"{'─'*100}")
                
                # 输出表头
                if show_delta:
                    print(f"{'代码CH':<8} {'采集器':<10} {'物理CH':<8} {'编码器位置':>14} {'变化量':>14} {'累计运动':>14} {'状态':<10}")
                else:
                    print(f"{'代码CH':<8} {'采集器':<10} {'物理CH':<8} {'编码器位置':>14} {'状态':<10}")
                print(f"{'─'*100}")
                
                # 输出每个通道的数据
                for ch in channels:
                    collector_ip, physical_ch, device_desc = get_channel_info(ch)
                    collector_str = f".1.{collector_ip.split('.')[-1]}"
                    
                    # 处理可能的None值
                    if current_values[ch] is not None:
                        enc_val = f"{current_values[ch]:.4f}"
                    else:
                        enc_val = "N/A"
                    
                    # 判断是否运动
                    if show_delta and deltas.get(ch) is not None and abs(deltas[ch]) > 0.01:
                        status = "● 运动中"
                        marker = "→"
                    else:
                        status = "○ 静止"
                        marker = " "
                    
                    if show_delta:
                        delta_str = f"{deltas[ch]:+.4f}" if deltas.get(ch) is not None else "---"
                        total_str = f"{total_movement[ch]:.4f}"
                        print(f"{marker} {ch:<7} {collector_str:<10} CH{physical_ch:<6} {enc_val:>14} {delta_str:>14} {total_str:>14} {status:<10}")
                    else:
                        print(f"{marker} {ch:<7} {collector_str:<10} CH{physical_ch:<6} {enc_val:>14} {status:<10}")
                
                print(f"{'─'*100}")
                
                # 显示运动提示
                if has_movement:
                    moving_list = []
                    for ch in channels:
                        if deltas.get(ch) is not None and abs(deltas[ch]) > 0.01:
                            collector_ip, physical_ch, device_desc = get_channel_info(ch)
                            moving_list.append(f"{ch}({device_desc})")
                    print(f"⚠ 正在运动: {', '.join(moving_list)}")
            else:
                # 简化输出：只显示计数
                print(f"\r[{timestamp}] 读取#{iteration:04d} - 所有通道静止", end="", flush=True)
            
            # 更新上次值
            for ch in channels:
                last_values[ch] = current_values[ch]
            
            time.sleep(interval)
        
        print(f"\n\n{'='*100}")
        print(f"读取完成，共执行 {iteration} 次，总运行时间: {time.time()-start_time:.1f}秒")
        if show_delta:
            print(f"\n各通道累计运动量:")
            for ch in channels:
                collector_ip, physical_ch, device_desc = get_channel_info(ch)
                print(f"  CH{ch:2d} (.1.{collector_ip.split('.')[-1]} CH{physical_ch}) - {device_desc:20s}: {total_movement[ch]:10.4f}")
        print(f"{'='*100}\n")
    
    except KeyboardInterrupt:
        print(f"\n\n{'='*100}")
        print("⚠ 用户中断")
        if iteration > 0:
            print(f"已执行 {iteration} 次读取，运行时间: {time.time()-start_time:.1f}秒")
            if show_delta:
                print(f"\n各通道累计运动量:")
                for ch in channels:
                    collector_ip, physical_ch, device_desc = get_channel_info(ch)
                    print(f"  CH{ch:2d} (.1.{collector_ip.split('.')[-1]} CH{physical_ch}) - {device_desc:20s}: {total_movement[ch]:10.4f}")
        print(f"{'='*100}\n")
    
    except tango.DevFailed as e:
        print(f"\n✗ Tango 错误:")
        for err in e.args:
            print(f"  - {err.reason}: {err.desc}")
        sys.exit(1)
    
    except Exception as e:
        print(f"\n✗ 错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='读取编码器设备的指定通道位置值',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
通道映射说明:
  代码通道 0-9:   192.168.1.199 物理通道 1-10 (六自由度、大行程、反射光部分)
  代码通道 10-17: 192.168.1.198 物理通道 1-8  (反射光部分、辅助支撑)

示例:
  # 读取六自由度所有轴 (通道 0-5)
  python3 %(prog)s -c 0 1 2 3 4 5
  
  # 读取大行程 (通道 6)
  python3 %(prog)s -c 6
  
  # 读取反射光成像设备 (通道 7-12)
  python3 %(prog)s -c 7 8 9 10 11 12
  
  # 读取辅助支撑设备 (通道 13-17)
  python3 %(prog)s -c 13 14 15 16 17
  
  # 指定设备名称和读取间隔
  python3 %(prog)s sys/encoder/1 -c 0 1 2 -i 0.2
  
  # 读取100次后停止
  python3 %(prog)s -c 0 1 2 --count 100
  
  # 不显示变化量
  python3 %(prog)s -c 0 1 2 --no-delta
        """
    )
    
    parser.add_argument(
        'device', 
        nargs='?', 
        default='sys/encoder/1',
        help='设备名称 (默认: sys/encoder/1)'
    )
    
    parser.add_argument(
        '-c', '--channels',
        type=int,
        nargs='+',
        required=True,
        help='要读取的通道号列表 (0-17)，例如: -c 0 1 2'
    )
    
    parser.add_argument(
        '-i', '--interval',
        type=float,
        default=0.5,
        help='读取间隔（秒）(默认: 0.5)'
    )
    
    parser.add_argument(
        '--count',
        type=int,
        default=None,
        help='读取次数，不指定则无限循环'
    )
    
    parser.add_argument(
        '--no-delta',
        action='store_true',
        help='不显示变化量和累计运动'
    )
    
    args = parser.parse_args()
    
    # 参数验证
    if args.interval < 0.05:
        print("⚠ 警告: 间隔时间过短可能影响设备性能，建议 >= 0.05秒")
        args.interval = 0.05
    
    test_read_channels(args.device, args.channels, args.interval, args.count, not args.no_delta)

if __name__ == "__main__":
    main()
