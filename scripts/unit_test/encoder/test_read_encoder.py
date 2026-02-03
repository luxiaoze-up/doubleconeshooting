#!/usr/bin/env python3
"""
编码器采集设备循环读取脚本
实时读取编码器设备的所有通道位置值
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

def read_encoder_values(device, num_channels=18):
    """读取所有通道的编码器值
    
    Args:
        device: 设备代理
        num_channels: 通道数量
    
    Returns:
        tuple: (encoder_values, success)
    """
    try:
        encoder_values = []
        for channel in range(1, num_channels + 1):
            try:
                value = device.command_inout("readEncoder", channel)
                encoder_values.append(value)
            except Exception as e:
                # 如果通道未配置，停止读取
                if "not configured" in str(e).lower() or "invalid" in str(e).lower():
                    break
                encoder_values.append(None)
        return (encoder_values, True)
    except Exception as e:
        return (None, False)

def calculate_change(current, previous):
    """计算编码器值变化"""
    if current is None or previous is None:
        return None
    return current - previous

def test_read_encoder(device_name="sys/encoder/1", interval=0.5, count=None, show_delta=True, max_channels=18):
    """循环读取编码器值
    
    Args:
        device_name: 设备名称
        interval: 读取间隔（秒）
        count: 读取次数，None表示无限循环
        show_delta: 是否显示变化量
        max_channels: 最大通道数
    """
    print(f"\n{'='*90}")
    print(f"编码器采集设备循环读取")
    print(f"设备名称: {device_name}")
    print(f"读取间隔: {interval}秒")
    print(f"读取次数: {'无限' if count is None else count}")
    print(f"显示变化: {'是' if show_delta else '否'}")
    print(f"{'='*90}\n")
    
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
        
        print(f"\n{'='*90}")
        print("开始循环读取 (按Ctrl+C停止)...")
        print(f"{'='*90}\n")
        
        iteration = 0
        last_values = None
        start_time = time.time()
        num_channels = 0  # 实际通道数
        total_movement = []  # 累计运动量
        
        # 首次读取确定通道数
        first_read, success = read_encoder_values(device, max_channels)
        if success and first_read:
            num_channels = len(first_read)
            total_movement = [0.0] * num_channels
            print(f"检测到 {num_channels} 个有效通道\n")
        else:
            print("⚠ 首次读取失败，请检查设备连接\n")
            return
        
        while count is None or iteration < count:
            iteration += 1
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            
            # 读取编码器值
            encoder_values, success = read_encoder_values(device, num_channels)
            
            if not success:
                print(f"[{timestamp}] 读取#{iteration:04d}: ⚠ 读取失败")
                time.sleep(interval)
                continue
            
            # 计算变化量
            deltas = None
            if show_delta and last_values is not None:
                deltas = [calculate_change(encoder_values[i], last_values[i]) for i in range(num_channels)]
                # 累计运动量
                for i in range(num_channels):
                    if deltas[i] is not None:
                        total_movement[i] += abs(deltas[i])
            
            # 检测是否有运动
            has_movement = False
            if deltas is not None:
                has_movement = any(d is not None and abs(d) > 0.01 for d in deltas)
            
            # 每次都输出或只在有变化时输出
            if has_movement or iteration % 10 == 1:
                print(f"\n[{timestamp}] 读取#{iteration:04d} (运行时间: {time.time()-start_time:.1f}s):")
                print(f"{'─'*110}")
                
                # 输出表头
                if show_delta:
                    print(f"{'采集器':<8} {'物理CH':<7} {'编码器位置':>14} {'变化量':>14} {'累计运动':>14} {'状态':>10} {'设备说明':<20}")
                else:
                    print(f"{'采集器':<8} {'物理CH':<7} {'编码器位置':>14} {'状态':>10} {'设备说明':<20}")
                print(f"{'─'*110}")
                
                # 输出每个通道的数据
                for i in range(num_channels):
                    axis = i + 1  # 通道号从1开始
                    # 获取通道映射信息
                    if axis in CHANNEL_MAPPING:
                        collector_ip, physical_ch, device_desc = CHANNEL_MAPPING[axis]
                        collector_str = f".1.{collector_ip}"
                        physical_str = f"CH{physical_ch}"
                    else:
                        collector_str = "未知"
                        physical_str = f"CH?"
                        device_desc = "未配置"
                    
                    # 处理可能的None值（使用数组索引i）
                    if encoder_values[i] is not None:
                        enc_val = f"{encoder_values[i]:.4f}"
                    else:
                        enc_val = "N/A"
                    
                    # 判断是否运动（使用数组索引i）
                    is_moving = False
                    if deltas and deltas[i] is not None and abs(deltas[i]) > 0.01:
                        status = "● 运动中"
                        marker = "→"
                        is_moving = True
                    else:
                        status = "○ 静止"
                        marker = " "
                    
                    if show_delta:
                        delta_str = f"{deltas[i]:+.4f}" if deltas and deltas[i] is not None else "---"
                        total_str = f"{total_movement[i]:.4f}"
                        print(f"{marker} {collector_str:<7} {physical_str:<7} {enc_val:>14} {delta_str:>14} {total_str:>14} {status:>10} {device_desc:<20}")
                    else:
                        print(f"{marker} {collector_str:<7} {physical_str:<7} {enc_val:>14} {status:>10} {device_desc:<20}")
                
                print(f"{'─'*110}")
                
                # 显示运动提示
                if has_movement:
                    moving_channels = []
                    for i in range(num_channels):
                        if deltas[i] is not None and abs(deltas[i]) > 0.01:
                            axis = i + 1  # 通道号从1开始
                            if axis in CHANNEL_MAPPING:
                                collector_ip, physical_ch, device_desc = CHANNEL_MAPPING[axis]
                                moving_channels.append(f"{collector_ip}-CH{physical_ch}({device_desc})")
                            else:
                                moving_channels.append(f"未知-{axis}")
                    print(f"⚠ 正在运动: {', '.join(moving_channels)}")
            else:
                # 简化输出：只显示计数和总体信息
                print(f"\r[{timestamp}] 读取#{iteration:04d} - 所有轴静止", end="", flush=True)
            
            last_values = encoder_values[:]
            time.sleep(interval)
        
        print(f"\n\n{'='*110}")
        print(f"读取完成，共执行 {iteration} 次，总运行时间: {time.time()-start_time:.1f}秒")
        if show_delta and num_channels > 0:
            print(f"\n各通道累计运动量:")
            for i in range(num_channels):
                channel = i + 1  # 通道号从1开始
                if channel in CHANNEL_MAPPING:
                    collector_ip, physical_ch, device_desc = CHANNEL_MAPPING[channel]
                    print(f"  .1.{collector_ip} CH{physical_ch:2d} ({device_desc:20s}): {total_movement[i]:10.4f}")
                else:
                    print(f"  未知通道 {channel}: {total_movement[i]:.4f}")
        print(f"{'='*110}\n")
    
    except KeyboardInterrupt:
        print(f"\n\n{'='*110}")
        print("⚠ 用户中断")
        if iteration > 0:
            print(f"已执行 {iteration} 次读取，运行时间: {time.time()-start_time:.1f}秒")
            if show_delta and num_channels > 0:
                print(f"\n各通道累计运动量:")
                for channel in range(1, num_channels + 1):
                    if channel in CHANNEL_MAPPING:
                        collector_ip, physical_ch, device_desc = CHANNEL_MAPPING[channel]
                        print(f"  .1.{collector_ip} CH{physical_ch:2d} ({device_desc:20s}): {total_movement[channel]:10.4f}")
                    else:
                        print(f"  未知通道 {channel}: {total_movement[channel]:.4f}")
        print(f"{'='*110}\n")
    
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
        description='循环读取编码器采集设备的各通道位置值',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 使用默认设备，0.5秒间隔，无限循环
  python3 %(prog)s
  
  # 指定设备名称
  python3 %(prog)s sys/encoder/1
  
  # 1秒间隔读取
  python3 %(prog)s --interval 1.0
  
  # 读取100次后停止
  python3 %(prog)s --count 100
  
  # 快速监控（0.1秒间隔）
  python3 %(prog)s -i 0.1
  
  # 不显示变化量
  python3 %(prog)s --no-delta
        """
    )
    
    parser.add_argument(
        'device', 
        nargs='?', 
        default='sys/encoder/1',
        help='设备名称 (默认: sys/encoder/1)'
    )
    
    parser.add_argument(
        '-i', '--interval',
        type=float,
        default=0.5,
        help='读取间隔（秒）(默认: 0.5)'
    )
    
    parser.add_argument(
        '-c', '--count',
        type=int,
        default=None,
        help='读取次数，不指定则无限循环'
    )
    
    parser.add_argument(
        '--no-delta',
        action='store_true',
        help='不显示变化量和累计运动'
    )
    
    parser.add_argument(
        '--max-channels',
        type=int,
        default=18,
        help='最大通道数 (默认: 18)'
    )
    
    args = parser.parse_args()
    
    # 参数验证
    if args.interval < 0.05:
        print("⚠ 警告: 间隔时间过短可能影响设备性能，建议 >= 0.05秒")
        args.interval = 0.05
    
    test_read_encoder(args.device, args.interval, args.count, not args.no_delta, args.max_channels)

if __name__ == "__main__":
    main()
