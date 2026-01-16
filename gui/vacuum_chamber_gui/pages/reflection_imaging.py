"""
真空腔体系统控制 - 反射光成像页面
Reflection Imaging Page

功能：
1. 双 Tab 页设计：配置控制页 / 反射光成像显示页
2. 三坐标控制模块 (上平台/下平台)
3. 开关控制模块 (环形光/CCD)
4. 电机状态显示（整合在页面右侧，只读模式）
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QGroupBox, QLabel, QFrame, QScrollArea, QTabWidget,
    QLineEdit, QPushButton, QFileDialog, QTableWidget,
    QTableWidgetItem, QHeaderView, QListWidget, QFormLayout,
    QComboBox, QSpinBox, QDoubleSpinBox, QSplitter,
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

from ..config import REFLECTION_IMAGING_CONFIG, DEVICES
from ..widgets import ControlButton


class MotorStatusRow:
    """电机状态行辅助类（整合到页面中，只读模式）"""
    def __init__(self, motor_config: dict):
        self.config = motor_config
        self.name_label = QLabel(motor_config["name"])
        self.name_label.setStyleSheet("color: #8fa6c5;")
        
        # 反射光成像页面：只读模式，不提供输入框和控制按钮
        self.input_edit = None
        self.readonly = True
        
        self.pos_label = QLabel("0.000")
        self.pos_label.setStyleSheet("color: #90EE90;") # 绿色文字
        
        self.enc_label = QLabel("0.000")
        self.enc_label.setStyleSheet("color: #90EE90;")
        
        self.state_label = QLabel("静止")
        self.state_label.setAlignment(Qt.AlignCenter)
        self.state_label.setStyleSheet("background-color: #1c3146; border-radius: 4px; padding: 2px; min-width: 40px;")
        
        self.switch_label = QLabel("---")
        self.switch_label.setAlignment(Qt.AlignCenter)
        
        # 只读模式，不创建控制按钮
        self.btn_execute = None
        self.btn_stop = None
        self.btn_reset = None


def _ts():
    """返回当前时间戳字符串，精确到毫秒"""
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


class ReflectionImagingWorker(QThread):
    """反射光成像设备后台通信线程
    
    设计模式（参考target_positioning.py的LargeStrokeWorker）：
    - 纯轮询模式，避免高频信号导致UI卡顿
    - 状态数据缓存在Worker中，UI定时拉取
    - 使用QMutex保护共享数据
    - 关键命令白名单：stop/reset 即使在连接不健康时也允许执行
    """
    # 命令完成信号（低频）
    command_done = pyqtSignal(str, bool, str)  # cmd_name, success, message
    connection_status = pyqtSignal(bool, str)  # connected, message
    params_read_done = pyqtSignal(str, bool, object)  # camera_id, success, data(dict|str)
    params_write_done = pyqtSignal(str, bool, str)    # camera_id, success, message
    
    # 关键命令白名单：这些命令即使在连接不健康时也应该尝试执行
    CRITICAL_COMMANDS = ["stop", "reset", "upperStop", "lowerStop", "__read_attrs__", "__write_attrs__"]
    
    def __init__(self, device_name="sys/reflection/1", parent=None):
        super().__init__(parent)
        self.device_name = device_name
        self.device = None
        self._running = True
        self._stop_requested = False
        self._command_queue = deque()
        self._queue_mutex = QMutex()
        self._read_attrs = [
            # 上平台单轴属性
            "upperPlatformPosX",
            "upperPlatformPosY",
            "upperPlatformPosZ",
            "upperPlatformStateX",
            "upperPlatformStateY",
            "upperPlatformStateZ",
            # 下平台单轴属性
            "lowerPlatformPosX",
            "lowerPlatformPosY",
            "lowerPlatformPosZ",
            "lowerPlatformStateX",
            "lowerPlatformStateY",
            "lowerPlatformStateZ",
            # CCD状态
            "upperCCD1xState",
            "upperCCD10xState",
            "lowerCCD1xState",
            "lowerCCD10xState",
        ]
        self._reconnect_interval = 3.0
        self._last_reconnect = 0.0
        
        # 状态数据缓存（UI定时拉取）
        self._status_cache = {}
        self._status_mutex = QMutex()
        self._cache_updated = False
        
        # 零等待连接检查：由后台轮询更新，命令执行前只读取此标志（不做网络操作）
        self._connection_healthy = False
        self._consecutive_failures = 0  # 连续失败次数
        self._max_failures_before_unhealthy = 2  # 连续失败多少次后标记为不健康
        
        # 命令队列保护：断连/超时场景下，避免用户连点堆积导致"延时后连弹很多错误框"
        self._max_queue_size = 3  # 允许少量排队
    
    def _clear_command_queue(self, reason: str = ""):
        """清空待执行命令队列（线程安全）。"""
        with QMutexLocker(self._queue_mutex):
            dropped = len(self._command_queue)
            self._command_queue.clear()
        if dropped > 0:
            print(f"[ReflectionImagingWorker {_ts()}] Cleared {dropped} queued commands. reason={reason}", flush=True)
    
    def queue_command(self, cmd_name: str, args=None):
        """将命令加入队列（线程安全）"""
        print(f"[ReflectionImagingWorker {_ts()}] queue_command: {cmd_name}({args})", flush=True)

        # 如果设备尚未连接（正在重连），不要把命令静默排队：立即反馈失败。
        if not self.device:
            self._connection_healthy = False
            self.command_done.emit(cmd_name, False, "设备未连接（正在重连），请稍后重试或检查设备服务器/网络")
            return

        # 连接不健康时，直接拒绝排队（避免积压）；由UI层做友好提示
        # 注意：这里不做任何网络操作，只读标志位。
        # stop/reset 等关键命令即使在连接不健康时也允许入队
        if not self._connection_healthy and cmd_name not in self.CRITICAL_COMMANDS:
            print(f"[ReflectionImagingWorker {_ts()}] Rejecting command due to unhealthy connection: {cmd_name}", flush=True)
            self.command_done.emit(cmd_name, False, "设备连接不健康，请等待自动重连或检查设备网络连接")
            return

        with QMutexLocker(self._queue_mutex):
            # 队列限流：超过上限则丢弃新命令（避免连点堆积）
            if len(self._command_queue) >= self._max_queue_size:
                print(
                    f"[ReflectionImagingWorker {_ts()}] Command queue full (size={len(self._command_queue)}), dropping: {cmd_name}",
                    flush=True,
                )
                self.command_done.emit(cmd_name, False, "命令队列繁忙，请稍后重试")
                return
            self._command_queue.append((cmd_name, args))
            queue_size = len(self._command_queue)
            print(f"[ReflectionImagingWorker {_ts()}] Command queued. Queue size: {queue_size}", flush=True)

    def queue_read_attributes(self, camera_id: str, attr_names: list):
        """读取属性（线程安全，后台线程执行）"""
        print(f"[ReflectionImagingWorker {_ts()}] queue_read_attributes: {camera_id} {attr_names}", flush=True)
        with QMutexLocker(self._queue_mutex):
            self._command_queue.append(("__read_attrs__", (camera_id, list(attr_names))))

    def queue_write_attributes(self, camera_id: str, attr_values: dict):
        """写入属性（线程安全，后台线程执行）"""
        keys = list(attr_values.keys())
        print(f"[ReflectionImagingWorker {_ts()}] queue_write_attributes: {camera_id} keys={keys}", flush=True)
        with QMutexLocker(self._queue_mutex):
            self._command_queue.append(("__write_attrs__", (camera_id, dict(attr_values))))
    
    def _pop_command(self):
        """从队列取出命令（线程安全）"""
        with QMutexLocker(self._queue_mutex):
            if self._command_queue:
                return self._command_queue.popleft()
            return None
    
    def get_cached_status(self):
        """获取缓存的状态数据（由UI定时调用，线程安全）
        
        Returns:
            dict: 属性名到值的映射，如果没有缓存数据则返回 None
        """
        with QMutexLocker(self._status_mutex):
            # 始终返回缓存数据（如果有），不再依赖 _cache_updated 标志
            # 这样多个定时器可以同时获取数据
            if not self._status_cache:
                return None  # 缓存为空
            return dict(self._status_cache)  # 返回副本
    
    def _update_cache(self, data: dict):
        """更新状态缓存（由后台线程调用）"""
        with QMutexLocker(self._status_mutex):
            self._status_cache.update(data)
            self._cache_updated = True
    
    def stop(self):
        """停止线程（从主线程调用）"""
        print(f"[ReflectionImagingWorker {_ts()}] Requesting stop...", flush=True)
        self._stop_requested = True
        self._running = False
        
        # 等待线程结束（最多5秒）
        if not self.wait(5000):
            print(f"[ReflectionImagingWorker {_ts()}] Warning: Thread did not stop in time, terminating...", flush=True)
            self.terminate()
            self.wait(1000)
        
        print(f"[ReflectionImagingWorker {_ts()}] Worker stopped.", flush=True)
    
    def _poll_attributes(self):
        """轮询模式：批量读取属性，更新缓存，同时更新连接健康标志"""
        if not self.device or not self._read_attrs:
            return
        
        if self._stop_requested:
            return
        
        # 初始化失败计数器（用于减少重复的失败日志）
        if not hasattr(self, '_fail_counters'):
            self._fail_counters = {}
        
        try:
            # 设置适当的超时时间（2000ms），因为属性读取可能涉及多个网络调用（编码器、力传感器等）
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(2000)
            
            try:
                # 使用批量读取
                attrs = self.device.read_attributes(self._read_attrs)
            finally:
                self.device.set_timeout_millis(old_timeout)
            
            status_data = {}
            successful_attrs = 0
            for attr in attrs:
                try:
                    if not attr.has_failed:
                        status_data[attr.name] = attr.value
                        successful_attrs += 1
                        # 成功读取时，重置失败计数器
                        if attr.name in self._fail_counters:
                            del self._fail_counters[attr.name]
                    else:
                        # 失败时，使用计数器，每100次才输出一次
                        if attr.name not in self._fail_counters:
                            self._fail_counters[attr.name] = 0
                        self._fail_counters[attr.name] += 1
                        if self._fail_counters[attr.name] % 100 == 1:
                            # 尝试获取详细错误信息
                            err_msg = ""
                            try:
                                err_stack = attr.get_err_stack()
                                if err_stack and len(err_stack) > 0:
                                    err_msg = f" - {err_stack[0].reason}: {err_stack[0].desc}"
                            except Exception:
                                pass
                            print(f"[ReflectionImagingWorker {_ts()}] Poll: {attr.name} FAILED (count={self._fail_counters[attr.name]}){err_msg}", flush=True)
                except Exception as e:
                    # 异常也使用计数器
                    if attr.name not in self._fail_counters:
                        self._fail_counters[attr.name] = 0
                    self._fail_counters[attr.name] += 1
                    if self._fail_counters[attr.name] % 100 == 1:
                        print(f"[ReflectionImagingWorker {_ts()}] Poll: Error reading {attr.name}: {e} (count={self._fail_counters[attr.name]})", flush=True)
            
            if status_data:
                self._update_cache(status_data)

            # 连接健康判定：至少有一个属性读成功才算“数据可用”。
            if successful_attrs == 0:
                self._consecutive_failures += 1
                if self._consecutive_failures >= self._max_failures_before_unhealthy:
                    if self._connection_healthy:
                        print(
                            f"[ReflectionImagingWorker {_ts()}] Connection became unhealthy (no attributes updated, failures={self._consecutive_failures})",
                            flush=True,
                        )
                    self._connection_healthy = False
            else:
                self._consecutive_failures = 0
                if not self._connection_healthy:
                    print(f"[ReflectionImagingWorker {_ts()}] Connection became healthy", flush=True)
                self._connection_healthy = True
        except Exception as e:
            # 轮询失败，增加失败计数
            self._consecutive_failures += 1
            if self._consecutive_failures >= self._max_failures_before_unhealthy:
                if self._connection_healthy:
                    print(f"[ReflectionImagingWorker {_ts()}] Connection became unhealthy after {self._consecutive_failures} failures: {e}", flush=True)
                    self._connection_healthy = False
            
            # 只在首次失败时输出详细日志
            if self._consecutive_failures == 1:
                print(f"[ReflectionImagingWorker {_ts()}] Poll failed: {e}", flush=True)
                import traceback
                traceback.print_exc()
            
            self.device = None
    
    def _attempt_connect(self):
        """尝试连接设备，失败不抛异常"""
        if self._stop_requested:
            return False
        
        try:
            print(f"[ReflectionImagingWorker {_ts()}] Connecting to {self.device_name}...", flush=True)
            dev = PyTango.DeviceProxy(self.device_name)
            # 使用较短的超时（1秒）加快失败检测
            dev.set_timeout_millis(1000)
            state = dev.state()
            
            if self._stop_requested:
                return False
            
            self.device = dev
            # 连接成功：能连上 server 不代表设备已完全就绪，初始标记为健康
            self._connection_healthy = state in (PyTango.DevState.ON, PyTango.DevState.MOVING)
            self._consecutive_failures = 0
            
            status_msg = f"Connected (state: {state})"
            print(f"[ReflectionImagingWorker {_ts()}] Connected OK: {status_msg}", flush=True)
            self.connection_status.emit(True, status_msg)
            
            # 首次轮询填充缓存
            if self._read_attrs:
                self._poll_attributes()
            return True
        except Exception as e:
            self.device = None
            # 连接失败，标记连接不健康
            self._connection_healthy = False
            
            error_msg = str(e)
            print(f"[ReflectionImagingWorker {_ts()}] Connection failed: {error_msg}", flush=True)
            import traceback
            traceback.print_exc()
            self.connection_status.emit(False, error_msg)
            return False
    
    def run(self):
        """主循环：纯轮询模式"""
        if not PyTango:
            print(f"[ReflectionImagingWorker {_ts()}] PyTango not available", flush=True)
            self.connection_status.emit(False, "PyTango not installed")
            return
        
        self._last_reconnect = time.monotonic() - self._reconnect_interval
        poll_interval_ms = 500  # 轮询间隔500ms
        
        while self._running and not self._stop_requested:
            # 尝试重连
            if not self.device:
                now = time.monotonic()
                if now - self._last_reconnect >= self._reconnect_interval:
                    self._last_reconnect = now
                    self._attempt_connect()
                self.msleep(300)
                continue
            
            # 1. 处理命令队列（高优先级）
            cmd = self._pop_command()
            while cmd is not None:
                self._execute_command(cmd[0], cmd[1])
                if self._stop_requested:
                    break
                cmd = self._pop_command()
            
            if self._stop_requested:
                break
            
            # 2. 轮询属性
            self._poll_attributes()
            
            # 3. 等待下次轮询
            for _ in range(poll_interval_ms // 50):  # 50ms为单位
                if self._stop_requested:
                    break
                with QMutexLocker(self._queue_mutex):
                    has_cmd = len(self._command_queue) > 0
                if has_cmd:
                    break
                self.msleep(50)
        
        print(f"[ReflectionImagingWorker {_ts()}] Worker thread exiting.", flush=True)
    
    def _execute_command(self, cmd_name, args):
        """执行Tango命令（带零等待连接检查）"""
        if not self.device:
            error_msg = "Device not connected"
            print(f"[ReflectionImagingWorker {_ts()}] _execute_command: {cmd_name} FAILED - {error_msg}", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
        
        if self._stop_requested:
            error_msg = "Worker stopping"
            print(f"[ReflectionImagingWorker {_ts()}] _execute_command: {cmd_name} CANCELLED - {error_msg}", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return

        # 扩展：属性读写（这些命令不检查连接健康，因为它们可能用于诊断）
        if cmd_name == "__read_attrs__":
            camera_id, attr_names = args
            self._execute_read_attrs(camera_id, attr_names)
            return
        if cmd_name == "__write_attrs__":
            camera_id, attr_values = args
            self._execute_write_attrs(camera_id, attr_values)
            return
        
        # 关键命令白名单：stop/reset 即使在连接不健康时也应该尝试执行
        is_critical = cmd_name in self.CRITICAL_COMMANDS
        
        # 零等待连接检查：检查后台更新的连接健康标志（纯内存操作，不做网络调用）
        # 注意：stop/reset 等关键命令跳过此检查，允许在连接不健康时尝试执行
        if not self._connection_healthy and not is_critical:
            error_msg = "设备连接不健康，请等待自动重连或手动重启设备服务器"
            print(f"[ReflectionImagingWorker {_ts()}] _execute_command: {cmd_name} FAILED - Connection unhealthy (zero-wait check)", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
        
        try:
            print(f"[ReflectionImagingWorker {_ts()}] _execute_command: Starting {cmd_name}({args})", flush=True)
            
            # 设置命令超时（1秒，之前是3秒，缩短以加快失败检测）
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(1000)
            print(f"[ReflectionImagingWorker {_ts()}] _execute_command: Timeout set to 1000ms (was {old_timeout}ms)", flush=True)
            
            try:
                if args is not None and args != [] and args != ():
                    result = self.device.command_inout(cmd_name, args)
                else:
                    result = self.device.command_inout(cmd_name)
            finally:
                self.device.set_timeout_millis(old_timeout)
                print(f"[ReflectionImagingWorker {_ts()}] _execute_command: Timeout restored to {old_timeout}ms", flush=True)
            
            print(f"[ReflectionImagingWorker {_ts()}] _execute_command: {cmd_name} SUCCEEDED, result={result}", flush=True)
            self.command_done.emit(cmd_name, True, str(result))
        except Exception as e:
            error_msg = str(e)
            print(f"[ReflectionImagingWorker {_ts()}] _execute_command: {cmd_name} FAILED - {error_msg}", flush=True)
            import traceback
            traceback.print_exc()
            self.command_done.emit(cmd_name, False, error_msg)
            # 如果是超时错误，标记连接不健康并清理设备
            if "timeout" in error_msg.lower():
                print(f"[ReflectionImagingWorker {_ts()}] _execute_command: Timeout detected, marking device for reconnect", flush=True)
                self.device = None
                self._connection_healthy = False
                # 发生超时后，清空后续排队命令，避免延时后连续弹出很多失败提示
                self._clear_command_queue(reason="timeout")
            # 网络/硬件断开类错误：清空排队，避免错误提示堆积
            elif "not connected" in error_msg.lower() or "connection" in error_msg.lower():
                self._connection_healthy = False
                self._clear_command_queue(reason="connection error")

    def _execute_read_attrs(self, camera_id: str, attr_names: list):
        if not self.device:
            self.params_read_done.emit(camera_id, False, "Device not connected")
            return
        if self._stop_requested:
            self.params_read_done.emit(camera_id, False, "Worker stopping")
            return
        try:
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(1000)  # 缩短超时时间
            try:
                attrs = self.device.read_attributes(attr_names)
            finally:
                self.device.set_timeout_millis(old_timeout)

            data = {}
            for a in attrs:
                if getattr(a, "has_failed", False):
                    continue
                data[a.name] = a.value
            self.params_read_done.emit(camera_id, True, data)
        except Exception as e:
            error_msg = str(e)
            print(f"[ReflectionImagingWorker {_ts()}] _execute_read_attrs FAILED: {error_msg}", flush=True)
            self.params_read_done.emit(camera_id, False, error_msg)
            if "timeout" in error_msg.lower():
                self.device = None
                self._connection_healthy = False

    def _execute_write_attrs(self, camera_id: str, attr_values: dict):
        if not self.device:
            self.params_write_done.emit(camera_id, False, "Device not connected")
            return
        if self._stop_requested:
            self.params_write_done.emit(camera_id, False, "Worker stopping")
            return
        try:
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(1000)  # 缩短超时时间
            try:
                for name, value in attr_values.items():
                    self.device.write_attribute(name, value)
            finally:
                self.device.set_timeout_millis(old_timeout)
            self.params_write_done.emit(camera_id, True, "OK")
        except Exception as e:
            error_msg = str(e)
            print(f"[ReflectionImagingWorker {_ts()}] _execute_write_attrs FAILED: {error_msg}", flush=True)
            self.params_write_done.emit(camera_id, False, error_msg)
            if "timeout" in error_msg.lower():
                self.device = None
                self._connection_healthy = False


class SingleAxisControlWidget(QFrame):
    """单轴控制组件 - 紧凑单行布局"""
    def __init__(self, axis_name, platform_type="upper", axis="X", worker=None, parent=None):
        super().__init__(parent)
        self.platform_type = platform_type  # "upper" or "lower"
        self.axis = axis  # "X", "Y", "Z"
        self.axis_name = axis_name
        self._worker = worker
        self._setup_ui()
        self._setup_connections()
        
    def _setup_ui(self):
        self.setStyleSheet("QFrame { background-color: #0b1621; border-radius: 4px; }")
        layout = QHBoxLayout(self)
        layout.setSpacing(8)
        layout.setContentsMargins(12, 6, 12, 6)
        
        # 轴名称
        axis_label = QLabel(f"{self.axis_name}:")
        axis_label.setStyleSheet("color: #00a0e9; font-weight: bold; min-width: 30px;")
        layout.addWidget(axis_label)
        
        # 当前位置
        self.pos_value = QLabel("0.000")
        self.pos_value.setStyleSheet("color: #29d6ff; font-weight: bold; min-width: 70px;")
        self.pos_value.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        layout.addWidget(self.pos_value)
        
        mm_label = QLabel("mm")
        mm_label.setStyleSheet("color: #8fa6c5;")
        layout.addWidget(mm_label)
        
        # 状态指示
        self.state_value = QLabel("●")
        self.state_value.setStyleSheet("color: #39e072; font-size: 10px;")  # 绿色=静止
        self.state_value.setFixedWidth(16)
        layout.addWidget(self.state_value)
        
        # 分隔符
        sep = QLabel("|")
        sep.setStyleSheet("color: #1c3146;")
        layout.addWidget(sep)

        # 位置/位移输入（单一数值，不区分绝对/相对）
        val_label = QLabel("数值:")
        val_label.setStyleSheet("color: #8fa6c5;")
        layout.addWidget(val_label)

        self.input_value = QLineEdit()
        self.input_value.setPlaceholderText("0")
        self.input_value.setAlignment(Qt.AlignCenter)
        self.input_value.setFixedWidth(70)
        self.input_value.setStyleSheet("background-color: #0c1724; border: 1px solid #1c3146; color: white; padding: 2px;")
        layout.addWidget(self.input_value)
        
        # 分隔符
        sep2 = QLabel("|")
        sep2.setStyleSheet("color: #1c3146;")
        layout.addWidget(sep2)
        
        # 控制按钮 - 增加宽度以显示完整中文（四个中文字符需要更宽）
        self.btn_abs = ControlButton("绝对运动", role="move_absolute")
        self.btn_abs.setFixedWidth(95)
        self.btn_rel = ControlButton("相对运动", role="move_relative")
        self.btn_rel.setFixedWidth(95)
        self.btn_stop = ControlButton("停止", role="stop")
        self.btn_stop.setFixedWidth(65)
        self.btn_home = ControlButton("回零")
        self.btn_home.setFixedWidth(65)
        self.btn_reset = ControlButton("复位")
        self.btn_reset.setFixedWidth(65)
        
        layout.addWidget(self.btn_abs)
        layout.addWidget(self.btn_rel)
        layout.addWidget(self.btn_stop)
        layout.addWidget(self.btn_home)
        layout.addWidget(self.btn_reset)
        layout.addStretch()
    
    def set_worker(self, worker):
        """设置Worker（延迟设置）"""
        self._worker = worker
        self._setup_connections()
    
    def _setup_connections(self):
        """连接按钮信号到Tango命令"""
        if not self._worker:
            return
        
        # 断开旧连接
        try: self.btn_abs.clicked.disconnect()
        except TypeError: pass
        try: self.btn_rel.clicked.disconnect()
        except TypeError: pass
        try: self.btn_stop.clicked.disconnect()
        except TypeError: pass
        try: self.btn_home.clicked.disconnect()
        except TypeError: pass
        try: self.btn_reset.clicked.disconnect()
        except TypeError: pass
        
        self.btn_abs.clicked.connect(self._on_absolute)
        self.btn_rel.clicked.connect(self._on_relative)
        self.btn_stop.clicked.connect(self._on_stop)
        self.btn_home.clicked.connect(self._on_home)
        self.btn_reset.clicked.connect(self._on_reset)
    
    def _get_cmd_prefix(self):
        """获取命令前缀，如 'upperX' 或 'lowerZ'"""
        return f"{self.platform_type}{self.axis}"

    def _read_input_value(self) -> float:
        return float(self.input_value.text() or "0")
    
    def _on_absolute(self):
        """绝对运动"""
        if not self._worker:
            show_warning("错误", "设备未连接")
            return
        try:
            val = self._read_input_value()
            cmd_name = f"{self._get_cmd_prefix()}MoveAbsolute"
            self._worker.queue_command(cmd_name, val)
        except ValueError:
            show_warning("输入错误", "请输入有效的数字")
    
    def _on_relative(self):
        """相对运动"""
        if not self._worker:
            show_warning("错误", "设备未连接")
            return
        try:
            val = self._read_input_value()
            cmd_name = f"{self._get_cmd_prefix()}MoveRelative"
            self._worker.queue_command(cmd_name, val)
        except ValueError:
            show_warning("输入错误", "请输入有效的数字")
    
    def _on_stop(self):
        """停止"""
        if not self._worker:
            return
        cmd_name = f"{self._get_cmd_prefix()}Stop"
        self._worker.queue_command(cmd_name)
    
    def _on_home(self):
        """回零"""
        if not self._worker:
            return
        cmd_name = f"{self._get_cmd_prefix()}MoveZero"
        self._worker.queue_command(cmd_name)
    
    def _on_reset(self):
        """复位"""
        if not self._worker:
            return
        cmd_name = f"{self._get_cmd_prefix()}Reset"
        self._worker.queue_command(cmd_name)
    
    def update_status(self, pos, is_moving):
        """更新状态显示"""
        self.pos_value.setText(f"{pos:.3f}")
        if is_moving:
            self.state_value.setStyleSheet("color: #FFA500; font-size: 10px;")  # 橙色=运动中
        else:
            self.state_value.setStyleSheet("color: #39e072; font-size: 10px;")  # 绿色=静止


class ThreeAxisControlWidget(QGroupBox):
    """三坐标控制组件 (包含 X, Y, Z 三个独立单轴控制) - 紧凑布局"""
    def __init__(self, title, platform_type="upper", worker=None, parent=None):
        super().__init__(title, parent)
        self.platform_type = platform_type  # "upper" or "lower"
        self._worker = worker
        self._setup_ui()
        
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setSpacing(4)
        layout.setContentsMargins(10, 16, 10, 10)
        
        # 创建三个单轴控制组件
        self.axis_x = SingleAxisControlWidget("X", self.platform_type, "X", self._worker)
        self.axis_y = SingleAxisControlWidget("Y", self.platform_type, "Y", self._worker)
        self.axis_z = SingleAxisControlWidget("Z", self.platform_type, "Z", self._worker)
        
        layout.addWidget(self.axis_x)
        layout.addWidget(self.axis_y)
        layout.addWidget(self.axis_z)
    
    def set_worker(self, worker):
        """设置Worker（延迟设置）"""
        self._worker = worker
        self.axis_x.set_worker(worker)
        self.axis_y.set_worker(worker)
        self.axis_z.set_worker(worker)
    
    def update_status(self, pos_x, pos_y, pos_z, state_x, state_y, state_z):
        """更新三轴状态"""
        self.axis_x.update_status(pos_x, state_x)
        self.axis_y.update_status(pos_y, state_y)
        self.axis_z.update_status(pos_z, state_z)


class SwitchControlGroup(QGroupBox):
    """单个开关控制组 (环形光 + CCD)"""
    def __init__(self, title, ccd_type="upper_1x", worker=None, parent=None):
        super().__init__(title, parent)
        self.ccd_type = ccd_type  # "upper_1x", "upper_10x", "lower_1x", "lower_10x"
        self._worker = worker
        self._setup_ui()
        self._setup_connections()
    
    def set_worker(self, worker):
        """设置Worker（延迟设置）"""
        self._worker = worker
        self._setup_connections()
    
    def _setup_connections(self):
        """连接按钮信号到Tango命令"""
        if not self._worker:
            return

        # 断开旧连接（防止 set_worker 多次调用导致重复触发）
        for btn in (self.btn_light_on, self.btn_light_off, self.btn_ccd_on, self.btn_ccd_off):
            try:
                btn.clicked.disconnect()
            except TypeError:
                pass

        # 环形光独立控制
        self.btn_light_on.clicked.connect(lambda: self._on_light_switch(True))
        self.btn_light_off.clicked.connect(lambda: self._on_light_switch(False))

        # CCD 开关控制
        self.btn_ccd_on.clicked.connect(lambda: self._on_ccd_switch(True))
        self.btn_ccd_off.clicked.connect(lambda: self._on_ccd_switch(False))

    def _on_light_switch(self, on: bool):
        """环形光开关控制（独立于CCD）"""
        if not self._worker:
            show_warning("错误", "设备未连接")
            return

        cmd_map = {
            "upper_1x": "upperCCD1xRingLightSwitch",
            "upper_10x": "upperCCD10xRingLightSwitch",
            "lower_1x": "lowerCCD1xRingLightSwitch",
            "lower_10x": "lowerCCD10xRingLightSwitch",
        }
        cmd_name = cmd_map.get(self.ccd_type)
        if not cmd_name:
            show_warning("错误", f"未知CCD类型: {self.ccd_type}")
            return
        self._worker.queue_command(cmd_name, 1 if on else 0)
    
    def _on_ccd_switch(self, on: bool):
        """CCD开关控制"""
        if not self._worker:
            show_warning("错误", "设备未连接")
            return
        
        # 根据CCD类型确定命令名称
        cmd_map = {
            "upper_1x": "upperCCD1xSwitch",
            "upper_10x": "upperCCD10xSwitch",
            "lower_1x": "lowerCCD1xSwitch",
            "lower_10x": "lowerCCD10xSwitch",
        }
        
        cmd_name = cmd_map.get(self.ccd_type)
        if cmd_name:
            self._worker.queue_command(cmd_name, 1 if on else 0)
        
    def _setup_ui(self):
        layout = QHBoxLayout(self)
        layout.setSpacing(10)
        layout.setContentsMargins(10, 20, 10, 20)
        
        self.btn_light_on = ControlButton("环形光开")
        self.btn_light_off = ControlButton("环形光关")
        self.btn_ccd_on = ControlButton("CCD开")
        self.btn_ccd_off = ControlButton("CCD关")
        
        layout.addWidget(self.btn_light_on)
        layout.addWidget(self.btn_light_off)
        layout.addWidget(self.btn_ccd_on)
        layout.addWidget(self.btn_ccd_off)


class ConfigControlTab(QWidget):
    """Tab 1: 配置控制页"""
    def __init__(self, worker=None, parent=None):
        super().__init__(parent)
        self._worker = worker
        self._setup_ui()
    
    def set_worker(self, worker):
        """设置Worker（延迟设置）"""
        self._worker = worker
        # 设置Worker到子组件
        self.upper_platform.set_worker(worker)
        self.lower_platform.set_worker(worker)
        self.group_upper_1x.set_worker(worker)
        self.group_upper_10x.set_worker(worker)
        self.group_lower_1x.set_worker(worker)
        self.group_lower_10x.set_worker(worker)
        
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setSpacing(20)
        layout.setContentsMargins(20, 20, 20, 20)
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        
        content = QWidget()
        content_layout = QVBoxLayout(content)
        content_layout.setSpacing(20)
        
        # 1. 三坐标控制模块
        self.upper_platform = ThreeAxisControlWidget("上平台三坐标控制", "upper", self._worker)
        content_layout.addWidget(self.upper_platform)
        
        self.lower_platform = ThreeAxisControlWidget("下平台三坐标控制", "lower", self._worker)
        content_layout.addWidget(self.lower_platform)
        
        # 2. 开关控制模块
        switch_container = QGroupBox("开关控制")
        switch_layout = QGridLayout(switch_container)
        switch_layout.setSpacing(20)
        
        # 四组控制
        self.group_upper_1x = SwitchControlGroup("上CCD-1倍", "upper_1x", self._worker)
        self.group_upper_10x = SwitchControlGroup("上CCD-10倍", "upper_10x", self._worker)
        self.group_lower_1x = SwitchControlGroup("下CCD-1倍", "lower_1x", self._worker)
        self.group_lower_10x = SwitchControlGroup("下CCD-10倍", "lower_10x", self._worker)
        
        switch_layout.addWidget(self.group_upper_1x, 0, 0)
        switch_layout.addWidget(self.group_upper_10x, 0, 1)
        switch_layout.addWidget(self.group_lower_1x, 1, 0)
        switch_layout.addWidget(self.group_lower_10x, 1, 1)
        
        content_layout.addWidget(switch_container)
        
        content_layout.addStretch()
        scroll.setWidget(content)
        layout.addWidget(scroll)


class CCDConfigTab(QWidget):
    """Tab 3: CCD 参数配置页"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.param_widgets = {}
        self._worker = None
        self._cameras = REFLECTION_IMAGING_CONFIG.get("cameras", [])
        self._setup_ui()

    def set_worker(self, worker):
        """设置Worker（延迟设置）"""
        self._worker = worker
        # 防止重复连接
        try:
            self.btn_read.clicked.disconnect()
        except TypeError:
            pass
        try:
            self.btn_set.clicked.disconnect()
        except TypeError:
            pass

        self.btn_read.clicked.connect(self._on_read_params)
        self.btn_set.clicked.connect(self._on_apply_params)

        if self._worker:
            try:
                self._worker.params_read_done.disconnect()
            except TypeError:
                pass
            try:
                self._worker.params_write_done.disconnect()
            except TypeError:
                pass
            self._worker.params_read_done.connect(self._on_params_read_done)
            self._worker.params_write_done.connect(self._on_params_write_done)
        
    def _setup_ui(self):
        layout = QHBoxLayout(self)
        layout.setSpacing(20)
        layout.setContentsMargins(20, 20, 20, 20)
        
        # 左侧：相机列表
        list_container = QGroupBox("相机列表")
        list_container.setStyleSheet("""
            QGroupBox {
                border: 1px solid #1c3146;
                border-radius: 4px;
                margin-top: 12px;
                color: #8fa6c5;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 5px;
                left: 10px;
            }
        """)
        list_layout = QVBoxLayout(list_container)
        
        self.camera_list = QListWidget()
        self.camera_list.setStyleSheet("""
            QListWidget {
                background-color: #0b1621;
                border: 1px solid #1c3146;
                color: #8fa6c5;
                font-size: 14px;
                outline: none;
            }
            QListWidget::item {
                padding: 12px;
                border-bottom: 1px solid #1c3146;
            }
            QListWidget::item:selected {
                background-color: #00a0e9;
                color: white;
            }
            QListWidget::item:hover:!selected {
                background-color: #1c3146;
            }
        """)
        
        cameras = REFLECTION_IMAGING_CONFIG.get("cameras", [])
        for cam in cameras:
            self.camera_list.addItem(cam["name"])
            
        list_layout.addWidget(self.camera_list)
        layout.addWidget(list_container, 1)
        
        # 右侧：参数配置表单
        form_container = QGroupBox("参数配置")
        form_container.setStyleSheet("""
            QGroupBox {
                border: 1px solid #1c3146;
                border-radius: 4px;
                margin-top: 12px;
                color: #8fa6c5;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 5px;
                left: 10px;
            }
        """)
        form_layout = QVBoxLayout(form_container)
        
        # 滚动区域
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setStyleSheet("background: transparent;")
        
        form_widget = QWidget()
        form_widget.setStyleSheet("background: transparent;")
        self.form_layout = QFormLayout(form_widget)
        self.form_layout.setSpacing(20)
        self.form_layout.setLabelAlignment(Qt.AlignRight)
        self.form_layout.setContentsMargins(20, 20, 40, 20)
        
        params = REFLECTION_IMAGING_CONFIG.get("ccd_params", [])
        
        for param in params:
            label = QLabel(f"{param['name']}:")
            label.setStyleSheet("color: #8fa6c5; font-weight: bold; font-size: 13px;")
            
            widget = None
            p_type = param.get("type", "string")
            
            if p_type == "float":
                widget = QDoubleSpinBox()
                widget.setRange(param.get("min", 0), param.get("max", 10000))
                widget.setValue(param.get("default", 0))
                widget.setSuffix(f" {param.get('unit', '')}")
                widget.setDecimals(2)
            elif p_type == "int":
                widget = QSpinBox()
                widget.setRange(param.get("min", 0), param.get("max", 10000))
                widget.setValue(param.get("default", 0))
                widget.setSuffix(f" {param.get('unit', '')}")
            elif p_type == "enum":
                widget = QComboBox()
                widget.addItems(param.get("options", []))
                widget.setCurrentText(str(param.get("default", "")))
            else:
                widget = QLineEdit()
                widget.setText(str(param.get("default", "")))
                
            # 通用样式
            widget.setMinimumHeight(32)
            widget.setStyleSheet("""
                QSpinBox, QDoubleSpinBox, QLineEdit, QComboBox {
                    background-color: #0c1724; 
                    border: 1px solid #1c3146; 
                    color: white; 
                    padding: 4px 8px;
                    border-radius: 4px;
                    font-size: 13px;
                }
                
                /* SpinBox 上下按钮样式 */
                QSpinBox::up-button, QDoubleSpinBox::up-button {
                    subcontrol-origin: padding;
                    subcontrol-position: top right;
                    width: 24px; /* 加宽一点 */
                    background: #1c3146;
                    border-left: 1px solid #0c1724;
                    border-bottom: 1px solid #0c1724;
                    border-top-right-radius: 4px;
                }
                QSpinBox::down-button, QDoubleSpinBox::down-button {
                    subcontrol-origin: padding;
                    subcontrol-position: bottom right;
                    width: 24px;
                    background: #1c3146;
                    border-left: 1px solid #0c1724;
                    border-top: 1px solid #0c1724;
                    border-bottom-right-radius: 4px;
                }
                QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
                    width: 0;
                    height: 0;
                    border-left: 5px solid transparent;
                    border-right: 5px solid transparent;
                    border-bottom: 6px solid #8fa6c5; /* 加大箭头 */
                }
                QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
                    width: 0;
                    height: 0;
                    border-left: 5px solid transparent;
                    border-right: 5px solid transparent;
                    border-top: 6px solid #8fa6c5;
                }

                /* ComboBox 下拉箭头样式 */
                QComboBox::drop-down {
                    border: none;
                    background: #1c3146;
                    width: 20px;
                    border-top-right-radius: 4px;
                    border-bottom-right-radius: 4px;
                }
                QComboBox::down-arrow {
                    width: 0;
                    height: 0;
                    border-left: 4px solid transparent;
                    border-right: 4px solid transparent;
                    border-top: 5px solid #8fa6c5;
                }
                QComboBox QAbstractItemView {
                    background-color: #0c1724;
                    color: white;
                    selection-background-color: #00a0e9;
                    selection-color: white;
                    border: 1px solid #1c3146;
                }
            """)
            
            if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                 widget.setButtonSymbols(QSpinBox.UpDownArrows)
            
            self.param_widgets[param["id"]] = widget
            self.form_layout.addRow(label, widget)
            
        scroll.setWidget(form_widget)
        form_layout.addWidget(scroll)
        
        # 底部操作按钮
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        
        self.btn_read = QPushButton("读取参数")
        self.btn_read.setCursor(Qt.PointingHandCursor)
        self.btn_read.setStyleSheet("""
            QPushButton {
                background-color: #2c3e50; 
                color: white; 
                border: none; 
                padding: 8px 24px; 
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #34495e; }
            QPushButton:pressed { background-color: #2c3e50; }
        """)
        
        self.btn_set = QPushButton("应用设置")
        self.btn_set.setCursor(Qt.PointingHandCursor)
        self.btn_set.setStyleSheet("""
            QPushButton {
                background-color: #00a0e9; 
                color: white; 
                border: none; 
                padding: 8px 24px; 
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #00b4f0; }
            QPushButton:pressed { background-color: #008ac9; }
        """)
        
        btn_layout.addWidget(self.btn_read)
        btn_layout.addWidget(self.btn_set)
        
        form_layout.addLayout(btn_layout)
        
        layout.addWidget(form_container, 3)
        
        # 默认选中第一个
        if self.camera_list.count() > 0:
            self.camera_list.setCurrentRow(0)

    def _selected_camera_id(self):
        idx = self.camera_list.currentRow()
        if idx < 0 or idx >= len(self._cameras):
            return None
        return self._cameras[idx]["id"]

    def _camera_prefix(self, camera_id: str):
        return {
            "upper_1x": "upperCCD1x",
            "upper_10x": "upperCCD10x",
            "lower_1x": "lowerCCD1x",
            "lower_10x": "lowerCCD10x",
        }.get(camera_id)

    def _build_attr_list(self, camera_id: str):
        prefix = self._camera_prefix(camera_id)
        if not prefix:
            return None
        attrs = []
        for param in REFLECTION_IMAGING_CONFIG.get("ccd_params", []):
            pid = param.get("id")
            if pid == "exposure":
                attrs.append(f"{prefix}Exposure")
            elif pid == "gain":
                attrs.append(f"{prefix}Gain")
            elif pid == "brightness":
                attrs.append(f"{prefix}Brightness")
            elif pid == "contrast":
                attrs.append(f"{prefix}Contrast")
            elif pid == "resolution":
                attrs.append(f"{prefix}Resolution")
            elif pid == "trigger_mode":
                attrs.append(f"{prefix}TriggerMode")
        return attrs

    def _on_read_params(self):
        if not self._worker:
            show_warning("错误", "设备未连接")
            return
        camera_id = self._selected_camera_id()
        if not camera_id:
            show_warning("错误", "未选择相机")
            return
        attrs = self._build_attr_list(camera_id)
        if not attrs:
            show_warning("错误", f"未知相机: {camera_id}")
            return
        self._worker.queue_read_attributes(camera_id, attrs)

    def _on_apply_params(self):
        if not self._worker:
            show_warning("错误", "设备未连接")
            return
        camera_id = self._selected_camera_id()
        if not camera_id:
            show_warning("错误", "未选择相机")
            return

        prefix = self._camera_prefix(camera_id)
        if not prefix:
            show_warning("错误", f"未知相机: {camera_id}")
            return

        values = {}
        for pid, widget in self.param_widgets.items():
            if pid == "exposure":
                # UI单位ms，设备端Exposure属性使用秒
                ms_val = float(widget.value()) if hasattr(widget, "value") else float(widget.text() or "0")
                values[f"{prefix}Exposure"] = ms_val / 1000.0
            elif pid == "gain":
                values[f"{prefix}Gain"] = float(widget.value())
            elif pid == "brightness":
                values[f"{prefix}Brightness"] = float(int(widget.value()))
            elif pid == "contrast":
                values[f"{prefix}Contrast"] = float(int(widget.value()))
            elif pid == "resolution":
                values[f"{prefix}Resolution"] = str(widget.currentText())
            elif pid == "trigger_mode":
                values[f"{prefix}TriggerMode"] = str(widget.currentText())

        if not values:
            show_warning("提示", "没有可写参数")
            return
        self._worker.queue_write_attributes(camera_id, values)

    def _on_params_read_done(self, camera_id: str, success: bool, data):
        if not success:
            show_warning("读取参数失败", f"相机: {camera_id}\n错误: {data}")
            return
        selected = self._selected_camera_id()
        if selected != camera_id:
            # 只更新当前选中的相机
            return
        prefix = self._camera_prefix(camera_id)
        if not prefix:
            return
        # 回填UI
        for pid, widget in self.param_widgets.items():
            if pid == "exposure":
                sec = float(data.get(f"{prefix}Exposure", 0.0))
                widget.setValue(sec * 1000.0)
            elif pid == "gain":
                widget.setValue(float(data.get(f"{prefix}Gain", 0.0)))
            elif pid == "brightness":
                widget.setValue(int(float(data.get(f"{prefix}Brightness", 0.0))))
            elif pid == "contrast":
                widget.setValue(int(float(data.get(f"{prefix}Contrast", 0.0))))
            elif pid == "resolution":
                widget.setCurrentText(str(data.get(f"{prefix}Resolution", widget.currentText())))
            elif pid == "trigger_mode":
                widget.setCurrentText(str(data.get(f"{prefix}TriggerMode", widget.currentText())))

    def _on_params_write_done(self, camera_id: str, success: bool, message: str):
        if not success:
            show_warning("应用设置失败", f"相机: {camera_id}\n错误: {message}")
            return
        # 写成功后，主动读取一次回显
        if self._worker and self._selected_camera_id() == camera_id:
            attrs = self._build_attr_list(camera_id)
            if attrs:
                self._worker.queue_read_attributes(camera_id, attrs)


class ImageDisplayTab(QWidget):
    """Tab 2: 反射光成像显示页"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self._setup_ui()
        
    def _setup_ui(self):
        main_layout = QVBoxLayout(self)
        main_layout.setSpacing(10)
        main_layout.setContentsMargins(10, 10, 10, 10)
        
        # 1. 保存路径设置区域
        path_container = QWidget()
        path_layout = QHBoxLayout(path_container)
        path_layout.setContentsMargins(0, 0, 0, 0)
        
        path_label = QLabel("保存路径:")
        path_label.setStyleSheet("color: #8fa6c5;")
        
        self.path_edit = QLineEdit()
        self.path_edit.setText(REFLECTION_IMAGING_CONFIG.get("default_save_path", ""))
        self.path_edit.setReadOnly(True)
        self.path_edit.setStyleSheet("background-color: #0c1724; border: 1px solid #1c3146; color: white; padding: 4px;")
        
        self.btn_browse = QPushButton("浏览...")
        self.btn_browse.setStyleSheet("background-color: #2c3e50; color: white; border: none; padding: 6px 12px; border-radius: 4px;")
        self.btn_browse.clicked.connect(self._select_path)
        
        path_layout.addWidget(path_label)
        path_layout.addWidget(self.path_edit)
        path_layout.addWidget(self.btn_browse)
        
        main_layout.addWidget(path_container)
        
        # 2. 图像显示区域 (Grid)
        grid_container = QWidget()
        grid_layout = QGridLayout(grid_container)
        grid_layout.setSpacing(10)
        grid_layout.setContentsMargins(0, 0, 0, 0)
        
        cameras = REFLECTION_IMAGING_CONFIG.get("cameras", [])
        positions = [(0, 0), (0, 1), (1, 0), (1, 1)]
        
        for i, cam in enumerate(cameras):
            if i >= 4: break
            row, col = positions[i]
            
            frame = QFrame()
            frame.setFrameShape(QFrame.StyledPanel)
            frame.setStyleSheet("background-color: black; border: 1px solid #333;")
            
            vbox = QVBoxLayout(frame)
            vbox.setSpacing(0)
            vbox.setContentsMargins(1, 1, 1, 1)
            
            # 1. 标题栏
            label = QLabel(cam["name"])
            label.setStyleSheet("background-color: #1c3146; color: white; font-weight: bold; padding: 6px;")
            label.setAlignment(Qt.AlignCenter)
            vbox.addWidget(label)
            
            # 2. 图像显示区
            img_placeholder = QLabel("No Signal")
            img_placeholder.setStyleSheet("background-color: black; color: #4b5c74; font-size: 14px;")
            img_placeholder.setAlignment(Qt.AlignCenter)
            vbox.addWidget(img_placeholder)
            vbox.setStretch(1, 1)
            
            # 3. 底部工具栏
            toolbar = QWidget()
            toolbar.setStyleSheet("background-color: #0b1621; border-top: 1px solid #1c3146;")
            tool_layout = QHBoxLayout(toolbar)
            tool_layout.setContentsMargins(8, 8, 8, 8)
            
            btn_capture = QPushButton("截图")
            btn_capture.setCursor(Qt.PointingHandCursor)
            btn_capture.setEnabled(False) # 默认无信号，禁用
            btn_capture.setStyleSheet("""
                QPushButton {
                    background-color: #00a0e9;
                    color: white;
                    border: none;
                    padding: 6px 20px;
                    border-radius: 4px;
                    font-weight: bold;
                }
                QPushButton:hover { background-color: #00b4f0; }
                QPushButton:pressed { background-color: #008ac9; }
                QPushButton:disabled { 
                    background-color: #1c3146; 
                    color: #4b5c74; 
                    border: 1px solid #2c3e50;
                }
            """)
            
            tool_layout.addStretch()
            tool_layout.addWidget(btn_capture)
            tool_layout.addStretch()
            
            vbox.addWidget(toolbar)
            
            grid_layout.addWidget(frame, row, col)
            
        main_layout.addWidget(grid_container)
        main_layout.setStretch(1, 1)

    def _select_path(self):
        path = QFileDialog.getExistingDirectory(self, "选择保存路径", self.path_edit.text())
        if path:
            self.path_edit.setText(path)


class ReflectionImagingPage(QWidget):
    """反射光成像主页面"""
    
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
        layout.setContentsMargins(0, 0, 0, 0)
        
        # 顶部标题
        header_container = QWidget()
        header_layout = QHBoxLayout(header_container)
        header_layout.setContentsMargins(20, 10, 20, 0)
        header = QLabel("反射光成像控制")
        header.setProperty("role", "title")
        header_layout.addWidget(header)
        layout.addWidget(header_container)
        
        # 使用水平分割器：左侧是Tab区域，右侧是状态面板
        main_splitter = QSplitter(Qt.Horizontal)
        main_splitter.setHandleWidth(1)
        main_splitter.setStyleSheet("QSplitter::handle { background-color: #1c3146; }")
        
        # 左侧：Tab Widget
        self.tabs = QTabWidget()
        self.tabs.setStyleSheet("""
            QTabWidget::pane {
                border: 1px solid #1c3146;
                background: #0b1621;
            }
            QTabBar::tab {
                background: #1c3146;
                color: #8fa6c5;
                padding: 10px 20px;
                margin-right: 2px;
                border-top-left-radius: 4px;
                border-top-right-radius: 4px;
            }
            QTabBar::tab:selected {
                background: #00a0e9;
                color: white;
            }
            QTabBar::tab:hover {
                background: #2c4a69;
            }
        """)
        
        self.tab_config = ConfigControlTab()
        self.tab_display = ImageDisplayTab()
        self.tab_ccd_params = CCDConfigTab()
        
        self.tabs.addTab(self.tab_config, "配置控制页")
        self.tabs.addTab(self.tab_display, "反射光成像显示页")
        self.tabs.addTab(self.tab_ccd_params, "CCD参数配置页")
        
        main_splitter.addWidget(self.tabs)
        
        # 右侧：状态面板（整合到页面中）
        self._setup_status_panel()
        main_splitter.addWidget(self.status_panel_widget)
        
        # 设置右侧状态面板宽度：初始宽度来自配置，但允许用户拖动分割条微调
        from ..config import UI_SETTINGS
        page_key = "reflection_imaging"
        status_panel_width = (
            UI_SETTINGS.get("status_panel_widths", {}).get(page_key)
            or UI_SETTINGS.get("status_panel_width", 400)
        )
        self.status_panel_widget.setMinimumWidth(300)
        self.status_panel_widget.setMaximumWidth(800)
        
        # 设置分割器比例（左侧自适应，右侧固定宽度）
        main_splitter.setStretchFactor(0, 1)
        main_splitter.setStretchFactor(1, 0)
        main_splitter.setCollapsible(1, True)

        # 设置初始分割尺寸（右侧按配置值）
        QTimer.singleShot(0, lambda: main_splitter.setSizes([1200, status_panel_width]))
        
        # 设置分割器尺寸策略
        from PyQt5.QtWidgets import QSizePolicy
        main_splitter.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        
        layout.addWidget(main_splitter, 1)  # 添加拉伸因子
    
    def _setup_worker(self):
        """设置Tango Worker"""
        print(f"[ReflectionImagingPage {_ts()}] _setup_worker() called", flush=True)
        if not PyTango:
            print(f"[ReflectionImagingPage {_ts()}] PyTango not available", flush=True)
            return
        
        # 创建Worker
        device_name = DEVICES.get("reflection", "sys/reflection/1")
        print(f"[ReflectionImagingPage {_ts()}] Creating worker for device: {device_name}", flush=True)
        self._worker = ReflectionImagingWorker(device_name, self)
        
        # 连接信号
        self._worker.command_done.connect(self._on_command_done)
        self._worker.connection_status.connect(self._on_connection_status)
        
        # 设置Worker到Tab页
        self.tab_config.set_worker(self._worker)
        self.tab_ccd_params.set_worker(self._worker)
        
        # 启动Worker
        self._worker.start()
        
        # 创建状态更新定时器（500ms间隔）
        self._status_timer = QTimer(self)
        self._status_timer.timeout.connect(self._update_status)
        self._status_timer.start(500)
        
        # 创建电机状态更新定时器（如果状态面板已创建）
        if hasattr(self, 'motor_list_layout'):
            self._motor_status_timer = QTimer(self)
            self._motor_status_timer.timeout.connect(self._update_motor_status)
            self._motor_status_timer.start(500)  # 500ms更新一次
        
        print(f"[ReflectionImagingPage {_ts()}] Worker setup completed", flush=True)
    
    def _update_status(self):
        """定时更新状态（从Worker缓存拉取）"""
        if not self._worker:
            return
        
        status_data = self._worker.get_cached_status()
        if status_data:
            # 更新上平台位置和状态
            upper_pos_x = status_data.get("upperPlatformPosX", 0.0)
            upper_pos_y = status_data.get("upperPlatformPosY", 0.0)
            upper_pos_z = status_data.get("upperPlatformPosZ", 0.0)
            upper_state_x = bool(status_data.get("upperPlatformStateX", False))
            upper_state_y = bool(status_data.get("upperPlatformStateY", False))
            upper_state_z = bool(status_data.get("upperPlatformStateZ", False))
            self.tab_config.upper_platform.update_status(
                upper_pos_x, upper_pos_y, upper_pos_z,
                upper_state_x, upper_state_y, upper_state_z
            )
            
            # 更新下平台位置和状态
            lower_pos_x = status_data.get("lowerPlatformPosX", 0.0)
            lower_pos_y = status_data.get("lowerPlatformPosY", 0.0)
            lower_pos_z = status_data.get("lowerPlatformPosZ", 0.0)
            lower_state_x = bool(status_data.get("lowerPlatformStateX", False))
            lower_state_y = bool(status_data.get("lowerPlatformStateY", False))
            lower_state_z = bool(status_data.get("lowerPlatformStateZ", False))
            self.tab_config.lower_platform.update_status(
                lower_pos_x, lower_pos_y, lower_pos_z,
                lower_state_x, lower_state_y, lower_state_z
            )
    
    def _on_command_done(self, cmd_name: str, success: bool, message: str):
        """命令完成回调"""
        status_str = "SUCCEEDED" if success else "FAILED"
        print(f"[ReflectionImagingPage {_ts()}] _on_command_done: {cmd_name} {status_str}: {message}", flush=True)
        if not success:
            show_warning("命令执行失败", 
                              f"命令: {cmd_name}\n"
                              f"错误: {message}")
    
    def _on_connection_status(self, connected: bool, message: str):
        """连接状态回调"""
        status_str = "CONNECTED" if connected else "FAILED"
        print(f"[ReflectionImagingPage {_ts()}] _on_connection_status: {status_str}: {message}", flush=True)
    
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
        # 右侧表格左右有8px内边距，这里补同样的左内边距保持对齐
        status_title.setStyleSheet("font-size: 16px; font-weight: bold; color: #00a0e9; margin-bottom: 10px; padding-left: 8px;")
        status_layout.addWidget(status_title)

        # 固定表头区域（只随水平滚动同步）
        self.motor_header_scroll = QScrollArea()
        self.motor_header_scroll.setWidgetResizable(True)
        self.motor_header_scroll.setFrameShape(QFrame.NoFrame)
        self.motor_header_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.motor_header_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        self.motor_header_container = QWidget()
        self.motor_header_layout = QGridLayout(self.motor_header_container)
        self.motor_header_layout.setSpacing(8)
        self.motor_header_layout.setVerticalSpacing(0)
        self.motor_header_layout.setContentsMargins(8, 0, 8, 0)
        # 列配置会在 _init_motor_list() 中根据表头动态设置

        self.motor_header_scroll.setWidget(self.motor_header_container)
        self.motor_header_scroll.setFixedHeight(40)
        status_layout.addWidget(self.motor_header_scroll)
        
        # 电机状态内容区（只滚动内容）
        self.motor_body_scroll = QScrollArea()
        self.motor_body_scroll.setWidgetResizable(True)
        self.motor_body_scroll.setFrameShape(QFrame.NoFrame)
        self.motor_body_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.motor_body_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        
        self.motor_list_container = QWidget()
        self.motor_list_layout = QGridLayout(self.motor_list_container)
        self.motor_list_layout.setSpacing(8)
        self.motor_list_layout.setVerticalSpacing(10)
        self.motor_list_layout.setContentsMargins(8, 8, 8, 8)
        # 列配置会在 _init_motor_list() 中根据表头动态设置
        self.motor_rows = []
        
        self.motor_body_scroll.setWidget(self.motor_list_container)
        status_layout.addWidget(self.motor_body_scroll, 1)  # 添加拉伸因子

        # 水平滚动同步：内容区滚动时，表头同步滚动
        self.motor_body_scroll.horizontalScrollBar().valueChanged.connect(
            self.motor_header_scroll.horizontalScrollBar().setValue
        )
        
        # 立即初始化电机列表（不等待Worker启动）
        self._init_motor_list()
    
    def _init_motor_list(self):
        """初始化电机列表（只读模式）"""
        motors = REFLECTION_IMAGING_CONFIG.get("status_motors", [])
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

        # 表头/内容区列配置保持一致（与靶定位只读表一致）
        col_count = len(headers)
        min_widths = [110, 80, 80, 90, 90]
        stretches = [2, 1, 1, 1, 1]
        for layout_obj in [getattr(self, 'motor_header_layout', None), self.motor_list_layout]:
            if not layout_obj:
                continue
            for col in range(col_count):
                layout_obj.setColumnStretch(col, stretches[col] if col < len(stretches) else 1)
                layout_obj.setColumnMinimumWidth(col, min_widths[col] if col < len(min_widths) else 60)

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
            if col == 0:
                label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
            else:
                label.setAlignment(Qt.AlignCenter)
            if hasattr(self, 'motor_header_layout'):
                self.motor_header_layout.addWidget(label, 0, col, alignment=Qt.AlignCenter)
        
        # 生成新电机行
        for row_idx, motor in enumerate(motors, 0):
            row_obj = MotorStatusRow(motor)
            self.motor_rows.append(row_obj)

            # 只读模式：保持与靶定位页面一致的字体/样式（不强制更小字号）
            row_obj.name_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
            
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
        
        # 获取状态数据
        status_data = self._worker.get_cached_status() if hasattr(self._worker, 'get_cached_status') else None
        if not status_data:
            return
        
        # 遍历所有电机行，更新状态
        for row in self.motor_rows:
            motor_config = row.config
            device_id = motor_config.get("device")
            axis = motor_config.get("axis", 0)
            
            # 反射光成像：上平台（axis 0-2）和下平台（axis 3-5）
            if axis < 3:
                # 上平台
                if axis == 0 and "upperPlatformPosX" in status_data:
                    pos = status_data["upperPlatformPosX"]
                    row.pos_label.setText(f"{pos:.3f}")
                elif axis == 1 and "upperPlatformPosY" in status_data:
                    pos = status_data["upperPlatformPosY"]
                    row.pos_label.setText(f"{pos:.3f}")
                elif axis == 2 and "upperPlatformPosZ" in status_data:
                    pos = status_data["upperPlatformPosZ"]
                    row.pos_label.setText(f"{pos:.3f}")
                
                # 更新编码器（使用位置值）
                if f"upperPlatformPos{['X', 'Y', 'Z'][axis]}" in status_data:
                    pos = status_data[f"upperPlatformPos{['X', 'Y', 'Z'][axis]}"]
                    row.enc_label.setText(f"{pos:.3f}")
                
                # 更新运动状态
                state_key = f"upperPlatformState{['X', 'Y', 'Z'][axis]}"
                if state_key in status_data:
                    is_moving = bool(status_data[state_key])
                    row.state_label.setText("运动中" if is_moving else "静止")
            else:
                # 下平台
                lower_axis = axis - 3
                if lower_axis == 0 and "lowerPlatformPosX" in status_data:
                    pos = status_data["lowerPlatformPosX"]
                    row.pos_label.setText(f"{pos:.3f}")
                elif lower_axis == 1 and "lowerPlatformPosY" in status_data:
                    pos = status_data["lowerPlatformPosY"]
                    row.pos_label.setText(f"{pos:.3f}")
                elif lower_axis == 2 and "lowerPlatformPosZ" in status_data:
                    pos = status_data["lowerPlatformPosZ"]
                    row.pos_label.setText(f"{pos:.3f}")
                
                # 更新编码器（使用位置值）
                if f"lowerPlatformPos{['X', 'Y', 'Z'][lower_axis]}" in status_data:
                    pos = status_data[f"lowerPlatformPos{['X', 'Y', 'Z'][lower_axis]}"]
                    row.enc_label.setText(f"{pos:.3f}")
                
                # 更新运动状态
                state_key = f"lowerPlatformState{['X', 'Y', 'Z'][lower_axis]}"
                if state_key in status_data:
                    is_moving = bool(status_data[state_key])
                    row.state_label.setText("运动中" if is_moving else "静止")
        
    def get_worker(self):
        """获取 Worker（供状态面板使用）"""
        return self._worker
    
    def cleanup(self):
        """清理资源"""
        print(f"[ReflectionImagingPage {_ts()}] cleanup() called", flush=True)
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
        print(f"[ReflectionImagingPage {_ts()}] cleanup: Completed", flush=True)
