#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""测试辅助支撑设备的力传感器读数。

支持两种方式读取力传感器值：
1. 读取属性 forceValue（实际力传感器值）
2. 调用命令 readForce

示例：
  # 测试所有辅助支撑设备
  python scripts/test_force_sensor_read.py

  # 测试指定设备（设备1-5）
  python scripts/test_force_sensor_read.py --device 1

  # 连续读取，每秒一次，持续30秒
  python scripts/test_force_sensor_read.py --device 2 --interval 1.0 --duration 30

  # 显示详细配置信息
  python scripts/test_force_sensor_read.py --device 1 --verbose

  # 使用命令方式读取
  python scripts/test_force_sensor_read.py --device 1 --use-command

  # 对比属性读取和命令读取
  python scripts/test_force_sensor_read.py --device 1 --compare
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Optional

try:
    import PyTango
except ImportError:
    print("错误：未安装 PyTango。请运行: pip install pytango")
    sys.exit(1)


# 默认设备列表
DEFAULT_DEVICES = [f"sys/auxiliary/{i}" for i in range(1, 6)]


class ForceSensorTester:
    """力传感器测试器"""
    
    def __init__(self, device_name: str, verbose: bool = False):
        self.device_name = device_name
        self.verbose = verbose
        self.device: Optional[PyTango.DeviceProxy] = None
        
    def connect(self) -> bool:
        """连接到设备"""
        try:
            if self.verbose:
                print(f"[连接] 正在连接到设备: {self.device_name}")
            self.device = PyTango.DeviceProxy(self.device_name)
            self.device.ping()
            if self.verbose:
                print(f"[连接] 成功连接到设备")
            return True
        except PyTango.DevFailed as e:
            print(f"[错误] 连接设备失败: {self.device_name}")
            for err in e.args:
                print(f"  - {err.desc}")
            return False
        except Exception as e:
            print(f"[错误] 连接异常: {e}")
            return False
    
    def get_config_info(self) -> dict:
        """获取力传感器配置信息"""
        config = {}
        try:
            config['force_sensor_channel'] = self.device.read_attribute("forceSensorChannel").value
            config['force_sensor_controller'] = self.device.read_attribute("forceSensorController").value
            config['device_name'] = self.device.read_attribute("deviceName").value
            config['device_id'] = self.device.read_attribute("deviceID").value
            # 注意：forceSensorScale 和 forceSensorOffset 可能不在属性中，它们可能在配置中
            # 尝试读取状态信息
            config['state'] = self.device.state()
            config['status'] = self.device.status()
        except Exception as e:
            if self.verbose:
                print(f"[警告] 获取配置信息失败: {e}")
        return config
    
    def read_force_attribute(self) -> tuple[bool, float, Optional[str]]:
        """通过属性读取力传感器值"""
        try:
            attr = self.device.read_attribute("forceValue")
            force_value = float(attr.value)
            timestamp = time.time()
            return True, force_value, None
        except PyTango.DevFailed as e:
            error_msg = "; ".join(err.desc for err in e.args)
            return False, 0.0, error_msg
        except Exception as e:
            return False, 0.0, str(e)
    
    def read_force_command(self) -> tuple[bool, float, Optional[str]]:
        """通过命令读取力传感器值"""
        try:
            result = self.device.command_inout("readForce")
            force_value = float(result)
            timestamp = time.time()
            return True, force_value, None
        except PyTango.DevFailed as e:
            error_msg = "; ".join(err.desc for err in e.args)
            return False, 0.0, error_msg
        except Exception as e:
            return False, 0.0, str(e)
    
    def read_target_force(self) -> tuple[bool, float, Optional[str]]:
        """读取目标力值（缓存值）"""
        try:
            attr = self.device.read_attribute("targetForce")
            target_force = float(attr.value)
            return True, target_force, None
        except PyTango.DevFailed as e:
            error_msg = "; ".join(err.desc for err in e.args)
            return False, 0.0, error_msg
        except Exception as e:
            return False, 0.0, str(e)
    
    def print_config(self):
        """打印配置信息"""
        config = self.get_config_info()
        print(f"\n{'='*60}")
        print(f"设备: {self.device_name}")
        print(f"{'='*60}")
        print(f"设备名称: {config.get('device_name', 'N/A')}")
        print(f"设备ID: {config.get('device_id', 'N/A')}")
        print(f"力传感器通道: {config.get('force_sensor_channel', 'N/A')}")
        print(f"力传感器控制器: {config.get('force_sensor_controller', 'N/A')}")
        print(f"设备状态: {config.get('state', 'N/A')}")
        print(f"状态信息: {config.get('status', 'N/A')}")
        print(f"{'='*60}\n")
    
    def test_single_read(self, use_command: bool = False, compare: bool = False):
        """单次读取测试"""
        if not self.connect():
            return False
        
        if self.verbose or compare:
            self.print_config()
        
        if compare:
            # 对比模式：同时使用属性和命令读取
            print("=== 对比读取模式 ===")
            
            # 读取属性
            success_attr, force_attr, error_attr = self.read_force_attribute()
            if success_attr:
                print(f"属性读取 (forceValue): {force_attr:.6f} N")
            else:
                print(f"属性读取失败: {error_attr}")
            
            # 读取命令
            success_cmd, force_cmd, error_cmd = self.read_force_command()
            if success_cmd:
                print(f"命令读取 (readForce): {force_cmd:.6f} N")
            else:
                print(f"命令读取失败: {error_cmd}")
            
            # 读取目标力值
            success_target, target_force, error_target = self.read_target_force()
            if success_target:
                print(f"目标力值 (targetForce): {target_force:.6f} N")
            else:
                print(f"目标力值读取失败: {error_target}")
            
            # 对比结果
            if success_attr and success_cmd:
                diff = abs(force_attr - force_cmd)
                print(f"\n差值: {diff:.6f} N")
                if diff < 0.0001:
                    print("✓ 属性值和命令值一致")
                else:
                    print(f"⚠ 属性值和命令值不一致（差值: {diff:.6f} N）")
            
            return True
        elif use_command:
            # 命令模式
            success, force_value, error = self.read_force_command()
            if success:
                print(f"{self.device_name}: {force_value:.6f} N (命令读取)")
                return True
            else:
                print(f"{self.device_name}: 读取失败 - {error}")
                return False
        else:
            # 属性模式（默认）
            success, force_value, error = self.read_force_attribute()
            if success:
                print(f"{self.device_name}: {force_value:.6f} N (属性读取)")
                return True
            else:
                print(f"{self.device_name}: 读取失败 - {error}")
                return False
    
    def test_continuous_read(self, interval: float, duration: Optional[float] = None, 
                            use_command: bool = False):
        """连续读取测试"""
        if not self.connect():
            return False
        
        if self.verbose:
            self.print_config()
        
        print(f"\n开始连续读取，间隔: {interval:.1f}秒", end="")
        if duration:
            print(f"，持续: {duration:.1f}秒")
        else:
            print("（按 Ctrl+C 停止）")
        print(f"{'='*60}")
        
        start_time = time.time()
        count = 0
        success_count = 0
        values = []
        
        try:
            while True:
                count += 1
                timestamp = time.time()
                elapsed = timestamp - start_time
                
                if duration and elapsed >= duration:
                    break
                
                # 读取力值
                if use_command:
                    success, force_value, error = self.read_force_command()
                    method = "命令"
                else:
                    success, force_value, error = self.read_force_attribute()
                    method = "属性"
                
                if success:
                    success_count += 1
                    values.append(force_value)
                    print(f"[{elapsed:6.1f}s] 第{count:4d}次: {force_value:10.6f} N ({method})")
                else:
                    print(f"[{elapsed:6.1f}s] 第{count:4d}次: 读取失败 - {error}")
                
                # 等待下一次读取
                time.sleep(interval)
                
        except KeyboardInterrupt:
            print("\n\n用户中断")
        
        # 统计信息
        elapsed_total = time.time() - start_time
        print(f"\n{'='*60}")
        print(f"统计信息:")
        print(f"  总读取次数: {count}")
        print(f"  成功次数: {success_count}")
        print(f"  失败次数: {count - success_count}")
        print(f"  成功率: {success_count/count*100:.1f}%" if count > 0 else "  成功率: N/A")
        print(f"  总耗时: {elapsed_total:.1f}秒")
        
        if values:
            print(f"  最小值: {min(values):.6f} N")
            print(f"  最大值: {max(values):.6f} N")
            print(f"  平均值: {sum(values)/len(values):.6f} N")
            if len(values) > 1:
                variance = sum((x - sum(values)/len(values))**2 for x in values) / len(values)
                print(f"  标准差: {variance**0.5:.6f} N")


def main():
    parser = argparse.ArgumentParser(
        description="测试辅助支撑设备的力传感器读数",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        "--device", "-d",
        type=int,
        choices=range(1, 6),
        help="指定要测试的设备编号 (1-5)，默认测试所有设备"
    )
    
    parser.add_argument(
        "--interval", "-i",
        type=float,
        default=1.0,
        help="连续读取的时间间隔（秒），默认 1.0"
    )
    
    parser.add_argument(
        "--duration", "-t",
        type=float,
        help="连续读取的持续时间（秒），不指定则持续运行直到 Ctrl+C"
    )
    
    parser.add_argument(
        "--use-command", "-c",
        action="store_true",
        help="使用命令 readForce 而不是属性 forceValue 读取"
    )
    
    parser.add_argument(
        "--compare", "-m",
        action="store_true",
        help="对比模式：同时使用属性和命令读取，并显示对比结果"
    )
    
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="显示详细配置信息"
    )
    
    args = parser.parse_args()
    
    # 确定要测试的设备列表
    if args.device:
        devices = [f"sys/auxiliary/{args.device}"]
    else:
        devices = DEFAULT_DEVICES
    
    # 确定是否为连续读取模式
    # 如果指定了持续时间，或者指定了设备且间隔小于1000秒，认为是连续模式
    continuous = args.duration is not None or (args.device is not None and args.interval < 1000)
    
    # 执行测试
    if args.device and continuous:
        # 单个设备连续读取
        tester = ForceSensorTester(devices[0], verbose=args.verbose)
        tester.test_continuous_read(
            interval=args.interval,
            duration=args.duration,
            use_command=args.use_command
        )
    else:
        # 单次读取（一个或多个设备）
        success_count = 0
        for device_name in devices:
            tester = ForceSensorTester(device_name, verbose=args.verbose)
            if tester.test_single_read(
                use_command=args.use_command,
                compare=args.compare
            ):
                success_count += 1
        
        if len(devices) > 1:
            print(f"\n测试完成: {success_count}/{len(devices)} 个设备成功")


if __name__ == "__main__":
    main()

