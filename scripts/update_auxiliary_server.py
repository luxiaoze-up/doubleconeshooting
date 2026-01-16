#!/usr/bin/env python3
"""
更新辅助支撑服务器的设备列表
使用 Tango Python API 直接操作数据库
"""
import tango
import sys

SERVER_NAME = "auxiliary_support_server/auxiliary"
VALID_DEVICES = [
    "sys/auxiliary/1",
    "sys/auxiliary/2",
    "sys/auxiliary/3",
    "sys/auxiliary/4",
    "sys/auxiliary/5"
]

def main():
    try:
        db = tango.Database()
        
        print("=" * 60)
        print("Updating auxiliary_support_server device list")
        print("=" * 60)
        print()
        
        # 方法：使用 add_server 命令更新服务器定义
        # 注意：这需要先删除旧服务器定义，然后重新添加
        
        print("Current approach:")
        print("Since devices are dynamically created at server startup,")
        print("the device list is likely defined in the server configuration.")
        print()
        print("To fix this, you have two options:")
        print()
        print("OPTION 1: Use Jive GUI tool (recommended)")
        print("  1. Run: jive")
        print("  2. Navigate to: auxiliary_support_server/auxiliary")
        print("  3. Right-click -> Server Properties")
        print("  4. Update device list to only include:")
        for dev in VALID_DEVICES:
            print(f"     - {dev}")
        print()
        print("OPTION 2: Use tango_admin command line")
        print("  First delete the server definition:")
        print(f"    tango_admin --delete-server {SERVER_NAME}")
        print()
        print("  Then add it back with correct device list:")
        cmd = f"tango_admin --add-server {SERVER_NAME} AuxiliarySupportDevice " + ",".join(VALID_DEVICES)
        print(f"    {cmd}")
        print()
        print("OPTION 3: Check if devices are defined in code")
        print("  The device list might be hardcoded in the server source code.")
        print("  Check: src/device_services/auxiliary_support_device.cpp")
        print("  Look for device_factory() or any hardcoded device names")
        print()
        
        # 检查数据库中是否有这些设备的定义
        print("Checking database for device definitions...")
        all_devices = db.get_device_name("*", "*")
        found_devices = []
        for dev in all_devices:
            if any(extra in dev for extra in ["ray_upper", "ray_lower", "reflection_upper", "reflection_lower", "targeting"]):
                if "auxiliary" in dev:
                    found_devices.append(dev)
        
        if found_devices:
            print(f"\nFound {len(found_devices)} devices in database:")
            for dev in found_devices:
                print(f"  - {dev}")
            print("\nThese can be deleted using:")
            print("  python3 scripts/cleanup_auxiliary_devices.py")
        else:
            print("\n✓ No extra devices found in database")
            print("  Devices are likely defined in server startup configuration")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()

