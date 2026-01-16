#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IO输出端口配置工具（直接使用SMC接口）
简化版本：只提供手动设置OUT端口值的功能

运行方式:
    python scripts/test_io_output.py <controller_ip> <port> <value>
    
示例:
    python scripts/test_io_output.py 192.168.1.13 OUT6 0    # 设置OUT6为0 (LOW)
    python scripts/test_io_output.py 192.168.1.13 OUT6 1    # 设置OUT6为1 (HIGH)
    python scripts/test_io_output.py 192.168.1.13 6 0      # 设置OUT6为0 (端口号)
    python scripts/test_io_output.py 192.168.1.13 OUT6      # 读取OUT6状态
"""

import ctypes
import sys
import os
from pathlib import Path
from typing import Optional

# 控制器配置（从devices_config.json提取）
CONTROLLER_CONFIGS = {
    "192.168.1.11": {"card_id": 0, "name": "motion/1"},
    "192.168.1.12": {"card_id": 0, "name": "motion/2"},
    "192.168.1.13": {"card_id": 0, "name": "motion/3"},
}

# 端口配置（保留active_low信息用于显示）
PORT_CONFIGS = {
    "OUT0": {"description": "六自由度+大行程驱动器上电", "active_low": True},
    "OUT3": {"description": "六自由度刹车供电", "active_low": True},
    "OUT4": {"description": "大行程刹车供电", "active_low": True},
    "OUT5": {"description": "反射光成像驱动器上电", "active_low": True},
    "OUT6": {"description": "辅助支撑驱动器上电", "active_low": True},
}


class SMCIOController:
    """SMC IO控制器（直接调用SMC库）"""
    
    def __init__(self):
        self.smc = None
        self.is_windows = os.name == 'nt'
        self._load_library()
    
    def _load_library(self):
        """加载SMC库"""
        workspace_root = Path(__file__).parent.parent
        if self.is_windows:
            dll_path = workspace_root / "lib" / "LTSMC.dll"
            if not dll_path.exists():
                raise FileNotFoundError(f"SMC库未找到: {dll_path}")
            self.smc = ctypes.WinDLL(str(dll_path))
        else:
            so_path = workspace_root / "lib" / "libLTSMC.so"
            if not so_path.exists():
                raise FileNotFoundError(f"SMC库未找到: {so_path}")
            self.smc = ctypes.CDLL(str(so_path))
        
        # 定义函数签名
        self.smc.smc_set_connect_timeout.argtypes = [ctypes.c_uint32]
        self.smc.smc_set_connect_timeout.restype = ctypes.c_int16
        
        self.smc.smc_board_init.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_char_p, ctypes.c_uint32]
        self.smc.smc_board_init.restype = ctypes.c_int16
        
        self.smc.smc_board_close.argtypes = [ctypes.c_uint16]
        self.smc.smc_board_close.restype = ctypes.c_int16
        
        self.smc.smc_write_outbit.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]
        self.smc.smc_write_outbit.restype = ctypes.c_int16
        
        self.smc.smc_read_outbit.argtypes = [ctypes.c_uint16, ctypes.c_uint16]
        self.smc.smc_read_outbit.restype = ctypes.c_int16
    
    def connect(self, controller_ip: str, card_id: int = 0) -> bool:
        """连接控制器"""
        try:
            # 设置连接超时
            self.smc.smc_set_connect_timeout(3000)  # 3秒
            
            # 初始化连接
            ip_bytes = controller_ip.encode('utf-8')
            ret = self.smc.smc_board_init(card_id, 2, ip_bytes, 0)  # type=2表示网络连接
            if ret != 0:
                print(f"❌ 连接失败: 错误码 {ret}")
                return False
            print(f"✅ 连接成功: {controller_ip} (card_id={card_id})")
            return True
        except Exception as e:
            print(f"❌ 连接异常: {e}")
            return False
    
    def disconnect(self, card_id: int = 0):
        """断开连接"""
        try:
            self.smc.smc_board_close(card_id)
            print("✅ 已断开连接")
        except Exception as e:
            print(f"⚠️ 断开连接异常: {e}")
    
    def write_outbit(self, card_id: int, bitno: int, value: int) -> bool:
        """写入OUT端口值
        
        Args:
            card_id: 控制器ID
            bitno: 端口号 (0-11)
            value: 硬件值 (0=LOW, 1=HIGH)
        """
        try:
            ret = self.smc.smc_write_outbit(card_id, bitno, value)
            if ret != 0:
                print(f"❌ 写入失败: 错误码 {ret}")
                return False
            return True
        except Exception as e:
            print(f"❌ 写入异常: {e}")
            return False
    
    def read_outbit(self, card_id: int, bitno: int) -> Optional[int]:
        """读取OUT端口值
        
        Returns:
            硬件值 (0=LOW, 1=HIGH) 或 None（失败）
        """
        try:
            value = self.smc.smc_read_outbit(card_id, bitno)
            return value
        except Exception as e:
            print(f"❌ 读取异常: {e}")
            return None


def parse_port(port_str: str) -> Optional[int]:
    """解析端口名称或编号"""
    port_str = port_str.upper().strip()
    if port_str.startswith("OUT"):
        try:
            return int(port_str[3:])
        except ValueError:
            return None
    try:
        port_num = int(port_str)
        if 0 <= port_num <= 11:
            return port_num
    except ValueError:
        pass
    return None


def port_name(port_num: int) -> str:
    """端口号转换为名称"""
    return f"OUT{port_num}"


def main():
    """主函数"""
    if len(sys.argv) < 3:
        print(__doc__)
        print("\n用法:")
        print("  设置端口值: python test_io_output.py <controller_ip> <port> <value>")
        print("  读取端口值: python test_io_output.py <controller_ip> <port>")
        print("\n示例:")
        print("  python test_io_output.py 192.168.1.13 OUT6 0")
        print("  python test_io_output.py 192.168.1.13 OUT6 1")
        print("  python test_io_output.py 192.168.1.13 OUT6")
        sys.exit(1)
    
    controller_ip = sys.argv[1]
    port_str = sys.argv[2]
    
    # 获取card_id
    config = CONTROLLER_CONFIGS.get(controller_ip, {})
    card_id = config.get("card_id", 0)
    
    # 解析端口号
    port_num = parse_port(port_str)
    if port_num is None:
        print(f"❌ 无效的端口号: {port_str}")
        sys.exit(1)
    
    port_name_str = port_name(port_num)
    port_config = PORT_CONFIGS.get(port_name_str, {})
    active_low = port_config.get("active_low", False)
    description = port_config.get("description", "")
    
    # 创建控制器
    controller = SMCIOController()
    
    # 连接
    if not controller.connect(controller_ip, card_id):
        sys.exit(1)
    
    try:
        # 如果是读取模式
        if len(sys.argv) == 3:
            value = controller.read_outbit(card_id, port_num)
            if value is not None:
                hw_desc = "HIGH" if value == 1 else "LOW"
                if active_low:
                    logic_value = 1 - value
                    logic_desc = "开启" if logic_value == 1 else "关闭"
                    print(f"\n端口 {port_name_str} ({description}):")
                    print(f"  硬件值: {value} ({hw_desc})")
                    print(f"  逻辑值: {logic_value} ({logic_desc}) [低电平有效]")
                else:
                    logic_value = value
                    logic_desc = "开启" if logic_value == 1 else "关闭"
                    print(f"\n端口 {port_name_str} ({description}):")
                    print(f"  硬件值: {value} ({hw_desc})")
                    print(f"  逻辑值: {logic_value} ({logic_desc})")
            else:
                print(f"❌ 读取端口 {port_name_str} 失败")
                sys.exit(1)
        
        # 如果是写入模式
        elif len(sys.argv) == 4:
            try:
                hw_value = int(sys.argv[3])
                if hw_value not in [0, 1]:
                    print("❌ 硬件值必须是 0 (LOW) 或 1 (HIGH)")
                    sys.exit(1)
            except ValueError:
                print("❌ 无效的硬件值，请输入 0 或 1")
                sys.exit(1)
            
            # 读取当前值
            current_value = controller.read_outbit(card_id, port_num)
            if current_value is not None:
                current_hw_desc = "HIGH" if current_value == 1 else "LOW"
                print(f"\n当前状态: 硬件值={current_value} ({current_hw_desc})")
            
            # 写入新值
            print(f"\n设置端口 {port_name_str} ({description}):")
            hw_desc = "HIGH" if hw_value == 1 else "LOW"
            if active_low:
                logic_value = 1 - hw_value
                logic_desc = "开启" if logic_value == 1 else "关闭"
                print(f"  硬件值: {hw_value} ({hw_desc})")
                print(f"  逻辑值: {logic_value} ({logic_desc}) [低电平有效]")
            else:
                logic_value = hw_value
                logic_desc = "开启" if logic_value == 1 else "关闭"
                print(f"  硬件值: {hw_value} ({hw_desc})")
                print(f"  逻辑值: {logic_value} ({logic_desc})")
            
            if controller.write_outbit(card_id, port_num, hw_value):
                print(f"✅ 设置成功")
                
                # 验证写入
                import time
                time.sleep(0.1)
                verify_value = controller.read_outbit(card_id, port_num)
                if verify_value is not None:
                    if verify_value == hw_value:
                        print(f"✅ 验证成功: 硬件值={verify_value}")
                    else:
                        print(f"⚠️ 验证失败: 期望={hw_value}, 实际={verify_value}")
            else:
                print(f"❌ 设置失败")
                sys.exit(1)
        
        else:
            print("❌ 参数错误")
            sys.exit(1)
    
    finally:
        controller.disconnect(card_id)


if __name__ == "__main__":
    main()
