#!/usr/bin/env python3
"""
列出服务器定义中的设备列表
"""
import tango
import sys

SERVER_NAME = "auxiliary_support_server/auxiliary"

try:
    db = tango.Database()
    
    # 方法1: 使用 get_server_class_list 获取服务器信息
    print("=== Method 1: get_server_class_list ===")
    try:
        server_list = db.get_server_class_list(SERVER_NAME)
        print(f"Server class list: {server_list}")
    except Exception as e:
        print(f"Error: {e}")
    
    print("\n=== Method 2: Query all devices for this server ===")
    all_devices = db.get_device_name("*", "*")
    server_devices = []
    
    for dev_name in all_devices:
        if "auxiliary" in dev_name.lower() and "dserver" not in dev_name.lower():
            try:
                # 使用 get_device_info 获取设备信息
                dev_info = db.get_device_info(dev_name)
                # 尝试访问 server 属性
                server_attr = None
                for attr in ['ds_full_name', 'server', 'server_name']:
                    if hasattr(dev_info, attr):
                        server_attr = getattr(dev_info, attr)
                        break
                
                if server_attr and SERVER_NAME in str(server_attr):
                    server_devices.append(dev_name)
            except:
                pass
    
    print(f"\nDevices found for {SERVER_NAME}:")
    for dev in sorted(server_devices):
        print(f"  {dev}")
    
    print(f"\nTotal: {len(server_devices)} devices")
    
    # 检查是否有我们想删除的设备
    extra_devices = ["ray_upper", "ray_lower", "reflection_upper", "reflection_lower", "targeting"]
    found_extra = [d for d in server_devices if any(extra in d for extra in extra_devices)]
    
    if found_extra:
        print(f"\n⚠ Found {len(found_extra)} extra devices:")
        for dev in found_extra:
            print(f"  - {dev}")
    else:
        print("\n✓ No extra devices found in database!")
        print("  This means the extra devices are created at runtime,")
        print("  possibly from a configuration file or command line arguments.")
        
except Exception as e:
    print(f"Error: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

