"""
全局模拟模式管理器
Global Simulation Mode Manager

管理所有设备的模拟/真实模式切换，提供统一的接口供所有页面使用。
运行时切换只影响当前会话，server 重启后恢复配置文件的值。
"""

from PyQt5.QtCore import QObject, pyqtSignal
from typing import Dict, Optional
import PyTango


class SimulationModeManager(QObject):
    """全局模拟模式管理器（单例模式）"""
    
    # 模式变化信号
    mode_changed = pyqtSignal(bool)  # bool: True=模拟模式, False=真实模式
    
    _instance: Optional['SimulationModeManager'] = None
    
    def __init__(self):
        super().__init__()
        self._sim_mode = False  # 默认真实模式
        self._devices: Dict[str, Optional[PyTango.DeviceProxy]] = {}
        self._registered_names: set = set()  # 仅记录设备名，延迟连接
        
    @classmethod
    def instance(cls) -> 'SimulationModeManager':
        """获取单例实例"""
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance
    
    def register_device(self, device_name: str, device_proxy: Optional[PyTango.DeviceProxy] = None):
        """
        注册设备（延迟连接模式，避免启动时阻塞）
        
        Args:
            device_name: 设备名称
            device_proxy: 可选的已有 DeviceProxy，如果不提供则延迟创建
        """
        if device_proxy is not None:
            # 如果提供了 proxy，直接使用
            self._devices[device_name] = device_proxy
        else:
            # 延迟连接：只记录设备名，不立即创建 DeviceProxy
            self._registered_names.add(device_name)
            # 初始化为 None，等需要时再创建
            if device_name not in self._devices:
                self._devices[device_name] = None
    
    def _get_or_create_proxy(self, device_name: str) -> Optional[PyTango.DeviceProxy]:
        """获取或创建 DeviceProxy（按需连接）"""
        if device_name in self._devices and self._devices[device_name] is not None:
            return self._devices[device_name]
        
        try:
            print(f"[SimulationModeManager] Connecting to {device_name}...")
            proxy = PyTango.DeviceProxy(device_name)
            proxy.set_timeout_millis(2000)  # 设置超时以避免长时间阻塞
            self._devices[device_name] = proxy
            return proxy
        except Exception as e:
            print(f"[SimulationModeManager] Failed to create proxy for {device_name}: {e}")
            self._devices[device_name] = None
            return None
    
    def get_sim_mode(self) -> bool:
        """获取当前模拟模式状态"""
        return self._sim_mode
    
    def set_sim_mode(self, sim_mode: bool, device_names: Optional[list] = None) -> bool:
        """
        设置模拟模式
        
        Args:
            sim_mode: True=模拟模式, False=真实模式
            device_names: 要切换的设备列表，如果为 None 则切换所有已注册的设备
        
        Returns:
            bool: 是否成功
        """
        if device_names is None:
            # 使用所有已注册的设备名
            device_names = list(self._registered_names | set(self._devices.keys()))
        
        success_count = 0
        failed_devices = []
        
        for device_name in device_names:
            # 使用延迟连接获取 proxy
            device_proxy = self._get_or_create_proxy(device_name)
            if device_proxy is None:
                failed_devices.append(device_name)
                continue
            
            try:
                # 调用 simSwitch 命令
                mode_value = 1 if sim_mode else 0
                device_proxy.command_inout("simSwitch", mode_value)
                success_count += 1
                print(f"[SimulationModeManager] {device_name}: simSwitch({mode_value}) succeeded")
            except PyTango.DevFailed as e:
                # Tango 设备错误（可能不支持 simSwitch 命令）
                error_msg = str(e)
                if len(e.args) > 0 and hasattr(e.args[0], '__iter__') and len(e.args[0]) > 0:
                    error_msg = e.args[0][0].desc
                failed_devices.append(device_name)
                print(f"[SimulationModeManager] {device_name}: simSwitch failed - {error_msg}")
            except Exception as e:
                failed_devices.append(device_name)
                print(f"[SimulationModeManager] {device_name}: simSwitch failed - {e}")
        
        if success_count > 0:
            self._sim_mode = sim_mode
            self.mode_changed.emit(sim_mode)
            if failed_devices:
                print(f"[SimulationModeManager] Warning: Failed to switch {len(failed_devices)} devices: {failed_devices}")
            return True
        else:
            print(f"[SimulationModeManager] Error: All devices failed to switch mode")
            return False
    
    def toggle_mode(self, device_names: Optional[list] = None) -> bool:
        """切换模式（模拟 <-> 真实）"""
        new_mode = not self._sim_mode
        return self.set_sim_mode(new_mode, device_names)
    
    def get_mode_text(self) -> str:
        """获取模式文本描述"""
        return "模拟模式" if self._sim_mode else "真实模式"
    
    def get_mode_status_text(self) -> str:
        """获取模式状态文本（用于显示）"""
        return f"当前：{self.get_mode_text()}"

