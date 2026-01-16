#!/usr/bin/env python3
"""
修复辅助支撑服务器的设备列表
方法：通过查询服务器下的所有设备，然后使用 Tango 命令行工具更新服务器定义
"""
import tango
import sys
import logging
import subprocess

logging.basicConfig(level=logging.INFO, format="%(message)s")
logger = logging.getLogger(__name__)

SERVER_NAME = "auxiliary_support_server/auxiliary"
VALID_DEVICES = [
    "sys/auxiliary/1",
    "sys/auxiliary/2",
    "sys/auxiliary/3",
    "sys/auxiliary/4",
    "sys/auxiliary/5"
]

def get_server_devices_via_tango_admin(server_name):
    """使用 tango_admin 命令获取服务器设备列表"""
    try:
        result = subprocess.run(
            ["tango_admin", "--server", server_name, "--device-list"],
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            # 跳过标题行，提取设备名
            devices = []
            for line in lines:
                line = line.strip()
                if line and not line.startswith('Device') and not line.startswith('---'):
                    devices.append(line)
            return devices
        else:
            logger.warning(f"tango_admin command failed: {result.stderr}")
            return []
    except FileNotFoundError:
        logger.warning("tango_admin command not found, trying alternative method...")
        return []
    except Exception as e:
        logger.warning(f"Error running tango_admin: {e}")
        return []

def update_server_definition_via_tango_admin(server_name, device_list):
    """使用 tango_admin 命令更新服务器定义"""
    # 构建命令：tango_admin --server server_name --device-list device1 device2 ...
    cmd = ["tango_admin", "--server", server_name, "--device-list"] + device_list
    logger.info(f"Running: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        if result.returncode == 0:
            logger.info("✓ Server definition updated successfully!")
            return True
        else:
            logger.error(f"✗ Failed to update server definition: {result.stderr}")
            return False
    except FileNotFoundError:
        logger.error("✗ tango_admin command not found. Please install Tango tools.")
        return False
    except Exception as e:
        logger.error(f"✗ Error: {e}")
        return False

def main():
    try:
        logger.info("Connecting to Tango Database...")
        db = tango.Database()
        db.get_server_list()  # 测试连接
        logger.info("✓ Connected to Tango Database\n")
    except Exception as e:
        logger.error(f"✗ Failed to connect to Tango Database: {e}")
        sys.exit(1)
    
    try:
        # 方法1：通过数据库查询属于该服务器的设备
        logger.info("Querying devices for server...")
        all_devices = db.get_device_name("*", "*")
        
        # 筛选出属于 auxiliary_support_server/auxiliary 的设备
        server_devices = []
        for dev_name in all_devices:
            try:
                dev_info = db.get_device_info(dev_name)
                if dev_info.ds_full_name == SERVER_NAME:
                    server_devices.append(dev_name)
            except:
                pass
        
        logger.info(f"\nFound {len(server_devices)} device(s) in database for {SERVER_NAME}:")
        for dev in sorted(server_devices):
            marker = "✓" if dev in VALID_DEVICES else "✗"
            logger.info(f"  {marker} {dev}")
        
        # 找出需要移除的设备
        devices_to_remove = [d for d in server_devices if d not in VALID_DEVICES]
        devices_missing = [d for d in VALID_DEVICES if d not in server_devices]
        
        if not devices_to_remove and not devices_missing:
            logger.info("\n✓ All devices are correct! No changes needed.")
            return
        
        if devices_to_remove:
            logger.info(f"\n⚠ {len(devices_to_remove)} device(s) need to be removed:")
            for dev in devices_to_remove:
                logger.info(f"  - {dev}")
            logger.info("\nNote: These devices are not in database, they may be defined in server startup config.")
            logger.info("You may need to check the server's startup configuration file or command line arguments.")
        
        if devices_missing:
            logger.info(f"\n⚠ {len(devices_missing)} device(s) need to be registered:")
            for dev in devices_missing:
                logger.info(f"  + {dev}")
        
        logger.info("\n" + "="*60)
        logger.info("IMPORTANT: Server device list is managed differently.")
        logger.info("="*60)
        logger.info("\nThe device list is defined when the server starts, typically:")
        logger.info("  1. In a server configuration file (e.g., /etc/tango/servers/...)")
        logger.info("  2. Via command line arguments when starting the server")
        logger.info("  3. In the Tango database server definition (rare)")
        logger.info("\nTo fix this, you need to:")
        logger.info("  1. Find where auxiliary_support_server is configured")
        logger.info("  2. Update the device list to only include:")
        for dev in VALID_DEVICES:
            logger.info(f"     - {dev}")
        logger.info("\nCheck:")
        logger.info(f"  - Server startup script: scripts/start_servers.py")
        logger.info(f"  - Tango server config: /etc/tango/servers/auxiliary_support_server.ini")
        logger.info(f"  - Or check how the server is started")
        
    except Exception as e:
        logger.error(f"✗ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
