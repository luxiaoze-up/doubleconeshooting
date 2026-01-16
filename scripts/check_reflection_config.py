#!/usr/bin/env python3
"""scripts/check_reflection_config.py

检查指定设备在 Tango 数据库中的 *全部设备属性(device properties)*，并可与
config/devices_config.json 中的期望值对比。
"""

from __future__ import annotations

import json
import os
import sys

import tango


def _find_device_expected_props(config: object, device_name: str) -> dict[str, object] | None:
    """在 devices_config.json 的任意层级中，查找 name==device_name 的对象并返回其 props。"""

    if isinstance(config, dict):
        if config.get("name") == device_name and isinstance(config.get("props"), dict):
            return config["props"]
        for value in config.values():
            found = _find_device_expected_props(value, device_name)
            if found is not None:
                return found
    elif isinstance(config, list):
        for item in config:
            found = _find_device_expected_props(item, device_name)
            if found is not None:
                return found
    return None


def _load_devices_config_props(device_name: str) -> dict[str, object] | None:
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
    config_path = os.path.join(repo_root, "config", "devices_config.json")
    try:
        with open(config_path, "r", encoding="utf-8") as f:
            config_obj = json.load(f)
    except FileNotFoundError:
        return None
    except json.JSONDecodeError:
        return None
    return _find_device_expected_props(config_obj, device_name)


def _to_string_list(values: object) -> list[str]:
    """把 tango 返回的 vector-like/list-like 值转换为 list[str]，不做任何去重。"""
    if values is None:
        return []
    try:
        return [str(v) for v in list(values)]
    except Exception:
        return [str(values)]


def _get_all_device_property_names(db: tango.Database, device_name: str) -> list[str]:
    """获取设备在 Tango DB 中已定义的 device properties 名称列表（按 DB 返回顺序）。"""
    prop_list_datum = db.get_device_property_list(device_name, "*")
    # PyTango: DbDatum.value_string 包含属性名列表
    return list(prop_list_datum.value_string)

def check_device_properties(device_name: str):
    """检查设备属性"""
    try:
        db = tango.Database()
        
        # 检查设备是否存在
        try:
            info = db.get_device_info(device_name)
            print(f"✓ 设备 {device_name} 已注册")
            print(f"  服务器: {info.ds_full_name}")
        except tango.DevFailed as e:
            print(f"✗ 设备 {device_name} 未注册: {e}")
            return
        
        # 读取数据库中的全部 device properties
        print("\n检查属性值(数据库全部 device properties):")
        try:
            prop_names = _get_all_device_property_names(db, device_name)
            if not prop_names:
                print("  (该设备未设置任何 device properties)")
                prop_values = {}
            else:
                prop_values = db.get_device_property(device_name, prop_names)
                for prop_name in prop_names:
                    if prop_name in prop_values and prop_values[prop_name]:
                        values = prop_values[prop_name]
                        print(f"  {prop_name}: {_to_string_list(values)} (类型: {type(values).__name__})")
                    else:
                        print(f"  {prop_name}: (未配置)")
        except Exception as e:
            print(f"  ✗ 读取属性失败: {e}")
            prop_values = {}
        
        # 对比配置文件中的期望值
        expected_props = _load_devices_config_props(device_name)
        print("\n期望值 (从 config/devices_config.json):")
        if expected_props is None:
            print("  (未找到该设备的期望配置: 请确认 config/devices_config.json 中存在对应 name)")
        else:
            # 只打印配置文件中显式定义的项，避免刷屏
            for prop_name, expected_value in expected_props.items():
                if isinstance(expected_value, list):
                    expected_list = [str(v) for v in expected_value]
                else:
                    expected_list = [str(expected_value)]
                print(f"  {prop_name}: {expected_list}")

            # 简单差异提示：只比较配置文件中定义的项
            print("\n差异检查(仅比较配置文件定义项):")
            try:
                any_diff = False
                for prop_name, expected_value in expected_props.items():
                    if isinstance(expected_value, list):
                        expected_list = [str(v) for v in expected_value]
                    else:
                        expected_list = [str(expected_value)]

                    actual_raw = prop_values.get(prop_name)
                    actual_list = _to_string_list(actual_raw) if actual_raw else []
                    if actual_list != expected_list:
                        any_diff = True
                        print(f"  ✗ {prop_name}: 实际={actual_list} 期望={expected_list}")
                if not any_diff:
                    print("  ✓ 所有检查项与期望一致")
            except Exception as e:
                print(f"  ✗ 差异检查失败: {e}")
        
    except Exception as e:
        print(f"✗ 错误: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    device_name = "sys/reflection/1"
    if len(sys.argv) > 1:
        device_name = sys.argv[1]
    
    print(f"=== 检查设备配置: {device_name} ===\n")
    check_device_properties(device_name)

