#!/usr/bin/env python3
"""
六自由度限位信号循环读取脚本
读取所有6个轴的限位状态（正限位、负限位、原点信号）
"""

import sys
import time
import tango
from datetime import datetime

def format_limit_status(el_value):
    """格式化限位状态显示
    
    Args:
        el_value: readEL返回值
            1 = 无限位触发
            -1 = 正限位(EL+)触发
            2 = 负限位(EL-)触发
    
    Returns:
        格式化的状态字符串
    """
    if el_value == 0:
        return "○ 正常"
    elif el_value == 1:
        return "● EL+"
    elif el_value == -1:
        return "● EL-"
    else:
        return f"? {el_value}"

def read_all_limits(device):
    """读取所有6轴的限位和原点信号
    
    Returns:
        tuple: (limit_states, org_states, encoder_values, success)
    """
    limit_states = []
    org_states = []
    encoder_values = []
    
    try:
        # 读取所有轴的限位状态
        for axis in range(6):
            try:
                el_state = device.command_inout("readEL", axis)
                limit_states.append(el_state)
            except Exception as e:
                limit_states.append(None)
        
        # 读取所有轴的原点状态
        for axis in range(6):
            try:
                org_state = device.command_inout("readOrg", axis)
                org_states.append(org_state)
            except Exception as e:
                org_states.append(None)
        
        # 读取编码器位置
        try:
            encoder_values = device.command_inout("readEncoder")
        except Exception as e:
            encoder_values = [None] * 6
        
        return (limit_states, org_states, encoder_values, True)
    
    except Exception as e:
        return ([], [], [], False)

def test_read_limits(device_name="sys/motion/3", interval=0.5, count=None):
    """循环读取限位信号
    
    Args:
        device_name: 设备名称
        interval: 读取间隔（秒）
        count: 读取次数，None表示无限循环
    """
    print(f"\n{'='*80}")
    print(f"六自由度限位信号循环读取")
    print(f"设备名称: {device_name}")
    print(f"读取间隔: {interval}秒")
    print(f"读取次数: {'无限' if count is None else count}")
    print(f"{'='*80}\n")
    
    print("说明:")
    print("  限位状态: ○ 正常 | ● EL+ (正限位触发) | ● EL- (负限位触发)")
    print("  原点状态: ✓ 在原点 | ✗ 不在原点")
    print(f"\n{'='*80}\n")
    
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
        
        print(f"\n{'='*80}")
        print("开始循环读取 (按Ctrl+C停止)...")
        print(f"{'='*80}\n")
        
        iteration = 0
        last_limit_states = None
        last_org_states = None
        
        while count is None or iteration < count:
            iteration += 1
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            
            # 读取所有限位信号
            limit_states, org_states, encoder_values, success = read_all_limits(device)
            
            if not success:
                print(f"[{timestamp}] 读取#{iteration:04d}: ⚠ 读取失败")
                time.sleep(interval)
                continue
            
            # 检测状态变化
            limits_changed = (limit_states != last_limit_states)
            org_changed = (org_states != last_org_states)
            
            # 只有状态变化时或每10次输出一次完整信息
            if limits_changed or org_changed or iteration % 10 == 1:
                print(f"\n[{timestamp}] 读取#{iteration:04d}:")
                print(f"{'─'*80}")
                
                # 输出表头
                print(f"{'轴':<4} {'编码器位置':>12} {'限位状态':>12} {'原点状态':>12}")
                print(f"{'─'*80}")
                
                # 输出每个轴的数据
                for axis in range(6):
                    enc_val = f"{encoder_values[axis]:.2f}" if encoder_values[axis] is not None else "N/A"
                    limit_status = format_limit_status(limit_states[axis]) if limit_states[axis] is not None else "N/A"
                    org_status = "✓ 在原点" if org_states[axis] else "✗ 不在原点" if org_states[axis] is not None else "N/A"
                    
                    # 如果该轴限位触发，高亮显示
                    marker = "⚠" if limit_states[axis] in [1, 2] else " "
                    
                    print(f"{marker} {axis:<3} {enc_val:>12} {limit_status:>12} {org_status:>12}")
                
                print(f"{'─'*80}")
                
                # 显示变化提示
                if limits_changed and last_limit_states is not None:
                    print("⚠ 限位状态已变化!")
                if org_changed and last_org_states is not None:
                    print("⚠ 原点状态已变化!")
                
                last_limit_states = limit_states[:]
                last_org_states = org_states[:]
            else:
                # 简化输出：只显示计数
                print(f"\r[{timestamp}] 读取#{iteration:04d} - 无变化", end="", flush=True)
            
            time.sleep(interval)
        
        print(f"\n\n{'='*80}")
        print(f"读取完成，共执行 {iteration} 次")
        print(f"{'='*80}\n")
    
    except KeyboardInterrupt:
        print(f"\n\n{'='*80}")
        print("⚠ 用户中断")
        print(f"{'='*80}\n")
    
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
        description='循环读取六自由度设备的限位和原点信号',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 使用默认设备，0.5秒间隔，无限循环
  python3 %(prog)s
  
  # 指定设备名称
  python3 %(prog)s sys/six_dof/1
  
  # 1秒间隔读取
  python3 %(prog)s --interval 1.0
  
  # 读取100次后停止
  python3 %(prog)s --count 100
  
  # 快速监控（0.1秒间隔）
  python3 %(prog)s -i 0.1
        """
    )
    
    parser.add_argument(
        'device', 
        nargs='?', 
        default='sys/six_dof/1',
        help='设备名称 (默认: sys/six_dof/1)'
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
    
    args = parser.parse_args()
    
    # 参数验证
    if args.interval < 0.05:
        print("⚠ 警告: 间隔时间过短可能影响设备性能，建议 >= 0.05秒")
        args.interval = 0.05
    
    test_read_limits(args.device, args.interval, args.count)

if __name__ == "__main__":
    main()
