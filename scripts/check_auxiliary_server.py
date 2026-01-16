#!/usr/bin/env python3
"""
检查辅助支撑服务器的配置
"""
import tango
import sys

try:
    db = tango.Database()
    
    # 检查服务器是否定义
    print("=== Checking server definition ===")
    servers = db.get_server_list()
    server_found = False
    for s in servers:
        if "auxiliary_support" in s.lower():
            print(f"Found server: {s}")
            server_found = True
            
            # 尝试获取服务器实例列表
            try:
                instances = db.get_server_instance_list(s)
                print(f"  Instances: {list(instances)}")
            except:
                pass
    
    if not server_found:
        print("Server not found in database")
    
    print("\n=== Checking devices ===")
    all_devices = db.get_device_name("*", "*")
    auxiliary_devices = []
    
    for dev_name in all_devices:
        if "auxiliary" in dev_name.lower() and "dserver" not in dev_name.lower():
            auxiliary_devices.append(dev_name)
    
    if auxiliary_devices:
        print(f"\nFound {len(auxiliary_devices)} auxiliary devices in database:")
        for dev in sorted(auxiliary_devices):
            print(f"  {dev}")
            try:
                dev_info = db.get_device_info(dev)
                server = getattr(dev_info, 'ds_full_name', 'unknown')
                print(f"    -> Server: {server}")
            except:
                pass
    else:
        print("No auxiliary devices found in database")
        print("\nThis means devices are created dynamically at server startup,")
        print("likely from command line arguments or a configuration file.")
        
    print("\n=== Next steps ===")
    print("Since devices are not in database, they are likely defined:")
    print("  1. In server startup command line arguments")
    print("  2. In a server configuration file")
    print("  3. Hardcoded in the server code")
    print("\nCheck:")
    print("  - How the server is started (scripts/start_servers.py)")
    print("  - Server config files: /etc/tango/servers/*.ini")
    print("  - Server source code for hardcoded device lists")
    
except Exception as e:
    print(f"Error: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

