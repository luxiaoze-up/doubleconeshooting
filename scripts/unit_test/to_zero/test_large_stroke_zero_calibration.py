#!/usr/bin/env python3
"""
大行程设备回零功能测试脚本
功能：移动到负限位并记录编码器位置，用于零点标定
"""

import sys
import time
import tango
from datetime import datetime

def test_zero_calibration(device_name="sys/large_stroke/1"):
    """
    测试大行程回零功能
    
    Args:
        device_name: 大行程设备名称
    """
    print(f"\n{'='*70}")
    print(f"大行程设备 - 回零功能测试")
    print(f"设备: {device_name}")
    print(f"时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'='*70}\n")
    
    try:
        # 连接设备
        print(f"[1/6] 连接到设备 {device_name}...")
        device = tango.DeviceProxy(device_name)
        print("✓ 设备连接成功\n")
        
        # 检查初始状态
        print(f"[2/6] 检查设备状态...")
        state = device.state()
        print(f"设备状态: {state}")
        
        if state == tango.DevState.FAULT:
            print("⚠ 设备处于 FAULT 状态，尝试复位...")
            device.command_inout("reset")
            time.sleep(2)
            state = device.state()
            print(f"复位后状态: {state}")
        
        if state not in [tango.DevState.ON, tango.DevState.STANDBY]:
            print(f"✗ 设备状态不适合运动: {state}")
            print("提示: 设备需要处于 ON 或 STANDBY 状态")
            return False
        print("✓ 设备状态正常\n")
        
        # 读取当前编码器位置
        print(f"[3/6] 读取当前编码器位置...")
        try:
            current_pos = device.command_inout("readEncoder")
            print(f"当前编码器位置: {current_pos:.2f} mm")
        except Exception as e:
            print(f"⚠ 无法读取编码器位置: {e}")
            current_pos = None
        print()
        
        # 执行回零操作
        print(f"[4/6] 执行回零操作（移动到负限位并保存编码器位置）...")
        print("提示: 设备将向负限位方向运动，触发限位后自动保存编码器位置")
        
        # 确认操作
        response = input("是否继续？(y/n): ")
        if response.lower() != 'y':
            print("操作已取消")
            return False
        
        print("\n开始回零...")
        start_time = time.time()
        
        try:
            device.command_inout("moveToZero")
            print("✓ 回零命令已发送（将自动移动到负限位并保存编码器位置）")
        except tango.DevFailed as e:
            print(f"✗ 回零命令失败: {e.args[0].desc}")
            return False
        
        # 监控运动状态
        print("\n[5/6] 监控运动状态...")
        print(f"{'时间(s)':<10} {'状态':<12} {'编码器位置(mm)':<20} {'限位状态':<15}")
        print(f"{'-'*70}")
        
        motion_timeout = 120  # 2分钟超时
        last_pos = current_pos
        stable_count = 0
        
        while True:
            elapsed = time.time() - start_time
            
            # 检查超时
            if elapsed > motion_timeout:
                print(f"\n⚠ 运动超时（{motion_timeout}秒）")
                device.command_inout("stop")
                return False
            
            # 读取状态
            state = device.state()
            
            # 读取编码器位置
            try:
                encoder_pos = device.command_inout("readEncoder")
            except:
                encoder_pos = None
            
            # 读取限位状态
            try:
                el_state = device.command_inout("readEL")
                el_text = {0: "无限位", 1: "正限位(EL+)", -1: "负限位(EL-)"}
                el_status = el_text.get(el_state, f"未知({el_state})")
            except:
                el_status = "读取失败"
                el_state = None
            
            # 显示状态
            pos_str = f"{encoder_pos:.2f}" if encoder_pos is not None else "N/A"
            print(f"{elapsed:<10.1f} {str(state):<12} {pos_str:<20} {el_status:<15}")
            
            # 检查是否到达负限位
            if el_state == -1:  # 负限位
                print(f"\n✓ 已到达负限位")
                # 等待状态稳定
                time.sleep(1)
                break
            
            # 检查运动是否完成
            if state == tango.DevState.ON:
                # 检查位置是否稳定
                if encoder_pos is not None and last_pos is not None:
                    if abs(encoder_pos - last_pos) < 0.01:  # 位置变化小于0.01mm
                        stable_count += 1
                        if stable_count >= 3:  # 连续3次稳定
                            print(f"\n✓ 运动完成（位置稳定）")
                            break
                    else:
                        stable_count = 0
                last_pos = encoder_pos
            
            time.sleep(0.5)
        
        # 读取最终编码器位置（已由 moveToZero 自动保存）
        print(f"\n[6/6] 读取零点编码器位置...")
        time.sleep(0.5)  # 确保运动完全停止
        
        try:
            final_pos = device.command_inout("readEncoder")
            print(f"零点编码器位置: {final_pos:.3f} mm")
            print("✓ 编码器位置已由 moveToZero 命令自动保存到数据库")
            
        except Exception as e:
            print(f"⚠ 无法读取编码器位置: {e}")
            final_pos = None
        
        # 显示总结
        print(f"\n{'='*70}")
        print("回零功能测试完成")
        print(f"{'='*70}")
        print(f"运动时间: {elapsed:.1f} 秒")
        if current_pos is not None and final_pos is not None:
            print(f"运动距离: {abs(final_pos - current_pos):.2f} mm")
        print(f"零点位置: {final_pos:.3f} mm" if final_pos is not None else "零点位置: 未知")
        print(f"{'='*70}\n")
        
        return True
        
    except tango.DevFailed as e:
        print(f"\n✗ Tango错误: {e.args[0].desc}")
        return False
    except KeyboardInterrupt:
        print(f"\n\n⚠ 用户中断，停止运动...")
        try:
            device.command_inout("stop")
            print("✓ 已发送停止命令")
        except:
            pass
        return False
    except Exception as e:
        print(f"\n✗ 未知错误: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='大行程设备回零功能测试',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 使用默认设备
  python3 scripts/test_large_stroke_zero_calibration.py
  
  # 指定设备名称
  python3 scripts/test_large_stroke_zero_calibration.py sys/large_stroke/1
  
  # 设置 TANGO_HOST
  TANGO_HOST=192.168.1.177:10000 python3 scripts/test_large_stroke_zero_calibration.py

功能说明:
  1. 连接到大行程设备
  2. 检查设备状态
  3. 读取当前编码器位置
  4. 执行回零操作（移动到负限位）
  5. 监控运动状态和编码器位置
  6. 记录并保存零点编码器位置
  
注意事项:
  - 回零操作会使设备运动到负限位
  - 请确保运动路径安全，无障碍物
  - 编码器位置将自动保存到数据库
  - 可使用 Ctrl+C 中断运动
        """
    )
    
    parser.add_argument('device', nargs='?', default='sys/large_stroke/1',
                       help='设备名称 (默认: sys/large_stroke/1)')
    
    args = parser.parse_args()
    
    # 执行测试
    success = test_zero_calibration(args.device)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
