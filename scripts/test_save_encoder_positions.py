#!/usr/bin/env python3
"""
测试保存六自由度编码器位置到数据库

功能说明:
1. 读取当前的编码器位置
2. 调用saveEncoderPositions命令将位置保存到数据库
3. 验证数据已成功保存到axis_pos property

使用方法:
    python test_save_encoder_positions.py
"""

import tango
import sys
import json

def test_save_encoder_positions():
    """测试保存编码器位置功能"""
    
    device_name = "sys/six_dof/1"
    
    print(f"连接到设备: {device_name}")
    try:
        device = tango.DeviceProxy(device_name)
        print(f"✓ 设备连接成功")
        print(f"  状态: {device.state()}")
        print()
        
        # 1. 读取当前编码器位置
        print("=" * 60)
        print("1. 读取当前编码器位置")
        print("=" * 60)
        try:
            axis_pos = device.read_attribute("axisPos").value
            print(f"当前编码器位置:")
            for i, pos in enumerate(axis_pos):
                print(f"  轴{i+1}: {pos:.3f} mm")
            print()
        except Exception as e:
            print(f"✗ 读取编码器位置失败: {e}")
            return False
        
        # 2. 保存编码器位置到数据库
        print("=" * 60)
        print("2. 保存编码器位置到数据库")
        print("=" * 60)
        try:
            device.command_inout("saveEncoderPositions")
            print("✓ saveEncoderPositions 命令执行成功")
            print()
        except Exception as e:
            print(f"✗ 保存编码器位置失败: {e}")
            return False
        
        # 3. 验证数据已保存到数据库
        print("=" * 60)
        print("3. 验证数据库中的axis_pos属性")
        print("=" * 60)
        try:
            db = tango.Database()
            axis_pos_prop = db.get_device_property(device_name, "axis_pos")["axis_pos"]
            
            print("数据库中保存的编码器位置:")
            for i, pos_str in enumerate(axis_pos_prop):
                print(f"  轴{i+1}: {pos_str} mm")
            print()
            
            # 对比内存中的值和数据库中的值
            print("数据一致性检查:")
            all_match = True
            for i, (mem_val, db_val) in enumerate(zip(axis_pos, axis_pos_prop)):
                db_float = float(db_val)
                match = abs(mem_val - db_float) < 0.001
                status = "✓" if match else "✗"
                print(f"  {status} 轴{i+1}: 内存={mem_val:.3f}, 数据库={db_float:.3f}")
                if not match:
                    all_match = False
            
            if all_match:
                print("\n✓ 所有数据一致，保存成功！")
                return True
            else:
                print("\n✗ 数据不一致")
                return False
                
        except Exception as e:
            print(f"✗ 验证数据库属性失败: {e}")
            return False
            
    except tango.DevFailed as e:
        print(f"✗ Tango错误: {e.args[0].desc}")
        return False
    except Exception as e:
        print(f"✗ 未知错误: {e}")
        return False

def show_usage_example():
    """显示在其他程序中的使用示例"""
    print("\n" + "=" * 60)
    print("在其他程序中使用示例")
    print("=" * 60)
    
    python_example = '''
# Python示例
import tango

device = tango.DeviceProxy("sys/six_dof/1")

# 在运动完成后保存编码器位置
device.command_inout("movePoseAbsolute", [0, 0, 0, 0, 0, 0])
# ... 等待运动完成 ...
device.command_inout("saveEncoderPositions")
print("编码器位置已保存到数据库")
'''
    
    cpp_example = '''
// C++示例
Tango::DeviceProxy* device = new Tango::DeviceProxy("sys/six_dof/1");

// 在运动完成后保存编码器位置
Tango::DeviceData data;
std::vector<double> pose = {0, 0, 0, 0, 0, 0};
data << pose;
device->command_inout("movePoseAbsolute", data);
// ... 等待运动完成 ...
device->command_inout("saveEncoderPositions");
std::cout << "编码器位置已保存到数据库" << std::endl;
'''
    
    print("Python示例:")
    print(python_example)
    print("\nC++示例:")
    print(cpp_example)

def main():
    print("\n" + "=" * 60)
    print("六自由度编码器位置保存测试")
    print("=" * 60)
    print()
    
    success = test_save_encoder_positions()
    
    if success:
        print("\n✓ 测试通过！")
        show_usage_example()
        return 0
    else:
        print("\n✗ 测试失败")
        return 1

if __name__ == "__main__":
    sys.exit(main())
