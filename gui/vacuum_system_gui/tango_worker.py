"""
真空系统 Tango 通信 Worker

使用 QThread 进行后台轮询，避免阻塞 UI
"""

import json
import time
import random
from typing import Optional, Dict, Any, Callable
from queue import Queue, Empty
from PyQt5.QtCore import QThread, QMutex, pyqtSignal

try:
    import tango
except ImportError:
    tango = None
    # 延迟导入 logger，避免循环依赖
    import logging
    logging.warning("PyTango 未安装，将使用模拟模式")

from config import VACUUM_SYSTEM_DEVICE, AUTO_MOLECULAR_PUMP_CONFIG
from utils.logger import get_logger

logger = get_logger(__name__)


class VacuumTangoWorker(QThread):
    """真空系统 Tango 通信 Worker"""
    
    # 信号定义
    connection_changed = pyqtSignal(bool)   # 连接状态变化 (Tango)
    plc_connection_changed = pyqtSignal(bool) # PLC 连接状态变化
    alarm_received = pyqtSignal(dict)        # 收到新报警
    command_result = pyqtSignal(str, bool, str)  # 命令结果 (cmd_name, success, message)
    
    def __init__(self, device_name: str = VACUUM_SYSTEM_DEVICE, parent=None):
        super().__init__(parent)
        
        self.device_name = device_name
        self.device: Optional[tango.DeviceProxy] = None
        
        self._running = True
        self._poll_interval = 0.1  # 100ms
        
        # 状态缓存
        self._status_cache: Dict[str, Any] = {}
        self._last_plc_connected = False
        self._last_tango_connected = False  # 跟踪 Tango 连接状态，避免重复发送信号
        self._cache_mutex = QMutex()
        
        # 命令队列
        self._command_queue: Queue = Queue()
        
        # 上次报警（线程安全）
        self._last_alarm_count = 0
        self._alarm_count_mutex = QMutex()
        
        # 连接重试控制（防止 Tango 1000ms 连接限制）
        self._last_connect_attempt = 0.0  # 上次连接尝试时间戳
        self._min_connect_interval = 1.2  # 最小连接间隔（秒），略大于 Tango 的 1000ms 限制
        self._connect_fail_count = 0  # 连续连接失败次数
        self._last_connect_error = None  # 上次连接错误信息
        
    def run(self):
        """主循环"""
        self._connect_device()
        
        while self._running:
            try:
                # 处理命令队列
                self._process_commands()
                
                # 轮询状态
                if self.device:
                    self._poll_status()
                    self._check_alarms()
                else:
                    # 尝试重连
                    self._connect_device()
                    
            except Exception as e:
                # 设备断开或通信失败，更新连接状态
                if self.device is not None:
                    logger.warning(f"Tango 设备连接失败，更新连接状态: {e}")
                    self.device = None
                    # 只有在状态变化时才发送信号
                    if self._last_tango_connected:
                        self._last_tango_connected = False
                        self.connection_changed.emit(False)
                else:
                    # 设备已为 None，可能是重连失败，只记录日志
                    logger.debug(f"Tango Worker 错误（设备已断开）: {e}")
                
            time.sleep(self._poll_interval)
            
    def stop(self):
        """停止 Worker"""
        self._running = False
        self.wait(2000)
        
    def _connect_device(self):
        """连接 Tango 设备
        
        注意：Tango 有保护机制，不允许在 1000ms 内重复连接请求。
        此方法会检查距离上次连接尝试的时间，如果不足最小间隔则跳过。
        """
        if tango is None:
            # 模拟模式
            self.connection_changed.emit(True)
            return
        
        # 检查连接间隔，防止触发 Tango 的 1000ms 连接限制
        current_time = time.time()
        time_since_last_attempt = current_time - self._last_connect_attempt
        if time_since_last_attempt < self._min_connect_interval:
            # 距离上次尝试不足最小间隔，跳过本次连接
            return
        
        # 更新上次连接尝试时间
        self._last_connect_attempt = current_time
            
        try:
            self.device = tango.DeviceProxy(self.device_name)
            self.device.set_timeout_millis(3000)
            # 测试连接
            _ = self.device.state()
            # 连接成功，重置失败计数
            if self._connect_fail_count > 0:
                logger.info(f"已重新连接到 {self.device_name}（之前失败 {self._connect_fail_count} 次）")
            else:
                logger.info(f"已连接到 {self.device_name}")
            self._connect_fail_count = 0
            self._last_connect_error = None
            # 只有在状态变化时才发送信号
            if not self._last_tango_connected:
                self._last_tango_connected = True
                self.connection_changed.emit(True)
        except Exception as e:
            self._connect_fail_count += 1
            error_msg = str(e)
            
            # 只在以下情况打印完整 Traceback：
            # 1. 第一次失败
            # 2. 错误信息发生变化
            # 3. 每 10 次失败打印一次（避免日志过多）
            should_print_traceback = (
                self._connect_fail_count == 1 or  # 第一次失败
                error_msg != self._last_connect_error or  # 错误信息变化
                self._connect_fail_count % 10 == 0  # 每 10 次打印一次
            )
            
            if should_print_traceback:
                logger.error(
                    f"连接 {self.device_name} 失败 (第 {self._connect_fail_count} 次): {e}",
                    exc_info=True
                )
            else:
                # 只打印简短错误信息，不打印 Traceback
                logger.warning(
                    f"连接 {self.device_name} 失败 (第 {self._connect_fail_count} 次): {error_msg}"
                )
            
            self._last_connect_error = error_msg
            self.device = None
            # 只有在状态变化时才发送信号
            if self._last_tango_connected:
                self._last_tango_connected = False
                self.connection_changed.emit(False)
            
    def _poll_status(self):
        """轮询设备状态"""
        if not self.device:
            return
            
        try:
            # 首先测试连接状态，如果断开会抛出异常
            _ = self.device.state()
            
            status = {}
            
            # 系统状态
            # 使用 read_attribute 强制从设备读取最新值，避免使用缓存
            try:
                attr = self.device.read_attribute("operationMode")
                mode_val = int(attr.value)
                status['operationMode'] = mode_val
            except Exception as e:
                logger.error(f"[ERROR] 读取 operationMode 失败: {e}", exc_info=True)
                status['operationMode'] = 1  # 默认手动模式
                
            try:
                status['systemState'] = int(self.device.read_attribute("systemState").value)
            except Exception as e:
                logger.error(f"读取 systemState 失败: {e}")
                status['systemState'] = 0  # 默认空闲
                
            try:
                status['simulatorMode'] = bool(self.device.read_attribute("simulatorMode").value)
            except Exception as e:
                logger.error(f"读取 simulatorMode 失败: {e}")
                status['simulatorMode'] = False
                
            try:
                status['autoSequenceStep'] = int(self.device.read_attribute("autoSequenceStep").value)
            except Exception as e:
                logger.error(f"读取 autoSequenceStep 失败: {e}")
                status['autoSequenceStep'] = 0
                
            try:
                plc_connected = bool(self.device.read_attribute("plcConnected").value)
                status['plcConnected'] = plc_connected
                if plc_connected != self._last_plc_connected:
                    self._last_plc_connected = plc_connected
                    self.plc_connection_changed.emit(plc_connected)
            except Exception as e:
                # 如果读取失败，默认为未连接（除非是模拟模式）
                # 检查是否为模拟模式
                try:
                    sim_mode = bool(self.device.read_attribute("simulatorMode").value)
                    # 模拟模式下，PLC 连接状态应该为 True（因为不需要真实 PLC）
                    status['plcConnected'] = sim_mode
                except:
                    # 如果连 simulatorMode 都读不到，默认为未连接
                    status['plcConnected'] = False
                
                if status['plcConnected'] != self._last_plc_connected:
                    self._last_plc_connected = status['plcConnected']
                    self.plc_connection_changed.emit(status['plcConnected'])
            
            # 泵状态
            status['screwPumpPower'] = bool(self.device.screwPumpPower)
            status['rootsPumpPower'] = bool(self.device.rootsPumpPower)
            status['molecularPump1Power'] = bool(self.device.molecularPump1Power)
            status['molecularPump2Power'] = bool(self.device.molecularPump2Power)
            status['molecularPump3Power'] = bool(self.device.molecularPump3Power)
            status['molecularPump1Speed'] = int(self.device.molecularPump1Speed)
            status['molecularPump2Speed'] = int(self.device.molecularPump2Speed)
            status['molecularPump3Speed'] = int(self.device.molecularPump3Speed)
            
            # 分子泵启用配置（从Tango设备读取，同步到本地配置）
            try:
                status['molecularPump1Enabled'] = bool(self.device.read_attribute("molecularPump1Enabled").value)
                status['molecularPump2Enabled'] = bool(self.device.read_attribute("molecularPump2Enabled").value)
                status['molecularPump3Enabled'] = bool(self.device.read_attribute("molecularPump3Enabled").value)
                # 同步到本地配置
                AUTO_MOLECULAR_PUMP_CONFIG['molecularPump1Enabled'] = status['molecularPump1Enabled']
                AUTO_MOLECULAR_PUMP_CONFIG['molecularPump2Enabled'] = status['molecularPump2Enabled']
                AUTO_MOLECULAR_PUMP_CONFIG['molecularPump3Enabled'] = status['molecularPump3Enabled']
            except Exception as e:
                logger.error(f"读取分子泵启用配置失败: {e}")
                # 使用本地配置作为默认值
                status['molecularPump1Enabled'] = AUTO_MOLECULAR_PUMP_CONFIG.get('molecularPump1Enabled', True)
                status['molecularPump2Enabled'] = AUTO_MOLECULAR_PUMP_CONFIG.get('molecularPump2Enabled', True)
                status['molecularPump3Enabled'] = AUTO_MOLECULAR_PUMP_CONFIG.get('molecularPump3Enabled', True)
            
            # 闸板阀状态
            for i in range(1, 6):
                status[f'gateValve{i}Open'] = bool(getattr(self.device, f'gateValve{i}Open'))
                status[f'gateValve{i}Close'] = bool(getattr(self.device, f'gateValve{i}Close'))
                status[f'gateValve{i}ActionState'] = int(getattr(self.device, f'gateValve{i}ActionState'))
                
            # 电磁阀状态
            for i in range(1, 5):
                status[f'electromagneticValve{i}Open'] = bool(getattr(self.device, f'electromagneticValve{i}Open'))
                status[f'electromagneticValve{i}Close'] = bool(getattr(self.device, f'electromagneticValve{i}Close'))
                
            # 放气阀状态
            for i in range(1, 3):
                status[f'ventValve{i}Open'] = bool(getattr(self.device, f'ventValve{i}Open'))
                status[f'ventValve{i}Close'] = bool(getattr(self.device, f'ventValve{i}Close'))
                
            # 传感器
            status['vacuumGauge1'] = float(self.device.vacuumGauge1)
            status['vacuumGauge2'] = float(self.device.vacuumGauge2)
            status['vacuumGauge3'] = float(self.device.vacuumGauge3)
            status['airPressure'] = float(self.device.airPressure)
            
            # 新增属性 - 泵频率
            status['screwPumpFrequency'] = int(self.device.screwPumpFrequency)
            status['rootsPumpFrequency'] = int(self.device.rootsPumpFrequency)
            
            # 新增属性 - 系统联锁信号
            status['phaseSequenceOk'] = bool(self.device.phaseSequenceOk)
            status['motionSystemOnline'] = bool(self.device.motionSystemOnline)
            status['gateValve5Permit'] = bool(self.device.gateValve5Permit)
            
            # 新增属性 - 水电磁阀状态
            for i in range(1, 7):
                status[f'waterValve{i}State'] = bool(getattr(self.device, f'waterValve{i}State'))
            status['airMainValveState'] = bool(self.device.airMainValveState)
            
            # 从水电磁阀状态推导水路状态（用于兼容现有代码）
            # 水路1-4对应水电磁阀1-4的状态
            for i in range(1, 5):
                status[f'waterFlow{i}'] = status.get(f'waterValve{i}State', False)
            
            # 从气源压力推导气路状态（用于兼容现有代码）
            # 气路正常：气源压力 >= 0.4 MPa
            air_pressure = status.get('airPressure', 0)
            status['airSupplyOk'] = air_pressure >= 0.4
            
            # 报警
            status['activeAlarmCount'] = int(self.device.activeAlarmCount)
            status['hasUnacknowledgedAlarm'] = bool(self.device.hasUnacknowledgedAlarm)
            
            # 更新缓存
            self._cache_mutex.lock()
            self._status_cache = status
            self._cache_mutex.unlock()
            
        except Exception as e:
            # 连接断开或设备无响应，记录错误并重新抛出异常
            # 让 run() 方法处理连接状态更新
            logger.warning(f"轮询状态失败，可能设备已断开: {e}")
            # 重新抛出异常，让 run() 方法捕获并更新连接状态
            raise
            
    def _check_alarms(self):
        """检查新报警"""
        if not self.device:
            return
            
        try:
            current_count = int(self.device.activeAlarmCount)
            
            # 线程安全地读取和更新报警计数
            self._alarm_count_mutex.lock()
            last_count = self._last_alarm_count
            self._alarm_count_mutex.unlock()
            
            if current_count > last_count:
                # 有新报警
                alarm_json = self.device.latestAlarmJson
                if alarm_json:
                    alarm_data = json.loads(alarm_json)
                    self.alarm_received.emit(alarm_data)
            
            # 线程安全地更新报警计数
            self._alarm_count_mutex.lock()
            self._last_alarm_count = current_count
            self._alarm_count_mutex.unlock()
            
        except Exception as e:
            logger.error(f"检查报警失败: {e}", exc_info=True)
            
    def _process_commands(self):
        """处理命令队列"""
        try:
            while not self._command_queue.empty():
                cmd_name, args = self._command_queue.get_nowait()
                self._execute_command(cmd_name, args)
        except Empty:
            pass
            
    def _execute_command(self, cmd_name: str, args):
        """执行单个命令"""
        print(f"[PRINT DEBUG] _execute_command: 开始执行 {cmd_name}, args={args}, device={self.device}")
        if not self.device:
            print(f"[PRINT DEBUG] _execute_command: 设备未连接，命令 {cmd_name} 被拒绝")
            self.command_result.emit(cmd_name, False, "设备未连接")
            return
            
        try:
            print(f"[PRINT DEBUG] _execute_command: 获取命令对象 {cmd_name}")
            cmd = getattr(self.device, cmd_name)
            print(f"[PRINT DEBUG] _execute_command: 命令对象获取成功，准备调用")
            if args is not None:
                print(f"[PRINT DEBUG] _execute_command: 调用命令 {cmd_name}，参数={args}")
                cmd(args)
            else:
                print(f"[PRINT DEBUG] _execute_command: 调用命令 {cmd_name}，无参数")
                cmd()
            print(f"[PRINT DEBUG] _execute_command: 命令 {cmd_name} 调用完成（无异常）")
            
            # 命令执行成功后，立即更新缓存（避免等待下次轮询和属性缓存问题）
            # 这对于模式切换等关键操作特别重要
            if cmd_name in ["SwitchToAuto", "SwitchToManual", "OneKeyVacuumStart", 
                          "OneKeyVacuumStop", "ChamberVent", "FaultReset"]:
                # 对于模式切换命令，直接更新缓存，不依赖属性读取（避免 PyTango 缓存问题）
                if cmd_name in ["SwitchToAuto", "SwitchToManual"]:
                    # 直接更新缓存，不等待属性读取
                    new_mode = 0 if cmd_name == "SwitchToAuto" else 1
                    self._cache_mutex.lock()
                    if 'operationMode' in self._status_cache:
                        self._status_cache['operationMode'] = new_mode
                        self._status_cache['systemState'] = 0  # IDLE
                        self._status_cache['autoSequenceStep'] = 0
                    self._cache_mutex.unlock()
                else:
                    # 其他命令等待50ms后轮询
                    time.sleep(0.05)  # 50ms
                    # 立即轮询一次状态，确保GUI能立即看到变化
                    print(f"[PRINT DEBUG] _execute_command: 立即轮询状态")
                    self._poll_status()
                
                logger.info(f"[DEBUG] _execute_command: {cmd_name} 执行成功，已更新状态缓存")
            
            print(f"[PRINT DEBUG] _execute_command: {cmd_name} 执行成功（无异常），发送成功信号")
            self.command_result.emit(cmd_name, True, "执行成功")
        except Exception as e:
            print(f"[PRINT DEBUG] _execute_command: {cmd_name} 执行失败，异常: {e}")
            import traceback
            traceback.print_exc()
            self.command_result.emit(cmd_name, False, str(e))
            
    # =========================================================================
    # 公共接口
    # =========================================================================
    
    def get_cached_status(self) -> Dict[str, Any]:
        """获取缓存的状态（线程安全）"""
        self._cache_mutex.lock()
        status = self._status_cache.copy()
        self._cache_mutex.unlock()
        return status
        
    def queue_command(self, cmd_name: str, args=None):
        """将命令加入队列"""
        print(f"[PRINT DEBUG] queue_command: 加入命令队列 {cmd_name}, args={args}")
        self._command_queue.put((cmd_name, args))
        
    def is_connected(self) -> bool:
        """是否已连接"""
        return self.device is not None
        
    # =========================================================================
    # 便捷方法
    # =========================================================================
    
    def switch_to_auto(self):
        print("[PRINT DEBUG] switch_to_auto: 正在发送 SwitchToAuto 命令")
        self.queue_command("SwitchToAuto")
        
    def switch_to_manual(self):
        print("[PRINT DEBUG] switch_to_manual: 正在发送 SwitchToManual 命令")
        self.queue_command("SwitchToManual")
        
    def one_key_vacuum_start(self):
        self.queue_command("OneKeyVacuumStart")
        
    def one_key_vacuum_stop(self):
        self.queue_command("OneKeyVacuumStop")
        
    def chamber_vent(self):
        self.queue_command("ChamberVent")
        
    def fault_reset(self):
        self.queue_command("FaultReset")
        
    def set_screw_pump_power(self, state: bool):
        self.queue_command("SetScrewPumpPower", state)
        
    def set_roots_pump_power(self, state: bool):
        self.queue_command("SetRootsPumpPower", state)
        
    def set_molecular_pump_power(self, index: int, state: bool):
        self.queue_command("SetMolecularPumpPower", [index, 1 if state else 0])
        
    def set_gate_valve(self, index: int, open_valve: bool):
        self.queue_command("SetGateValve", [index, 1 if open_valve else 0])
        
    def set_electromagnetic_valve(self, index: int, state: bool):
        self.queue_command("SetElectromagneticValve", [index, 1 if state else 0])
        
    def set_vent_valve(self, index: int, state: bool):
        self.queue_command("SetVentValve", [index, 1 if state else 0])
        
    def acknowledge_alarm(self, alarm_code: int):
        self.queue_command("AcknowledgeAlarm", alarm_code)
        
    def acknowledge_all_alarms(self):
        self.queue_command("AcknowledgeAllAlarms")
    
    # =========================================================================
    # 新增便捷方法 - 泵启停控制
    # =========================================================================
    
    def set_screw_pump_start_stop(self, state: bool):
        """螺杆泵启停控制（上电后的启动/停止）"""
        self.queue_command("SetScrewPumpStartStop", state)
    
    def set_molecular_pump_start_stop(self, index: int, state: bool):
        """分子泵启停控制（上电后的启动/停止）"""
        self.queue_command("SetMolecularPumpStartStop", [index, 1 if state else 0])
    
    def set_molecular_pump_enabled(self, index: int, enabled: bool):
        """设置分子泵启用配置（写入Tango属性）"""
        if not self.device:
            logger.warning(f"设备未连接，无法设置分子泵{index}启用配置")
            return
        
        try:
            attr_name = f"molecularPump{index}Enabled"
            self.device.write_attribute(attr_name, enabled)
            logger.info(f"已设置分子泵{index}启用配置: {enabled} (已写入Tango属性 {attr_name})")
        except Exception as e:
            logger.error(f"设置分子泵{index}启用配置失败: {e}", exc_info=True)
    
    # =========================================================================
    # 新增便捷方法 - 水/气阀控制
    # =========================================================================
    
    def set_water_valve(self, index: int, state: bool):
        """水电磁阀控制 (1-6)"""
        self.queue_command("SetWaterValve", [index, 1 if state else 0])
    
    def set_air_main_valve(self, state: bool):
        """气主电磁阀控制"""
        self.queue_command("SetAirMainValve", state)
    
    # =========================================================================
    # 新增便捷方法 - 报警和调试
    # =========================================================================
    
    def clear_alarm_history(self):
        """清除报警历史"""
        self.queue_command("ClearAlarmHistory")
    
    def get_active_alarms(self):
        """获取活跃报警列表 (返回通过 command_result 信号)"""
        self.queue_command("GetActiveAlarms")
    
    def reset_device(self):
        """设备复位"""
        self.queue_command("Reset")
    
    def self_check(self):
        """设备自检"""
        self.queue_command("SelfCheck")


class MockTangoWorker(VacuumTangoWorker):
    """模拟 Tango Worker（用于无 Tango 环境的测试）"""
    
    def __init__(self, device_name: str = VACUUM_SYSTEM_DEVICE, parent=None):
        super().__init__(device_name, parent)
        
        # 初始化模拟数据
        self._mock_status = {
            'operationMode': 1,  # 手动
            'systemState': 0,    # 空闲
            'simulatorMode': True,
            'autoSequenceStep': 0,
            
            'screwPumpPower': False,
            'rootsPumpPower': False,
            'screwPumpFrequency': 0,  # 螺杆泵频率
            # 注意: Device Server 没有 rootsPumpFrequency 属性，此处保留用于 Mock 模拟
            'rootsPumpFrequency': 0,  # (Mock专用) 罗茨泵频率
            'molecularPump1Power': False,
            'molecularPump2Power': False,
            'molecularPump3Power': False,
            'molecularPump1Speed': 0,
            'molecularPump2Speed': 0,
            'molecularPump3Speed': 0,
            
            'vacuumGauge1': 101325.0,
            'vacuumGauge2': 101325.0,
            'vacuumGauge3': 101325.0,
            'airPressure': 0.5,
            
            # 水电磁阀状态 (6个水电磁阀) - Device Server 实际属性
            # 注意: Device Server 没有 waterFlow1-4 和 waterFlowOk 属性
            # 水路状态应该从 waterValve1-4State 推导
            'waterValve1State': True,
            'waterValve2State': True,
            'waterValve3State': True,
            'waterValve4State': True,
            'waterValve5State': False,
            'waterValve6State': False,
            
            # 从水电磁阀状态推导水路状态（用于兼容现有代码）
            'waterFlow1': True,  # 对应 waterValve1State
            'waterFlow2': True,  # 对应 waterValve2State
            'waterFlow3': True,  # 对应 waterValve3State
            'waterFlow4': True,  # 对应 waterValve4State
            
            # 气主电磁阀状态
            'airMainValveState': False,
            
            # 从气源压力推导气路状态（用于兼容现有代码）
            # 气路正常：气源压力 >= 0.4 MPa
            'airSupplyOk': True,  # 0.5 >= 0.4
            
            # 系统联锁状态
            'phaseSequenceOk': True,      # 相序保护正常
            'motionSystemOnline': True,   # 运动系统在线
            'gateValve5Permit': True,     # GV5动作许可
            
            'activeAlarmCount': 0,
            'hasUnacknowledgedAlarm': False,
        }
        
        # 阀门状态
        for i in range(1, 6):
            self._mock_status[f'gateValve{i}Open'] = False
            self._mock_status[f'gateValve{i}Close'] = True
            self._mock_status[f'gateValve{i}ActionState'] = 0
            
        for i in range(1, 5):
            self._mock_status[f'electromagneticValve{i}Open'] = False
            self._mock_status[f'electromagneticValve{i}Close'] = True
            
        for i in range(1, 3):
            self._mock_status[f'ventValve{i}Open'] = False
            self._mock_status[f'ventValve{i}Close'] = True
            
        # 阀门操作计时器
        self._valve_timers = {}
        self._last_step_time = 0
        
        # 立即初始化缓存，确保启动后立即可用
        self._cache_mutex.lock()
        self._status_cache = self._mock_status.copy()
        self._cache_mutex.unlock()
        
    def _connect_device(self):
        """模拟连接"""
        self.device = "MOCK_DEVICE"  # 设置一个非空值，确保主循环能调用 _poll_status
        if not self._last_tango_connected:
            self._last_tango_connected = True
            self.connection_changed.emit(True)
        
    def _check_alarms(self):
        """模拟检查报警"""
        # 简单模拟，暂不处理
        pass
        
    def _poll_status(self):
        """更新模拟状态"""
        # 模拟随机故障 (已暂时禁用，设为 0 关闭)
        # if self._mock_status['systemState'] not in [4, 5]:
        #     if random.random() < 0.005:
        #         self._trigger_mock_fault()

        # 模拟真空度变化
        self._simulate_vacuum()
        
        # 处理阀门动作状态
        self._process_valve_timers()
        
        # 处理自动序列逻辑
        self._process_auto_sequence()
        
        # 从水电磁阀状态推导水路状态（用于兼容现有代码）
        # 水路1-4对应水电磁阀1-4的状态
        for i in range(1, 5):
            self._mock_status[f'waterFlow{i}'] = self._mock_status.get(f'waterValve{i}State', False)
        
        # 从气源压力推导气路状态（用于兼容现有代码）
        # 气路正常：气源压力 >= 0.4 MPa
        air_pressure = self._mock_status.get('airPressure', 0)
        self._mock_status['airSupplyOk'] = air_pressure >= 0.4
        
        # 更新缓存
        self._cache_mutex.lock()
        self._status_cache = self._mock_status.copy()
        self._cache_mutex.unlock()
        
    def _trigger_mock_fault(self):
        """触发一个模拟故障 - 仅报警，不关停设备"""
        self._mock_status['systemState'] = 4  # FAULT
        self._mock_status['activeAlarmCount'] += 1
        self._mock_status['hasUnacknowledgedAlarm'] = True
        
        # 发送报警信号
        fault_msgs = [
            "螺杆泵电机过载保护触发",
            "气源压力低于安全阈值 (0.2MPa)",
            "前级真空计通讯链路中断",
            "分子泵控制器检测到异常振动",
            "冷却水流量骤减故障"
        ]
        alarm_data = {
            "alarm_code": random.randint(1000, 9999),
            "alarm_type": "ERROR",
            "description": random.choice(fault_msgs),
            "device_name": "PLC_MOCK",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S")
        }
        self.alarm_received.emit(alarm_data)
        logger.warning(f"MOCK故障触发: {alarm_data['description']}")

    def _simulate_vacuum(self):
        """模拟真空度变化 (物理逻辑增强)"""
        ATMOSPHERIC = 101325.0
        
        # 1. 检查各部件状态
        # 连通性：如果任何主抽或旁路闸板阀打开，腔室与泵组连通
        connected = any([
            self._mock_status.get('gateValve1Open', False),
            self._mock_status.get('gateValve2Open', False),
            self._mock_status.get('gateValve3Open', False),
            self._mock_status.get('gateValve4Open', False)
        ])
        
        venting = self._mock_status.get('ventValve1Open', False) or self._mock_status.get('ventValve2Open', False)
        
        # 泵运行模拟
        if self._mock_status['screwPumpPower']:
            current_freq = self._mock_status.get('screwPumpFrequency', 0)
            if current_freq < 110:
                self._mock_status['screwPumpFrequency'] = min(110, current_freq + 10)
        else:
            self._mock_status['screwPumpFrequency'] = 0
            
        screw_running = self._mock_status['screwPumpPower'] and self._mock_status['screwPumpFrequency'] > 0
        roots_running = self._mock_status.get('rootsPumpPower', False) and self._mock_status.get('rootsPumpFrequency', 0) > 0
        
        mp_running = any([
            self._mock_status.get('molecularPump1Power', False),
            self._mock_status.get('molecularPump2Power', False),
            self._mock_status.get('molecularPump3Power', False)
        ]) and any([
            self._mock_status.get('molecularPump1Speed', 0) > 5000,
            self._mock_status.get('molecularPump2Speed', 0) > 5000,
            self._mock_status.get('molecularPump3Speed', 0) > 5000
        ])

        # 2. 模拟前级真空 G1 (Foreline)
        v1 = self._mock_status.get('vacuumGauge1', ATMOSPHERIC)
        if screw_running:
            target = 40.0 if roots_running else 1000.0 # 下调至40Pa
            speed = 0.92 if roots_running else 0.95
            if v1 > target:
                v1 = max(target, v1 * speed)
        elif not venting:
            v1 = min(ATMOSPHERIC, v1 * 1.001 + 1)
        self._mock_status['vacuumGauge1'] = v1

        # 3. 模拟腔室真空 G2, G3 (Main 1, Main 2)
        v2 = self._mock_status.get('vacuumGauge2', ATMOSPHERIC)
        v3 = self._mock_status.get('vacuumGauge3', ATMOSPHERIC)
        
        if venting:
            # 放气优先
            v2 = min(ATMOSPHERIC, v2 * 1.8 + 2000)
            v3 = min(ATMOSPHERIC, v3 * 1.8 + 2000)
            if connected: v1 = min(ATMOSPHERIC, v1 * 1.8 + 2000)
        elif connected:
            # 连通：随泵组下降，并与 G1 趋同
            target = 0.001 if mp_running else (45.0 if roots_running else 1000.0)
            speed = 0.85 if mp_running else (0.92 if roots_running else 0.95)
            
            if v2 > target:
                v2 = max(target, v2 * speed)
            
            # 压差平衡：G2 和 G1 相互拉近
            avg = (v2 + v1) / 2.0
            v2 = v2 * 0.7 + avg * 0.3
            self._mock_status['vacuumGauge1'] = v1 * 0.7 + avg * 0.3
            v3 = v2 # G3 同步 G2
        else:
            # 隔离：漏气上升
            v2 = min(ATMOSPHERIC, v2 * 1.0005 + 0.1)
            v3 = min(ATMOSPHERIC, v3 * 1.0005 + 0.1)
            
        self._mock_status['vacuumGauge2'] = v2
        self._mock_status['vacuumGauge3'] = v3
        
        # 气压模拟
        self._mock_status['airPressure'] = 0.6
        self._mock_status['airSupplyOk'] = True
        
        # 注意: Device Server 没有 waterFlowOk 和 airSupplyOk 属性
        # GUI 应该从 waterValve1-4State 和 airPressure >= 0.4 推导状态
                    
    def _process_valve_timers(self):
        """处理阀门动作计时器"""
        current_time = time.time()
        completed = []
        
        for valve_key, (start_time, target_open) in self._valve_timers.items():
            elapsed = current_time - start_time
            
            if elapsed >= 2.0:  # 2秒后动作完成
                # 检查是否超时 (模拟：超过5秒视为超时)
                if elapsed >= 5.0:
                    if target_open:
                        self._mock_status[f'{valve_key}ActionState'] = 3  # OPEN_TIMEOUT
                    else:
                        self._mock_status[f'{valve_key}ActionState'] = 4  # CLOSE_TIMEOUT
                else:
                    # 正常完成
                    self._mock_status[f'{valve_key}Open'] = target_open
                    self._mock_status[f'{valve_key}Close'] = not target_open
                    self._mock_status[f'{valve_key}ActionState'] = 0  # IDLE
                    completed.append(valve_key)
                    
        for key in completed:
            del self._valve_timers[key]
        
    def _execute_command(self, cmd_name: str, args):
        """模拟执行命令"""
        logger.debug(f"模拟执行命令: {cmd_name}, 参数: {args}")
        
        # 模拟命令效果
        if cmd_name == "SwitchToAuto":
            self._mock_status['operationMode'] = 0
        elif cmd_name == "SwitchToManual":
            self._mock_status['operationMode'] = 1
            self._mock_status['autoSequenceStep'] = 0
        elif cmd_name == "SetScrewPumpPower":
            self._mock_status['screwPumpPower'] = bool(args)
        elif cmd_name == "SetRootsPumpPower":
            self._mock_status['rootsPumpPower'] = bool(args)
        elif cmd_name == "SetMolecularPumpPower":
            index, state = args
            self._mock_status[f'molecularPump{index}Power'] = bool(state)
            if state:
                # 模拟转速逐渐上升
                self._mock_status[f'molecularPump{index}Speed'] = 30000
            else:
                self._mock_status[f'molecularPump{index}Speed'] = 0
        elif cmd_name == "SetGateValve":
            index, open_valve = args
            valve_key = f'gateValve{index}'
            
            # 设置动作状态
            if open_valve:
                self._mock_status[f'{valve_key}ActionState'] = 1  # OPENING
            else:
                self._mock_status[f'{valve_key}ActionState'] = 2  # CLOSING
                
            # 添加计时器
            self._valve_timers[valve_key] = (time.time(), bool(open_valve))
            
        elif cmd_name == "SetElectromagneticValve":
            index, state = args
            self._mock_status[f'electromagneticValve{index}Open'] = bool(state)
            self._mock_status[f'electromagneticValve{index}Close'] = not bool(state)
        elif cmd_name == "SetVentValve":
            index, state = args
            self._mock_status[f'ventValve{index}Open'] = bool(state)
            self._mock_status[f'ventValve{index}Close'] = not bool(state)
        elif cmd_name == "OneKeyVacuumStart":
            # 在判断前先更新真空度模拟，确保使用最新的值
            self._simulate_vacuum()
            
            # 判断当前真空度，选择执行流程
            # 非真空状态：≥3000Pa，使用步骤1-10
            # 低真空状态：<3000Pa，使用步骤100-114
            current_vacuum = self._mock_status.get('vacuumGauge2', 101325)  # 腔室真空度
            start_step = 1 if current_vacuum >= 3000.0 else 100
            flow_type = "非真空状态" if current_vacuum >= 3000.0 else "低真空状态"
            
            logger.info(f"一键抽真空启动 - 判断流程类型: 当前真空度G2={current_vacuum}Pa, "
                       f"判断条件: {current_vacuum} {'<' if current_vacuum < 3000.0 else '>='} 3000Pa, "
                       f"结果={flow_type}, 起始步骤={start_step}")
            
            self._mock_status['systemState'] = 1  # PUMPING
            self._mock_status['autoSequenceStep'] = start_step
            # 模拟启动序列
            self._start_auto_sequence()
            
            logger.info(f"一键抽真空启动 - {flow_type}流程，当前真空度={current_vacuum}Pa，步骤={start_step}")
        elif cmd_name == "OneKeyVacuumStop":
            self._mock_status['systemState'] = 2  # STOPPING
            self._mock_status['autoSequenceStep'] = 1  # 与C++代码保持一致
        elif cmd_name == "ChamberVent":
            self._mock_status['systemState'] = 3  # VENTING
            self._mock_status['autoSequenceStep'] = 1  # 与C++代码保持一致
        elif cmd_name == "FaultReset":
            self._mock_status['systemState'] = 0  # IDLE
            self._mock_status['autoSequenceStep'] = 0
            self._mock_status['activeAlarmCount'] = 0
            self._mock_status['hasUnacknowledgedAlarm'] = False
            # 清除所有阀门超时状态
            for i in range(1, 6):
                self._mock_status[f'gateValve{i}ActionState'] = 0
        elif cmd_name == "EmergencyStop":
            # 紧急停止 - 关闭所有设备
            self._mock_status['systemState'] = 5  # EMERGENCY_STOP
            self._mock_status['autoSequenceStep'] = 0
            self._mock_status['operationMode'] = 1  # 切换到手动模式
            
            # 关闭所有泵
            self._mock_status['screwPumpPower'] = False
            self._mock_status['rootsPumpPower'] = False
            for i in range(1, 4):
                self._mock_status[f'molecularPump{i}Power'] = False
                self._mock_status[f'molecularPump{i}Speed'] = 0
                
            # 关闭所有阀门
            for i in range(1, 6):
                self._mock_status[f'gateValve{i}Open'] = False
                self._mock_status[f'gateValve{i}Close'] = True
                self._mock_status[f'gateValve{i}ActionState'] = 0
            for i in range(1, 5):
                self._mock_status[f'electromagneticValve{i}Open'] = False
                self._mock_status[f'electromagneticValve{i}Close'] = True
            for i in range(1, 3):
                self._mock_status[f'ventValve{i}Open'] = False
                self._mock_status[f'ventValve{i}Close'] = True
                
            # 清除阀门计时器
            self._valve_timers.clear()
        
        # 关键修复：命令执行后立即更新缓存，确保 GUI 能立即看到变化
        # 不等待下一次轮询周期
        self._cache_mutex.lock()
        self._status_cache = self._mock_status.copy()
        self._cache_mutex.unlock()
        
        # 使用 print 确保能看到调试输出
        print(f"[PRINT DEBUG] MockTangoWorker._execute_command: {cmd_name} 执行成功，"
              f"operationMode={self._mock_status.get('operationMode')}, 已更新缓存")
        logger.info(f"[DEBUG] MockTangoWorker._execute_command: {cmd_name} 执行成功，"
                   f"operationMode={self._mock_status.get('operationMode')}, 已更新缓存")
            
        self.command_result.emit(cmd_name, True, "模拟执行成功")
        
    def _process_auto_sequence(self):
        """处理自动序列流程步进 - 严格按照《真空系统操作全流程及配置规范》执行"""
        state = self._mock_status['systemState']
        step = self._mock_status['autoSequenceStep']
        current_time = time.time()
        
        # 只有在非空闲且非故障状态下才处理
        if state == 0 or state >= 4:
            return
            
        # 控制步进频率 (约1秒一步)
        if current_time - self._last_step_time < 1.0:
            return
            
        self._last_step_time = current_time
        
        # =============================================================================
        # 1. 一键抽真空流程
        # 非真空状态流程（≥3000Pa，步骤1-10）：
        #  步骤1: 开启电磁阀4
        #  步骤2: 开电磁阀123（检测开到位）
        #  步骤3: 开闸板阀123（检测开到位）
        #  步骤4: 启动螺杆泵
        #  步骤5: 等待110Hz稳定
        #  步骤6: 等待<7000Pa，启动罗茨泵
        #  步骤7: 等待<45Pa，启动分子泵123
        #  步骤8: 等待分子泵满转
        #  步骤9: 延时1分钟关闭罗茨泵
        #  步骤10: 流程完成
        #
        # 低真空状态流程（<3000Pa，步骤100-114）：
        #  步骤100: 开启电磁阀4
        #  步骤101: 开放气阀1
        #  步骤102: 平衡至大气压
        #  步骤103: 关放气阀1，启动螺杆泵
        #  步骤104: 等待110Hz
        #  步骤105: 等待<7000Pa，启动罗茨泵
        #  步骤106: 等待<3000Pa，开闸板阀4
        #  步骤107: 开电磁阀123
        #  步骤108: 开闸板阀123
        #  步骤109: 关闸板阀4
        #  步骤110: 等待<45Pa
        #  步骤111: 启动分子泵123
        #  步骤112: 等待分子泵满转
        #  步骤113: 延时1分钟关闭罗茨泵
        #  步骤114: 流程完成
        # =============================================================================
        if state == 1:  # PUMPING
            # ========================================================================
            # 非真空状态流程（≥3000Pa，步骤1-10）
            # ========================================================================
            if step == 1:
                # 步骤1: 开启电磁阀4
                self._mock_status['electromagneticValve4Open'] = True
                self._mock_status['electromagneticValve4Close'] = False
                self._mock_status['autoSequenceStep'] = 2
                
            elif step == 2:
                # 步骤2: 开电磁阀123（检测开到位）- 规范要求：先全部开启
                self._mock_status['electromagneticValve1Open'] = True
                self._mock_status['electromagneticValve1Close'] = False
                self._mock_status['electromagneticValve2Open'] = True
                self._mock_status['electromagneticValve2Close'] = False
                self._mock_status['electromagneticValve3Open'] = True
                self._mock_status['electromagneticValve3Close'] = False
                self._mock_status['autoSequenceStep'] = 3
                
            elif step == 3:
                # 步骤3: 等待电磁阀123全部开到位，开闸板阀123
                if (self._mock_status['electromagneticValve1Open'] and
                    self._mock_status['electromagneticValve2Open'] and
                    self._mock_status['electromagneticValve3Open']):
                    self._mock_status['gateValve1Open'] = True
                    self._mock_status['gateValve1Close'] = False
                    self._mock_status['gateValve2Open'] = True
                    self._mock_status['gateValve2Close'] = False
                    self._mock_status['gateValve3Open'] = True
                    self._mock_status['gateValve3Close'] = False
                    self._mock_status['autoSequenceStep'] = 4
                    
            elif step == 4:
                # 步骤4: 等待闸板阀123全部开到位，启动螺杆泵
                if (self._mock_status['gateValve1Open'] and
                    self._mock_status['gateValve2Open'] and
                    self._mock_status['gateValve3Open']):
                    self._mock_status['screwPumpPower'] = True
                    self._mock_status['autoSequenceStep'] = 5
                    
            elif step == 5:
                # 步骤5: 等待螺杆泵达110Hz稳定
                freq = self._mock_status.get('screwPumpFrequency', 0)
                if freq >= 110:
                    self._mock_status['autoSequenceStep'] = 6
                else:
                    # 模拟频率上升
                    self._mock_status['screwPumpFrequency'] = min(110, freq + 10)
                    
            elif step == 6:
                # 步骤6: 等待真空度<7000Pa，启动罗茨泵
                vacuum3 = self._mock_status.get('vacuumGauge3', 101325)
                if vacuum3 < 7000:
                    self._mock_status['rootsPumpPower'] = True
                    self._mock_status['autoSequenceStep'] = 7
                    
            elif step == 7:
                # 步骤7: 等待真空度<=45Pa，启动分子泵123
                vacuum1 = self._mock_status.get('vacuumGauge1', 101325)
                vacuum2 = self._mock_status.get('vacuumGauge2', 101325)
                if vacuum1 <= 45 and vacuum2 <= 45:
                    for i in range(1, 4):
                        if AUTO_MOLECULAR_PUMP_CONFIG.get(f'molecularPump{i}Enabled', True):
                            self._mock_status[f'molecularPump{i}Power'] = True
                    self._mock_status['autoSequenceStep'] = 8
                    
            elif step == 8:
                # 步骤8: 等待分子泵满转
                all_full_speed = True
                for i in range(1, 4):
                    if self._mock_status[f'molecularPump{i}Power']:
                        speed = self._mock_status[f'molecularPump{i}Speed']
                        if speed < 30000:
                            self._mock_status[f'molecularPump{i}Speed'] = min(30000, speed + 6000)
                            all_full_speed = False
                if all_full_speed:
                    self._mock_status['autoSequenceStep'] = 9
                    self._last_step_time = current_time  # 重置计时器，开始1分钟延时
                    
            elif step == 9:
                # 步骤9: 延时1分钟后关闭罗茨泵
                elapsed = current_time - self._last_step_time
                if elapsed >= 60:  # 1分钟 = 60秒
                    self._mock_status['rootsPumpPower'] = False
                    self._mock_status['rootsPumpFrequency'] = 0
                    self._mock_status['autoSequenceStep'] = 10
                    
            elif step == 10:
                # 步骤10: 流程完成
                self._mock_status['systemState'] = 0  # IDLE
                self._mock_status['autoSequenceStep'] = 0
                
            # ========================================================================
            # 低真空状态流程（<3000Pa，步骤100-114）
            # ========================================================================
            elif step == 100:
                # 步骤100: 开启电磁阀4
                self._mock_status['electromagneticValve4Open'] = True
                self._mock_status['electromagneticValve4Close'] = False
                self._mock_status['autoSequenceStep'] = 101
                
            elif step == 101:
                # 步骤101: 等待电磁阀4到位，开放气阀1
                if self._mock_status['electromagneticValve4Open']:
                    self._mock_status['ventValve1Open'] = True
                    self._mock_status['ventValve1Close'] = False
                    self._mock_status['autoSequenceStep'] = 102
                    
            elif step == 102:
                # 步骤102: 平衡至大气压（等待真空计3≥80000Pa）
                vacuum3 = self._mock_status.get('vacuumGauge3', 101325)
                if vacuum3 >= 80000:
                    self._mock_status['ventValve1Open'] = False
                    self._mock_status['ventValve1Close'] = True
                    self._mock_status['autoSequenceStep'] = 103
                else:
                    # 模拟压力上升
                    if vacuum3 < 101325:
                        self._mock_status['vacuumGauge3'] = min(101325, vacuum3 * 1.2 + 5000)
                    
            elif step == 103:
                # 步骤103: 等待放气阀1关闭，启动螺杆泵
                if self._mock_status['ventValve1Close']:
                    self._mock_status['screwPumpPower'] = True
                    self._mock_status['autoSequenceStep'] = 104
                    
            elif step == 104:
                # 步骤104: 等待螺杆泵达110Hz
                freq = self._mock_status.get('screwPumpFrequency', 0)
                if freq >= 110:
                    self._mock_status['autoSequenceStep'] = 105
                else:
                    self._mock_status['screwPumpFrequency'] = min(110, freq + 10)
                    
            elif step == 105:
                # 步骤105: 等待真空度<7000Pa，启动罗茨泵
                vacuum1 = self._mock_status.get('vacuumGauge1', 101325)
                if vacuum1 < 7000:
                    self._mock_status['rootsPumpPower'] = True
                    self._mock_status['autoSequenceStep'] = 106
                    
            elif step == 106:
                # 步骤106: 等待真空度<3000Pa，开闸板阀4
                vacuum2 = self._mock_status.get('vacuumGauge2', 101325)
                if vacuum2 < 3000:
                    self._mock_status['gateValve4Open'] = True
                    self._mock_status['gateValve4Close'] = False
                    self._mock_status['autoSequenceStep'] = 107
                    
            elif step == 107:
                # 步骤107: 等待闸板阀4开启，开电磁阀123
                if self._mock_status['gateValve4Open']:
                    self._mock_status['electromagneticValve1Open'] = True
                    self._mock_status['electromagneticValve1Close'] = False
                    self._mock_status['electromagneticValve2Open'] = True
                    self._mock_status['electromagneticValve2Close'] = False
                    self._mock_status['electromagneticValve3Open'] = True
                    self._mock_status['electromagneticValve3Close'] = False
                    self._mock_status['autoSequenceStep'] = 108
                    
            elif step == 108:
                # 步骤108: 等待电磁阀123全部开到位，开闸板阀123
                if (self._mock_status['electromagneticValve1Open'] and
                    self._mock_status['electromagneticValve2Open'] and
                    self._mock_status['electromagneticValve3Open']):
                    self._mock_status['gateValve1Open'] = True
                    self._mock_status['gateValve1Close'] = False
                    self._mock_status['gateValve2Open'] = True
                    self._mock_status['gateValve2Close'] = False
                    self._mock_status['gateValve3Open'] = True
                    self._mock_status['gateValve3Close'] = False
                    self._mock_status['autoSequenceStep'] = 109
                    
            elif step == 109:
                # 步骤109: 等待闸板阀123全部开到位，关闸板阀4
                if (self._mock_status['gateValve1Open'] and
                    self._mock_status['gateValve2Open'] and
                    self._mock_status['gateValve3Open']):
                    self._mock_status['gateValve4Open'] = False
                    self._mock_status['gateValve4Close'] = True
                    self._mock_status['autoSequenceStep'] = 110
                    
            elif step == 110:
                # 步骤110: 等待闸板阀4关闭，等待真空度<=45Pa
                if self._mock_status.get('gateValve4Close', True):
                    vacuum1 = self._mock_status.get('vacuumGauge1', 101325)
                    vacuum2 = self._mock_status.get('vacuumGauge2', 101325)
                    if vacuum1 <= 45 and vacuum2 <= 45:
                        self._mock_status['autoSequenceStep'] = 111
                    
            elif step == 111:
                # 步骤111: 启动分子泵123
                for i in range(1, 4):
                    if AUTO_MOLECULAR_PUMP_CONFIG.get(f'molecularPump{i}Enabled', True):
                        self._mock_status[f'molecularPump{i}Power'] = True
                self._mock_status['autoSequenceStep'] = 112
                    
            elif step == 112:
                # 步骤112: 等待分子泵满转
                all_full_speed = True
                for i in range(1, 4):
                    if self._mock_status[f'molecularPump{i}Power']:
                        speed = self._mock_status[f'molecularPump{i}Speed']
                        if speed < 30000:
                            self._mock_status[f'molecularPump{i}Speed'] = min(30000, speed + 6000)
                            all_full_speed = False
                if all_full_speed:
                    self._mock_status['autoSequenceStep'] = 113
                    self._last_step_time = current_time  # 重置计时器，开始1分钟延时
                    
            elif step == 113:
                # 步骤113: 延时1分钟后关闭罗茨泵
                elapsed = current_time - self._last_step_time
                if elapsed >= 60:  # 1分钟 = 60秒
                    self._mock_status['rootsPumpPower'] = False
                    self._mock_status['rootsPumpFrequency'] = 0
                    self._mock_status['autoSequenceStep'] = 114
                    
            elif step == 114:
                # 步骤114: 流程完成
                self._mock_status['systemState'] = 0  # IDLE
                self._mock_status['autoSequenceStep'] = 0
                
        # =============================================================================
        # 2. 一键停机流程 (步骤1-6，与C++代码保持一致)
        # 按照C++代码：停止分子泵1-3 → 等待分子泵停止 → 关闭闸板阀1-3 → 
        #          关闭电磁阀1-3 → 停止罗茨泵 → 停止螺杆泵 → 关闭电磁阀4
        # =============================================================================
        elif state == 2: # STOPPING
            if step == 1:
                # 步骤1: 停止分子泵1-3
                for i in range(1, 4):
                    self._mock_status[f'molecularPump{i}Power'] = False
                self._mock_status['autoSequenceStep'] = 2
            elif step == 2:
                # 步骤2: 等待分子泵停止到0Hz
                any_spinning = False
                for i in range(1, 4):
                    speed = self._mock_status[f'molecularPump{i}Speed']
                    if speed > 0:
                        self._mock_status[f'molecularPump{i}Speed'] = max(0, speed - 10000)
                        any_spinning = True
                if not any_spinning:
                    # 关闭闸板阀1-3
                    for i in range(1, 4):
                        self._mock_status[f'gateValve{i}Open'] = False
                        self._mock_status[f'gateValve{i}Close'] = True
                    self._mock_status['autoSequenceStep'] = 3
            elif step == 3:
                # 步骤3: 等待闸板阀关闭，然后关闭电磁阀1-3
                if all([not self._mock_status[f'gateValve{i}Open'] for i in range(1, 4)]):
                    for i in range(1, 4):
                        self._mock_status[f'electromagneticValve{i}Open'] = False
                        self._mock_status[f'electromagneticValve{i}Close'] = True
                    self._mock_status['autoSequenceStep'] = 4
            elif step == 4:
                # 步骤4: 等待电磁阀关闭，然后停止罗茨泵
                if all([not self._mock_status[f'electromagneticValve{i}Open'] for i in range(1, 4)]):
                    self._mock_status['rootsPumpPower'] = False
                    self._mock_status['rootsPumpFrequency'] = 0
                    self._mock_status['autoSequenceStep'] = 5
            elif step == 5:
                # 步骤5: 停止螺杆泵
                if not self._mock_status['rootsPumpPower']:
                    self._mock_status['screwPumpPower'] = False
                    self._mock_status['screwPumpFrequency'] = 0
                    self._mock_status['autoSequenceStep'] = 6
            elif step == 6:
                # 步骤6: 关闭电磁阀4，停机完成
                if not self._mock_status['screwPumpPower']:
                    self._mock_status['electromagneticValve4Open'] = False
                    self._mock_status['electromagneticValve4Close'] = True
                    self._mock_status['systemState'] = 0  # IDLE
                    self._mock_status['autoSequenceStep'] = 0

        # =============================================================================
        # 3. 腔室放气流程 (步骤1-2，与C++代码保持一致)
        # 按照C++代码：检查闸板阀关闭 → 开启放气阀2 → 等待压力达到大气压 → 关闭放气阀
        # =============================================================================
        elif state == 3: # VENTING
            if step == 1:
                # 步骤1: 检查所有闸板阀是否关闭，然后开启放气阀2
                all_closed = all([not self._mock_status[f'gateValve{i}Open'] for i in range(1, 6)])
                if all_closed:
                    self._mock_status['ventValve2Open'] = True
                    self._mock_status['ventValve2Close'] = False
                    self._mock_status['autoSequenceStep'] = 2
            elif step == 2:
                # 步骤2: 等待压力达到大气压（≥80000Pa），然后关闭放气阀
                vacuum1 = self._mock_status['vacuumGauge1']
                vacuum2 = self._mock_status['vacuumGauge2']
                if vacuum1 >= 80000 and vacuum2 >= 80000:
                    self._mock_status['ventValve2Open'] = False
                    self._mock_status['ventValve2Close'] = True
                    self._mock_status['systemState'] = 0  # IDLE
                    self._mock_status['autoSequenceStep'] = 0
                else:
                    # 模拟压力上升
                    if vacuum1 < 101325:
                        self._mock_status['vacuumGauge1'] = min(101325, vacuum1 * 1.1 + 1000)
                    if vacuum2 < 101325:
                        self._mock_status['vacuumGauge2'] = min(101325, vacuum2 * 1.1 + 1000)

    def _start_auto_sequence(self):
        """启动自动序列模拟"""
        # 现在由 _process_auto_sequence 驱动
        self._last_step_time = time.time()
        self._mock_status['autoSequenceStep'] = 1
