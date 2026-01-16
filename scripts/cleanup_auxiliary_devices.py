#!/usr/bin/env python3
"""
清理辅助支撑设备中多余的设备
删除：ray_upper, ray_lower, reflection_upper, reflection_lower, targeting
保留：sys/auxiliary/1 到 sys/auxiliary/5
"""
import tango
import sys
import logging

logging.basicConfig(level=logging.INFO, format="%(message)s")
logger = logging.getLogger(__name__)

# 要删除的设备列表
DEVICES_TO_DELETE = [
    "sys/auxiliary/ray_upper",
    "sys/auxiliary/ray_lower",
    "sys/auxiliary/reflection_upper",
    "sys/auxiliary/reflection_lower",
    "sys/auxiliary/targeting"
]

def main():
    try:
        logger.info("Connecting to Tango Database...")
        db = tango.Database()
        db.get_server_list()  # 测试连接
        logger.info("✓ Connected to Tango Database\n")
    except Exception as e:
        logger.error(f"✗ Failed to connect to Tango Database: {e}")
        sys.exit(1)
    
    logger.info(f"Will delete {len(DEVICES_TO_DELETE)} devices:\n")
    for dev in DEVICES_TO_DELETE:
        logger.info(f"  - {dev}")
    
    response = input("\nContinue? (yes/no): ")
    if response.lower() not in ["yes", "y"]:
        logger.info("Cancelled.")
        return
    
    logger.info("\nDeleting devices...")
    deleted_count = 0
    failed_count = 0
    
    for device_name in DEVICES_TO_DELETE:
        try:
            # 检查设备是否存在
            try:
                db.get_device_info(device_name)
                # 设备存在，删除它
                db.delete_device(device_name)
                logger.info(f"  ✓ Deleted: {device_name}")
                deleted_count += 1
            except tango.DevFailed as e:
                if "No such device" in str(e) or "not found" in str(e).lower():
                    logger.info(f"  ⊘ Not found (already deleted?): {device_name}")
                else:
                    raise
        except Exception as e:
            logger.error(f"  ✗ Failed to delete {device_name}: {e}")
            failed_count += 1
    
    logger.info(f"\n✓ Deleted: {deleted_count} devices")
    if failed_count > 0:
        logger.warning(f"✗ Failed: {failed_count} devices")
    
    logger.info("\nNote: You may need to restart the auxiliary_support_server for changes to take effect.")

if __name__ == "__main__":
    main()

