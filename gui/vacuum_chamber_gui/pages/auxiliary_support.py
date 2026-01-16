"""
真空腔体系统控制 - 辅助支撑页面
Auxiliary Support Page

功能：
1. 5组辅助支撑设备的独立控制
2. 夹持/释放操作
3. 上下移动控制
4. 电机状态显示（整合在页面右侧，只读模式）
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QGroupBox, QLabel, QFrame, QScrollArea, QLineEdit, QSplitter,
    QSpacerItem, QSizePolicy
)

from ..utils import show_warning
from PyQt5.QtCore import Qt, QThread, QTimer, QMutex, QMutexLocker, pyqtSignal
from collections import deque
import time
import datetime

try:
    import PyTango
except ImportError:
    PyTango = None

from ..config import AUXILIARY_SUPPORT_CONFIG, DEVICES
from ..widgets import ControlButton, StatusIndicator


class MotorStatusRow:
    """电机状态行辅助类（整合到页面中，只读模式）"""
    def __init__(self, motor_config: dict):
        self.config = motor_config
        # 简化名称显示（去掉设备编号，只保留核心名称）
        name = motor_config["name"]
        if "(" in name:
            name = name.split("(")[0].strip()  # 提取括号前的部分
        self.name_label = QLabel(name)
        
        # 辅助支撑页面：只读模式，不提供输入框和控制按钮
        self.input_edit = None
        self.readonly = True
        
        self.pos_label = QLabel("0.000")
        self.enc_label = QLabel("0.000")
        self.state_label = QLabel("静止")
        self.state_label.setAlignment(Qt.AlignCenter)
        self.switch_label = QLabel("---")
        self.switch_label.setAlignment(Qt.AlignCenter)
        
        # 只读模式，不创建控制按钮
        self.btn_execute = None
        self.btn_stop = None
        self.btn_reset = None


def _ts():
    """返回当前时间戳字符串，精确到毫秒"""
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


class AuxiliarySupportWorker(QThread):
    """辅助支撑设备后台通信线程（支持多个设备实例）
    
    设计模式（参考target_positioning.py的LargeStrokeWorker）：
    - 纯轮询模式，避免高频信号导致UI卡顿
    - 状态数据缓存在Worker中，UI定时拉取
    - 使用QMutex保护共享数据
    - 支持多个设备实例（5个辅助支撑设备）
    - 关键命令白名单：Stop/reset 即使在连接不健康时也允许执行
    """
    # 命令完成信号（低频）
    command_done = pyqtSignal(str, str, bool, str)  # device_id, cmd_name, success, message
    connection_status = pyqtSignal(str, bool, str)  # device_id, connected, message
    
    # 关键命令白名单：这些命令即使在连接不健康时也应该尝试执行
    CRITICAL_COMMANDS = ["Stop", "reset"]
    
    def __init__(self, device_configs: dict, parent=None):
        """
        Args:
            device_configs: {device_id: device_name} 字典，例如：
                {"1": "sys/auxiliary/1", "2": "sys/auxiliary/2", ...}
        """
        super().__init__(parent)
        self.device_configs = device_configs
        self.devices = {}  # {device_id: PyTango.DeviceProxy}
        self._running = True
        self._stop_requested = False
        self._command_queue = deque()  # [(device_id, cmd_name, args), ...]
        self._queue_mutex = QMutex()
        self._read_attrs = [
            "tokenAssistPos",      # 当前位置
            "direPos",             # 指令位置
            "AssistState",         # 运动状态
            "targetForce",         # 目标力值
            "forceValue",          # 实际力传感器值（通过readForce读取）
            "faultState",          # 故障状态
            "AssistLimOrgState",   # 限位/原点状态
        ]
        self._reconnect_interval = 3.0
        self._last_reconnect = {}  # {device_id: last_time}
        
        # 状态数据缓存（UI定时拉取）
        self._status_cache = {}  # {device_id: {attr_name: value, ...}}
        self._status_mutex = QMutex()
        self._cache_updated = False
        
        # 零等待连接检查：由后台轮询更新，命令执行前只读取此标志（不做网络操作）
        self._connection_healthy = {}  # {device_id: bool}
        self._consecutive_failures = {}  # {device_id: int}
        self._max_failures_before_unhealthy = 2  # 连续失败多少次后标记为不健康
        
        # 命令队列保护：断连/超时场景下，避免用户连点堆积导致"延时后连弹很多错误框"
        self._max_queue_size = 3  # 每个设备允许少量排队
        
        # 初始化重连时间记录和连接健康标志
        for device_id in device_configs.keys():
            self._last_reconnect[device_id] = 0.0
            self._connection_healthy[device_id] = False
            self._consecutive_failures[device_id] = 0
    
    def _clear_command_queue(self, reason: str = ""):
        """清空待执行命令队列（线程安全）。"""
        with QMutexLocker(self._queue_mutex):
            dropped = len(self._command_queue)
            self._command_queue.clear()
        if dropped > 0:
            print(f"[AuxiliarySupportWorker {_ts()}] Cleared {dropped} queued commands. reason={reason}", flush=True)
    
    def queue_command(self, device_id: str, cmd_name: str, args=None):
        """将命令加入队列（线程安全）"""
        print(f"[AuxiliarySupportWorker {_ts()}] queue_command: {device_id}.{cmd_name}({args})", flush=True)

        # 如果设备尚未连接（正在重连），不要把命令静默排队：立即反馈失败。
        if device_id not in self.devices or self.devices.get(device_id) is None:
            self._connection_healthy[device_id] = False
            self.command_done.emit(device_id, cmd_name, False, "设备未连接（正在重连），请稍后重试或检查设备服务器/网络")
            return

        # 连接不健康时，直接拒绝排队（避免积压）；由UI层做友好提示
        # 注意：这里不做任何网络操作，只读标志位。
        # Stop/reset 等关键命令即使在连接不健康时也允许入队
        if not self._connection_healthy.get(device_id, False) and cmd_name not in self.CRITICAL_COMMANDS:
            print(f"[AuxiliarySupportWorker {_ts()}] Rejecting command due to unhealthy connection: {device_id}.{cmd_name}", flush=True)
            self.command_done.emit(device_id, cmd_name, False, "设备连接不健康，请等待自动重连或检查设备网络连接")
            return

        with QMutexLocker(self._queue_mutex):
            # 队列限流：超过上限则丢弃新命令（避免连点堆积）
            if len(self._command_queue) >= self._max_queue_size:
                print(
                    f"[AuxiliarySupportWorker {_ts()}] Command queue full (size={len(self._command_queue)}), dropping: {device_id}.{cmd_name}",
                    flush=True,
                )
                self.command_done.emit(device_id, cmd_name, False, "命令队列繁忙，请稍后重试")
                return
            self._command_queue.append((device_id, cmd_name, args))
            queue_size = len(self._command_queue)
            print(f"[AuxiliarySupportWorker {_ts()}] Command queued. Queue size: {queue_size}", flush=True)
    
    def _pop_command(self):
        """从队列取出命令（线程安全）"""
        with QMutexLocker(self._queue_mutex):
            if self._command_queue:
                return self._command_queue.popleft()
            return None
    
    def get_cached_status(self, device_id: str = None):
        """获取缓存的状态数据（由UI定时调用，线程安全）
        
        Args:
            device_id: 如果指定，只返回该设备的状态；否则返回所有设备的状态
        
        Returns:
            dict: {device_id: {attr_name: value, ...}} 或 {attr_name: value, ...}
            如果没有缓存数据则返回 None
        """
        with QMutexLocker(self._status_mutex):
            # 始终返回缓存数据（如果有），不再依赖 _cache_updated 标志
            # 这样多个定时器可以同时获取数据
            if not self._status_cache:
                return None  # 缓存为空
            if device_id:
                return self._status_cache.get(device_id, {}).copy()
            return {k: v.copy() for k, v in self._status_cache.items()}
    
    def _update_cache(self, device_id: str, data: dict):
        """更新状态缓存（由后台线程调用）"""
        with QMutexLocker(self._status_mutex):
            if device_id not in self._status_cache:
                self._status_cache[device_id] = {}
            self._status_cache[device_id].update(data)
            self._cache_updated = True
    
    def stop(self):
        """停止线程（从主线程调用）"""
        print(f"[AuxiliarySupportWorker {_ts()}] Requesting stop...", flush=True)
        self._stop_requested = True
        self._running = False
        
        # 等待线程结束（最多5秒）
        if not self.wait(5000):
            print(f"[AuxiliarySupportWorker {_ts()}] Warning: Thread did not stop in time, terminating...", flush=True)
            self.terminate()
            self.wait(1000)
        
        print(f"[AuxiliarySupportWorker {_ts()}] Worker stopped.", flush=True)
    
    def _poll_attributes(self, device_id: str, device):
        """轮询模式：批量读取属性，更新缓存，同时更新连接健康标志"""
        if not device or not self._read_attrs:
            return
        
        if self._stop_requested:
            return
        
        # 初始化失败计数器（用于减少重复的失败日志）
        if not hasattr(self, '_fail_counters'):
            self._fail_counters = {}
        
        try:
            # 设置适当的超时时间（2000ms），因为属性读取可能涉及多个网络调用（编码器、力传感器等）
            old_timeout = device.get_timeout_millis()
            device.set_timeout_millis(2000)
            
            try:
                # 使用批量读取
                attrs = device.read_attributes(self._read_attrs)
            finally:
                device.set_timeout_millis(old_timeout)
            
            status_data = {}
            successful_attrs = 0
            for attr in attrs:
                try:
                    if not attr.has_failed:
                        status_data[attr.name] = attr.value
                        successful_attrs += 1
                        # 成功读取时，重置失败计数器
                        key = f"{device_id}.{attr.name}"
                        if key in self._fail_counters:
                            del self._fail_counters[key]
                    else:
                        # 失败时，使用计数器，每100次才输出一次
                        key = f"{device_id}.{attr.name}"
                        if key not in self._fail_counters:
                            self._fail_counters[key] = 0
                        self._fail_counters[key] += 1
                        if self._fail_counters[key] % 100 == 1:
                            # 尝试获取详细错误信息
                            err_msg = ""
                            try:
                                err_stack = attr.get_err_stack()
                                if err_stack and len(err_stack) > 0:
                                    err_msg = f" - {err_stack[0].reason}: {err_stack[0].desc}"
                            except Exception:
                                pass
                            print(f"[AuxiliarySupportWorker {_ts()}] Poll: {device_id}.{attr.name} FAILED (count={self._fail_counters[key]}){err_msg}", flush=True)
                except Exception as e:
                    # 异常也使用计数器
                    key = f"{device_id}.{attr.name}"
                    if key not in self._fail_counters:
                        self._fail_counters[key] = 0
                    self._fail_counters[key] += 1
                    if self._fail_counters[key] % 100 == 1:
                        print(f"[AuxiliarySupportWorker {_ts()}] Poll: Error reading {device_id}.{attr.name}: {e} (count={self._fail_counters[key]})", flush=True)
            
            if status_data:
                self._update_cache(device_id, status_data)

            # 连接健康判定：至少有一个属性读成功才算“数据可用”。
            if successful_attrs == 0:
                self._consecutive_failures[device_id] = self._consecutive_failures.get(device_id, 0) + 1
                if self._consecutive_failures[device_id] >= self._max_failures_before_unhealthy:
                    if self._connection_healthy.get(device_id, False):
                        print(
                            f"[AuxiliarySupportWorker {_ts()}] Connection became unhealthy for {device_id} (no attributes updated, failures={self._consecutive_failures[device_id]})",
                            flush=True,
                        )
                    self._connection_healthy[device_id] = False
            else:
                self._consecutive_failures[device_id] = 0
                if not self._connection_healthy.get(device_id, False):
                    print(f"[AuxiliarySupportWorker {_ts()}] Connection became healthy for {device_id}", flush=True)
                self._connection_healthy[device_id] = True
        except Exception as e:
            # 轮询失败，增加失败计数
            self._consecutive_failures[device_id] = self._consecutive_failures.get(device_id, 0) + 1
            if self._consecutive_failures[device_id] >= self._max_failures_before_unhealthy:
                if self._connection_healthy.get(device_id, False):
                    print(f"[AuxiliarySupportWorker {_ts()}] Connection became unhealthy for {device_id} after {self._consecutive_failures[device_id]} failures: {e}", flush=True)
                    self._connection_healthy[device_id] = False
            
            # 只在首次失败时输出详细日志
            if self._consecutive_failures[device_id] == 1:
                print(f"[AuxiliarySupportWorker {_ts()}] Poll failed for {device_id}: {e}", flush=True)
                import traceback
                traceback.print_exc()
            
            self.devices[device_id] = None
    
    def _attempt_connect(self, device_id: str, device_name: str):
        """尝试连接设备，失败不抛异常"""
        if self._stop_requested:
            return False
        
        try:
            print(f"[AuxiliarySupportWorker {_ts()}] Connecting to {device_name}...", flush=True)
            dev = PyTango.DeviceProxy(device_name)
            # 使用较短的超时（1秒）加快失败检测
            dev.set_timeout_millis(1000)
            state = dev.state()
            
            if self._stop_requested:
                return False
            
            self.devices[device_id] = dev
            # 连接成功：能连上 server 不代表设备已完全就绪，初始标记为健康
            self._connection_healthy[device_id] = state in (PyTango.DevState.ON, PyTango.DevState.MOVING)
            self._consecutive_failures[device_id] = 0
            
            status_msg = f"Connected (state: {state})"
            print(f"[AuxiliarySupportWorker {_ts()}] Connected {device_id}: {status_msg}", flush=True)
            self.connection_status.emit(device_id, True, status_msg)
            
            # 首次轮询填充缓存
            if self._read_attrs:
                self._poll_attributes(device_id, dev)
            return True
        except Exception as e:
            self.devices[device_id] = None
            # 连接失败，标记连接不健康
            self._connection_healthy[device_id] = False
            
            error_msg = str(e)
            print(f"[AuxiliarySupportWorker {_ts()}] Connection failed for {device_id}: {error_msg}", flush=True)
            import traceback
            traceback.print_exc()
            self.connection_status.emit(device_id, False, error_msg)
            return False
    
    def run(self):
        """主循环：纯轮询模式"""
        if not PyTango:
            print(f"[AuxiliarySupportWorker {_ts()}] PyTango not available", flush=True)
            for device_id in self.device_configs.keys():
                self.connection_status.emit(device_id, False, "PyTango not installed")
            return
        
        # 初始化重连时间
        for device_id in self.device_configs.keys():
            self._last_reconnect[device_id] = time.monotonic() - self._reconnect_interval
        
        poll_interval_ms = 500  # 轮询间隔500ms
        
        while self._running and not self._stop_requested:
            # 尝试重连所有未连接的设备
            for device_id, device_name in self.device_configs.items():
                if device_id not in self.devices or self.devices[device_id] is None:
                    now = time.monotonic()
                    if now - self._last_reconnect[device_id] >= self._reconnect_interval:
                        self._last_reconnect[device_id] = now
                        self._attempt_connect(device_id, device_name)
                    self.msleep(100)  # 避免频繁重连
                    continue
            
            # 1. 处理命令队列（高优先级）
            cmd = self._pop_command()
            while cmd is not None:
                device_id, cmd_name, args = cmd
                self._execute_command(device_id, cmd_name, args)
                if self._stop_requested:
                    break
                cmd = self._pop_command()
            
            if self._stop_requested:
                break
            
            # 2. 轮询所有已连接设备的属性
            for device_id, device in list(self.devices.items()):
                if device:
                    self._poll_attributes(device_id, device)
            
            # 3. 等待下次轮询
            for _ in range(poll_interval_ms // 50):  # 50ms为单位
                if self._stop_requested:
                    break
                with QMutexLocker(self._queue_mutex):
                    has_cmd = len(self._command_queue) > 0
                if has_cmd:
                    break
                self.msleep(50)
        
        print(f"[AuxiliarySupportWorker {_ts()}] Worker thread exiting.", flush=True)
    
    def _execute_command(self, device_id: str, cmd_name: str, args):
        """执行Tango命令（带零等待连接检查）"""
        # 关键命令白名单：Stop/reset 即使在连接不健康时也应该尝试执行
        is_critical = cmd_name in self.CRITICAL_COMMANDS
        
        device = self.devices.get(device_id)
        if not device:
            error_msg = "Device not connected"
            print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: {device_id}.{cmd_name} FAILED - {error_msg}", flush=True)
            self.command_done.emit(device_id, cmd_name, False, error_msg)
            return
        
        if self._stop_requested:
            error_msg = "Worker stopping"
            print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: {device_id}.{cmd_name} CANCELLED - {error_msg}", flush=True)
            self.command_done.emit(device_id, cmd_name, False, error_msg)
            return
        
        # 零等待连接检查：检查后台更新的连接健康标志（纯内存操作，不做网络调用）
        # 注意：Stop/reset 等关键命令跳过此检查，允许在连接不健康时尝试执行
        if not self._connection_healthy.get(device_id, False) and not is_critical:
            error_msg = "设备连接不健康，请等待自动重连或手动重启设备服务器"
            print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: {device_id}.{cmd_name} FAILED - Connection unhealthy (zero-wait check)", flush=True)
            self.command_done.emit(device_id, cmd_name, False, error_msg)
            return
        
        try:
            print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: Starting {device_id}.{cmd_name}({args})", flush=True)
            
            # 设置命令超时（1秒，之前是3秒，缩短以加快失败检测）
            old_timeout = device.get_timeout_millis()
            device.set_timeout_millis(1000)
            print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: Timeout set to 1000ms (was {old_timeout}ms)", flush=True)
            
            try:
                if args is not None and args != [] and args != ():
                    result = device.command_inout(cmd_name, args)
                else:
                    result = device.command_inout(cmd_name)
            finally:
                device.set_timeout_millis(old_timeout)
                print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: Timeout restored to {old_timeout}ms", flush=True)
            
            print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: {device_id}.{cmd_name} SUCCEEDED, result={result}", flush=True)
            self.command_done.emit(device_id, cmd_name, True, str(result))
        except Exception as e:
            error_msg = str(e)
            print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: {device_id}.{cmd_name} FAILED - {error_msg}", flush=True)
            import traceback
            traceback.print_exc()
            self.command_done.emit(device_id, cmd_name, False, error_msg)
            # 如果是超时错误，标记连接不健康并清理设备
            if "timeout" in error_msg.lower():
                print(f"[AuxiliarySupportWorker {_ts()}] _execute_command: Timeout detected, marking device for reconnect", flush=True)
                self.devices[device_id] = None
                self._connection_healthy[device_id] = False
                # 发生超时后，清空后续排队命令，避免延时后连续弹出很多失败提示
                self._clear_command_queue(reason=f"timeout for {device_id}")
            # 网络/硬件断开类错误：清空排队，避免错误提示堆积
            elif "not connected" in error_msg.lower() or "connection" in error_msg.lower():
                self._connection_healthy[device_id] = False
                self._clear_command_queue(reason=f"connection error for {device_id}")


class AuxiliaryGroupControl(QGroupBox):
    """单个辅助支撑组控制面板"""
    
    def __init__(self, group_config: dict, worker=None, parent=None):
        super().__init__(group_config["name"], parent)
        self.config = group_config
        self.device_id = group_config["id"]
        self._worker = worker
        self._setup_ui()
        self._setup_connections()
        
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setSpacing(16)
        layout.setContentsMargins(16, 24, 16, 16)
        
        # 1. 参数显示区
        info_container = QWidget()
        info_container.setStyleSheet("background-color: #0b1621; border-radius: 4px; padding: 8px;")
        info_layout = QGridLayout(info_container)
        info_layout.setSpacing(8)
        
        # 位置
        info_layout.addWidget(QLabel("当前位置:"), 0, 0)
        self.pos_value = QLabel("0.000 mm")
        self.pos_value.setStyleSheet("color: #29d6ff; font-weight: bold; font-size: 14px;")
        self.pos_value.setAlignment(Qt.AlignRight)
        info_layout.addWidget(self.pos_value, 0, 1)
        
        # 状态
        info_layout.addWidget(QLabel("运动状态:"), 1, 0)
        self.state_value = QLabel("静止")
        self.state_value.setStyleSheet("color: #8fa6c5;")
        self.state_value.setAlignment(Qt.AlignRight)
        info_layout.addWidget(self.state_value, 1, 1)
        
        # 力值
        info_layout.addWidget(QLabel("受力数值:"), 2, 0)
        self.force_value = QLabel("0.0 N")
        self.force_value.setStyleSheet("color: #39e072; font-weight: bold;")
        self.force_value.setAlignment(Qt.AlignRight)
        info_layout.addWidget(self.force_value, 2, 1)
        
        layout.addWidget(info_container)
        
        # 2. 行程输入区
        input_container = QWidget()
        input_layout = QHBoxLayout(input_container)
        input_layout.setContentsMargins(0, 0, 0, 0)
        
        input_label = QLabel("运动行程:")
        self.input_stroke = QLineEdit()
        self.input_stroke.setPlaceholderText("0.00")
        self.input_stroke.setAlignment(Qt.AlignCenter)
        self.input_stroke.setStyleSheet("""
            QLineEdit {
                background-color: #0c1724;
                border: 1px solid #1c3146;
                color: white;
                padding: 6px;
                border-radius: 4px;
            }
            QLineEdit:focus {
                border: 1px solid #00a0e9;
            }
        """)
        
        unit_label = QLabel("mm")
        unit_label.setStyleSheet("color: #8fa6c5;")
        
        input_layout.addWidget(input_label)
        input_layout.addWidget(self.input_stroke)
        input_layout.addWidget(unit_label)
        
        layout.addWidget(input_container)
        
        # 3. 操作按钮区
        btn_layout = QHBoxLayout()
        btn_layout.setSpacing(8)
        
        self.btn_exec = ControlButton("相对运动")
        self.btn_exec.setStyleSheet("background-color: #00a0e9; border: none;")
        
        self.btn_stop = ControlButton("停止", role="stop")
        self.btn_stop.setStyleSheet("background-color: #e74c3c; border: none;")
        
        self.btn_reset = ControlButton("复位")
        self.btn_reset.setStyleSheet("background-color: #2c3e50; border: none;")
        
        btn_layout.addWidget(self.btn_exec)
        btn_layout.addWidget(self.btn_stop)
        btn_layout.addWidget(self.btn_reset)
        
        layout.addLayout(btn_layout)
        layout.addStretch()
    
    def set_worker(self, worker):
        """设置Worker（延迟设置）"""
        self._worker = worker
        self._setup_connections()
    
    def _setup_connections(self):
        """连接按钮信号到Tango命令"""
        if not self._worker:
            return
        
        # 先断开旧连接，避免信号重复连接导致命令多次执行
        try:
            self.btn_exec.clicked.disconnect()
        except TypeError:
            pass  # 首次连接时无需断开
        try:
            self.btn_stop.clicked.disconnect()
        except TypeError:
            pass
        try:
            self.btn_reset.clicked.disconnect()
        except TypeError:
            pass
        
        # 执行按钮：相对运动（使用行程输入）
        self.btn_exec.clicked.connect(self._on_execute)
        # 停止按钮
        self.btn_stop.clicked.connect(self._on_stop)
        # 复位按钮
        self.btn_reset.clicked.connect(self._on_reset)
    
    def _on_execute(self):
        """执行运动（相对运动）"""
        if not self._worker:
            show_warning("错误", "设备未连接")
            return
        
        try:
            stroke_text = self.input_stroke.text() or "0"
            distance = float(stroke_text)
            self._worker.queue_command(self.device_id, "moveRelative", distance)
        except ValueError:
            show_warning("输入错误", "请输入有效的数字")
    
    def _on_stop(self):
        """停止运动"""
        if not self._worker:
            return
        # 注意：辅助支撑设备的停止命令是 "Stop" (大写S)
        self._worker.queue_command(self.device_id, "Stop")
    
    def _on_reset(self):
        """复位（清除报警状态）"""
        if not self._worker:
            return
        self._worker.queue_command(self.device_id, "reset")
    
    def update_status(self, status_data: dict):
        """更新状态显示"""
        if not status_data:
            return
        
        # 更新当前位置
        if "tokenAssistPos" in status_data:
            pos = status_data["tokenAssistPos"]
            self.pos_value.setText(f"{pos:.3f} mm")
        
        # 更新运动状态（优先显示故障状态）
        fault_state = status_data.get("faultState", "NORMAL")
        is_moving = bool(status_data.get("AssistState", False))
        
        if fault_state and fault_state != "NORMAL":
            # 故障状态优先显示
            self.state_value.setText(f"故障: {fault_state}")
            self.state_value.setStyleSheet("color: #e74c3c; font-weight: bold;")
        elif is_moving:
            self.state_value.setText("运动中")
            self.state_value.setStyleSheet("color: #FFA500;")
        else:
            self.state_value.setText("静止")
            self.state_value.setStyleSheet("color: #8fa6c5;")
        
        # 更新力值（优先显示实际力传感器值，如果没有则显示目标力值）
        if "forceValue" in status_data:
            force = status_data["forceValue"]
            self.force_value.setText(f"{force:.1f} N")
        elif "targetForce" in status_data:
            force = status_data["targetForce"]
            self.force_value.setText(f"{force:.1f} N (目标)")


class AuxiliarySupportPage(QWidget):
    """辅助支撑主页面"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self._worker = None
        self._status_timer = None
        self._worker_started = False  # 标记 Worker 是否已启动
        self._setup_ui()
        # 注意：不在初始化时启动 Worker，等待外部调用 start_worker()
    
    def start_worker(self):
        """启动 Worker（延迟启动，由 MainWindow 调用）"""
        if self._worker_started:
            return
        self._worker_started = True
        self._setup_worker()
        
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setSpacing(20)
        layout.setContentsMargins(20, 20, 20, 20)
        
        # 顶部标题
        header_container = QWidget()
        header_layout = QHBoxLayout(header_container)
        header_layout.setContentsMargins(0, 0, 0, 0)
        
        header = QLabel("辅助支撑控制")
        header.setProperty("role", "title")
        header_layout.addWidget(header)
        header_layout.addStretch()
        
        layout.addWidget(header_container)
        
        # 使用水平分割器：左侧是功能区域，右侧是状态面板
        main_splitter = QSplitter(Qt.Horizontal)
        main_splitter.setHandleWidth(1)
        main_splitter.setStyleSheet("QSplitter::handle { background-color: #1c3146; }")
        
        # 左侧：使用滚动区域容纳所有组
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setStyleSheet("background: transparent;")
        
        content = QWidget()
        content.setStyleSheet("background: transparent;")
        grid = QGridLayout(content)
        grid.setSpacing(20)
        grid.setContentsMargins(0, 0, 0, 0)
        
        self.groups = {}
        group_configs = AUXILIARY_SUPPORT_CONFIG["groups"]
        
        # 自动网格布局：每行2个（5组时更均衡，减少右侧大空白）
        cols = 2
        for i, config in enumerate(group_configs):
            row = i // cols
            col = i % cols
            
            group_control = AuxiliaryGroupControl(config)
            grid.addWidget(group_control, row, col)
            self.groups[config["id"]] = group_control
            
        # 填充空白单元格以保持布局整齐
        total_items = len(group_configs)
        if total_items % cols != 0:
            spacer = QWidget()
            spacer.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
            grid.addWidget(spacer, total_items // cols, total_items % cols)
            
        scroll.setWidget(content)
        main_splitter.addWidget(scroll)
        
        # 右侧：状态面板（整合到页面中）
        self._setup_status_panel()
        main_splitter.addWidget(self.status_panel_widget)
        
        # 设置右侧状态面板宽度：初始宽度来自配置，但允许用户拖动分割条微调
        from ..config import UI_SETTINGS
        page_key = "auxiliary_support"
        status_panel_width = (
            UI_SETTINGS.get("status_panel_widths", {}).get(page_key)
            or UI_SETTINGS.get("status_panel_width", 400)
        )
        self.status_panel_widget.setMinimumWidth(300)
        self.status_panel_widget.setMaximumWidth(700)
        
        # 设置分割器比例（左侧自适应，右侧固定宽度）
        main_splitter.setStretchFactor(0, 1)
        main_splitter.setStretchFactor(1, 0)
        main_splitter.setCollapsible(1, True)

        # 设置初始分割尺寸（右侧按配置值）
        QTimer.singleShot(0, lambda: main_splitter.setSizes([1200, status_panel_width]))
        
        # 设置分割器尺寸策略
        main_splitter.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        
        layout.addWidget(main_splitter, 1)  # 添加拉伸因子
    
    def _setup_worker(self):
        """设置Tango Worker"""
        print(f"[AuxiliarySupportPage {_ts()}] _setup_worker() called", flush=True)
        if not PyTango:
            print(f"[AuxiliarySupportPage {_ts()}] PyTango not available", flush=True)
            return
        
        # 构建设备配置字典
        device_configs = {}
        for group_config in AUXILIARY_SUPPORT_CONFIG["groups"]:
            device_id = group_config["id"]
            device_name = group_config.get("device", DEVICES.get(f"auxiliary_{device_id}", ""))
            if device_name:
                device_configs[device_id] = device_name
        
        if not device_configs:
            print(f"[AuxiliarySupportPage {_ts()}] No device configs found", flush=True)
            return
        
        print(f"[AuxiliarySupportPage {_ts()}] Creating worker for {len(device_configs)} devices", flush=True)
        self._worker = AuxiliarySupportWorker(device_configs, self)
        
        # 连接信号
        self._worker.command_done.connect(self._on_command_done)
        self._worker.connection_status.connect(self._on_connection_status)
        
        # 设置Worker到控件
        for group_id, group_control in self.groups.items():
            group_control.set_worker(self._worker)
        
        # 启动Worker
        self._worker.start()
        
        # 创建状态更新定时器（500ms间隔）
        self._status_timer = QTimer(self)
        self._status_timer.timeout.connect(self._update_status)
        self._status_timer.start(500)
        
        # 初始化电机状态列表（如果状态面板已创建）
        if hasattr(self, 'motor_list_layout'):
            self._init_motor_list()
            
            # 创建电机状态更新定时器
            self._motor_status_timer = QTimer(self)
            self._motor_status_timer.timeout.connect(self._update_motor_status)
            self._motor_status_timer.start(500)  # 500ms更新一次
        
        print(f"[AuxiliarySupportPage {_ts()}] Worker setup completed", flush=True)
    
    def _setup_status_panel(self):
        """创建并设置状态面板（整合到页面中，只读模式）"""
        # 状态面板容器
        self.status_panel_widget = QFrame()
        self.status_panel_widget.setProperty("role", "status_panel")
        status_layout = QVBoxLayout(self.status_panel_widget)
        status_layout.setSpacing(16)
        status_layout.setContentsMargins(16, 16, 16, 16)
        
        # 标题
        status_title = QLabel("电机状态")
        status_title.setProperty("role", "title")
        # 与靶定位右侧状态栏保持一致：标题与表格左内边距对齐
        status_title.setStyleSheet("font-size: 16px; font-weight: bold; color: #00a0e9; margin-bottom: 10px; padding-left: 8px;")
        status_layout.addWidget(status_title)

        # 固定表头区域（只随水平滚动同步）
        self.motor_header_scroll = QScrollArea()
        self.motor_header_scroll.setWidgetResizable(True)
        self.motor_header_scroll.setFrameShape(QFrame.NoFrame)
        self.motor_header_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.motor_header_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.motor_header_scroll.setStyleSheet("QScrollArea { background: transparent; }")

        self.motor_header_container = QWidget()
        self.motor_header_layout = QGridLayout(self.motor_header_container)
        self.motor_header_layout.setSpacing(6)
        self.motor_header_layout.setVerticalSpacing(0)
        # 更紧凑：减少左右内边距，避免触发水平滚动
        self.motor_header_layout.setContentsMargins(4, 0, 4, 0)
        self.motor_header_layout.setColumnStretch(0, 2)
        self.motor_header_layout.setColumnStretch(1, 1)
        self.motor_header_layout.setColumnStretch(2, 1)
        self.motor_header_layout.setColumnStretch(3, 1)
        self.motor_header_layout.setColumnStretch(4, 1)
        # 更紧凑：降低列最小宽度，默认面板宽度下尽量不出现水平滚动条
        self.motor_header_layout.setColumnMinimumWidth(0, 95)
        self.motor_header_layout.setColumnMinimumWidth(1, 60)
        self.motor_header_layout.setColumnMinimumWidth(2, 60)
        self.motor_header_layout.setColumnMinimumWidth(3, 70)
        self.motor_header_layout.setColumnMinimumWidth(4, 70)

        self.motor_header_scroll.setWidget(self.motor_header_container)
        self.motor_header_scroll.setFixedHeight(40)
        status_layout.addWidget(self.motor_header_scroll)
        
        # 电机状态内容区（只滚动内容）
        self.motor_body_scroll = QScrollArea()
        self.motor_body_scroll.setWidgetResizable(True)
        self.motor_body_scroll.setFrameShape(QFrame.NoFrame)
        self.motor_body_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.motor_body_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.motor_body_scroll.setStyleSheet("QScrollArea { background: transparent; }")
        
        self.motor_list_container = QWidget()
        self.motor_list_layout = QGridLayout(self.motor_list_container)
        self.motor_list_layout.setSpacing(6)
        self.motor_list_layout.setVerticalSpacing(10)
        # 更紧凑：减少左右内边距，避免触发水平滚动
        self.motor_list_layout.setContentsMargins(4, 8, 4, 8)
        self.motor_list_layout.setColumnStretch(0, 2)  # 名称列
        self.motor_list_layout.setColumnStretch(1, 1)  # 位置列
        self.motor_list_layout.setColumnStretch(2, 1)  # 编码器列
        self.motor_list_layout.setColumnStretch(3, 1)  # 运动状态列
        self.motor_list_layout.setColumnStretch(4, 1)  # 开关状态列
        # 更紧凑：降低列最小宽度，默认面板宽度下尽量不出现水平滚动条
        self.motor_list_layout.setColumnMinimumWidth(0, 95)
        self.motor_list_layout.setColumnMinimumWidth(1, 60)
        self.motor_list_layout.setColumnMinimumWidth(2, 60)
        self.motor_list_layout.setColumnMinimumWidth(3, 70)
        self.motor_list_layout.setColumnMinimumWidth(4, 70)
        self.motor_rows = []
        
        self.motor_body_scroll.setWidget(self.motor_list_container)
        status_layout.addWidget(self.motor_body_scroll, 1)  # 添加拉伸因子

        # 水平滚动同步：内容区滚动时，表头同步滚动（表头固定不随垂直滚动）
        self.motor_body_scroll.horizontalScrollBar().valueChanged.connect(
            self.motor_header_scroll.horizontalScrollBar().setValue
        )
        
        # 立即初始化电机列表（不等待Worker启动）
        self._init_motor_list()
    
    def _init_motor_list(self):
        """初始化电机列表（只读模式）"""
        motors = AUXILIARY_SUPPORT_CONFIG.get("status_motors", [])
        if not motors:
            return
        
        # 清除旧内容（表头+内容）
        if hasattr(self, 'motor_header_layout'):
            while self.motor_header_layout.count():
                item = self.motor_header_layout.takeAt(0)
                widget = item.widget()
                if widget:
                    widget.deleteLater()

        while self.motor_list_layout.count():
            item = self.motor_list_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()
        
        self.motor_rows.clear()
        
        # 固定表头（只读模式：不显示输入框和控制列）
        headers = ["名称", "位置(mm)", "编码器", "运动状态", "开关状态"]
        for col, text in enumerate(headers):
            label = QLabel(text)
            label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
            label.setFixedHeight(34)
            label.setStyleSheet("""
                color: #00a0e9; 
                font-weight: bold; 
                font-size: 12px;
                padding: 6px 4px;
                background-color: #0c1724;
                border-bottom: 1px solid #1c3146;
            """)
            label.setAlignment(Qt.AlignCenter)
            if hasattr(self, 'motor_header_layout'):
                self.motor_header_layout.addWidget(label, 0, col, alignment=Qt.AlignCenter)
        
        # 生成新电机行（使用简化的MotorStatusRow，只读模式）
        for row_idx, motor in enumerate(motors, 0):
            row_obj = MotorStatusRow(motor)
            self.motor_rows.append(row_obj)
            
            # 只读模式：不添加输入框和控制按钮（样式与靶定位保持一致）
            row_obj.name_label.setStyleSheet("color: #8fa6c5;")
            row_obj.name_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
            
            row_obj.pos_label.setStyleSheet("color: #90EE90;")
            row_obj.enc_label.setStyleSheet("color: #90EE90;")
            
            row_obj.state_label.setStyleSheet("background-color: #1c3146; border-radius: 4px; padding: 2px; min-width: 40px;")
            
            self.motor_list_layout.addWidget(row_obj.name_label, row_idx, 0)
            self.motor_list_layout.addWidget(row_obj.pos_label, row_idx, 1, alignment=Qt.AlignCenter)
            self.motor_list_layout.addWidget(row_obj.enc_label, row_idx, 2, alignment=Qt.AlignCenter)
            self.motor_list_layout.addWidget(row_obj.state_label, row_idx, 3, alignment=Qt.AlignCenter)
            self.motor_list_layout.addWidget(row_obj.switch_label, row_idx, 4, alignment=Qt.AlignCenter)

        # 吸收滚动区域额外高度，避免表头/行被拉伸成“大空白块”
        spacer_row = len(motors)
        self.motor_list_layout.addItem(
            QSpacerItem(0, 0, QSizePolicy.Minimum, QSizePolicy.Expanding),
            spacer_row,
            0,
            1,
            len(headers),
        )
        self.motor_list_layout.setRowStretch(spacer_row, 1)
    
    def _update_motor_status(self):
        """定时更新电机状态（从Worker缓存拉取）"""
        if not self.motor_rows or not self._worker:
            return
        
        # 获取所有设备的状态
        all_status = self._worker.get_cached_status() if hasattr(self._worker, 'get_cached_status') else None
        if not all_status:
            return
        
        # 遍历所有电机行，更新状态
        for row in self.motor_rows:
            motor_config = row.config
            # 配置中 device 是 "auxiliary_1" 等，Worker 缓存键是 "1" 等
            # 需要提取数字部分作为键
            device_config_name = motor_config.get("device", "")
            # 从 "auxiliary_1" 提取 "1"
            if device_config_name.startswith("auxiliary_"):
                device_id = device_config_name.replace("auxiliary_", "")
            else:
                device_id = device_config_name
            
            # 从所有设备状态中获取对应设备的状态
            if device_id in all_status:
                status_data = all_status[device_id]
                
                # 更新位置显示
                if "tokenAssistPos" in status_data:
                    pos = status_data["tokenAssistPos"]
                    row.pos_label.setText(f"{pos:.3f}")
                    # 编码器位置使用位置值
                    row.enc_label.setText(f"{pos:.3f}")
                
                # 更新运动状态
                if "AssistState" in status_data:
                    is_moving = bool(status_data["AssistState"])
                    row.state_label.setText("运动中" if is_moving else "静止")
    
    def _update_status(self):
        """定时更新状态（从Worker缓存拉取）"""
        if not self._worker:
            return
        
        # 获取所有设备的状态
        all_status = self._worker.get_cached_status()
        if all_status:
            for device_id, status_data in all_status.items():
                if device_id in self.groups:
                    self.groups[device_id].update_status(status_data)
    
    def _on_command_done(self, device_id: str, cmd_name: str, success: bool, message: str):
        """命令完成回调"""
        status_str = "SUCCEEDED" if success else "FAILED"
        print(f"[AuxiliarySupportPage {_ts()}] _on_command_done: {device_id}.{cmd_name} {status_str}: {message}", flush=True)
        if not success:
            show_warning("命令执行失败", 
                              f"设备: {device_id}\n"
                              f"命令: {cmd_name}\n"
                              f"错误: {message}")
    
    def _on_connection_status(self, device_id: str, connected: bool, message: str):
        """连接状态回调"""
        status_str = "CONNECTED" if connected else "FAILED"
        print(f"[AuxiliarySupportPage {_ts()}] _on_connection_status: {device_id} {status_str}: {message}", flush=True)
        
    def get_worker(self):
        """获取 Worker（供状态面板使用）"""
        return self._worker
    
    def cleanup(self):
        """清理资源"""
        print(f"[AuxiliarySupportPage {_ts()}] cleanup() called", flush=True)
        # 停止状态更新定时器
        if self._status_timer:
            self._status_timer.stop()
            self._status_timer = None
        
        # 停止电机状态更新定时器
        if hasattr(self, '_motor_status_timer') and self._motor_status_timer:
            self._motor_status_timer.stop()
            self._motor_status_timer = None
        
        # 停止Worker线程
        if self._worker:
            self._worker.stop()
            self._worker = None
        print(f"[AuxiliarySupportPage {_ts()}] cleanup: Completed", flush=True)
