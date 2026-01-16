"""
真空腔体系统控制 - 靶定位页面
Target Positioning Page

功能：
1. 靶定位六自由度位姿 (显示/输入/控制)
2. 大行程控制
3. 电机状态显示和控制（整合在页面右侧）
   - 六自由度电机：每个电机独立控制（执行、停止、复位）
   - 大行程电机：只显示状态，不提供控制
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QGroupBox, QLabel, QFrame, QScrollArea, QSplitter,
    QLineEdit, QSizePolicy, QSpacerItem
)
from ..widgets import ControlButton

from ..utils import show_warning
from PyQt5.QtCore import Qt, QThread, QTimer, QMutex, QMutexLocker, pyqtSignal
from collections import deque
import time
import datetime
import math

try:
    import PyTango
except ImportError:
    PyTango = None

from ..config import DEVICES, TARGET_POSITIONING_CONFIG
from ..widgets import ControlButton, ParameterInput, StatusIndicator, ValueDisplay


class MotorStatusRow:
    """电机状态行辅助类（整合到页面中）"""
    def __init__(self, motor_config: dict):
        self.config = motor_config
        self.name_label = QLabel(motor_config["name"])
        self.name_label.setStyleSheet("color: #8fa6c5;")
        
        # 是否为只读模式（大行程电机）
        self.readonly = motor_config.get("readonly", False)
        
        # 输入框（只读模式下不创建）
        if not self.readonly:
            self.input_edit = QLineEdit()
            self.input_edit.setPlaceholderText("0.00")
            self.input_edit.setStyleSheet("background-color: #0c1724; border: 1px solid #1c3146; color: white; padding: 4px;")
        else:
            self.input_edit = None
        
        self.pos_label = QLabel("0.000")
        self.pos_label.setStyleSheet("color: #90EE90;") # 绿色文字
        
        self.enc_label = QLabel("0.000")
        self.enc_label.setStyleSheet("color: #90EE90;")
        
        self.state_label = QLabel("静止")
        self.state_label.setAlignment(Qt.AlignCenter)
        self.state_label.setStyleSheet("background-color: #1c3146; border-radius: 4px; padding: 2px; min-width: 40px;")
        
        self.switch_label = QLabel("---")
        self.switch_label.setAlignment(Qt.AlignCenter)
        
        # 独立控制按钮（只读模式下不创建）
        if not self.readonly:
            self.btn_execute = ControlButton("相对运动", role="move_relative")
            self.btn_execute.setStyleSheet("border-radius: 4px; padding: 4px 8px; font-size: 11px; min-width: 48px;")
            self.btn_stop = ControlButton("停止", role="stop")
            self.btn_stop.setStyleSheet("border-radius: 4px; padding: 4px 8px; font-size: 11px; min-width: 48px;")
            self.btn_reset = ControlButton("复位")
            self.btn_reset.setStyleSheet("border-radius: 4px; padding: 4px 8px; font-size: 11px; min-width: 48px;")

            # 让按钮更紧凑，减少控制列留白
            self.btn_execute.setMinimumWidth(0)
            self.btn_stop.setMinimumWidth(0)
            self.btn_reset.setMinimumWidth(0)
        else:
            self.btn_execute = None
            self.btn_stop = None
            self.btn_reset = None


def _ts():
    """返回当前时间戳字符串，精确到毫秒"""
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


class LargeStrokeWorker(QThread):
    """大行程设备后台通信线程
    
    设计模式（参考vacuum_control.py的TangoWorker）：
    - 纯轮询模式，避免高频信号导致UI卡顿
    - 状态数据缓存在Worker中，UI定时拉取
    - 使用QMutex保护共享数据
    - 零等待连接检查：使用 _connection_healthy 标志，命令执行时只检查标志
    """
    # 命令完成信号（低频）
    command_done = pyqtSignal(str, bool, str)  # cmd_name, success, message
    connection_status = pyqtSignal(bool, str)  # connected, message
    
    def __init__(self, device_name="sys/large_stroke/1", parent=None):
        super().__init__(parent)
        self.device_name = device_name
        self.device = None
        self._running = True
        self._stop_requested = False
        self._command_queue = deque()
        self._queue_mutex = QMutex()
        self._read_attrs = [
            "largeRangePos",      # 当前位置
            "LargeRangeState",    # 运动状态
            "hostPlugState",      # 闸板阀状态
            "State",              # 设备状态
            "positionUnit",       # 位置单位（用于动态显示单位标签）
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

        # 命令队列保护：断连/超时场景下，避免用户连点堆积导致“延时后连弹很多错误框”
        # 大行程运动类命令通常是互斥/不需要排队太多，默认只允许少量排队
        self._max_queue_size = 2

    def _clear_command_queue(self, reason: str = ""):
        """清空待执行命令队列（线程安全）。"""
        with QMutexLocker(self._queue_mutex):
            dropped = len(self._command_queue)
            self._command_queue.clear()
        if dropped > 0:
            print(f"[LargeStrokeWorker {_ts()}] Cleared {dropped} queued commands. reason={reason}", flush=True)
        
    def queue_command(self, cmd_name: str, args=None):
        """将命令加入队列（线程安全）"""
        print(f"[LargeStrokeWorker {_ts()}] queue_command: {cmd_name}({args})", flush=True)

        # 如果设备尚未连接（正在重连），不要把命令静默排队：立即反馈失败。
        # 否则会出现“第一次点击无弹框，第二次才弹”的体验。
        if not self.device:
            self._connection_healthy = False
            self.command_done.emit(cmd_name, False, "设备未连接（正在重连），请稍后重试或检查运动控制器/网络")
            return

        # 连接不健康时，直接拒绝排队（避免积压）；由UI层做友好提示
        # 注意：这里不做任何网络操作，只读标志位。
        if not self._connection_healthy and cmd_name not in ["stop", "reset", "init"]:
            print(f"[LargeStrokeWorker {_ts()}] Rejecting command due to unhealthy connection: {cmd_name}", flush=True)
            self.command_done.emit(cmd_name, False, "设备连接不健康，请等待自动重连或检查运动控制器网络连接")
            return

        with QMutexLocker(self._queue_mutex):
            # 队列限流：超过上限则丢弃新命令（避免连点堆积）
            if len(self._command_queue) >= self._max_queue_size:
                print(
                    f"[LargeStrokeWorker {_ts()}] Command queue full (size={len(self._command_queue)}), dropping: {cmd_name}",
                    flush=True,
                )
                self.command_done.emit(cmd_name, False, "命令队列繁忙，请稍后重试")
                return
            self._command_queue.append((cmd_name, args))
            queue_size = len(self._command_queue)
            print(f"[LargeStrokeWorker {_ts()}] Command queued. Queue size: {queue_size}", flush=True)
    
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
        print(f"[LargeStrokeWorker {_ts()}] Requesting stop...", flush=True)
        self._stop_requested = True
        self._running = False
        
        # 等待线程结束（最多5秒）
        if not self.wait(5000):
            print(f"[LargeStrokeWorker {_ts()}] Warning: Thread did not stop in time, terminating...", flush=True)
            self.terminate()
            self.wait(1000)
        
        print(f"[LargeStrokeWorker {_ts()}] Worker stopped.", flush=True)
    
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
            # 设置适当的超时时间（2000ms），因为属性读取可能涉及多个网络调用（编码器等）
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
                        # 只在调试模式下输出成功读取的日志
                        # print(f"[LargeStrokeWorker {_ts()}] Poll: {attr.name} = {attr.value}", flush=True)
                    else:
                        # 失败时，使用计数器，每100次才输出一次
                        if attr.name not in self._fail_counters:
                            self._fail_counters[attr.name] = 0
                        self._fail_counters[attr.name] += 1
                        if self._fail_counters[attr.name] % 100 == 1:  # 只在第1次和第100次、200次等输出
                            err_msg = ""
                            try:
                                err_stack = attr.get_err_stack()
                                if err_stack and len(err_stack) > 0:
                                    err_msg = f" - {err_stack[0].reason}: {err_stack[0].desc}"
                            except Exception:
                                pass
                            print(
                                f"[LargeStrokeWorker {_ts()}] Poll: {attr.name} FAILED (count={self._fail_counters[attr.name]}){err_msg}",
                                flush=True,
                            )
                except Exception as e:
                    # 异常也使用计数器
                    if attr.name not in self._fail_counters:
                        self._fail_counters[attr.name] = 0
                    self._fail_counters[attr.name] += 1
                    if self._fail_counters[attr.name] % 100 == 1:
                        print(f"[LargeStrokeWorker {_ts()}] Poll: Error reading {attr.name}: {e} (count={self._fail_counters[attr.name]})", flush=True)

            # 若本轮 0 个属性读取成功（典型：API_AttrValueNotSet），不要把连接视为“健康”。
            # 这种情况 read_attributes() 没抛异常，但对 UI 来说数据不可用。
            if successful_attrs == 0:
                self._consecutive_failures += 1
                if self._consecutive_failures >= self._max_failures_before_unhealthy:
                    self._connection_healthy = False
            else:
                # 本轮至少有一个属性成功：重置连续失败计数
                self._consecutive_failures = 0
            
            # 读取Status属性以检测模拟模式（低频，每3次轮询读取一次，以提高响应速度）
            if not hasattr(self, '_status_poll_counter'):
                self._status_poll_counter = 0
            self._status_poll_counter += 1
            if self._status_poll_counter >= 3:  # 每3次轮询读取一次Status
                self._status_poll_counter = 0
                try:
                    status_str = self.device.status()
                    status_data["_device_status"] = status_str  # 使用特殊键名避免冲突
                    # 只在状态变化时输出
                    if not hasattr(self, '_last_status'):
                        self._last_status = None
                    if status_str != self._last_status:
                        print(f"[LargeStrokeWorker {_ts()}] Poll: Status changed: {self._last_status} -> {status_str}", flush=True)
                        self._last_status = status_str
                except Exception as e:
                    print(f"[LargeStrokeWorker {_ts()}] Poll: Error reading Status: {e}", flush=True)
            
            if status_data:
                self._update_cache(status_data)

                # 关键：把“连接健康”定义为“设备可执行运动命令”，避免 motion controller 断连时
                # 因为自动 init/超时等待而出现“点击后延时才报错”。
                try:
                    dev_state = status_data.get("State")
                    dev_status = status_data.get("_device_status", "") or ""
                    if dev_state is not None:
                        healthy = dev_state in (PyTango.DevState.ON, PyTango.DevState.MOVING)
                        status_l = str(dev_status).lower()
                        if ("not connected" in status_l) or ("network connection lost" in status_l):
                            healthy = False
                        if healthy != self._connection_healthy:
                            print(
                                f"[LargeStrokeWorker {_ts()}] Health changed by State/Status: {self._connection_healthy} -> {healthy} (State={dev_state}, Status={dev_status})",
                                flush=True,
                            )
                        self._connection_healthy = healthy
                except Exception:
                    pass
                # 只在调试模式下输出更新日志
                # print(f"[LargeStrokeWorker {_ts()}] Poll: Updated {len(status_data)} attributes", flush=True)
        except Exception as e:
            # 轮询失败，增加失败计数
            self._consecutive_failures += 1
            # 轮询失败就认为不适合执行运动命令（fail-fast）。
            if self._connection_healthy:
                print(
                    f"[LargeStrokeWorker {_ts()}] Connection became unhealthy after poll failure (count={self._consecutive_failures}): {e}",
                    flush=True,
                )
            self._connection_healthy = False
            
            # 只在首次失败时输出详细日志
            if self._consecutive_failures == 1:
                print(f"[LargeStrokeWorker {_ts()}] Poll failed: {e}", flush=True)
                import traceback
                traceback.print_exc()
            
            self.device = None
    
    def _attempt_connect(self):
        """尝试连接设备，失败不抛异常"""
        if self._stop_requested:
            print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Stop requested, aborting", flush=True)
            return False
            
        try:
            print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Connecting to {self.device_name}...", flush=True)
            dev = PyTango.DeviceProxy(self.device_name)
            print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: DeviceProxy created", flush=True)
            # 使用较短的超时（1秒）加快失败检测
            dev.set_timeout_millis(1000)
            print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Timeout set to 1000ms", flush=True)
            state = dev.state()
            print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Device state = {state}", flush=True)
            
            if self._stop_requested:
                print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Stop requested after state check", flush=True)
                return False
                
            self.device = dev
            # 连接成功：能连上 server 不代表运动控制器已连接。
            self._connection_healthy = state in (PyTango.DevState.ON, PyTango.DevState.MOVING)
            self._consecutive_failures = 0
            
            status_msg = f"Connected (state: {state})"
            print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Emitting connection_status signal: {status_msg}", flush=True)
            self.connection_status.emit(True, status_msg)
            # 首次轮询填充缓存
            if self._read_attrs:
                print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Performing initial poll with {len(self._read_attrs)} attributes", flush=True)
                self._poll_attributes()
            # 连接后立即读取一次 Status，避免 UI 初始显示"真实模式"但随后才刷新为"模拟模式"
            # （否则用户会误以为点击运动按钮触发了模式切换）
            try:
                status_str = dev.status()
                self._update_cache({"_device_status": status_str})
                if not hasattr(self, '_last_status'):
                    self._last_status = None
                if status_str != self._last_status:
                    print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Status = {status_str}", flush=True)
                    self._last_status = status_str
            except Exception as e:
                print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Failed to read Status after connect: {e}", flush=True)
            print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Connection successful, connection_healthy=True", flush=True)
            return True
        except Exception as e:
            self.device = None
            # 连接失败，标记连接不健康
            self._connection_healthy = False
            
            error_msg = str(e)
            print(f"[LargeStrokeWorker {_ts()}] _attempt_connect: Connection failed - {error_msg}", flush=True)
            import traceback
            traceback.print_exc()
            self.connection_status.emit(False, error_msg)
            return False

    def run(self):
        """主循环：纯轮询模式"""
        if not PyTango:
            print(f"[LargeStrokeWorker {_ts()}] PyTango not available", flush=True)
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
        
        print(f"[LargeStrokeWorker {_ts()}] Worker thread exiting.", flush=True)
    
    def _check_and_init_if_needed(self, cmd_name):
        """检查设备状态，如果是UNKNOWN/OFF/FAULT且命令需要ON状态，则自动初始化"""
        # 需要ON状态的命令列表
        require_on_commands = ["moveRelative", "moveAbsolute", "largeMoveAuto", "openValue", "runAction"]
        
        if cmd_name not in require_on_commands:
            return True, None  # 不需要检查
        
        try:
            # 使用较短的超时（500ms）加快失败检测
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(500)
            try:
                state = self.device.state()
            finally:
                self.device.set_timeout_millis(old_timeout)
            
            state_name = PyTango.DevStateName[state] if hasattr(PyTango, 'DevStateName') else str(state)
            print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Device state = {state} ({state_name})", flush=True)
            
            # 如果已经是ON状态，直接返回
            if state == PyTango.DevState.ON:
                print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Device is already ON", flush=True)
                return True, None
            
            # 大行程：FAULT/UNKNOWN 时不要自动 init（这会阻塞等待，造成延时提示）。
            if state in [PyTango.DevState.FAULT, PyTango.DevState.UNKNOWN]:
                self._connection_healthy = False
                return False, f"设备当前状态为 {state_name}，可能运动控制器未连接/网络异常。请检查网络后点击‘复位’。"

            # 仅在 OFF 时做一次“快速 init”，不循环等待
            if state == PyTango.DevState.OFF:
                print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Device state is OFF, attempting fast init...", flush=True)
                try:
                    init_old_timeout = self.device.get_timeout_millis()
                    self.device.set_timeout_millis(800)
                    try:
                        self.device.command_inout("init")
                        print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Init command sent", flush=True)
                    finally:
                        self.device.set_timeout_millis(init_old_timeout)

                    # 不做循环等待，避免阻塞；只快速检查一次
                    self.device.set_timeout_millis(500)
                    try:
                        new_state = self.device.state()
                    finally:
                        self.device.set_timeout_millis(init_old_timeout)
                    if new_state == PyTango.DevState.ON:
                        self._connection_healthy = True
                        return True, None
                    new_state_name = PyTango.DevStateName[new_state] if hasattr(PyTango, 'DevStateName') else str(new_state)
                    self._connection_healthy = False
                    return False, f"设备初始化未就绪（当前状态 {new_state_name}）。请稍后重试或点击‘复位’。"
                    
                except PyTango.DevFailed as init_e:
                    error_detail = str(init_e)
                    if len(init_e.args) > 0 and hasattr(init_e.args[0], '__iter__') and len(init_e.args[0]) > 0:
                        error_detail = init_e.args[0][0].desc
                    error_msg = f"初始化命令执行失败: {error_detail}"
                    print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Auto-init command failed: {error_msg}", flush=True)
                    # 如果是超时错误，标记连接为不健康
                    if "timeout" in error_detail.lower():
                        self._connection_healthy = False
                        self.device = None
                        print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Timeout detected, marking connection unhealthy", flush=True)
                    import traceback
                    traceback.print_exc()
                    return False, error_msg
                except Exception as init_e:
                    error_msg = f"初始化过程出错: {str(init_e)}"
                    print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Auto-init exception: {error_msg}", flush=True)
                    # 如果是超时错误，标记连接为不健康
                    if "timeout" in str(init_e).lower():
                        self._connection_healthy = False
                        self.device = None
                        print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Timeout detected, marking connection unhealthy", flush=True)
                    import traceback
                    traceback.print_exc()
                    return False, error_msg
            else:
                # 其他状态（如MOVING）不允许初始化
                error_msg = f"设备当前状态为 {state_name}，无法执行初始化。请等待设备就绪。"
                print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: {error_msg}", flush=True)
                return False, error_msg
        except Exception as e:
            error_msg = f"检查设备状态时出错: {str(e)}"
            print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Error checking state: {error_msg}", flush=True)
            # 如果是超时错误，标记连接为不健康
            if "timeout" in str(e).lower():
                self._connection_healthy = False
                self.device = None
                print(f"[LargeStrokeWorker {_ts()}] _check_and_init_if_needed: Timeout detected, marking connection unhealthy", flush=True)
            import traceback
            traceback.print_exc()
            return False, error_msg
    
    def _execute_command(self, cmd_name, args):
        """执行Tango命令（带零等待连接检查）"""
        # 关键命令白名单：stop/reset/init 即使在连接不健康时也应该尝试执行
        critical_commands = ["stop", "reset", "init"]
        is_critical = cmd_name in critical_commands
        
        if not self.device:
            error_msg = "Device not connected"
            print(f"[LargeStrokeWorker {_ts()}] _execute_command: {cmd_name} FAILED - {error_msg}", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
        
        if self._stop_requested:
            error_msg = "Worker stopping"
            print(f"[LargeStrokeWorker {_ts()}] _execute_command: {cmd_name} CANCELLED - {error_msg}", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
        
        # 零等待连接检查：检查后台更新的连接健康标志（纯内存操作，不做网络调用）
        # 注意：stop/reset/init 等关键命令跳过此检查，允许在连接不健康时尝试执行
        if not self._connection_healthy and not is_critical:
            error_msg = "设备连接不健康，请等待自动重连或手动重启设备服务器"
            print(f"[LargeStrokeWorker {_ts()}] _execute_command: {cmd_name} FAILED - Connection unhealthy (zero-wait check)", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
        
        # 检查设备状态，如果需要ON状态但当前不是ON，尝试自动初始化
        check_ok, check_error = self._check_and_init_if_needed(cmd_name)
        if not check_ok:
            error_msg = check_error if check_error else f"Device state check failed for command {cmd_name}. Please initialize device first."
            print(f"[LargeStrokeWorker {_ts()}] _execute_command: {cmd_name} FAILED - {error_msg}", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
            
        try:
            print(f"[LargeStrokeWorker {_ts()}] _execute_command: Starting {cmd_name}({args})", flush=True)
            
            # 设置命令超时（1秒，之前是3秒，缩短以加快失败检测）
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(1000)
            print(f"[LargeStrokeWorker {_ts()}] _execute_command: Timeout set to 1000ms (was {old_timeout}ms)", flush=True)
            
            try:
                if args is not None and args != [] and args != ():
                    print(f"[LargeStrokeWorker {_ts()}] _execute_command: Calling command_inout with args", flush=True)
                    result = self.device.command_inout(cmd_name, args)
                else:
                    print(f"[LargeStrokeWorker {_ts()}] _execute_command: Calling command_inout without args", flush=True)
                    result = self.device.command_inout(cmd_name)
            finally:
                self.device.set_timeout_millis(old_timeout)
                print(f"[LargeStrokeWorker {_ts()}] _execute_command: Timeout restored to {old_timeout}ms", flush=True)
                
            print(f"[LargeStrokeWorker {_ts()}] _execute_command: {cmd_name} SUCCEEDED, result={result}", flush=True)
            
            # 如果是simSwitch命令，立即读取Status属性以更新模拟模式状态显示
            if cmd_name == "simSwitch":
                try:
                    print(f"[LargeStrokeWorker {_ts()}] _execute_command: simSwitch succeeded, immediately reading Status", flush=True)
                    status_str = self.device.status()
                    status_data = {"_device_status": status_str}
                    self._update_cache(status_data)
                    print(f"[LargeStrokeWorker {_ts()}] _execute_command: Status updated immediately: {status_str}", flush=True)
                except Exception as status_e:
                    print(f"[LargeStrokeWorker {_ts()}] _execute_command: Failed to read Status immediately: {status_e}", flush=True)
                    # 不影响命令成功状态
            
            self.command_done.emit(cmd_name, True, str(result))
        except Exception as e:
            error_msg = str(e)
            print(f"[LargeStrokeWorker {_ts()}] _execute_command: {cmd_name} FAILED - {error_msg}", flush=True)
            import traceback
            traceback.print_exc()
            
            # 如果错误是因为状态不对，尝试初始化后再提示
            if "not allowed in" in error_msg and "state" in error_msg.lower():
                print(f"[LargeStrokeWorker {_ts()}] _execute_command: State error detected, suggesting init", flush=True)
                error_msg = f"{error_msg}\n\n提示：设备需要先初始化。请点击'复位'按钮或等待自动初始化完成。"
            
            self.command_done.emit(cmd_name, False, error_msg)
            # 如果是超时错误，可能需要重连
            if "timeout" in error_msg.lower():
                print(f"[LargeStrokeWorker {_ts()}] _execute_command: Timeout detected, marking device for reconnect", flush=True)
                self.device = None
                self._connection_healthy = False
                # 发生超时后，清空后续排队命令，避免延时后连续弹出很多失败提示
                self._clear_command_queue(reason="timeout")
            # 网络/硬件断开类错误：清空排队，避免错误提示堆积
            elif "not connected" in error_msg.lower() or "connection" in error_msg.lower():
                self._connection_healthy = False
                self._clear_command_queue(reason="connection error")


class SixDofWorker(QThread):
    """六自由度设备后台通信线程
    
    设计模式（参考LargeStrokeWorker）：
    - 纯轮询模式，避免高频信号导致UI卡顿
    - 状态数据缓存在Worker中，UI定时拉取
    - 使用QMutex保护共享数据
    - 零等待连接检查：使用 _connection_healthy 标志，命令执行时只检查标志
    - 关键命令白名单：stop/reset 即使在连接不健康时也允许执行
    """
    # 命令完成信号（低频）
    command_done = pyqtSignal(str, bool, str)  # cmd_name, success, message
    connection_status = pyqtSignal(bool, str)  # connected, message
    
    # 关键命令白名单：这些命令即使在连接不健康时也应该尝试执行
    CRITICAL_COMMANDS = ["stop", "reset"]
    
    def __init__(self, device_name="sys/six_dof/1", parent=None):
        super().__init__(parent)
        self.device_name = device_name
        self.device = None
        self._running = True
        self._stop_requested = False
        self._command_queue = deque()
        self._queue_mutex = QMutex()
        self._read_attrs = [
            "sixFreedomPose",  # 六自由度位姿 [X, Y, Z, ThetaX, ThetaY, ThetaZ]
            "axisPos",          # 各轴编码器位置（数组，用于状态面板显示）
            "sdofState",        # 各轴运动状态（数组）
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
            print(f"[SixDofWorker {_ts()}] Cleared {dropped} queued commands. reason={reason}", flush=True)
        
    def queue_command(self, cmd_name: str, args=None):
        """将命令加入队列（线程安全）"""
        print(f"[SixDofWorker {_ts()}] queue_command: {cmd_name}({args})", flush=True)

        # 如果设备尚未连接（正在重连），不要把命令静默排队：立即反馈失败。
        if not self.device:
            self._connection_healthy = False
            self.command_done.emit(cmd_name, False, "设备未连接（正在重连），请稍后重试或检查设备服务器/网络")
            return

        # 连接不健康时，直接拒绝排队（避免积压）；由UI层做友好提示
        # 注意：这里不做任何网络操作，只读标志位。
        if not self._connection_healthy and cmd_name not in ["stop", "reset"]:
            print(f"[SixDofWorker {_ts()}] Rejecting command due to unhealthy connection: {cmd_name}", flush=True)
            self.command_done.emit(cmd_name, False, "设备连接不健康，请等待自动重连或检查设备网络连接")
            return

        with QMutexLocker(self._queue_mutex):
            # 队列限流：超过上限则丢弃新命令（避免连点堆积）
            if len(self._command_queue) >= self._max_queue_size:
                print(
                    f"[SixDofWorker {_ts()}] Command queue full (size={len(self._command_queue)}), dropping: {cmd_name}",
                    flush=True,
                )
                self.command_done.emit(cmd_name, False, "命令队列繁忙，请稍后重试")
                return
            self._command_queue.append((cmd_name, args))
            queue_size = len(self._command_queue)
            print(f"[SixDofWorker {_ts()}] Command queued. Queue size: {queue_size}", flush=True)
    
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
        print(f"[SixDofWorker {_ts()}] Requesting stop...", flush=True)
        self._stop_requested = True
        self._running = False
        
        # 等待线程结束（最多5秒）
        if not self.wait(5000):
            print(f"[SixDofWorker {_ts()}] Warning: Thread did not stop in time, terminating...", flush=True)
            self.terminate()
            self.wait(1000)
        
        print(f"[SixDofWorker {_ts()}] Worker stopped.", flush=True)
    
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
            # 设置适当的超时时间（2000ms），因为属性读取可能涉及多个网络调用（编码器等）
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
                            err_msg = ""
                            try:
                                err_stack = attr.get_err_stack()
                                if err_stack and len(err_stack) > 0:
                                    err_msg = f" - {err_stack[0].reason}: {err_stack[0].desc}"
                            except Exception:
                                pass
                            print(
                                f"[SixDofWorker {_ts()}] Poll: {attr.name} FAILED (count={self._fail_counters[attr.name]}){err_msg}",
                                flush=True,
                            )
                except Exception as e:
                    # 异常也使用计数器
                    if attr.name not in self._fail_counters:
                        self._fail_counters[attr.name] = 0
                    self._fail_counters[attr.name] += 1
                    if self._fail_counters[attr.name] % 100 == 1:
                        print(f"[SixDofWorker {_ts()}] Poll: Error reading {attr.name}: {e} (count={self._fail_counters[attr.name]})", flush=True)

            # 连接健康判定：至少有一个属性读成功才算“数据可用”。
            if successful_attrs == 0:
                self._consecutive_failures += 1
                if self._consecutive_failures >= self._max_failures_before_unhealthy:
                    if self._connection_healthy:
                        print(
                            f"[SixDofWorker {_ts()}] Connection became unhealthy (no attributes updated, failures={self._consecutive_failures})",
                            flush=True,
                        )
                    self._connection_healthy = False
            else:
                self._consecutive_failures = 0
                if not self._connection_healthy:
                    print(f"[SixDofWorker {_ts()}] Connection became healthy", flush=True)
                self._connection_healthy = True
            
            # 读取Status属性以检测模拟模式（低频，每3次轮询读取一次）
            if not hasattr(self, '_status_poll_counter'):
                self._status_poll_counter = 0
            self._status_poll_counter += 1
            if self._status_poll_counter >= 3:  # 每3次轮询读取一次Status
                self._status_poll_counter = 0
                try:
                    status_str = self.device.status()
                    status_data["_device_status"] = status_str  # 使用特殊键名避免冲突
                    # 只在状态变化时输出
                    if not hasattr(self, '_last_status'):
                        self._last_status = None
                    if status_str != self._last_status:
                        print(f"[SixDofWorker {_ts()}] Poll: Status changed: {self._last_status} -> {status_str}", flush=True)
                        self._last_status = status_str
                except Exception as e:
                    print(f"[SixDofWorker {_ts()}] Poll: Error reading Status: {e}", flush=True)
            
            if status_data:
                self._update_cache(status_data)
        except Exception as e:
            # 轮询失败，增加失败计数
            self._consecutive_failures += 1
            if self._consecutive_failures >= self._max_failures_before_unhealthy:
                if self._connection_healthy:
                    print(f"[SixDofWorker {_ts()}] Connection became unhealthy after {self._consecutive_failures} failures: {e}", flush=True)
                    self._connection_healthy = False
            
            # 只在首次失败时输出详细日志
            if self._consecutive_failures == 1:
                print(f"[SixDofWorker {_ts()}] Poll failed: {e}", flush=True)
                import traceback
                traceback.print_exc()
            
            self.device = None
    
    def _attempt_connect(self):
        """尝试连接设备，失败不抛异常"""
        if self._stop_requested:
            print(f"[SixDofWorker {_ts()}] _attempt_connect: Stop requested, aborting", flush=True)
            return False
            
        try:
            print(f"[SixDofWorker {_ts()}] _attempt_connect: Connecting to {self.device_name}...", flush=True)
            dev = PyTango.DeviceProxy(self.device_name)
            print(f"[SixDofWorker {_ts()}] _attempt_connect: DeviceProxy created", flush=True)
            # 使用较短的超时（1秒）加快失败检测
            dev.set_timeout_millis(1000)
            print(f"[SixDofWorker {_ts()}] _attempt_connect: Timeout set to 1000ms", flush=True)
            state = dev.state()
            print(f"[SixDofWorker {_ts()}] _attempt_connect: Device state = {state}", flush=True)
            
            if self._stop_requested:
                print(f"[SixDofWorker {_ts()}] _attempt_connect: Stop requested after state check", flush=True)
                return False
                
            self.device = dev
            # 连接成功：先按 state 初判；最终以首次轮询结果为准
            self._connection_healthy = state in (PyTango.DevState.ON, PyTango.DevState.MOVING)
            self._consecutive_failures = 0
            
            status_msg = f"Connected (state: {state})"
            print(f"[SixDofWorker {_ts()}] _attempt_connect: Emitting connection_status signal: {status_msg}", flush=True)
            self.connection_status.emit(True, status_msg)
            # 首次轮询填充缓存
            if self._read_attrs:
                print(f"[SixDofWorker {_ts()}] _attempt_connect: Performing initial poll with {len(self._read_attrs)} attributes", flush=True)
                self._poll_attributes()
            print(f"[SixDofWorker {_ts()}] _attempt_connect: Connection successful, connection_healthy=True", flush=True)
            return True
        except Exception as e:
            self.device = None
            # 连接失败，标记连接不健康
            self._connection_healthy = False
            
            error_msg = str(e)
            print(f"[SixDofWorker {_ts()}] _attempt_connect: Connection failed - {error_msg}", flush=True)
            import traceback
            traceback.print_exc()
            self.connection_status.emit(False, error_msg)
            return False

    def run(self):
        """主循环：纯轮询模式"""
        if not PyTango:
            print(f"[SixDofWorker {_ts()}] PyTango not available", flush=True)
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
        
        print(f"[SixDofWorker {_ts()}] Worker thread exiting.")
    
    def _check_and_init_if_needed(self, cmd_name):
        """检查设备状态，如果是UNKNOWN/OFF/FAULT且命令需要ON状态，则自动初始化"""
        # 需要ON状态的命令列表
        require_on_commands = ["movePoseAbsolute", "movePoseRelative", "sixMoveZero"]
        
        if cmd_name not in require_on_commands:
            return True, None  # 不需要检查
        
        try:
            # 使用较短的超时（500ms）加快失败检测
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(500)
            try:
                state = self.device.state()
            finally:
                self.device.set_timeout_millis(old_timeout)
            
            state_name = PyTango.DevStateName[state] if hasattr(PyTango, 'DevStateName') else str(state)
            print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Device state = {state} ({state_name})", flush=True)
            
            # 如果已经是ON状态，直接返回
            if state == PyTango.DevState.ON:
                print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Device is already ON", flush=True)
                return True, None
            
            # 如果状态允许初始化（UNKNOWN/OFF/FAULT），自动初始化
            if state in [PyTango.DevState.UNKNOWN, PyTango.DevState.OFF, PyTango.DevState.FAULT]:
                print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Device state is {state_name}, attempting auto-init...", flush=True)
                try:
                    # 尝试调用init命令（使用较短超时，加快失败检测）
                    init_old_timeout = self.device.get_timeout_millis()
                    self.device.set_timeout_millis(2000)  # init 超时设为2秒（之前是5秒）
                    try:
                        self.device.command_inout("init")
                        print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Init command sent", flush=True)
                    finally:
                        self.device.set_timeout_millis(old_timeout)
                    
                    # 等待初始化完成，最多等待2秒
                    import time
                    max_wait = 2.0
                    wait_interval = 0.2
                    waited = 0.0
                    while waited < max_wait:
                        time.sleep(wait_interval)
                        waited += wait_interval
                        new_state = self.device.state()
                        new_state_name = PyTango.DevStateName[new_state] if hasattr(PyTango, 'DevStateName') else str(new_state)
                        print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: After {waited:.1f}s, state = {new_state} ({new_state_name})", flush=True)
                        if new_state == PyTango.DevState.ON:
                            print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Auto-init successful", flush=True)
                            return True, None
                        elif new_state == PyTango.DevState.FAULT:
                            print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Device entered FAULT state after init", flush=True)
                            return False, f"设备初始化失败，当前状态为 FAULT。请检查设备连接和配置。"
                    
                    # 超时仍未变为ON
                    final_state = self.device.state()
                    final_state_name = PyTango.DevStateName[final_state] if hasattr(PyTango, 'DevStateName') else str(final_state)
                    error_msg = f"设备初始化超时，当前状态仍为 {final_state_name}。请手动点击'复位'按钮初始化。"
                    print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: {error_msg}", flush=True)
                    return False, error_msg
                    
                except PyTango.DevFailed as init_e:
                    error_detail = str(init_e)
                    if len(init_e.args) > 0 and hasattr(init_e.args[0], '__iter__') and len(init_e.args[0]) > 0:
                        error_detail = init_e.args[0][0].desc
                    error_msg = f"初始化命令执行失败: {error_detail}"
                    print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Auto-init command failed: {error_msg}", flush=True)
                    # 如果是超时错误，标记连接为不健康
                    if "timeout" in error_detail.lower():
                        self._connection_healthy = False
                        self.device = None
                        print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Timeout detected, marking connection unhealthy", flush=True)
                    import traceback
                    traceback.print_exc()
                    return False, error_msg
                except Exception as init_e:
                    error_msg = f"初始化过程出错: {str(init_e)}"
                    print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Auto-init exception: {error_msg}", flush=True)
                    # 如果是超时错误，标记连接为不健康
                    if "timeout" in str(init_e).lower():
                        self._connection_healthy = False
                        self.device = None
                        print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Timeout detected, marking connection unhealthy", flush=True)
                    import traceback
                    traceback.print_exc()
                    return False, error_msg
            else:
                # 其他状态（如MOVING）不允许初始化
                error_msg = f"设备当前状态为 {state_name}，无法执行初始化。请等待设备就绪。"
                print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: {error_msg}", flush=True)
                return False, error_msg
        except Exception as e:
            error_msg = f"检查设备状态时出错: {str(e)}"
            print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Error checking state: {error_msg}", flush=True)
            # 如果是超时错误，标记连接为不健康
            if "timeout" in str(e).lower():
                self._connection_healthy = False
                self.device = None
                print(f"[SixDofWorker {_ts()}] _check_and_init_if_needed: Timeout detected, marking connection unhealthy", flush=True)
            import traceback
            traceback.print_exc()
            return False, error_msg
    
    def _execute_command(self, cmd_name, args):
        """执行Tango命令（带零等待连接检查）"""
        # 关键命令白名单：stop/reset 即使在连接不健康时也应该尝试执行
        is_critical = cmd_name in self.CRITICAL_COMMANDS
        
        if not self.device:
            error_msg = "Device not connected"
            print(f"[SixDofWorker {_ts()}] _execute_command: {cmd_name} FAILED - {error_msg}", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
        
        if self._stop_requested:
            error_msg = "Worker stopping"
            print(f"[SixDofWorker {_ts()}] _execute_command: {cmd_name} CANCELLED - {error_msg}", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
        
        # 零等待连接检查：检查后台更新的连接健康标志（纯内存操作，不做网络调用）
        # 注意：stop/reset 等关键命令跳过此检查，允许在连接不健康时尝试执行
        if not self._connection_healthy and not is_critical:
            error_msg = "设备连接不健康，请等待自动重连或手动重启设备服务器"
            print(f"[SixDofWorker {_ts()}] _execute_command: {cmd_name} FAILED - Connection unhealthy (zero-wait check)", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
        
        # 检查设备状态，如果需要ON状态但当前不是ON，尝试自动初始化
        check_ok, check_error = self._check_and_init_if_needed(cmd_name)
        if not check_ok:
            error_msg = check_error if check_error else f"Device state check failed for command {cmd_name}. Please initialize device first."
            print(f"[SixDofWorker {_ts()}] _execute_command: {cmd_name} FAILED - {error_msg}", flush=True)
            self.command_done.emit(cmd_name, False, error_msg)
            return
            
        try:
            print(f"[SixDofWorker {_ts()}] _execute_command: Starting {cmd_name}({args})", flush=True)
            
            # 设置命令超时（2秒，之前是5秒，缩短以加快失败检测）
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(2000)
            print(f"[SixDofWorker {_ts()}] _execute_command: Timeout set to 2000ms (was {old_timeout}ms)", flush=True)
            
            try:
                if args is not None and args != [] and args != ():
                    print(f"[SixDofWorker {_ts()}] _execute_command: Calling command_inout with args", flush=True)
                    result = self.device.command_inout(cmd_name, args)
                else:
                    print(f"[SixDofWorker {_ts()}] _execute_command: Calling command_inout without args", flush=True)
                    result = self.device.command_inout(cmd_name)
            finally:
                self.device.set_timeout_millis(old_timeout)
                print(f"[SixDofWorker {_ts()}] _execute_command: Timeout restored to {old_timeout}ms", flush=True)
                
            print(f"[SixDofWorker {_ts()}] _execute_command: {cmd_name} SUCCEEDED, result={result}", flush=True)
            self.command_done.emit(cmd_name, True, str(result))
        except Exception as e:
            error_msg = str(e)
            print(f"[SixDofWorker {_ts()}] _execute_command: {cmd_name} FAILED - {error_msg}", flush=True)
            import traceback
            traceback.print_exc()
            
            # 如果错误是因为状态不对，尝试初始化后再提示
            if "not allowed in" in error_msg and "state" in error_msg.lower():
                print(f"[SixDofWorker {_ts()}] _execute_command: State error detected, suggesting init", flush=True)
                error_msg = f"{error_msg}\n\n提示：设备需要先初始化。请点击'复位'按钮或等待自动初始化完成。"
            
            self.command_done.emit(cmd_name, False, error_msg)
            # 如果是超时错误，标记连接不健康并清理设备
            if "timeout" in error_msg.lower():
                print(f"[SixDofWorker {_ts()}] _execute_command: Timeout detected, marking connection unhealthy", flush=True)
                self._connection_healthy = False
                self.device = None
                # 发生超时后，清空后续排队命令，避免延时后连续弹出很多失败提示
                self._clear_command_queue(reason="timeout")
            # 网络/硬件断开类错误：清空排队，避免错误提示堆积
            elif "not connected" in error_msg.lower() or "connection" in error_msg.lower():
                self._connection_healthy = False
                self._clear_command_queue(reason="connection error")


class PoseDisplayWidget(QGroupBox):
    """六自由度位姿显示组件"""
    def __init__(self, parent=None):
        super().__init__("靶定位六自由度位姿", parent)
        self._setup_ui()
        
    def _setup_ui(self):
        layout = QHBoxLayout(self)
        layout.setSpacing(20)
        layout.setContentsMargins(20, 20, 20, 20)
        
        # 第一组：X, Y, Z
        g1 = QGridLayout()
        g1.setHorizontalSpacing(10)
        self.val_x = self._add_value_field(g1, "X:", "mm", 0, 0)
        self.val_y = self._add_value_field(g1, "Y:", "mm", 0, 3)
        self.val_z = self._add_value_field(g1, "Z:", "mm", 0, 6)
        
        # 第二组：Xθ, Yθ, Zθ（角度单位：度）
        self.val_rx = self._add_value_field(g1, "Xθ:", "°", 1, 0)
        self.val_ry = self._add_value_field(g1, "Yθ:", "°", 1, 3)
        self.val_rz = self._add_value_field(g1, "Zθ:", "°", 1, 6)
        
        layout.addLayout(g1)
        
        # 状态显示区域
        status_layout = QVBoxLayout()
        status_layout.setContentsMargins(0, 0, 0, 0)
        status_layout.setSpacing(8)
        
        # 运动状态指示器
        motion_row = QHBoxLayout()
        motion_row.setSpacing(8)
        lbl_motion_title = QLabel("运动状态:")
        lbl_motion_title.setFixedWidth(72)
        lbl_motion_title.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        motion_row.addWidget(lbl_motion_title)
        self.lbl_motion_state = QLabel("静止")
        self.lbl_motion_state.setStyleSheet(
            "background-color: #90EE90; color: black; padding: 4px 12px; "
            "border-radius: 4px; min-width: 60px; qproperty-alignment: AlignCenter;"
        )
        self.lbl_motion_state.setFixedHeight(28)
        self.lbl_motion_state.setMaximumWidth(80)
        self.lbl_motion_state.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        motion_row.addWidget(self.lbl_motion_state)
        motion_row.addStretch(1)

        status_layout.addStretch(1)
        status_layout.addLayout(motion_row)
        status_layout.addStretch(1)
        
        layout.addLayout(status_layout)
        layout.addStretch()
        
    def _add_value_field(self, layout, label_text, unit, row, col):
        lbl = QLabel(label_text)
        lbl.setFixedWidth(30)
        layout.addWidget(lbl, row, col)
        
        val_display = QLabel("0")
        val_display.setAlignment(Qt.AlignCenter)
        val_display.setStyleSheet("background-color: #0c1724; border: 1px solid #1c3146; border-radius: 4px; min-width: 100px; padding: 4px; color: #00a0e9;")
        layout.addWidget(val_display, row, col + 1)
        
        unit_lbl = QLabel(unit)
        unit_lbl.setFixedWidth(30)
        layout.addWidget(unit_lbl, row, col + 2)
        return val_display
    
    def update_status(self, status_data):
        """更新位姿显示（从Worker缓存拉取）"""
        if not status_data:
            return
        
        # 更新六自由度位姿 [X, Y, Z, ThetaX, ThetaY, ThetaZ]
        if "sixFreedomPose" in status_data:
            pose = status_data["sixFreedomPose"]
            if isinstance(pose, (list, tuple)) and len(pose) >= 6:
                # 位置：X, Y, Z (mm) - 设备返回的是mm，直接显示
                self.val_x.setText(f"{pose[0]:.3f}")
                self.val_y.setText(f"{pose[1]:.3f}")
                self.val_z.setText(f"{pose[2]:.3f}")
                # 角度：ThetaX, ThetaY, ThetaZ - 设备返回的是弧度，需要转换为度显示
                self.val_rx.setText(f"{math.degrees(pose[3]):.3f}")
                self.val_ry.setText(f"{math.degrees(pose[4]):.3f}")
                self.val_rz.setText(f"{math.degrees(pose[5]):.3f}")
        
        # 更新运动状态（检查任意轴是否在运动）
        if "sdofState" in status_data:
            sdof_state = status_data["sdofState"]
            if isinstance(sdof_state, (list, tuple)):
                is_any_moving = any(sdof_state)
                if is_any_moving:
                    self.lbl_motion_state.setText("运动中")
                    self.lbl_motion_state.setStyleSheet(
                        "background-color: #FFA500; color: black; padding: 4px 12px; "
                        "border-radius: 4px; min-width: 60px; qproperty-alignment: AlignCenter;"
                    )
                else:
                    self.lbl_motion_state.setText("静止")
                    self.lbl_motion_state.setStyleSheet(
                        "background-color: #90EE90; color: black; padding: 4px 12px; "
                        "border-radius: 4px; min-width: 60px; qproperty-alignment: AlignCenter;"
                    )


class PoseInputWidget(QGroupBox):
    """六自由度位姿输入组件"""
    def __init__(self, parent=None, worker=None):
        super().__init__("靶定位六自由度位姿输入", parent)
        self.inputs = {}
        self._worker = worker
        self._setup_ui()
        self._setup_connections()
        
    def set_worker(self, worker):
        """设置Worker（延迟设置）"""
        self._worker = worker
        self._setup_connections()
        
    def _setup_ui(self):
        # 使用 QHBoxLayout 替代 Grid，与上方显示区域保持一致
        layout = QVBoxLayout(self)
        layout.setSpacing(20)
        layout.setContentsMargins(20, 20, 20, 20)
        
        # 输入区域容器
        input_container = QWidget()
        input_layout = QHBoxLayout(input_container)
        input_layout.setContentsMargins(0, 0, 0, 0)
        input_layout.setSpacing(20)
        
        g1 = QGridLayout()
        g1.setHorizontalSpacing(10)
        
        # Row 0: X, Y, Z
        self.inputs["X"] = self._add_input_field(g1, "X:", "mm", 0, 0)
        self.inputs["Y"] = self._add_input_field(g1, "Y:", "mm", 0, 3)
        self.inputs["Z"] = self._add_input_field(g1, "Z:", "mm", 0, 6)
        
        # Row 1: RX, RY, RZ（角度单位：度）
        self.inputs["RX"] = self._add_input_field(g1, "Xθ:", "°", 1, 0)
        self.inputs["RY"] = self._add_input_field(g1, "Yθ:", "°", 1, 3)
        self.inputs["RZ"] = self._add_input_field(g1, "Zθ:", "°", 1, 6)
        
        input_layout.addLayout(g1)
        input_layout.addStretch()
        
        layout.addWidget(input_container)
        
        # 按钮行
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        
        self.btn_clear = ControlButton("输入清零")
        self.btn_exec_abs = ControlButton("绝对运动", role="move_absolute")
        self.btn_exec_rel = ControlButton("相对运动", role="move_relative")
        self.btn_stop = ControlButton("停止", role="stop")
        self.btn_home = ControlButton("回零")
        self.btn_reset = ControlButton("复位")
        
        btn_layout.addWidget(self.btn_clear)
        btn_layout.addWidget(self.btn_exec_rel)
        btn_layout.addWidget(self.btn_exec_abs)
        btn_layout.addWidget(self.btn_stop)
        btn_layout.addWidget(self.btn_home)
        btn_layout.addWidget(self.btn_reset)
        btn_layout.addStretch()
        
        layout.addLayout(btn_layout)
        
    def _setup_connections(self):
        """连接按钮信号到Tango命令"""
        # 先断开所有可能的旧连接
        try:
            self.btn_clear.clicked.disconnect()
        except:
            pass
        try:
            self.btn_exec_abs.clicked.disconnect()
        except:
            pass
        try:
            self.btn_exec_rel.clicked.disconnect()
        except:
            pass
        try:
            self.btn_stop.clicked.disconnect()
        except:
            pass
        try:
            self.btn_home.clicked.disconnect()
        except:
            pass
        try:
            self.btn_reset.clicked.disconnect()
        except:
            pass
        
        # 清除输入按钮（不需要worker）
        self.btn_clear.clicked.connect(self._clear_inputs)
        
        # 其他按钮需要worker
        if self._worker:
            self.btn_exec_abs.clicked.connect(self._on_execute_absolute)
            self.btn_exec_rel.clicked.connect(self._on_execute_relative)
            self.btn_stop.clicked.connect(self._on_stop)
            self.btn_home.clicked.connect(self._on_home)
            self.btn_reset.clicked.connect(self._on_reset)
        
    def _add_input_field(self, layout, label_text, unit, row, col):
        lbl = QLabel(label_text)
        lbl.setFixedWidth(30)
        layout.addWidget(lbl, row, col)
        
        input_field = QLineEdit()
        input_field.setPlaceholderText("0")
        input_field.setAlignment(Qt.AlignCenter)
        # 保持与上方显示框一致的宽度和样式
        input_field.setStyleSheet("background-color: #0c1724; border: 1px solid #1c3146; color: white; padding: 4px; min-width: 100px;")
        layout.addWidget(input_field, row, col + 1)
        
        unit_lbl = QLabel(unit)
        unit_lbl.setFixedWidth(30)
        layout.addWidget(unit_lbl, row, col + 2)
        return input_field
        
    def _clear_inputs(self):
        for field in self.inputs.values():
            field.clear()
    
    def _on_execute_absolute(self):
        """执行位姿运动（绝对运动）"""
        print(f"[PoseInputWidget {_ts()}] _on_execute_absolute() called", flush=True)
        if not self._worker:
            print(f"[PoseInputWidget {_ts()}] _on_execute_absolute: Worker not available", flush=True)
            show_warning("错误", "设备未连接")
            return
        
        try:
            # 读取输入值，如果为空则默认为0
            pose = []
            for i, key in enumerate(["X", "Y", "Z", "RX", "RY", "RZ"]):
                text = self.inputs[key].text().strip()
                if text:
                    val = float(text)
                    # 位置值（前3个：X, Y, Z）保持原样（mm）
                    # 角度值（后3个：RX, RY, RZ）需要从度转换为弧度
                    if i >= 3:  # RX, RY, RZ
                        val = math.radians(val)  # 度转弧度
                    pose.append(val)
                else:
                    pose.append(0.0)
            
            print(f"[PoseInputWidget {_ts()}] _on_execute_absolute: Parsed pose = {pose} (angles converted to radians)", flush=True)
            # 调用绝对位姿运动命令
            self._worker.queue_command("movePoseAbsolute", pose)
            print(f"[PoseInputWidget {_ts()}] _on_execute_absolute: Command queued successfully", flush=True)
        except ValueError as e:
            print(f"[PoseInputWidget {_ts()}] _on_execute_absolute: ValueError - {e}", flush=True)
            show_warning("输入错误", "请输入有效的数字")
        except Exception as e:
            print(f"[PoseInputWidget {_ts()}] _on_execute_absolute: Exception - {e}", flush=True)
            show_warning("错误", f"执行失败: {str(e)}")
    
    def _on_execute_relative(self):
        """执行位姿运动（相对运动）"""
        print(f"[PoseInputWidget {_ts()}] _on_execute_relative() called", flush=True)
        if not self._worker:
            print(f"[PoseInputWidget {_ts()}] _on_execute_relative: Worker not available", flush=True)
            show_warning("错误", "设备未连接")
            return
        
        try:
            # 读取输入值，如果为空则默认为0
            pose = []
            for i, key in enumerate(["X", "Y", "Z", "RX", "RY", "RZ"]):
                text = self.inputs[key].text().strip()
                if text:
                    val = float(text)
                    # 位置值（前3个：X, Y, Z）保持原样（mm）
                    # 角度值（后3个：RX, RY, RZ）需要从度转换为弧度
                    if i >= 3:  # RX, RY, RZ
                        val = math.radians(val)  # 度转弧度
                    pose.append(val)
                else:
                    pose.append(0.0)
            
            print(f"[PoseInputWidget {_ts()}] _on_execute_relative: Parsed pose = {pose} (angles converted to radians)", flush=True)
            # 调用相对位姿运动命令
            self._worker.queue_command("movePoseRelative", pose)
            print(f"[PoseInputWidget {_ts()}] _on_execute_relative: Command queued successfully", flush=True)
        except ValueError as e:
            print(f"[PoseInputWidget {_ts()}] _on_execute_relative: ValueError - {e}", flush=True)
            show_warning("输入错误", "请输入有效的数字")
        except Exception as e:
            print(f"[PoseInputWidget {_ts()}] _on_execute_relative: Exception - {e}", flush=True)
            show_warning("错误", f"执行失败: {str(e)}")
    
    def _on_stop(self):
        """停止运动"""
        print(f"[PoseInputWidget {_ts()}] _on_stop() called", flush=True)
        if not self._worker:
            print(f"[PoseInputWidget {_ts()}] _on_stop: Worker not available", flush=True)
            return
        print(f"[PoseInputWidget {_ts()}] _on_stop: Queuing stop command", flush=True)
        self._worker.queue_command("stop")
    
    def _on_home(self):
        """回零（所有轴回到零位）"""
        print(f"[PoseInputWidget {_ts()}] _on_home() called", flush=True)
        if not self._worker:
            print(f"[PoseInputWidget {_ts()}] _on_home: Worker not available", flush=True)
            show_warning("错误", "设备未连接")
            return
        print(f"[PoseInputWidget {_ts()}] _on_home: Queuing sixMoveZero command", flush=True)
        self._worker.queue_command("sixMoveZero")
    
    def _on_reset(self):
        """复位（清除报警状态）"""
        print(f"[PoseInputWidget {_ts()}] _on_reset() called", flush=True)
        if not self._worker:
            print(f"[PoseInputWidget {_ts()}] _on_reset: Worker not available", flush=True)
            return
        print(f"[PoseInputWidget {_ts()}] _on_reset: Queuing reset command", flush=True)
        self._worker.queue_command("reset")


class LargeStrokeControl(QGroupBox):
    """大行程控制"""
    def __init__(self, parent=None, worker=None):
        super().__init__("大行程控制", parent)
        self._worker = worker
        self._setup_ui()
        self._setup_connections()
        
    def set_worker(self, worker):
        """设置Worker（延迟设置）"""
        self._worker = worker
        self._setup_connections()
        
    def _setup_ui(self):
        layout = QGridLayout(self)
        layout.setSpacing(20)
        layout.setContentsMargins(20, 20, 20, 20)
        
        # 第一行：当前位置 + 运动状态（同一行）
        layout.addWidget(QLabel("当前位置:"), 0, 0)
        pos_container = QHBoxLayout()
        self.lbl_pos = QLabel("0")
        self.lbl_pos.setStyleSheet("background-color: #90EE90; color: black; padding: 4px 8px; border-radius: 2px; min-width: 80px; qproperty-alignment: AlignCenter;")
        pos_container.addWidget(self.lbl_pos)
        self.lbl_pos_unit = QLabel("mm")  # 单位标签，可动态更新
        pos_container.addWidget(self.lbl_pos_unit)
        pos_container.addStretch()
        layout.addLayout(pos_container, 0, 1)

        layout.addWidget(QLabel("运动状态:"), 0, 2)
        state_container = QHBoxLayout()
        self.lbl_state = QLabel("静止")
        self.lbl_state.setStyleSheet("background-color: #90EE90; color: black; padding: 4px 8px; border-radius: 2px; qproperty-alignment: AlignCenter;")
        self.lbl_state.setMaximumWidth(80)
        state_container.addWidget(self.lbl_state)
        state_container.addStretch()
        layout.addLayout(state_container, 0, 3)
        
        # 闸板阀状态
        layout.addWidget(QLabel("闸板阀状态:"), 2, 0)
        valve_state_container = QHBoxLayout()
        self.lbl_valve = QLabel("已关闭")
        self.lbl_valve.setStyleSheet("background-color: #90EE90; color: black; padding: 4px 8px; border-radius: 2px; qproperty-alignment: AlignCenter;")
        self.lbl_valve.setFixedHeight(28)
        self.lbl_valve.setMaximumWidth(80)
        self.lbl_valve.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        valve_state_container.addWidget(self.lbl_valve)
        valve_state_container.addStretch()
        layout.addLayout(valve_state_container, 2, 1)

        # 第二行：运动距离输入 + 相对/绝对运动 + 停止/回零/复位（同一行，且与上方网格列对齐）
        layout.addWidget(QLabel("运动距离:"), 1, 0)
        dist_container = QHBoxLayout()
        self.input_dist = QLineEdit()
        self.input_dist.setPlaceholderText("0")
        self.input_dist.setFixedWidth(100)
        self.input_dist.setStyleSheet("background-color: #0c1724; border: 1px solid #1c3146; color: white; padding: 4px;")
        dist_container.addWidget(self.input_dist)
        self.lbl_input_unit = QLabel("mm")  # 输入单位标签，可动态更新
        dist_container.addWidget(self.lbl_input_unit)
        dist_container.addStretch()
        layout.addLayout(dist_container, 1, 1)

        btn_container = QHBoxLayout()
        self.btn_rel = ControlButton("相对运动", role="move_relative")
        self.btn_abs = ControlButton("绝对运动", role="move_absolute")
        btn_container.addWidget(self.btn_rel)
        btn_container.addWidget(self.btn_abs)

        self.btn_stop = ControlButton("停止", role="stop")
        self.btn_stop.setStyleSheet("border-radius: 4px; padding: 6px 16px;")
        self.btn_home = ControlButton("回零")
        self.btn_reset = ControlButton("复位")
        btn_container.addWidget(self.btn_stop)
        btn_container.addWidget(self.btn_home)
        btn_container.addWidget(self.btn_reset)
        btn_container.addStretch()
        layout.addLayout(btn_container, 1, 2, 1, 2)
        
        # 闸板阀控制
        valve_layout = QHBoxLayout()
        self.btn_v_open = ControlButton("闸板阀开")
        self.btn_v_close = ControlButton("闸板阀关")
        valve_layout.addWidget(self.btn_v_open)
        valve_layout.addWidget(self.btn_v_close)
        valve_layout.addStretch()
        
        layout.addLayout(valve_layout, 2, 2, 1, 2)
        
        # 设置列比例
        layout.setColumnStretch(3, 1)
    
    def _setup_connections(self):
        """连接按钮信号到Tango命令"""
        # 先断开所有可能的旧连接
        try:
            self.btn_rel.clicked.disconnect()
        except:
            pass
        try:
            self.btn_abs.clicked.disconnect()
        except:
            pass
        try:
            self.btn_stop.clicked.disconnect()
        except:
            pass
        try:
            self.btn_home.clicked.disconnect()
        except:
            pass
        try:
            self.btn_reset.clicked.disconnect()
        except:
            pass
        try:
            self.btn_v_open.clicked.disconnect()
        except:
            pass
        try:
            self.btn_v_close.clicked.disconnect()
        except:
            pass
        
        if not self._worker:
            return
            
        # 相对运动
        self.btn_rel.clicked.connect(self._on_move_relative)
        # 绝对运动
        self.btn_abs.clicked.connect(self._on_move_absolute)
        # 停止
        self.btn_stop.clicked.connect(self._on_stop)
        # 回零（使用绝对运动到0）
        self.btn_home.clicked.connect(self._on_home)
        # 复位
        self.btn_reset.clicked.connect(self._on_reset)
        # 闸板阀开
        self.btn_v_open.clicked.connect(lambda: self._on_open_value(0))
        # 闸板阀关
        self.btn_v_close.clicked.connect(lambda: self._on_open_value(1))
    
    def _on_move_relative(self):
        """相对运动"""
        print(f"[LargeStrokeControl {_ts()}] _on_move_relative() called", flush=True)
        if not self._worker:
            print(f"[LargeStrokeControl {_ts()}] _on_move_relative: Worker not available", flush=True)
            show_warning("错误", "设备未连接")
            return
            
        try:
            input_text = self.input_dist.text() or "0"
            print(f"[LargeStrokeControl {_ts()}] _on_move_relative: Input text = '{input_text}'", flush=True)
            distance = float(input_text)
            print(f"[LargeStrokeControl {_ts()}] _on_move_relative: Parsed distance = {distance}", flush=True)
            self._worker.queue_command("moveRelative", distance)
            print(f"[LargeStrokeControl {_ts()}] _on_move_relative: Command queued successfully", flush=True)
        except ValueError as e:
            print(f"[LargeStrokeControl {_ts()}] _on_move_relative: ValueError - {e}", flush=True)
            show_warning("输入错误", "请输入有效的数字")
    
    def _on_move_absolute(self):
        """绝对运动"""
        print(f"[LargeStrokeControl {_ts()}] _on_move_absolute() called", flush=True)
        if not self._worker:
            print(f"[LargeStrokeControl {_ts()}] _on_move_absolute: Worker not available", flush=True)
            show_warning("错误", "设备未连接")
            return
            
        try:
            input_text = self.input_dist.text() or "0"
            print(f"[LargeStrokeControl {_ts()}] _on_move_absolute: Input text = '{input_text}'", flush=True)
            position = float(input_text)
            print(f"[LargeStrokeControl {_ts()}] _on_move_absolute: Parsed position = {position}", flush=True)
            self._worker.queue_command("moveAbsolute", position)
            print(f"[LargeStrokeControl {_ts()}] _on_move_absolute: Command queued successfully", flush=True)
        except ValueError as e:
            print(f"[LargeStrokeControl {_ts()}] _on_move_absolute: ValueError - {e}", flush=True)
            show_warning("输入错误", "请输入有效的数字")
    
    def _on_stop(self):
        """停止运动"""
        print(f"[LargeStrokeControl {_ts()}] _on_stop() called", flush=True)
        if not self._worker:
            print(f"[LargeStrokeControl {_ts()}] _on_stop: Worker not available", flush=True)
            return
        print(f"[LargeStrokeControl {_ts()}] _on_stop: Queuing stop command", flush=True)
        self._worker.queue_command("stop")
    
    def _on_home(self):
        """回零（绝对运动到0）"""
        print(f"[LargeStrokeControl {_ts()}] _on_home() called", flush=True)
        if not self._worker:
            print(f"[LargeStrokeControl {_ts()}] _on_home: Worker not available", flush=True)
            show_warning("错误", "设备未连接")
            return
        print(f"[LargeStrokeControl {_ts()}] _on_home: Queuing moveAbsolute(0.0) command", flush=True)
        self._worker.queue_command("moveAbsolute", 0.0)
    
    def _on_reset(self):
        """复位（清除报警状态）"""
        print(f"[LargeStrokeControl {_ts()}] _on_reset() called", flush=True)
        if not self._worker:
            print(f"[LargeStrokeControl {_ts()}] _on_reset: Worker not available", flush=True)
            return
        print(f"[LargeStrokeControl {_ts()}] _on_reset: Queuing reset command", flush=True)
        self._worker.queue_command("reset")
    
    def _on_open_value(self, state):
        """闸板阀控制：state=0开，state=1关"""
        state_name = "开" if state == 0 else "关"
        print(f"[LargeStrokeControl {_ts()}] _on_open_value() called: state={state} ({state_name})", flush=True)
        if not self._worker:
            print(f"[LargeStrokeControl {_ts()}] _on_open_value: Worker not available", flush=True)
            show_warning("错误", "设备未连接")
            return
        print(f"[LargeStrokeControl {_ts()}] _on_open_value: Queuing openValue({state}) command", flush=True)
        self._worker.queue_command("openValue", state)
    
    def update_status(self, status_data):
        """更新状态显示"""
        if not status_data:
            # print(f"[LargeStrokeControl {_ts()}] update_status: No status data", flush=True)
            return
        
        # 只在调试模式下输出
        # print(f"[LargeStrokeControl {_ts()}] update_status: Updating with {len(status_data)} items", flush=True)
            
        # 更新位置单位标签（如果可用）
        if "positionUnit" in status_data:
            unit = status_data["positionUnit"]
            if unit:
                # 更新显示单位标签
                if unit != self.lbl_pos_unit.text():
                    self.lbl_pos_unit.setText(unit)
                # 更新输入单位标签
                if unit != self.lbl_input_unit.text():
                    self.lbl_input_unit.setText(unit)
        
        # 更新当前位置
        if "largeRangePos" in status_data:
            pos = status_data["largeRangePos"]
            old_text = self.lbl_pos.text()
            new_text = f"{pos:.3f}"
            self.lbl_pos.setText(new_text)
            # 只在调试模式下输出位置变化
            # if old_text != new_text:
            #     print(f"[LargeStrokeControl {_ts()}] update_status: Position changed: {old_text} -> {new_text}", flush=True)
        
        # 更新运动状态
        if "LargeRangeState" in status_data:
            is_moving = status_data["LargeRangeState"]
            old_text = self.lbl_state.text()
            new_text = "运动中" if is_moving else "静止"
            self.lbl_state.setText(new_text)
            # 只在调试模式下输出运动状态变化
            # if old_text != new_text:
            #     print(f"[LargeStrokeControl {_ts()}] update_status: Motion state changed: {old_text} -> {new_text}", flush=True)
            # 更新颜色
            if is_moving:
                self.lbl_state.setStyleSheet(
                    "background-color: #FFA500; color: black; padding: 4px 8px; "
                    "border-radius: 2px; min-width: 80px; qproperty-alignment: AlignCenter;"
                )
            else:
                self.lbl_state.setStyleSheet(
                    "background-color: #90EE90; color: black; padding: 4px 8px; "
                    "border-radius: 2px; min-width: 80px; qproperty-alignment: AlignCenter;"
                )
        
        # 更新闸板阀状态
        if "hostPlugState" in status_data:
            valve_state = status_data["hostPlugState"]
            old_text = self.lbl_valve.text()
            if valve_state == "OPENED":
                new_text = "已开启"
                self.lbl_valve.setText(new_text)
                self.lbl_valve.setStyleSheet(
                    "background-color: #90EE90; color: black; padding: 4px 8px; "
                    "border-radius: 2px; min-width: 80px; qproperty-alignment: AlignCenter;"
                )
            elif valve_state == "CLOSED":
                new_text = "已关闭"
                self.lbl_valve.setText(new_text)
                self.lbl_valve.setStyleSheet(
                    "background-color: #90EE90; color: black; padding: 4px 8px; "
                    "border-radius: 2px; min-width: 80px; qproperty-alignment: AlignCenter;"
                )
            else:
                new_text = "未知"
                self.lbl_valve.setText(new_text)
                self.lbl_valve.setStyleSheet(
                    "background-color: #cccccc; color: black; padding: 4px 8px; "
                    "border-radius: 2px; min-width: 80px; qproperty-alignment: AlignCenter;"
                )
            # 只在调试模式下输出闸板阀状态变化
            # if old_text != new_text:
            #     print(f"[LargeStrokeControl {_ts()}] update_status: Valve state changed: {old_text} -> {new_text} (raw={valve_state})", flush=True)


class TargetPositioningPage(QWidget):
    """靶定位控制主页面"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self._worker = None  # LargeStrokeWorker
        self._six_dof_worker = None  # SixDofWorker
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
        layout.setSpacing(16)
        layout.setContentsMargins(20, 20, 20, 20)
        
        # 顶部标题
        header = QLabel("靶定位控制") # 修改标题
        header.setProperty("role", "title")
        layout.addWidget(header)
        
        # 使用水平分割器：左侧是功能区域，右侧是状态面板
        main_splitter = QSplitter(Qt.Horizontal)
        main_splitter.setHandleWidth(1)
        main_splitter.setStyleSheet("QSplitter::handle { background-color: #1c3146; }")
        
        # 左侧：功能控制区域
        left_scroll = QScrollArea()
        left_scroll.setWidgetResizable(True)
        left_scroll.setFrameShape(QFrame.NoFrame)
        
        left_content = QWidget()
        left_layout = QVBoxLayout(left_content)
        left_layout.setSpacing(20)
        
        # 1. 六自由度位姿显示 (系统)
        self.pose_display = PoseDisplayWidget()
        left_layout.addWidget(self.pose_display)
        
        # 2. 六自由度位姿输入（稍后设置worker）
        self.pose_input = PoseInputWidget()
        left_layout.addWidget(self.pose_input)
        
        # 3. 大行程控制（稍后设置worker）
        self.large_stroke = LargeStrokeControl()
        left_layout.addWidget(self.large_stroke)
        
        left_layout.addStretch()
        left_scroll.setWidget(left_content)
        main_splitter.addWidget(left_scroll)
        
        # 右侧：状态面板（整合到页面中）
        self._setup_status_panel()
        main_splitter.addWidget(self.status_panel_widget)
        
        # 设置右侧状态面板的固定宽度
        from ..config import UI_SETTINGS
        page_key = "target_positioning"
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
        main_splitter.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        
        layout.addWidget(main_splitter, 1)  # 添加拉伸因子
    
    def _setup_status_panel(self):
        """创建并设置状态面板（整合到页面中）"""
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
        self.motor_header_scroll.setWidget(self.motor_header_container)
        self.motor_header_scroll.setFixedHeight(40)
        status_layout.addWidget(self.motor_header_scroll)
        
        # 电机状态内容区（只滚动内容）
        self.motor_body_scroll = QScrollArea()
        self.motor_body_scroll.setWidgetResizable(True)
        self.motor_body_scroll.setFrameShape(QFrame.NoFrame)
        # 由于控制按钮列最小宽度较大，水平滚动按需启用；表头会同步水平滚动
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
        
        # 创建电机状态更新定时器
        self._motor_status_timer = QTimer(self)
        self._motor_status_timer.timeout.connect(self._update_motor_status)
        self._motor_status_timer.start(500)  # 500ms更新一次
        
        # NOTE: 上面已初始化电机列表，避免重复初始化导致布局抖动
    
    def _init_motor_list(self):
        """初始化电机列表"""
        motors = TARGET_POSITIONING_CONFIG.get("status_motors", [])
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
        
        # 检查是否有只读电机（大行程）和有控制的电机
        has_readonly = any(motor.get("readonly", False) for motor in motors)
        has_controls = any(not motor.get("readonly", False) for motor in motors)
        
        # 添加表头
        if has_controls:
            # 将命令按钮放在“运动状态”下方，不再使用独立“控制”列
            headers = ["电机", "运动mm", "位置mm", "编码器位置", "运动/操作", "开关状态"]
        elif has_readonly:
            headers = ["电机", "位置mm", "编码器位置", "运动状态", "开关状态"]
        else:
            headers = ["电机", "运动mm", "位置mm", "编码器位置", "运动状态", "开关状态"]
        
        # 固定表头
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
                self.motor_header_layout.addWidget(label, 0, col)

        # 表头/内容区列配置保持一致（根据header列数调整）
        col_count = len(headers)
        if has_controls:
            # 6列：名称/输入/位置/编码器/运动(含按钮)/开关
            min_widths = [80, 70, 70, 70, 170, 90]
            stretches = [1, 1, 1, 1, 2, 1]
        elif has_readonly:
            # 5列：名称/位置/编码器/运动/开关
            min_widths = [110, 80, 80, 90, 90]
            stretches = [2, 1, 1, 1, 1]
        else:
            # 6列（无控制）：名称/输入/位置/编码器/运动/开关
            min_widths = [80, 70, 70, 70, 90, 90]
            stretches = [1, 1, 1, 1, 1, 1]

        for layout_obj in [self.motor_header_layout, self.motor_list_layout]:
            if not layout_obj:
                continue
            for col in range(col_count):
                layout_obj.setColumnStretch(col, stretches[col] if col < len(stretches) else 1)
                layout_obj.setColumnMinimumWidth(col, min_widths[col] if col < len(min_widths) else 60)
        
        # 生成新电机行（内容区从第0行开始）
        for row_idx, motor in enumerate(motors, 0):
            row_obj = MotorStatusRow(motor)
            self.motor_rows.append(row_obj)
            
            # 连接独立控制按钮信号（如果有）
            if not row_obj.readonly and row_obj.btn_execute:
                row_obj.btn_execute.clicked.connect(lambda checked, r=row_obj: self._on_single_execute(r))
                row_obj.btn_stop.clicked.connect(lambda checked, r=row_obj: self._on_single_stop(r))
                row_obj.btn_reset.clicked.connect(lambda checked, r=row_obj: self._on_single_reset(r))
            
            if row_obj.readonly:
                # 只读模式（大行程电机）：不显示输入框和控制按钮
                if has_controls:
                    # 控制表头仍然包含“输入mm”列：为只读行保留一个空占位，保持列对齐
                    self.motor_list_layout.addWidget(row_obj.name_label, row_idx, 0)
                    self.motor_list_layout.addWidget(QLabel(""), row_idx, 1)
                    self.motor_list_layout.addWidget(row_obj.pos_label, row_idx, 2, alignment=Qt.AlignCenter)
                    self.motor_list_layout.addWidget(row_obj.enc_label, row_idx, 3, alignment=Qt.AlignCenter)
                    self.motor_list_layout.addWidget(row_obj.state_label, row_idx, 4)
                    self.motor_list_layout.addWidget(row_obj.switch_label, row_idx, 5)
                else:
                    # 纯只读表头：名称/位置/编码器/运动状态/开关状态
                    self.motor_list_layout.addWidget(row_obj.name_label, row_idx, 0)
                    self.motor_list_layout.addWidget(row_obj.pos_label, row_idx, 1, alignment=Qt.AlignCenter)
                    self.motor_list_layout.addWidget(row_obj.enc_label, row_idx, 2, alignment=Qt.AlignCenter)
                    self.motor_list_layout.addWidget(row_obj.state_label, row_idx, 3)
                    self.motor_list_layout.addWidget(row_obj.switch_label, row_idx, 4)
            else:
                # 有控制模式（六自由度电机）：状态一行 + 操作一行（同一单元格）
                self.motor_list_layout.addWidget(row_obj.name_label, row_idx, 0)
                self.motor_list_layout.addWidget(row_obj.input_edit, row_idx, 1)
                self.motor_list_layout.addWidget(row_obj.pos_label, row_idx, 2, alignment=Qt.AlignCenter)
                self.motor_list_layout.addWidget(row_obj.enc_label, row_idx, 3, alignment=Qt.AlignCenter)

                state_cell = QWidget()
                state_layout = QVBoxLayout(state_cell)
                state_layout.setContentsMargins(0, 0, 0, 0)
                state_layout.setSpacing(6)
                state_layout.addWidget(row_obj.state_label)

                btn_row = QHBoxLayout()
                btn_row.setContentsMargins(0, 0, 0, 0)
                btn_row.setSpacing(6)
                btn_row.addWidget(row_obj.btn_execute)
                btn_row.addWidget(row_obj.btn_stop)
                btn_row.addWidget(row_obj.btn_reset)
                state_layout.addLayout(btn_row)

                self.motor_list_layout.addWidget(state_cell, row_idx, 4)
                self.motor_list_layout.addWidget(row_obj.switch_label, row_idx, 5)

        # 吸收滚动区域额外高度，避免行被拉伸成“大空白块”
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
        if not self.motor_rows:
            return
        
        # 遍历所有电机行，更新状态
        for row in self.motor_rows:
            motor_config = row.config
            device_id = motor_config.get("device")
            axis = motor_config.get("axis", 0)
            
            # 获取对应的Worker
            worker = None
            if device_id == "large_stroke":
                worker = self._worker
            elif device_id == "six_dof":
                worker = self._six_dof_worker
            else:
                continue
            
            if not worker:
                continue
            
            # 获取状态数据
            status_data = worker.get_cached_status() if hasattr(worker, 'get_cached_status') else None
            if not status_data:
                continue
            
            # 更新位置显示
            if device_id == "large_stroke" and "largeRangePos" in status_data:
                pos = status_data["largeRangePos"]
                row.pos_label.setText(f"{pos:.3f}")
                row.enc_label.setText(f"{pos:.3f}")
            elif device_id == "six_dof":
                # 优先使用 sixFreedomPose（位姿），如果没有则使用 axisPos（编码器位置）
                if "sixFreedomPose" in status_data:
                    pose = status_data["sixFreedomPose"]
                    if isinstance(pose, (list, tuple)) and len(pose) > axis:
                        row.pos_label.setText(f"{pose[axis]:.3f}")
                elif "axisPos" in status_data:
                    axis_pos = status_data["axisPos"]
                    if isinstance(axis_pos, (list, tuple)) and len(axis_pos) > axis:
                        row.pos_label.setText(f"{axis_pos[axis]:.3f}")
                
                # 更新编码器显示（使用 axisPos）
                if "axisPos" in status_data:
                    axis_pos = status_data["axisPos"]
                    if isinstance(axis_pos, (list, tuple)) and len(axis_pos) > axis:
                        row.enc_label.setText(f"{axis_pos[axis]:.3f}")
            
            # 更新运动状态
            if device_id == "large_stroke" and "LargeRangeState" in status_data:
                is_moving = status_data["LargeRangeState"]
                row.state_label.setText("运动中" if is_moving else "静止")
            elif device_id == "six_dof" and "sdofState" in status_data:
                states = status_data["sdofState"]
                if isinstance(states, (list, tuple)) and len(states) > axis:
                    is_moving = bool(states[axis])
                    row.state_label.setText("运动中" if is_moving else "静止")
    
    def _on_single_execute(self, row):
        """单个电机执行按钮：执行该电机的运动"""
        if not row.input_edit:
            return
        
        input_text = row.input_edit.text().strip()
        if not input_text:
            show_warning("提示", "请输入运动距离")
            return
        
        try:
            value = float(input_text)
            motor_config = row.config
            device_id = motor_config.get("device")
            axis = motor_config.get("axis", 0)
            
            # 六自由度设备：使用单轴相对运动命令
            if device_id == "six_dof" and self._six_dof_worker:
                # singleMoveRelative 需要 [axis, distance] 参数
                params = [float(axis), value]
                self._six_dof_worker.queue_command("singleMoveRelative", params)
            else:
                show_warning("错误", f"设备 {device_id} 不支持单轴控制")
        except ValueError:
            show_warning("输入错误", "请输入有效的数字")
        except Exception as e:
            show_warning("执行失败", f"执行命令时出错: {str(e)}")
    
    def _on_single_stop(self, row):
        """单个电机停止按钮：停止该电机的运动"""
        motor_config = row.config
        device_id = motor_config.get("device")
        
        try:
            # 六自由度设备：停止所有轴（设备端没有单轴停止命令）
            if device_id == "six_dof" and self._six_dof_worker:
                self._six_dof_worker.queue_command("stop")
            else:
                show_warning("错误", f"设备 {device_id} 不支持单轴停止")
        except Exception as e:
            show_warning("执行失败", f"停止命令执行失败: {str(e)}")
    
    def _on_single_reset(self, row):
        """单个电机复位按钮：复位该电机（清除报警状态）"""
        motor_config = row.config
        device_id = motor_config.get("device")
        axis = motor_config.get("axis", 0)
        
        try:
            # 六自由度设备：使用单轴复位命令
            if device_id == "six_dof" and self._six_dof_worker:
                self._six_dof_worker.queue_command("singleReset", int(axis))
            else:
                show_warning("错误", f"设备 {device_id} 不支持单轴复位")
        except Exception as e:
            show_warning("执行失败", f"复位命令执行失败: {str(e)}")
    
    def _setup_worker(self):
        """设置Tango Worker"""
        print(f"[TargetPositioningPage {_ts()}] _setup_worker() called", flush=True)
        if not PyTango:
            print(f"[TargetPositioningPage {_ts()}] PyTango not available", flush=True)
            return
            
        # 1. 创建大行程Worker
        device_name = DEVICES.get("large_stroke", "sys/large_stroke/1")
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Creating LargeStrokeWorker for device: {device_name}", flush=True)
        self._worker = LargeStrokeWorker(device_name, self)
        
        # 连接大行程Worker信号
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Connecting LargeStrokeWorker signals", flush=True)
        self._worker.command_done.connect(self._on_command_done)
        self._worker.connection_status.connect(self._on_connection_status)
        
        # 设置大行程Worker到控件
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Setting LargeStrokeWorker to control widget", flush=True)
        self.large_stroke.set_worker(self._worker)
        
        # 启动大行程Worker
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Starting LargeStrokeWorker thread", flush=True)
        self._worker.start()
        
        # 2. 创建六自由度Worker
        six_dof_device_name = DEVICES.get("six_dof", "sys/six_dof/1")
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Creating SixDofWorker for device: {six_dof_device_name}", flush=True)
        self._six_dof_worker = SixDofWorker(six_dof_device_name, self)
        
        # 连接六自由度Worker信号
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Connecting SixDofWorker signals", flush=True)
        self._six_dof_worker.command_done.connect(self._on_six_dof_command_done)
        self._six_dof_worker.connection_status.connect(self._on_six_dof_connection_status)
        
        # 设置六自由度Worker到控件
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Setting SixDofWorker to pose widgets", flush=True)
        self.pose_input.set_worker(self._six_dof_worker)
        
        # 启动六自由度Worker
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Starting SixDofWorker thread", flush=True)
        self._six_dof_worker.start()
        
        # 创建状态更新定时器（500ms间隔）
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Creating status update timer (500ms)", flush=True)
        self._status_timer = QTimer(self)
        self._status_timer.timeout.connect(self._update_status)
        self._status_timer.start(500)
        print(f"[TargetPositioningPage {_ts()}] _setup_worker: Worker setup completed", flush=True)

        # 错误弹窗节流：之前用于避免断连/超时时“延时后连弹”。
        # 现在命令侧已做 fail-fast + 清队列/拒绝排队，弹窗去重会造成“同按钮连按偶尔不弹”。
        # 为保证每次点击都有反馈，这里禁用去重窗口（设为0）。
        self._ls_last_error_ts = 0.0
        self._ls_last_error_key = None
        self._ls_error_suppress_window_sec = 0.0
    
    def _update_status(self):
        """定时更新状态（从Worker缓存拉取）"""
        # 更新大行程状态
        if self._worker:
            status_data = self._worker.get_cached_status()
            if status_data:
                self.large_stroke.update_status(status_data)
        
        # 更新六自由度位姿显示
        if self._six_dof_worker:
            status_data = self._six_dof_worker.get_cached_status()
            if status_data:
                self.pose_display.update_status(status_data)
    
    def _on_command_done(self, cmd_name, success, message):
        """命令完成回调"""
        status_str = "SUCCEEDED" if success else "FAILED"
        print(f"[TargetPositioningPage {_ts()}] _on_command_done: {cmd_name} {status_str}: {message}", flush=True)
        if not success:
            # 弹窗节流/去重：同类错误在短时间内只弹一次，避免“延时后连弹很多错误框”
            now = time.monotonic()
            error_key = (cmd_name, (message or "")[:200])
            if (
                self._ls_last_error_key == error_key
                and (now - self._ls_last_error_ts) < self._ls_error_suppress_window_sec
            ):
                print(
                    f"[TargetPositioningPage {_ts()}] _on_command_done: Suppressed duplicate error dialog for {cmd_name}",
                    flush=True,
                )
                return
            self._ls_last_error_key = error_key
            self._ls_last_error_ts = now

            print(f"[TargetPositioningPage {_ts()}] _on_command_done: Showing error message box", flush=True)
            
            # 检测不同类型的错误并显示相应的友好提示
            error_msg = message
            
            # 1. 硬件连接问题
            if "controller not connected" in message.lower() or "HardwareError" in message:
                show_warning("硬件连接失败", 
                                  f"命令 {cmd_name} 执行失败:\n\n"
                                  f"原因：大行程设备硬件未连接。\n\n"
                                  f"解决方案：\n"
                                  f"1. 检查运动控制器的电源和USB/网络连接\n"
                                  f"2. 确认运动控制器设备服务器已启动\n"
                                  f"3. 如果使用模拟模式，请检查系统配置\n"
                                  f"4. 查看设备服务器日志获取详细信息")
            
            # 2. 设备状态问题
            elif "not allowed in" in message and "UNKNOWN" in message:
                show_warning("设备未初始化", 
                                  f"命令 {cmd_name} 执行失败:\n\n"
                                  f"设备当前处于 UNKNOWN 状态，需要先初始化。\n\n"
                                  f"请点击'复位'按钮进行初始化，或等待系统自动初始化。")
            
            # 3. 代理连接错误
            elif "API_ProxyError" in message or "Proxy" in message:
                show_warning("设备通信错误", 
                                  f"命令 {cmd_name} 执行失败:\n\n"
                                  f"无法与底层设备通信。\n\n"
                                  f"可能原因：\n"
                                  f"- 运动控制器设备未启动或未连接\n"
                                  f"- 网络通信问题\n"
                                  f"- 设备配置错误\n\n"
                                  f"请检查设备服务器状态和网络连接。")
            
            # 4. 其他错误
            else:
                show_warning("命令执行失败", 
                                  f"命令 {cmd_name} 执行失败:\n\n{message}")
    
    def _on_connection_status(self, connected, message):
        """大行程设备连接状态回调"""
        status_str = "CONNECTED" if connected else "FAILED"
        print(f"[TargetPositioningPage {_ts()}] _on_connection_status (LargeStroke): {status_str}: {message}", flush=True)
        if not connected:
            # 连接失败时可以显示提示，但不阻塞UI
            print(f"[TargetPositioningPage {_ts()}] LargeStroke connection failed: {message}", flush=True)
    
    def _on_six_dof_command_done(self, cmd_name, success, message):
        """六自由度命令完成回调"""
        status_str = "SUCCEEDED" if success else "FAILED"
        print(f"[TargetPositioningPage {_ts()}] _on_six_dof_command_done: {cmd_name} {status_str}: {message}", flush=True)
        if not success:
            print(f"[TargetPositioningPage {_ts()}] _on_six_dof_command_done: Showing error message box", flush=True)
            
            # 检测不同类型的错误并显示相应的友好提示
            error_msg = message
            
            # 1. 硬件连接问题
            if "controller not connected" in message.lower() or "HardwareError" in message:
                show_warning("硬件连接失败", 
                                  f"命令 {cmd_name} 执行失败:\n\n"
                                  f"原因：六自由度设备硬件未连接。\n\n"
                                  f"解决方案：\n"
                                  f"1. 检查六自由度设备的电源和连接\n"
                                  f"2. 确认六自由度设备服务器已启动\n"
                                  f"3. 如果使用模拟模式，请检查系统配置\n"
                                  f"4. 查看设备服务器日志获取详细信息")
            
            # 2. 设备状态问题
            elif "not allowed in" in message and "UNKNOWN" in message:
                show_warning("设备未初始化", 
                                  f"命令 {cmd_name} 执行失败:\n\n"
                                  f"设备当前处于 UNKNOWN 状态，需要先初始化。\n\n"
                                  f"请点击'复位'按钮进行初始化，或等待系统自动初始化。")
            
            # 3. 代理连接错误
            elif "API_ProxyError" in message or "Proxy" in message:
                show_warning("设备通信错误", 
                                  f"命令 {cmd_name} 执行失败:\n\n"
                                  f"无法与六自由度设备通信。\n\n"
                                  f"可能原因：\n"
                                  f"- 六自由度设备未启动或未连接\n"
                                  f"- 网络通信问题\n"
                                  f"- 设备配置错误\n\n"
                                  f"请检查设备服务器状态和网络连接。")
            
            # 4. 其他错误
            else:
                show_warning("命令执行失败", 
                                  f"命令 {cmd_name} 执行失败:\n\n{message}")
    
    def _on_six_dof_connection_status(self, connected, message):
        """六自由度设备连接状态回调"""
        status_str = "CONNECTED" if connected else "FAILED"
        print(f"[TargetPositioningPage {_ts()}] _on_six_dof_connection_status: {status_str}: {message}", flush=True)
        if not connected:
            # 连接失败时可以显示提示，但不阻塞UI
            print(f"[TargetPositioningPage {_ts()}] SixDof connection failed: {message}", flush=True)
        
    def get_worker(self):
        """获取 Worker（供状态面板使用）
        
        Returns:
            dict: {"large_stroke": LargeStrokeWorker, "six_dof": SixDofWorker}
        """
        return {
            "large_stroke": self._worker,
            "six_dof": self._six_dof_worker
        }
    
    def cleanup(self):
        """清理资源"""
        print(f"[TargetPositioningPage {_ts()}] cleanup() called", flush=True)
        # 停止状态更新定时器
        if self._status_timer:
            print(f"[TargetPositioningPage {_ts()}] cleanup: Stopping status timer", flush=True)
            self._status_timer.stop()
            self._status_timer = None
        
        # 停止电机状态更新定时器
        if hasattr(self, '_motor_status_timer') and self._motor_status_timer:
            print(f"[TargetPositioningPage {_ts()}] cleanup: Stopping motor status timer", flush=True)
            self._motor_status_timer.stop()
            self._motor_status_timer = None
            
        # 停止大行程Worker线程
        if self._worker:
            print(f"[TargetPositioningPage {_ts()}] cleanup: Stopping LargeStrokeWorker thread", flush=True)
            self._worker.stop()
            self._worker = None
        
        # 停止六自由度Worker线程
        if self._six_dof_worker:
            print(f"[TargetPositioningPage {_ts()}] cleanup: Stopping SixDofWorker thread", flush=True)
            self._six_dof_worker.stop()
            self._six_dof_worker = None
        
        print(f"[TargetPositioningPage {_ts()}] cleanup: Completed", flush=True)
