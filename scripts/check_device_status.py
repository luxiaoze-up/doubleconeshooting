#!/usr/bin/env python3
"""
诊断脚本：检查 Tango 设备注册和服务器状态
用于排查 "Device is not exported" 错误
"""

import tango
import sys

def check_device(device_name):
    """检查设备是否在 Tango 数据库中注册"""
    try:
        db = tango.Database()
        info = db.get_device_info(device_name)
        print(f"✓ 设备 {device_name} 已在 Tango 数据库中注册")
        print(f"  服务器: {info.ds_full_name}")
        # 尝试获取类名（不同版本的 Tango API 可能使用不同的属性名）
        try:
            class_name = getattr(info, 'class_name', getattr(info, 'class', 'Unknown'))
            print(f"  类名: {class_name}")
        except:
            print(f"  类名: (无法获取)")
        return True, info.ds_full_name
    except tango.DevFailed as e:
        print(f"✗ 设备 {device_name} 未在 Tango 数据库中注册")
        print(f"  错误: {e}")
        return False, None

def check_device_accessible(device_name):
    """检查设备是否可访问（设备服务器已启动并导出设备）"""
    try:
        proxy = tango.DeviceProxy(device_name)
        proxy.ping()
        state = proxy.state()
        status = proxy.status()
        print(f"✓ 设备 {device_name} 可访问")
        # 获取状态名称
        state_map = {
            0: "ON",
            1: "OFF", 
            2: "CLOSE",
            3: "OPEN",
            4: "INSERT",
            5: "EXTRACT",
            6: "MOVING",
            7: "STANDBY",
            8: "FAULT",
            9: "INIT",
            10: "RUNNING",
            11: "ALARM",
            12: "DISABLE",
            13: "UNKNOWN",
        }
        state_name = state_map.get(state, f"Unknown({state})")
        print(f"  状态: {state_name}")
        if status:
            print(f"  状态信息: {status}")
        return True, state_name
    except tango.DevFailed as e:
        print(f"✗ 设备 {device_name} 不可访问")
        print(f"  错误: {e}")
        return False, None

def check_server_running(server_name):
    """检查设备服务器是否正在运行"""
    try:
        # 方法1: 尝试 ping DServer（最可靠的方法）
        dserver_name = f"dserver/{server_name}"
        try:
            dserver = tango.DeviceProxy(dserver_name)
            dserver.ping()
            print(f"✓ 服务器 {server_name} 正在运行 (DServer 可访问)")
            
            # 尝试获取导出的设备列表
            try:
                db = tango.Database()
                # 获取该服务器类的所有设备
                class_name = server_name.split('/')[0]
                device_list = db.get_device_exported_for_class(class_name)
                # 过滤出属于该服务器实例的设备
                exported_devices = [dev for dev in device_list if server_name in dev]
                if exported_devices:
                    print(f"  导出的设备数量: {len(exported_devices)}")
                    for dev in exported_devices:
                        print(f"    - {dev}")
            except Exception as e:
                print(f"  (无法获取设备列表: {e})")
            
            return True
        except tango.DevFailed as e:
            print(f"✗ 服务器 {server_name} 未运行或未找到")
            print(f"  错误: {e}")
            return False
    except Exception as e:
        print(f"✗ 检查服务器状态时出错: {e}")
        return False

def main():
    if len(sys.argv) < 2:
        print("用法: python check_device_status.py <device_name>")
        print("示例: python check_device_status.py sys/motion/2")
        sys.exit(1)
    
    device_name = sys.argv[1]
    print(f"\n检查设备: {device_name}\n")
    print("=" * 60)
    
    # 1. 检查数据库注册
    print("\n[1] 检查 Tango 数据库注册...")
    is_registered, server_name = check_device(device_name)
    
    if not is_registered:
        print("\n建议:")
        print("  1. 运行注册脚本: python scripts/register_devices.py")
        print("  2. 检查配置文件: config/devices_config.json")
        sys.exit(1)
    
    # 2. 检查服务器是否运行
    if server_name:
        print(f"\n[2] 检查设备服务器 {server_name} 是否运行...")
        check_server_running(server_name)
    
    # 3. 检查设备是否可访问
    print(f"\n[3] 检查设备是否可访问...")
    is_accessible, state_name = check_device_accessible(device_name)
    
    if not is_accessible:
        print("\n建议:")
        print("  1. 检查设备服务器是否启动:")
        print(f"     tango_admin --check-server {server_name}")
        print("  2. 查看设备服务器日志")
        print("  3. 尝试重启设备服务器:")
        print(f"     tango_admin --stop-server {server_name}")
        print(f"     tango_admin --start-server {server_name}")
        sys.exit(1)
    
    # 4. 如果设备状态不是 ON，提供额外建议
    if state_name and state_name != "ON":
        print(f"\n[4] 设备状态警告...")
        print(f"⚠ 设备处于 {state_name} 状态，可能存在问题。")
        print("\n建议:")
        if state_name == "FAULT":
            print("  1. 检查运动控制器硬件连接:")
            print("     - 确认控制器 IP: 192.168.1.12")
            print("     - 检查网络连接: ping 192.168.1.12")
            print("     - 检查控制器电源和状态")
            print("  2. 查看设备服务器日志获取详细错误信息")
            print("  3. 尝试初始化设备:")
            print(f"     python3 -c \"import tango; d=tango.DeviceProxy('{device_name}'); d.command_inout('init')\"")
        elif state_name == "INIT":
            print("  1. 等待设备初始化完成")
            print("  2. 如果长时间处于 INIT 状态，检查设备服务器日志")
        elif state_name == "UNKNOWN":
            print("  1. 设备可能未正确初始化")
            print("  2. 尝试重启设备服务器")
        print()
    
    print("=" * 60)
    if state_name == "ON":
        print("✓ 所有检查通过！设备正常工作。")
    else:
        print(f"⚠ 设备可访问，但状态为 {state_name}，请检查上述建议。")

if __name__ == "__main__":
    main()

