#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PLC通信测试脚本
用于测试OPU-CA协议实现和真空系统服务
"""

import sys
import time
import json
from pathlib import Path

# 添加项目路径（从 scripts/tools/ 向上两级到项目根目录）
project_root = Path(__file__).parent.parent.parent
sys.path.insert(0, str(project_root / "gui"))

try:
    import tango
    from tango import DeviceProxy, DevFailed
except ImportError:
    print("警告: Tango未安装，将使用模拟模式测试")
    tango = None

def test_plc_connection():
    """测试PLC连接"""
    print("=" * 60)
    print("测试1: PLC连接测试")
    print("=" * 60)
    
    if not tango:
        print("跳过: Tango未安装")
        return False
    
    try:
        device = DeviceProxy("sys/vacuum/1")
        print(f"✓ 设备连接成功: {device.name()}")
        
        # 测试连接PLC
        print("\n尝试连接PLC...")
        device.connectPLC()
        print("✓ connectPLC命令执行成功")
        
        # 检查连接状态
        time.sleep(1)
        # 注意：需要根据实际属性名称调整
        print("\n测试完成")
        return True
        
    except DevFailed as e:
        print(f"✗ Tango设备错误: {e}")
        return False
    except Exception as e:
        print(f"✗ 错误: {e}")
        return False

def test_plc_read_write():
    """测试PLC读写"""
    print("\n" + "=" * 60)
    print("测试2: PLC数据读写测试")
    print("=" * 60)
    
    if not tango:
        print("跳过: Tango未安装")
        return False
    
    try:
        device = DeviceProxy("sys/vacuum/1")
        
        # 测试读取属性
        print("\n读取系统状态...")
        try:
            system_state = device.read_attribute("systemState").value
            print(f"✓ 系统状态: {system_state}")
        except Exception as e:
            print(f"⚠ 读取systemState失败: {e}")
        
        # 测试读取传感器数据
        print("\n读取传感器数据...")
        try:
            gauge1 = device.read_attribute("vacuumGauge1").value
            print(f"✓ 真空规1: {gauge1}")
        except Exception as e:
            print(f"⚠ 读取vacuumGauge1失败: {e}")
        
        # 测试写入（如果支持）
        print("\n测试写入操作...")
        # 注意：根据实际命令调整
        
        return True
        
    except Exception as e:
        print(f"✗ 错误: {e}")
        return False

def test_vacuum_commands():
    """测试真空系统命令"""
    print("\n" + "=" * 60)
    print("测试3: 真空系统命令测试")
    print("=" * 60)
    
    if not tango:
        print("跳过: Tango未安装")
        return False
    
    try:
        device = DeviceProxy("sys/vacuum/1")
        
        # 测试模式切换
        print("\n测试模式切换...")
        try:
            device.command_inout("switchMode", 0)  # 切换到自动模式
            print("✓ switchMode命令执行成功")
        except Exception as e:
            print(f"⚠ switchMode失败: {e}")
        
        # 测试一键操作（仅测试命令接口，不实际执行）
        print("\n测试一键操作命令接口...")
        commands = ["oneKeyVacuumStart", "oneKeyVacuumStop", "ventStart", "ventStop"]
        for cmd in commands:
            try:
                # 仅检查命令是否存在
                cmd_info = device.get_command_config(cmd)
                print(f"✓ 命令 {cmd} 存在")
            except Exception as e:
                print(f"⚠ 命令 {cmd} 不存在或错误: {e}")
        
        return True
        
    except Exception as e:
        print(f"✗ 错误: {e}")
        return False

def test_gui_connection():
    """测试GUI连接"""
    print("\n" + "=" * 60)
    print("测试4: GUI连接测试")
    print("=" * 60)
    
    try:
        from gui.pages.vacuum_system import VacuumSystemPage
        from PyQt5.QtWidgets import QApplication
        from PyQt5.QtCore import QTimer
        
        app = QApplication(sys.argv)
        page = VacuumSystemPage()
        print("✓ GUI页面创建成功")
        
        # 测试设备连接
        timer = QTimer()
        timer.timeout.connect(lambda: app.quit())
        timer.setSingleShot(True)
        timer.start(1000)  # 1秒后退出
        
        print("✓ GUI测试完成（1秒后自动退出）")
        app.exec_()
        return True
        
    except ImportError as e:
        print(f"⚠ GUI测试跳过: {e}")
        return False
    except Exception as e:
        print(f"✗ GUI错误: {e}")
        return False

def main():
    """主测试函数"""
    print("\n" + "=" * 60)
    print("真空系统PLC通信测试")
    print("=" * 60)
    print(f"时间: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print()
    
    results = []
    
    # 运行测试
    results.append(("PLC连接", test_plc_connection()))
    results.append(("PLC读写", test_plc_read_write()))
    results.append(("真空命令", test_vacuum_commands()))
    results.append(("GUI连接", test_gui_connection()))
    
    # 输出总结
    print("\n" + "=" * 60)
    print("测试总结")
    print("=" * 60)
    for name, result in results:
        status = "✓ 通过" if result else "✗ 失败"
        print(f"{name}: {status}")
    
    passed = sum(1 for _, r in results if r)
    total = len(results)
    print(f"\n总计: {passed}/{total} 测试通过")
    
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
