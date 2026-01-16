#!/usr/bin/env python3
import tango
import time
import sys
import argparse
import json
import os
import logging
from typing import Dict, Any, Set

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format="%(message)s"
)
logger = logging.getLogger(__name__)

# 默认配置常量
DEFAULT_CONTROLLER_IP = "192.168.1.11"
DEFAULT_PLC_IP = "127.0.0.1"  # 与 config/system_config.json 保持一致
DEFAULT_AXIS_ID = "0"

def connect_to_db(retries: int = 5) -> tango.Database:
    """Connect to Tango Database with retry logic."""
    logger.info("Connecting to Tango Database...")
    
    for i in range(retries):
        try:
            db = tango.Database()
            # 验证连接
            # db.get_server_list()
            logger.info("✓ Tango Database connection established.")
            return db
        except (tango.DevFailed, Exception) as e:
            error_msg = str(e).replace("\n", " ")
            if i < retries - 1:
                logger.warning(f"  Attempt {i+1}/{retries} failed. Retrying... ({error_msg[:100]}...)")
                time.sleep(1)
            else:
                logger.error(f"\n✗ Could not connect to Tango Database after {retries} attempts.")
                logger.error(f"  Last error: {error_msg}")
                print_troubleshooting()
                sys.exit(1)
    return None

def print_troubleshooting():
    print("\nTroubleshooting:")
    print("  1. Check if Tango database is running: sudo systemctl status tango-db")
    print("  2. Test connection manually: tango_admin --ping-database")
    print("  3. Check Tango host: echo $TANGO_HOST")

def register_dserver(db: tango.Database, server_name: str, registered_cache: Set[str]):
    """Register the DServer device if not already processed."""
    if server_name in registered_cache:
        return

    dserver_name = f"dserver/{server_name}"
    try:
        db.get_device_info(dserver_name)
        logger.info(f"  ✓ DServer {dserver_name} already exists.")
    except tango.DevFailed:
        logger.info(f"  Registering DServer {dserver_name}...")
        dev_info = tango.DbDevInfo()
        dev_info.name = dserver_name
        dev_info._class = "DServer"
        dev_info.server = server_name
        try:
            db.add_device(dev_info)
            logger.info(f"  ✓ DServer {dserver_name} registered.")
        except tango.DevFailed as e:
            logger.error(f"  ✗ Failed to register DServer: {e}")
            raise
    
    registered_cache.add(server_name)

def register_device(db: tango.Database, dev_config: Dict[str, Any], force: bool = False, recreate: bool = False):
    """Register a single device and set its properties."""
    server_name = dev_config["server"]
    class_name = dev_config["class"]
    device_name = dev_config["name"]
    properties = dev_config.get("props", {})

    # Check device status
    exists = False
    should_update_props = True

    try:
        info = db.get_device_info(device_name)
        exists = True
        
        if recreate:
            logger.warning(f"  ⚠ Device {device_name} exists, recreate=True -> deleting...")
            db.delete_device(device_name)
            exists = False
        elif info.ds_full_name != server_name:
            logger.warning(f"  ⚠ Device {device_name} server mismatch ({info.ds_full_name}). Updating...")
            # Tango add_device will update the server info if it exists, so we don't strictly need to delete,
            # but treating it as a new registration is safer.
            exists = False 
        elif force:
            logger.info(f"  ℹ Device {device_name} exists, force=True -> updating properties.")
        else:
            logger.info(f"  ✓ Device {device_name} already exists.")
            # 如果不强制，通常我们认为配置即代码，仍然更新属性是安全的，
            # 但如果你希望不加 force 就不更新属性，可以将下行改为 False
            should_update_props = True 

    except tango.DevFailed:
        exists = False

    # Register/Update Device
    if not exists:
        logger.info(f"  Registering device {device_name}...")
        dev_info = tango.DbDevInfo()
        dev_info.name = device_name
        dev_info._class = class_name
        dev_info.server = server_name
        try:
            db.add_device(dev_info)
            logger.info(f"  ✓ Device {device_name} registered.")
        except tango.DevFailed as e:
            logger.error(f"  ✗ Failed to register {device_name}: {e}")
            raise

    # Set Properties
    if properties and should_update_props:
        try:
            # 检查属性是否真的需要更新可以进一步优化，但直接 put 开销也不大
            db.put_device_property(device_name, properties)
            logger.info(f"  ✓ Properties set for {device_name}")
        except tango.DevFailed as e:
            logger.warning(f"  ⚠ Failed to set properties for {device_name}: {e}")

def normalize_props(props: Dict[str, Any]) -> Dict[str, list]:
    """将属性值标准化为列表格式（Tango DB 要求）"""
    normalized = {}
    for key, value in props.items():
        if isinstance(value, list):
            normalized[key] = [str(v) for v in value]
        else:
            normalized[key] = [str(value)]
    return normalized


def load_devices_from_json(config_path: str) -> Dict[str, Dict[str, Any]]:
    """从 devices_config.json 加载设备配置"""
    logger.info(f"  Loading devices config from {config_path}")
    
    with open(config_path, "r", encoding="utf-8") as f:
        raw_config = json.load(f)
    
    devices = {}
    
    # 遍历所有设备类别
    for category, category_data in raw_config.items():
        # 跳过注释和版本字段
        if category.startswith("_"):
            continue
        
        # 遍历类别下的设备
        for dev_key, dev_config in category_data.items():
            # 跳过注释字段
            if dev_key.startswith("_"):
                continue
            
            # 提取必要字段
            if not all(k in dev_config for k in ["server", "class", "name"]):
                logger.warning(f"  ⚠ Skipping {dev_key}: missing required fields")
                continue
            
            # 标准化属性
            props = dev_config.get("props", {})
            normalized_props = normalize_props(props)
            
            devices[dev_key] = {
                "server": dev_config["server"],
                "class": dev_config["class"],
                "name": dev_config["name"],
                "props": normalized_props,
            }
    
    logger.info(f"  ✓ Loaded {len(devices)} devices from config")
    return devices


def get_default_config_path() -> str:
    """获取默认配置文件路径"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    return os.path.join(project_root, "config", "devices_config.json")


def load_config(args):
    """加载设备配置 - 优先从 JSON 文件读取"""
    
    # 1. 确定配置文件路径
    config_path = args.config or get_default_config_path()
    
    # 2. 如果配置文件存在，从 JSON 加载
    if os.path.exists(config_path):
        try:
            return load_devices_from_json(config_path)
        except Exception as e:
            logger.error(f"  ✗ Failed to load config file: {e}")
            logger.info("  Falling back to built-in defaults...")
    else:
        logger.warning(f"  ⚠ Config file not found: {config_path}")
        logger.info("  Using built-in defaults...")
    
    # 3. 回退：使用内置默认配置
    def env(name, default=None):
        return os.environ.get(name, default)

    ctrl_ip = args.controller_ip or env("CONTROLLER_IP", DEFAULT_CONTROLLER_IP)
    plc_ip = args.vacuum_plc_ip or env("VACUUM_PLC_IP", DEFAULT_PLC_IP)

    # 内置默认配置（精简版，完整配置应在 JSON 文件中）
    return {
        "motion_controller_1": {
            "server": "motion_controller_server/ctrl1",
            "class": "MotionControllerDevice",
            "name": "sys/motion/1",
            "props": {"controller_ip": [ctrl_ip], "card_id": ["0"]},
        },
        "large_stroke": {
            "server": "large_stroke_server/large_stroke",
            "class": "LargeStrokeDevice",
            "name": "sys/large_stroke/1",
            "props": {"motionControllerName": ["sys/motion/1"], "axisId": ["0"]},
        },
        "vacuum": {
            "server": "vacuum_server/vacuum",
            "class": "VacuumDevice",
            "name": "sys/vacuum/1",
            "props": {"plc_ip": [plc_ip]},
        },
        "vacuum_system": {
            "server": "vacuum_system_server/vacuum2",
            "class": "VacuumSystemDevice",
            "name": "sys/vacuum/2",
            "props": {"plc_ip": [plc_ip], "plc_port": ["4840"]},
        },
        "interlock": {
            "server": "interlock_server/interlock",
            "class": "InterlockService",
            "name": "sys/interlock/1",
            "props": {"large_stroke_device": ["sys/large_stroke/1"], "vacuum_system_device": ["sys/vacuum/2"]},
        },
    }

def parse_args():
    p = argparse.ArgumentParser(description="Register Tango devices with dynamic config.")
    p.add_argument("--controller-ip", help="Controller IP for motion devices")
    p.add_argument("--axis-id", help="Axis ID for large stroke")
    p.add_argument("--vacuum-plc-ip", help="PLC IP for vacuum device")
    p.add_argument("--config", help="JSON config file overriding all defaults")
    p.add_argument("--force", action="store_true", help="Force update properties even if device exists")
    p.add_argument("--recreate", action="store_true", help="Delete and re-add devices if they exist")
    p.add_argument("--devices", nargs="*", help="Limit to specific device keys")
    return p.parse_args()

def main():
    args = parse_args()
    cfg = load_config(args)
    selected_devices = set(args.devices) if args.devices else None

    db = connect_to_db()
    
    logger.info("\nRegistering devices...")
    
    # 缓存已注册的 Server，避免重复检查
    registered_servers = set()

    for key, dev in cfg.items():
        if selected_devices and key not in selected_devices:
            continue
        
        # 1. 确保 DServer 存在 (带缓存检查)
        register_dserver(db, dev["server"], registered_servers)
        
        # 2. 注册设备
        register_device(
            db,
            dev_config=dev,
            force=args.force,
            recreate=args.recreate
        )

    logger.info("\nAll devices processed.")

if __name__ == "__main__":
    main()
