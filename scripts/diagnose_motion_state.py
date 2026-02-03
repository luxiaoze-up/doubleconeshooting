#!/usr/bin/env python3
"""
诊断大行程设备运动状态问题的脚本
检查运动控制器状态和编码器读数
"""

import PyTango
import time

def diagnose_motion_state():
    """诊断运动状态"""
    
    # 连接到大行程设备
    large_stroke = PyTango.DeviceProxy("sys/large_stroke/1")
    
    # 连接到运动控制器
    motion_controller = PyTango.DeviceProxy("sys/motion/3")
    
    # 连接到编码器
    encoder = PyTango.DeviceProxy("sys/encoder/1")
    
    print("=" * 80)
    print("大行程设备运动状态诊断")
    print("=" * 80)
    print()
    
    # 1. 检查大行程设备状态
    print("1. 大行程设备状态:")
    print(f"   状态: {large_stroke.state()}")
    print(f"   状态描述: {large_stroke.status()}")
    try:
        large_range_state = large_stroke.read_attribute("LargeRangeState").value
        print(f"   LargeRangeState: {large_range_state}")
    except Exception as e:
        print(f"   LargeRangeState: 读取失败 - {e}")
    print()
    
    # 2. 检查运动控制器状态
    print("2. 运动控制器 (sys/motion/3) 状态:")
    try:
        mc_state = motion_controller.state()
        print(f"   状态: {mc_state}")
        print(f"   状态描述: {motion_controller.status()}")
        
        # 尝试读取轴6的状态
        try:
            axis_state = motion_controller.command_inout("readAxisState", 6)
            print(f"   轴6状态: {axis_state}")
        except Exception as e:
            print(f"   轴6状态: 读取失败 - {e}")
            
    except Exception as e:
        print(f"   运动控制器状态读取失败: {e}")
    print()
    
    # 3. 检查编码器读数
    print("3. 编码器 (sys/encoder/1) 读数:")
    try:
        # 通过大行程设备读取编码器
        encoder_pos = large_stroke.command_inout("readEncoder")
        print(f"   通过大行程设备读取: {encoder_pos} steps")
    except Exception as e:
        print(f"   通过大行程设备读取失败: {e}")
    
    try:
        # 直接从编码器读取
        encoder_data = encoder.command_inout("readEncoder", 6)  # 通道6
        print(f"   直接从编码器读取: {encoder_data}")
    except Exception as e:
        print(f"   直接从编码器读取失败: {e}")
    print()
    
    # 4. 持续监控状态变化（10秒）
    print("4. 持续监控状态变化 (10秒):")
    print("   时间      大行程状态    运动控制器状态    编码器位置")
    print("   " + "-" * 70)
    
    for i in range(20):  # 每0.5秒检查一次，共10秒
        try:
            ls_state = str(large_stroke.state())
            mc_state = str(motion_controller.state())
            encoder_pos = large_stroke.command_inout("readEncoder")
            
            timestamp = time.strftime("%H:%M:%S")
            print(f"   {timestamp}   {ls_state:12s}  {mc_state:16s}  {encoder_pos:12.2f}")
            
        except Exception as e:
            print(f"   监控失败: {e}")
        
        time.sleep(0.5)
    
    print()
    print("=" * 80)
    print("诊断完成")
    print()
    print("分析建议:")
    print("1. 如果运动控制器状态一直是MOVING，说明运动命令未完成")
    print("   - 检查运动控制器是否正常响应")
    print("   - 检查轴是否被卡住或遇到阻力")
    print("   - 检查限位开关状态")
    print()
    print("2. 如果编码器位置始终为0，说明编码器未工作")
    print("   - 检查编码器连接")
    print("   - 检查编码器配置")
    print("   - 检查编码器通道是否正确")
    print()
    print("3. 如果状态能正常切换但日志没有输出，可能是日志级别问题")
    print("   - 检查日志配置")
    print("   - 检查read_attr_hardware是否被正常调用")
    print("=" * 80)

if __name__ == "__main__":
    try:
        diagnose_motion_state()
    except KeyboardInterrupt:
        print("\n诊断被用户中断")
    except Exception as e:
        print(f"\n诊断失败: {e}")
        import traceback
        traceback.print_exc()
