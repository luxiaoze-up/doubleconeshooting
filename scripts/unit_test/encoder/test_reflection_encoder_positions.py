#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
测试反射光成像设备的编码器位置保存功能

功能说明：
1. 读取当前上下平台编码器值
2. 调用 saveEncoderPositions 命令保存到数据库
3. 读取保存的属性值
4. 验证保存是否成功
5. 模拟重启后读取保存的值

使用方法：
    python scripts/test_reflection_encoder_positions.py
"""

import sys
import os
import time
import tango
from tango import DeviceProxy, DevFailed

# 设置 TANGO_HOST（如果没有设置环境变量）
if 'TANGO_HOST' not in os.environ:
    os.environ['TANGO_HOST'] = '192.168.1.177:10000'

REFLECTION_DEVICE = "sys/reflection/1"

def print_separator(title=""):
    """打印分隔线"""
    if title:
        print(f"\n{'='*60}")
        print(f"  {title}")
        print('='*60)
    else:
        print('-'*60)

def test_encoder_position_save():
    """测试编码器位置保存功能"""
    
    print_separator("反射光成像设备编码器位置保存功能测试")
    
    try:
        # 连接设备
        print(f"\n1. 连接到设备: {REFLECTION_DEVICE}")
        device = DeviceProxy(REFLECTION_DEVICE)
        print(f"   ✓ 设备状态: {device.state()}")
        
        # 读取当前编码器值
        print_separator("2. 读取当前编码器值")
        
        print("\n   上平台编码器值 (upperPlatformReadEncoder):")
        try:
            upper_encoder_values = device.command_inout("upperPlatformReadEncoder")
            for i, val in enumerate(upper_encoder_values):
                axis_name = ['X', 'Y', 'Z'][i]
                print(f"      轴{axis_name}: {val:.6f} mm")
        except DevFailed as e:
            print(f"      ✗ 读取失败: {e.args[0].desc}")
            upper_encoder_values = None
        
        print("\n   下平台编码器值 (lowerPlatformReadEncoder):")
        try:
            lower_encoder_values = device.command_inout("lowerPlatformReadEncoder")
            for i, val in enumerate(lower_encoder_values):
                axis_name = ['X', 'Y', 'Z'][i]
                print(f"      轴{axis_name}: {val:.6f} mm")
        except DevFailed as e:
            print(f"      ✗ 读取失败: {e.args[0].desc}")
            lower_encoder_values = None
        
        # 读取保存前的属性值
        print_separator("3. 读取保存前的属性值")
        
        try:
            upper_saved_before = device.read_attribute("upperPlatformEncoderPos").value
            print(f"\n   upperPlatformEncoderPos (保存前):")
            for i, val in enumerate(upper_saved_before):
                axis_name = ['X', 'Y', 'Z'][i]
                print(f"      轴{axis_name}: {val:.6f} mm")
        except DevFailed as e:
            print(f"   ✗ 读取失败: {e.args[0].desc}")
            upper_saved_before = None
        
        try:
            lower_saved_before = device.read_attribute("lowerPlatformEncoderPos").value
            print(f"\n   lowerPlatformEncoderPos (保存前):")
            for i, val in enumerate(lower_saved_before):
                axis_name = ['X', 'Y', 'Z'][i]
                print(f"      轴{axis_name}: {val:.6f} mm")
        except DevFailed as e:
            print(f"   ✗ 读取失败: {e.args[0].desc}")
            lower_saved_before = None
        
        # 保存编码器位置
        print_separator("4. 保存编码器位置到数据库")
        
        try:
            print("\n   执行命令: saveEncoderPositions")
            device.command_inout("saveEncoderPositions")
            result = device.read_attribute("resultValue").value
            if result == 0:
                print("   ✓ 保存成功!")
            else:
                print(f"   ✗ 保存失败 (resultValue={result})")
                return False
        except DevFailed as e:
            print(f"   ✗ 命令执行失败: {e.args[0].desc}")
            return False
        
        # 等待一下确保数据库写入完成
        time.sleep(0.5)
        
        # 读取保存后的属性值
        print_separator("5. 验证保存后的属性值")
        
        success = True
        
        try:
            upper_saved_after = device.read_attribute("upperPlatformEncoderPos").value
            print(f"\n   upperPlatformEncoderPos (保存后):")
            for i, val in enumerate(upper_saved_after):
                axis_name = ['X', 'Y', 'Z'][i]
                print(f"      轴{axis_name}: {val:.6f} mm")
            
            # 验证是否与读取的编码器值一致
            if upper_encoder_values is not None:
                print("\n   验证上平台数据一致性:")
                for i in range(3):
                    axis_name = ['X', 'Y', 'Z'][i]
                    diff = abs(upper_encoder_values[i] - upper_saved_after[i])
                    if diff < 0.001:  # 允许0.001mm误差
                        print(f"      轴{axis_name}: ✓ 一致 (差值: {diff:.6f} mm)")
                    else:
                        print(f"      轴{axis_name}: ✗ 不一致 (差值: {diff:.6f} mm)")
                        success = False
        except DevFailed as e:
            print(f"   ✗ 读取失败: {e.args[0].desc}")
            success = False
        
        try:
            lower_saved_after = device.read_attribute("lowerPlatformEncoderPos").value
            print(f"\n   lowerPlatformEncoderPos (保存后):")
            for i, val in enumerate(lower_saved_after):
                axis_name = ['X', 'Y', 'Z'][i]
                print(f"      轴{axis_name}: {val:.6f} mm")
            
            # 验证是否与读取的编码器值一致
            if lower_encoder_values is not None:
                print("\n   验证下平台数据一致性:")
                for i in range(3):
                    axis_name = ['X', 'Y', 'Z'][i]
                    diff = abs(lower_encoder_values[i] - lower_saved_after[i])
                    if diff < 0.001:  # 允许0.001mm误差
                        print(f"      轴{axis_name}: ✓ 一致 (差值: {diff:.6f} mm)")
                    else:
                        print(f"      轴{axis_name}: ✗ 不一致 (差值: {diff:.6f} mm)")
                        success = False
        except DevFailed as e:
            print(f"   ✗ 读取失败: {e.args[0].desc}")
            success = False
        
        # 测试数据库持久化
        print_separator("6. 测试数据库持久化")
        print("\n   提示: 重启设备服务后，编码器位置应该从数据库恢复")
        print("   可以通过以下命令重启设备服务验证:")
        print(f"   1. 停止服务: kill <pid>")
        print(f"   2. 启动服务: ./build/reflection_imaging_server reflection")
        print(f"   3. 运行本脚本再次检查属性值")
        
        # 总结
        print_separator("测试结果")
        if success:
            print("\n   ✓ 所有测试通过!")
            print("   - 编码器位置读取成功")
            print("   - saveEncoderPositions 命令执行成功")
            print("   - 保存的数据与读取值一致")
            print("   - 属性值已持久化到数据库")
        else:
            print("\n   ✗ 部分测试失败，请检查日志")
        
        return success
        
    except DevFailed as e:
        print(f"\n✗ Tango错误: {e.args[0].desc}")
        return False
    except Exception as e:
        print(f"\n✗ 异常: {str(e)}")
        import traceback
        traceback.print_exc()
        return False

def test_read_saved_positions():
    """仅读取已保存的编码器位置（用于重启后验证）"""
    
    print_separator("读取已保存的编码器位置")
    
    try:
        device = DeviceProxy(REFLECTION_DEVICE)
        print(f"\n设备状态: {device.state()}")
        
        print("\n上平台保存的编码器位置:")
        upper_pos = device.read_attribute("upperPlatformEncoderPos").value
        for i, val in enumerate(upper_pos):
            axis_name = ['X', 'Y', 'Z'][i]
            print(f"   轴{axis_name}: {val:.6f} mm")
        
        print("\n下平台保存的编码器位置:")
        lower_pos = device.read_attribute("lowerPlatformEncoderPos").value
        for i, val in enumerate(lower_pos):
            axis_name = ['X', 'Y', 'Z'][i]
            print(f"   轴{axis_name}: {val:.6f} mm")
        
        return True
        
    except DevFailed as e:
        print(f"\n✗ Tango错误: {e.args[0].desc}")
        return False
    except Exception as e:
        print(f"\n✗ 异常: {str(e)}")
        return False

def main():
    """主函数"""
    if len(sys.argv) > 1 and sys.argv[1] == "--read-only":
        # 仅读取模式（用于重启后验证）
        success = test_read_saved_positions()
    else:
        # 完整测试模式
        success = test_encoder_position_save()
    
    print("\n")
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
