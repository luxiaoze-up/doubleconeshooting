#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
自动上电功能验证脚本
测试所有设备的驱动器和刹车自动上电逻辑

运行方式:
    python scripts/test_auto_power_on.py

功能:
1. 检查所有设备的配置参数
2. 验证设备启动后的电源状态
3. 测试手动控制命令
4. 生成测试报告
"""

import tango
import time
import json
from datetime import datetime
from typing import Dict, List, Tuple

# 测试设备列表
DEVICES = {
    "六自由度": {
        "name": "sys/six_dof/1",
        "has_brake": True,
        "expected_driver_port": 0,
        "expected_brake_port": 3,
        "expected_controller": "sys/motion/1"
    },
    "大行程": {
        "name": "sys/large_stroke/1",
        "has_brake": True,
        "expected_driver_port": 0,
        "expected_brake_port": 4,
        "expected_controller": "sys/motion/1"
    },
    "反射光成像": {
        "name": "sys/reflection/1",
        "has_brake": False,
        "expected_driver_port": 5,
        "expected_controller": "sys/motion/1"
    },
    "辅助支撑1": {
        "name": "sys/auxiliary/1",
        "has_brake": False,
        "expected_driver_port": 6,
        "expected_controller": "sys/motion/1"
    },
    "辅助支撑2": {
        "name": "sys/auxiliary/2",
        "has_brake": False,
        "expected_driver_port": 6,
        "expected_controller": "sys/motion/1"
    },
    "辅助支撑3": {
        "name": "sys/auxiliary/3",
        "has_brake": False,
        "expected_driver_port": 6,
        "expected_controller": "sys/motion/1"
    },
    "辅助支撑4": {
        "name": "sys/auxiliary/4",
        "has_brake": False,
        "expected_driver_port": 6,
        "expected_controller": "sys/motion/1"
    },
    "辅助支撑5": {
        "name": "sys/auxiliary/5",
        "has_brake": False,
        "expected_driver_port": 6,
        "expected_controller": "sys/motion/1"
    }
}

class PowerControlTester:
    def __init__(self):
        self.results = {}
        self.start_time = datetime.now()
        
    def log(self, message: str, level: str = "INFO"):
        """打印日志"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        prefix = {
            "INFO": "ℹ️",
            "SUCCESS": "✅",
            "ERROR": "❌",
            "WARNING": "⚠️",
            "TEST": "🔍"
        }.get(level, "•")
        print(f"[{timestamp}] {prefix} {message}")
    
    def test_device_connection(self, device_name: str) -> Tuple[bool, tango.DeviceProxy]:
        """测试设备连接"""
        try:
            device = tango.DeviceProxy(device_name)
            state = device.state()
            self.log(f"连接设备 {device_name} 成功, 状态: {state}", "SUCCESS")
            return True, device
        except Exception as e:
            self.log(f"连接设备 {device_name} 失败: {e}", "ERROR")
            return False, None
    
    def test_power_status_attribute(self, device: tango.DeviceProxy, has_brake: bool) -> Dict:
        """测试电源状态属性"""
        result = {
            "driver_power_status": None,
            "brake_status": None,
            "driver_power_readable": False,
            "brake_readable": False
        }
        
        # 测试驱动器电源状态
        try:
            driver_power = device.read_attribute("driverPowerStatus").value
            result["driver_power_status"] = driver_power
            result["driver_power_readable"] = True
            self.log(f"  驱动器电源状态: {driver_power}", "SUCCESS")
        except Exception as e:
            self.log(f"  读取驱动器电源状态失败: {e}", "ERROR")
        
        # 测试刹车状态（如果有）
        if has_brake:
            try:
                brake_status = device.read_attribute("brakeStatus").value
                result["brake_status"] = brake_status
                result["brake_readable"] = True
                self.log(f"  刹车状态: {brake_status}", "SUCCESS")
            except Exception as e:
                self.log(f"  读取刹车状态失败: {e}", "ERROR")
        
        return result
    
    def test_query_power_status_command(self, device: tango.DeviceProxy) -> Dict:
        """测试查询电源状态命令"""
        try:
            status_json = device.command_inout("queryPowerStatus")
            status = json.loads(status_json)
            self.log(f"  查询电源状态成功: {status}", "SUCCESS")
            return status
        except Exception as e:
            self.log(f"  查询电源状态失败: {e}", "ERROR")
            return {}
    
    def test_manual_control(self, device: tango.DeviceProxy, has_brake: bool) -> Dict:
        """测试手动控制命令（仅测试命令是否存在，不实际执行）"""
        result = {
            "enable_driver_exists": False,
            "disable_driver_exists": False,
            "release_brake_exists": False,
            "engage_brake_exists": False
        }
        
        try:
            commands = device.command_list_query()
            command_names = [cmd.cmd_name for cmd in commands]
            
            result["enable_driver_exists"] = "enableDriverPower" in command_names
            result["disable_driver_exists"] = "disableDriverPower" in command_names
            
            if has_brake:
                result["release_brake_exists"] = "releaseBrake" in command_names
                result["engage_brake_exists"] = "engageBrake" in command_names
            
            self.log(f"  命令检查: enableDriverPower={result['enable_driver_exists']}, "
                    f"disableDriverPower={result['disable_driver_exists']}", "SUCCESS")
            
            if has_brake:
                self.log(f"  刹车命令: releaseBrake={result['release_brake_exists']}, "
                        f"engageBrake={result['engage_brake_exists']}", "SUCCESS")
        except Exception as e:
            self.log(f"  命令检查失败: {e}", "ERROR")
        
        return result
    
    def verify_auto_power_on(self, device_info: Dict, status: Dict) -> bool:
        """验证自动上电是否成功"""
        success = True
        
        # 检查驱动器电源
        if status.get("driverPowerEnabled") is True:
            self.log(f"  ✅ 驱动器自动上电成功", "SUCCESS")
        else:
            self.log(f"  ❌ 驱动器未自动上电", "ERROR")
            success = False
        
        # 检查刹车（如果有）
        if device_info["has_brake"]:
            if status.get("brakeReleased") is True:
                self.log(f"  ✅ 刹车自动释放成功", "SUCCESS")
            else:
                self.log(f"  ❌ 刹车未自动释放", "ERROR")
                success = False
        
        # 检查配置参数
        if status.get("driverPowerPort") == device_info["expected_driver_port"]:
            self.log(f"  ✅ 驱动器端口配置正确: OUT{status.get('driverPowerPort')}", "SUCCESS")
        else:
            self.log(f"  ⚠️ 驱动器端口配置异常: 期望OUT{device_info['expected_driver_port']}, "
                    f"实际OUT{status.get('driverPowerPort')}", "WARNING")
        
        if device_info["has_brake"]:
            if status.get("brakePowerPort") == device_info["expected_brake_port"]:
                self.log(f"  ✅ 刹车端口配置正确: OUT{status.get('brakePowerPort')}", "SUCCESS")
            else:
                self.log(f"  ⚠️ 刹车端口配置异常: 期望OUT{device_info['expected_brake_port']}, "
                        f"实际OUT{status.get('brakePowerPort')}", "WARNING")
        
        return success
    
    def test_device(self, device_label: str, device_info: Dict) -> Dict:
        """测试单个设备"""
        self.log(f"\n{'='*60}", "INFO")
        self.log(f"测试设备: {device_label} ({device_info['name']})", "TEST")
        self.log(f"{'='*60}", "INFO")
        
        result = {
            "device_label": device_label,
            "device_name": device_info["name"],
            "connected": False,
            "auto_power_on_success": False,
            "attributes": {},
            "commands": {},
            "query_status": {}
        }
        
        # 1. 测试连接
        connected, device = self.test_device_connection(device_info["name"])
        result["connected"] = connected
        
        if not connected:
            return result
        
        # 2. 测试属性读取
        self.log("测试属性读取:", "TEST")
        result["attributes"] = self.test_power_status_attribute(device, device_info["has_brake"])
        
        # 3. 测试查询命令
        self.log("测试查询命令:", "TEST")
        result["query_status"] = self.test_query_power_status_command(device)
        
        # 4. 测试手动控制命令
        self.log("测试手动控制命令:", "TEST")
        result["commands"] = self.test_manual_control(device, device_info["has_brake"])
        
        # 5. 验证自动上电
        self.log("验证自动上电:", "TEST")
        result["auto_power_on_success"] = self.verify_auto_power_on(
            device_info, result["query_status"]
        )
        
        return result
    
    def run_all_tests(self):
        """运行所有测试"""
        self.log("\n" + "="*80, "INFO")
        self.log("开始自动上电功能验证测试", "INFO")
        self.log("="*80 + "\n", "INFO")
        
        for device_label, device_info in DEVICES.items():
            result = self.test_device(device_label, device_info)
            self.results[device_label] = result
            time.sleep(0.5)  # 避免过快请求
        
        self.generate_report()
    
    def generate_report(self):
        """生成测试报告"""
        self.log("\n" + "="*80, "INFO")
        self.log("测试报告", "INFO")
        self.log("="*80 + "\n", "INFO")
        
        total = len(self.results)
        connected = sum(1 for r in self.results.values() if r["connected"])
        auto_power_success = sum(1 for r in self.results.values() if r["auto_power_on_success"])
        
        self.log(f"测试设备总数: {total}", "INFO")
        self.log(f"成功连接: {connected}/{total}", "SUCCESS" if connected == total else "WARNING")
        self.log(f"自动上电成功: {auto_power_success}/{connected}", 
                "SUCCESS" if auto_power_success == connected else "ERROR")
        
        self.log("\n详细结果:", "INFO")
        for device_label, result in self.results.items():
            status_icon = "✅" if result["auto_power_on_success"] else "❌"
            self.log(f"{status_icon} {device_label}: {result['device_name']}", "INFO")
            
            if result["connected"]:
                attrs = result["attributes"]
                self.log(f"   驱动器电源: {attrs.get('driver_power_status', 'N/A')}", "INFO")
                if result.get("query_status", {}).get("brakeReleased") is not None:
                    self.log(f"   刹车状态: {attrs.get('brake_status', 'N/A')}", "INFO")
            else:
                self.log(f"   未连接", "ERROR")
        
        # 保存JSON报告
        report_file = f"test_auto_power_on_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        with open(report_file, 'w', encoding='utf-8') as f:
            json.dump({
                "test_time": self.start_time.isoformat(),
                "duration": (datetime.now() - self.start_time).total_seconds(),
                "summary": {
                    "total": total,
                    "connected": connected,
                    "auto_power_success": auto_power_success
                },
                "results": self.results
            }, f, indent=2, ensure_ascii=False)
        
        self.log(f"\n详细报告已保存到: {report_file}", "SUCCESS")
        
        # 最终结论
        self.log("\n" + "="*80, "INFO")
        if auto_power_success == connected and connected == total:
            self.log("🎉 所有设备自动上电功能正常！", "SUCCESS")
        elif auto_power_success > 0:
            self.log(f"⚠️ 部分设备自动上电成功 ({auto_power_success}/{total})", "WARNING")
        else:
            self.log("❌ 自动上电功能测试失败", "ERROR")
        self.log("="*80 + "\n", "INFO")

def main():
    """主函数"""
    tester = PowerControlTester()
    try:
        tester.run_all_tests()
    except KeyboardInterrupt:
        print("\n\n测试被用户中断")
    except Exception as e:
        print(f"\n\n测试过程中发生错误: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()

