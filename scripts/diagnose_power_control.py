#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
电源控制诊断脚本
用于检查驱动器/刹车上电失败的原因

运行方式:
    python scripts/diagnose_power_control.py
"""

import tango
import json
import sys

# 检查的设备列表
DEVICES = [
    ("六自由度", "sys/six_dof/1"),
    ("大行程", "sys/large_stroke/1"),
    ("反射光成像", "sys/reflection/1"),
    ("辅助支撑1", "sys/auxiliary/1"),
]

# 检查的运动控制器
MOTION_CONTROLLERS = [
    ("运动控制器1", "sys/motion/1"),
    ("运动控制器2", "sys/motion/2"),
    ("运动控制器3", "sys/motion/3"),
]

def check_tango_db():
    """检查 Tango 数据库连接"""
    print("\n" + "="*60)
    print("1. 检查 Tango 数据库连接")
    print("="*60)
    
    try:
        db = tango.Database()
        print(f"✓ Tango 数据库连接成功: {db.get_db_host()}:{db.get_db_port()}")
        return db
    except Exception as e:
        print(f"✗ Tango 数据库连接失败: {e}")
        return None

def check_device_properties(db, device_name: str, props: list):
    """检查设备属性是否已注册"""
    print(f"\n  检查设备属性: {device_name}")
    
    try:
        prop_values = db.get_device_property(device_name, props)
        for prop_name, values in prop_values.items():
            if values:
                print(f"    ✓ {prop_name} = {values}")
            else:
                print(f"    ⚠ {prop_name} = (未配置)")
        return True
    except Exception as e:
        print(f"    ✗ 无法读取属性: {e}")
        return False

def check_motion_controller_status(device_name: str):
    """检查运动控制器状态"""
    try:
        mc = tango.DeviceProxy(device_name)
        state = mc.state()
        print(f"  ✓ {device_name} 状态: {state}")
        
        # 检查模拟模式
        try:
            sim_mode = mc.read_attribute("simMode").value
            if sim_mode:
                print(f"    ⚠ 警告: 运动控制器处于模拟模式!")
        except:
            pass
        
        # 测试 writeIO 命令
        try:
            cmds = [cmd.cmd_name for cmd in mc.command_list_query()]
            if "writeIO" in cmds:
                print(f"    ✓ writeIO 命令可用")
            else:
                print(f"    ✗ writeIO 命令不存在!")
        except Exception as e:
            print(f"    ⚠ 无法检查命令列表: {e}")
            
        return True
    except Exception as e:
        print(f"  ✗ 无法连接 {device_name}: {e}")
        return False

def check_device_power_status(device_name: str):
    """检查设备电源状态"""
    try:
        device = tango.DeviceProxy(device_name)
        state = device.state()
        print(f"\n  设备状态: {state}")
        
        # 查询电源状态
        try:
            status = device.command_inout("queryPowerStatus")
            status_dict = json.loads(status)
            print(f"  电源状态: {json.dumps(status_dict, indent=4, ensure_ascii=False)}")
            
            # 分析问题
            if status_dict.get("driverPowerEnabled") == False:
                port = status_dict.get("driverPowerPort", -1)
                ctrl = status_dict.get("driverPowerController", "")
                
                if port < 0:
                    print(f"  ⚠ 问题: driverPowerPort 未配置 (值={port})")
                if not ctrl:
                    print(f"  ⚠ 问题: driverPowerController 未配置")
                else:
                    print(f"  → 驱动器控制器: {ctrl}, 端口: OUT{port}")
                    
        except Exception as e:
            print(f"  ⚠ queryPowerStatus 失败: {e}")
            
        # 尝试读取属性
        try:
            driver_power = device.read_attribute("driverPowerStatus").value
            print(f"  driverPowerStatus 属性: {driver_power}")
        except Exception as e:
            print(f"  ⚠ 无法读取 driverPowerStatus: {e}")
            
        return True
    except Exception as e:
        print(f"  ✗ 无法连接设备: {e}")
        return False

def test_write_io(motion_controller: str, port: int):
    """手动测试 writeIO 命令"""
    print(f"\n  测试 writeIO 命令: {motion_controller} OUT{port}")
    
    try:
        mc = tango.DeviceProxy(motion_controller)
        
        # 读取当前状态
        try:
            current = mc.command_inout("readIO", port)
            print(f"    当前状态: OUT{port} = {current}")
        except Exception as e:
            print(f"    无法读取当前状态: {e}")
        
        # 尝试写入
        params = [float(port), 1.0]  # HIGH
        mc.command_inout("writeIO", params)
        print(f"    ✓ writeIO 执行成功: OUT{port} = 1")
        
        # 再次读取确认
        try:
            after = mc.command_inout("readIO", port)
            print(f"    写入后状态: OUT{port} = {after}")
            if after == 1:
                print(f"    ✓ IO 控制正常!")
            else:
                print(f"    ⚠ IO 状态未变化，可能是硬件问题")
        except Exception as e:
            print(f"    无法读取写入后状态: {e}")
            
        return True
    except Exception as e:
        print(f"    ✗ writeIO 失败: {e}")
        return False

def main():
    print("="*60)
    print("电源控制诊断工具")
    print("="*60)
    
    # 1. 检查数据库
    db = check_tango_db()
    if not db:
        print("\n❌ 无法连接 Tango 数据库，请检查 Tango 环境")
        sys.exit(1)
    
    # 2. 检查运动控制器
    print("\n" + "="*60)
    print("2. 检查运动控制器状态")
    print("="*60)
    
    for name, device_name in MOTION_CONTROLLERS:
        print(f"\n检查 {name} ({device_name}):")
        check_motion_controller_status(device_name)
    
    # 3. 检查设备属性
    print("\n" + "="*60)
    print("3. 检查设备属性配置")
    print("="*60)
    
    power_props = ["driverPowerPort", "driverPowerController", "brakePowerPort", "brakePowerController"]
    
    for name, device_name in DEVICES:
        print(f"\n{name} ({device_name}):")
        check_device_properties(db, device_name, power_props)
    
    # 4. 检查设备电源状态
    print("\n" + "="*60)
    print("4. 检查设备电源状态")
    print("="*60)
    
    for name, device_name in DEVICES:
        print(f"\n{name} ({device_name}):")
        check_device_power_status(device_name)
    
    # 5. 建议
    print("\n" + "="*60)
    print("5. 诊断建议")
    print("="*60)
    
    print("""
如果看到 "driverPowerPort = (未配置)" 或 "port=-1":

  1. 重新运行注册脚本:
     python scripts/register_devices.py --force
  
  2. 重启设备服务:
     # 停止所有服务
     # 重新启动
     python scripts/start_servers.py

如果看到 "无法连接运动控制器":

  1. 确保运动控制器设备先启动
  2. 检查 Tango 设备名称是否正确

如果 writeIO 失败:

  1. 检查运动控制器连接状态
  2. 检查硬件接线
  3. 检查运动控制器是否处于模拟模式
""")
    
    # 6. 可选: 手动测试 writeIO
    print("\n" + "="*60)
    print("6. 手动测试 writeIO (可选)")
    print("="*60)
    
    response = input("\n是否手动测试 writeIO 命令? (y/n): ")
    if response.lower() == 'y':
        test_write_io("sys/motion/1", 0)  # 测试 OUT0
    
    print("\n诊断完成!")

if __name__ == "__main__":
    main()

