"""
真空腔体系统控制 - 真空抽气控制页面
Vacuum Control Page

功能：
1. 多Tab页设计：配置控制、数字孪生、报警记录、历史记录、趋势曲线
2. 完整的泵阀控制逻辑
3. 状态实时监控
4. 真空状态显示（整合在页面右侧）
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QGroupBox, QLabel, QFrame, QScrollArea, QTableWidget,
    QTableWidgetItem, QHeaderView, QTabWidget, QSpacerItem,
    QSizePolicy, QPushButton, QGraphicsView, QGraphicsScene,
    QGraphicsItem, QGraphicsRectItem, QGraphicsEllipseItem,
    QGraphicsLineItem, QMenu, QAction, QGraphicsTextItem,
    QGraphicsPolygonItem, QSplitter
)
from PyQt5.QtCore import Qt, QTimer, QSize, QRectF, QPointF, QEvent, QObject, QThread, pyqtSignal, QMutex, QMutexLocker
from PyQt5.QtGui import QColor, QPainter, QPen, QBrush, QFont, QPainterPath, QPolygonF
import datetime
import time
from collections import deque

try:
    import PyTango
except ImportError:
    PyTango = None

try:
    import pyqtgraph as pg
    HAS_PYQTGRAPH = True
except ImportError:
    HAS_PYQTGRAPH = False

from ..config import VACUUM_CONTROL_CONFIG
from ..widgets import ControlButton, StatusIndicator, ValueDisplay


class TangoWorker(QThread):
    """后台线程用于 Tango 设备通信
    
    改进设计（避免高频信号导致 UI 卡顿）：
    - 禁用事件订阅，使用纯轮询模式
    - 状态数据缓存在 Worker 中，UI 定时拉取
    - 使用 QMutex 保护共享数据
    """
    # 只保留命令完成信号（低频）
    command_done = pyqtSignal(str, bool, str)  # cmd_name, success, message
    connection_status = pyqtSignal(bool, str)  # connected, message
    
    def __init__(self, device_name="sys/vacuum/1", parent=None):
        super().__init__(parent)
        self.device_name = device_name
        self.device = None
        self._running = True
        self._stop_requested = False
        self._command_queue = deque()
        self._queue_mutex = QMutex()
        self._read_attrs = []
        self._reconnect_interval = 3.0
        self._last_reconnect = 0.0
        
        # 状态数据缓存（UI 定时拉取，避免高频信号）
        self._status_cache = {}
        self._status_mutex = QMutex()
        self._cache_updated = False  # 标记是否有新数据
        
    def set_read_attributes(self, attrs: list):
        """设置需要读取的属性列表"""
        self._read_attrs = attrs
        
    def queue_command(self, cmd_name: str, args=None):
        """将命令加入队列（线程安全）"""
        with QMutexLocker(self._queue_mutex):
            self._command_queue.append((cmd_name, args))
    
    def _pop_command(self):
        """从队列取出命令（线程安全）"""
        with QMutexLocker(self._queue_mutex):
            if self._command_queue:
                return self._command_queue.popleft()
            return None
    
    def get_cached_status(self):
        """获取缓存的状态数据（由 UI 定时调用，线程安全）
        
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
        print(f"[TangoWorker {_ts()}] Requesting stop...", flush=True)
        
        # 请求后台线程停止（事件取消在后台线程执行）
        self._stop_requested = True
        self._running = False
        
        # 等待线程结束（最多5秒）
        if not self.wait(5000):
            print(f"[TangoWorker {_ts()}] Warning: Thread did not stop in time, terminating...", flush=True)
            self.terminate()
            self.wait(1000)
        
        print(f"[TangoWorker {_ts()}] Worker stopped.", flush=True)
        
    def _poll_attributes(self):
        """轮询模式：批量读取属性，更新缓存（不发信号）"""
        if not self.device or not self._read_attrs:
            return
        
        if self._stop_requested:
            return
            
        try:
            # 使用批量读取
            attrs = self.device.read_attributes(self._read_attrs)
            status_data = {}
            for attr in attrs:
                try:
                    if not attr.has_failed:
                        status_data[attr.name] = attr.value
                except:
                    pass
            
            if status_data:
                self._update_cache(status_data)  # 更新缓存，不发信号
        except Exception as e:
            print(f"[TangoWorker {_ts()}] Poll failed: {e}", flush=True)
            self.device = None
    
    def _attempt_connect(self):
        """尝试连接设备，失败不抛异常"""
        if self._stop_requested:
            return False
            
        try:
            print(f"[TangoWorker {_ts()}] Connecting to {self.device_name}...", flush=True)
            dev = PyTango.DeviceProxy(self.device_name)
            dev.set_timeout_millis(3000)
            state = dev.state()
            
            if self._stop_requested:
                return False
                
            self.device = dev
            self.connection_status.emit(True, f"Connected (state: {state})")
            # 首次轮询填充缓存
            if self._read_attrs:
                self._poll_attributes()
            print(f"[TangoWorker {_ts()}] Connected OK", flush=True)
            return True
        except Exception as e:
            self.device = None
            print(f"[TangoWorker {_ts()}] Connection failed: {e}", flush=True)
            self.connection_status.emit(False, str(e))
            return False

    def run(self):
        """主循环：纯轮询模式，无事件订阅"""
        if not PyTango:
            print(f"[TangoWorker {_ts()}] PyTango not available", flush=True)
            self.connection_status.emit(False, "PyTango not installed")
            return

        self._last_reconnect = time.monotonic() - self._reconnect_interval
        poll_interval_ms = 500  # 轮询间隔 500ms

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
            
            # 3. 等待下次轮询（可被命令打断）
            for _ in range(poll_interval_ms // 50):  # 50ms 为单位
                if self._stop_requested:
                    break
                with QMutexLocker(self._queue_mutex):
                    has_cmd = len(self._command_queue) > 0
                if has_cmd:
                    break
                self.msleep(50)
        
        print(f"[TangoWorker {_ts()}] Worker thread exiting.", flush=True)
    
    def _execute_command(self, cmd_name, args):
        """执行Tango命令（带超时保护）"""
        if not self.device:
            self.command_done.emit(cmd_name, False, "Device not connected")
            return
        
        if self._stop_requested:
            self.command_done.emit(cmd_name, False, "Worker stopping")
            return
            
        try:
            print(f"[TangoWorker {_ts()}] Executing command: {cmd_name}({args})", flush=True)
            
            # 设置命令超时（3秒）
            old_timeout = self.device.get_timeout_millis()
            self.device.set_timeout_millis(3000)
            
            try:
                if args is not None and args != [] and args != ():
                    result = self.device.command_inout(cmd_name, args)
                else:
                    result = self.device.command_inout(cmd_name)
            finally:
                self.device.set_timeout_millis(old_timeout)
                
            print(f"[TangoWorker {_ts()}] Command {cmd_name} succeeded: {result}", flush=True)
            self.command_done.emit(cmd_name, True, str(result))
        except Exception as e:
            print(f"[TangoWorker {_ts()}] Command {cmd_name} failed: {e}", flush=True)
            self.command_done.emit(cmd_name, False, str(e))
            # 如果是超时错误，可能需要重连
            if "timeout" in str(e).lower():
                self.device = None
                self._use_events = False


def _ts():
    """返回当前时间戳字符串，精确到毫秒"""
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


class _ClickProbe(QObject):
    """用于在按钮 click 信号之前记录鼠标按下时间"""

    def __init__(self, label: str, parent=None):
        super().__init__(parent)
        self.label = label

    def eventFilter(self, obj, event):
        etype = event.type()
        if etype == QEvent.MouseButtonPress:
            print(f"[GUI {_ts()}] button mouse press: {self.label}", flush=True)
        elif etype == QEvent.MouseButtonRelease:
            print(f"[GUI {_ts()}] button mouse release: {self.label}", flush=True)
        return False

class VacuumControlPage(QWidget):
    """真空控制页面 - 使用共享 Worker 避免重复连接"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(20, 20, 20, 20)
        self.layout.setSpacing(10)
        
        # 标题
        title = QLabel("真空抽气控制")
        title.setProperty("role", "title")
        self.layout.addWidget(title)
        
        # 使用水平分割器：左侧是Tab区域，右侧是状态面板
        main_splitter = QSplitter(Qt.Horizontal)
        main_splitter.setHandleWidth(1)
        main_splitter.setStyleSheet("QSplitter::handle { background-color: #1c3146; }")
        
        self.tabs = QTabWidget()
        main_splitter.addWidget(self.tabs)
        
        # 右侧：状态面板（整合到页面中）
        self._setup_status_panel()
        main_splitter.addWidget(self.status_panel_widget)
        
        # 设置右侧状态面板的固定宽度
        from ..config import UI_SETTINGS
        status_panel_width = UI_SETTINGS.get("status_panel_width", 400)
        self.status_panel_widget.setFixedWidth(status_panel_width)
        self.status_panel_widget.setMinimumWidth(status_panel_width)
        self.status_panel_widget.setMaximumWidth(status_panel_width)
        
        # 设置分割器比例（左侧自适应，右侧固定宽度）
        main_splitter.setStretchFactor(0, 1)
        main_splitter.setStretchFactor(1, 0)
        main_splitter.setCollapsible(1, True)
        
        # 设置分割器尺寸策略
        from PyQt5.QtWidgets import QSizePolicy
        main_splitter.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        
        self.layout.addWidget(main_splitter, 1)  # 添加拉伸因子
        
        # 创建共享的 Worker（避免两个 Tab 各自创建）- 延迟启动
        self._shared_worker = None
        self._worker_started = False  # 标记 Worker 是否已启动
        
        # 初始化各Tab页（Worker 将延迟设置）
        self.tab_param = ParameterControlTab(shared_worker=None)
        self.tab_twin = DigitalTwinTab(shared_worker=None)
        self.tab_alarm = AlarmRecordTab()
        self.tab_history = HistoryRecordTab()
        self.tab_trend = TrendCurveTab()
        
        self.tabs.addTab(self.tab_param, "配置控制页")
        self.tabs.addTab(self.tab_twin, "数字孪生仿真页")
        self.tabs.addTab(self.tab_alarm, "报警记录页")
        self.tabs.addTab(self.tab_history, "历史记录页")
        self.tabs.addTab(self.tab_trend, "趋势曲线页")
        # 注意：不在初始化时启动 Worker，等待外部调用 start_worker()
    
    def start_worker(self):
        """启动 Worker（延迟启动，由 MainWindow 调用）"""
        if self._worker_started:
            return
        self._worker_started = True
        
        if not PyTango:
            return
        
        self._shared_worker = TangoWorker("sys/vacuum/1", self)
        
        # 设置 Worker 到各 Tab
        self.tab_param.set_worker(self._shared_worker)
        self.tab_twin.set_worker(self._shared_worker)
        
        # 收集所有 Tab 需要的属性
        all_attrs = set()
        all_attrs.update(self.tab_param.get_required_attrs())
        all_attrs.update(self.tab_twin.get_required_attrs())
        self._shared_worker.set_read_attributes(list(all_attrs))
        self._shared_worker.start()
        
        # 初始化真空状态显示（如果状态面板已创建）
        if hasattr(self, 'vacuum_list_layout'):
            self._init_vacuum_status()
            
            # 创建真空状态更新定时器
            self._vacuum_status_timer = QTimer(self)
            self._vacuum_status_timer.timeout.connect(self._update_vacuum_status)
            self._vacuum_status_timer.start(300)  # 300ms更新一次
    
    def _setup_status_panel(self):
        """创建并设置状态面板（整合到页面中，显示真空状态）"""
        # 状态面板容器
        self.status_panel_widget = QFrame()
        self.status_panel_widget.setProperty("role", "status_panel")
        status_layout = QVBoxLayout(self.status_panel_widget)
        status_layout.setSpacing(16)
        status_layout.setContentsMargins(16, 16, 16, 16)
        
        # 标题
        status_title = QLabel("真空状态")
        status_title.setProperty("role", "title")
        status_title.setStyleSheet("font-size: 16px; font-weight: bold; color: #00a0e9; margin-bottom: 10px;")
        status_layout.addWidget(status_title)
        
        # 真空状态列表区域（使用滚动区域）
        status_scroll = QScrollArea()
        status_scroll.setWidgetResizable(True)
        status_scroll.setFrameShape(QFrame.NoFrame)
        status_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        
        self.vacuum_list_container = QWidget()
        self.vacuum_list_layout = QGridLayout(self.vacuum_list_container)
        self.vacuum_list_layout.setSpacing(12)
        self.vacuum_list_layout.setVerticalSpacing(16)
        self.vacuum_list_layout.setContentsMargins(0, 0, 0, 0)
        self.vacuum_widgets = {}  # Map item name -> widgets dict
        
        status_scroll.setWidget(self.vacuum_list_container)
        status_layout.addWidget(status_scroll, 1)  # 添加拉伸因子
    
    def _init_vacuum_status(self):
        """初始化真空状态列表"""
        # 清除旧内容
        while self.vacuum_list_layout.count():
            item = self.vacuum_list_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()
        
        self.vacuum_widgets.clear()
        
        # 辅助函数：查找设备配置
        def find_device_config(name):
            # 搜索前级组
            for dev in VACUUM_CONTROL_CONFIG["foreline_group"]["devices"]:
                if dev["name"] == name: return dev
            # 搜索分子泵组
            for group in VACUUM_CONTROL_CONFIG["molecular_groups"]:
                for dev in group["devices"]:
                    if dev["name"] == name: return dev
            # 搜索主阀组
            for dev in VACUUM_CONTROL_CONFIG["main_vent_group"]["devices"]:
                if dev["name"] == name: return dev
            return None
            
        # 辅助函数：查找真空规配置
        def find_gauge_config(name):
            for gauge in VACUUM_CONTROL_CONFIG["gauges"]:
                if gauge["name"] == name: return gauge
            return None
        
        row = 0
        
        # 表头
        self.vacuum_list_layout.addWidget(QLabel("设备"), row, 0)
        self.vacuum_list_layout.addWidget(QLabel("状态"), row, 1)
        row += 1
        
        for category in VACUUM_CONTROL_CONFIG.get("status_items", []):
            # 分类标题
            cat_lbl = QLabel(category["category"])
            cat_lbl.setStyleSheet("color: #00a0e9; font-weight: bold; margin-top: 10px;")
            self.vacuum_list_layout.addWidget(cat_lbl, row, 0, 1, 2)
            row += 1
            
            for item_name in category["items"]:
                name_lbl = QLabel(item_name)
                name_lbl.setStyleSheet("color: #8fa6c5; padding-left: 10px;")
                
                # 状态容器
                status_widget = QWidget()
                status_layout = QHBoxLayout(status_widget)
                status_layout.setContentsMargins(0, 0, 0, 0)
                status_layout.setSpacing(10)
                
                widgets_entry = {}
                
                # 检查是否是真空规
                gauge_conf = find_gauge_config(item_name)
                if gauge_conf:
                    val_lbl = QLabel("--- " + gauge_conf["unit"])
                    val_lbl.setStyleSheet("color: #90EE90; font-weight: bold;")
                    status_layout.addWidget(val_lbl)
                    widgets_entry["val_lbl"] = val_lbl
                    widgets_entry["config"] = gauge_conf
                else:
                    status_lbl = QLabel("关闭") # 默认状态
                    status_lbl.setStyleSheet("color: gray;")
                    status_layout.addWidget(status_lbl)
                    widgets_entry["status_lbl"] = status_lbl
                    
                    # 检查是否需要显示频率
                    dev_conf = find_device_config(item_name)
                    if dev_conf:
                        widgets_entry["config"] = dev_conf
                        
                        # Water Status
                        if "water" in dev_conf.get("type", ""):
                            water_lbl = QLabel("水冷:正常")
                            water_lbl.setStyleSheet("color: #39e072; font-size: 11px;")
                            status_layout.addWidget(water_lbl)
                            widgets_entry["water_lbl"] = water_lbl
                            
                        # Frequency Status
                        if "attr_freq" in dev_conf:
                            freq_lbl = QLabel("0Hz")
                            freq_lbl.setStyleSheet("color: #29d6ff; font-weight: bold; font-size: 11px;")
                            status_layout.addWidget(freq_lbl)
                            widgets_entry["freq_lbl"] = freq_lbl
                
                status_layout.addStretch()
                
                self.vacuum_list_layout.addWidget(name_lbl, row, 0)
                self.vacuum_list_layout.addWidget(status_widget, row, 1)
                
                self.vacuum_widgets[item_name] = widgets_entry
                row += 1
    
    def _update_vacuum_status(self):
        """定时更新真空状态（从Worker缓存拉取）"""
        if not self._shared_worker or not self.vacuum_widgets:
            return
        
        # 从 Worker 的缓存获取数据
        status_data = self._shared_worker.get_cached_status() if hasattr(self._shared_worker, 'get_cached_status') else None
        if not status_data:
            return
        
        for name, widgets in self.vacuum_widgets.items():
            config = widgets.get("config")
            if not config:
                continue
            
            try:
                # Gauge (真空规)
                if "attr" in config and "unit" in config:
                    attr_name = config["attr"]
                    if attr_name in status_data:
                        val = status_data[attr_name]
                        widgets["val_lbl"].setText(f"{val:.2e} {config['unit']}")
                    continue
                
                # Device Status (开关状态)
                if "attr_state" in config:
                    attr_name = config["attr_state"]
                    if attr_name in status_data:
                        is_on = status_data[attr_name]
                        lbl = widgets.get("status_lbl")
                        if lbl:
                            lbl.setText("开启" if is_on else "关闭")
                            lbl.setStyleSheet("color: #39e072;" if is_on else "color: gray;")
                
                # Water
                if "attr_water" in config and "water_lbl" in widgets:
                    attr_name = config["attr_water"]
                    if attr_name in status_data:
                        fault = status_data[attr_name]
                        lbl = widgets["water_lbl"]
                        lbl.setText("水冷:异常" if fault else "水冷:正常")
                        lbl.setStyleSheet("color: red;" if fault else "color: #39e072; font-size: 11px;")
                    
                # Frequency
                if "attr_freq" in config and "freq_lbl" in widgets:
                    attr_name = config["attr_freq"]
                    if attr_name in status_data:
                        freq = status_data[attr_name]
                        widgets["freq_lbl"].setText(f"{int(freq)}Hz")
                    
            except Exception:
                pass
    
    def get_worker(self):
        """获取共享的 TangoWorker（供状态面板使用）"""
        return self._shared_worker
    
    def cleanup(self):
        """清理资源"""
        # 停止真空状态更新定时器
        if hasattr(self, '_vacuum_status_timer') and self._vacuum_status_timer:
            self._vacuum_status_timer.stop()
            self._vacuum_status_timer = None
        
        # 停止Worker线程
        if self._shared_worker:
            self._shared_worker.stop()
            self._shared_worker = None
    
    def cleanup(self):
        """清理所有后台线程（在应用退出前调用）"""
        print(f"[GUI {_ts()}] VacuumControlPage cleanup...", flush=True)
        if self._shared_worker:
            self._shared_worker.stop()
    
    def closeEvent(self, event):
        """窗口关闭时清理线程"""
        self.cleanup()
        super().closeEvent(event)

class ParameterControlTab(QWidget):
    """参数控制 Tab - 使用定时器拉取状态，避免信号导致的卡顿"""
    
    def __init__(self, parent=None, shared_worker=None):
        super().__init__(parent)
        self.device_widgets = {}
        self.gauges = {}
        self._probes = []
        self._worker = shared_worker
        
        self._setup_ui()
        
        # 使用 QTimer 定时拉取状态（不依赖信号）
        self._update_timer = QTimer(self)
        self._update_timer.timeout.connect(self._pull_status)
        self._update_timer.start(300)  # 每 300ms 拉取一次
        
        # 只连接命令完成信号（低频）
        if self._worker:
            self._worker.command_done.connect(self._on_command_done)
    
    def set_worker(self, worker):
        """设置 Worker（延迟设置）"""
        if self._worker:
            return  # 已设置，忽略
        self._worker = worker
        if self._worker:
            self._worker.command_done.connect(self._on_command_done)
    
    def get_required_attrs(self):
        """返回此 Tab 需要读取的属性列表"""
        attrs = []
        for g in VACUUM_CONTROL_CONFIG["gauges"]:
            if "attr" in g:
                attrs.append(g["attr"])
        for dev_id, widgets in self.device_widgets.items():
            conf = widgets.get("config", {})
            if "attr_state" in conf:
                attrs.append(conf["attr_state"])
            if "attr_water" in conf:
                attrs.append(conf["attr_water"])
            if "attr_freq" in conf:
                attrs.append(conf["attr_freq"])
        # 系统模式状态
        attrs.extend(["autoState", "manualState", "remoteState"])
        return attrs
        
    def _pull_status(self):
        """定时从 Worker 拉取状态数据并更新 UI（由 QTimer 调用）"""
        if not self._worker:
            return
        
        status_data = self._worker.get_cached_status()
        if status_data:
            self._apply_status_to_ui(status_data)
    
    def _apply_status_to_ui(self, status_data: dict):
        """实际更新 UI 的方法（在节流后调用）"""
        # 更新真空规
        for gauge_id, display in self.gauges.items():
            for g in VACUUM_CONTROL_CONFIG["gauges"]:
                if g["id"] == gauge_id and g["attr"] in status_data:
                    display.set_value(status_data[g["attr"]])
                    break
    
        # 更新设备状态
        for dev_id, widgets in self.device_widgets.items():
            conf = widgets.get("config", {})
        
            if "attr_state" in conf and conf["attr_state"] in status_data:
                is_on = bool(status_data[conf["attr_state"]])
                widgets["indicator"].set_status(is_on)
                widgets["status_text"].setText("开启" if is_on else "关闭")
                widgets["status_text"].setStyleSheet("color: #39e072;" if is_on else "color: gray;")
        
            if "water" in widgets and "attr_water" in conf and conf["attr_water"] in status_data:
                fault = status_data[conf["attr_water"]]
                if fault:
                    widgets["water"].setText("水冷:故障")
                    widgets["water"].setStyleSheet("color: #e74c3c; font-weight: bold; font-size: 11px;")
                else:
                    widgets["water"].setText("水冷:正常")
                    widgets["water"].setStyleSheet("color: #39e072; font-weight: bold; font-size: 11px;")
                
            if "freq" in widgets and "attr_freq" in conf and conf["attr_freq"] in status_data:
                freq = status_data[conf["attr_freq"]]
                widgets["freq"].setText(f"{freq:.1f}Hz")
                
    def _on_command_done(self, cmd_name: str, success: bool, message: str):
            """命令执行完成回调"""
            if success:
                print(f"[GUI {_ts()}] Command {cmd_name} succeeded: {message}", flush=True)
            else:
                print(f"[GUI {_ts()}] Command {cmd_name} failed: {message}", flush=True)

    def _handle_command(self, cmd_config):
            """处理设备命令发送（通过后台线程）"""
            print(f"[GUI {_ts()}] _handle_command called with: {cmd_config}", flush=True)
        
            if not self._worker:
                print(f"[GUI {_ts()}] Worker not running!", flush=True)
                return
        
            cmd_name = cmd_config["name"]
            args = cmd_config.get("args", None)
        
            # 处理参数类型转换
            if args:
                converted_args = []
                for arg in args:
                    if isinstance(arg, bool):
                        converted_args.append(1 if arg else 0)
                    else:
                        converted_args.append(arg)
            
                if len(converted_args) == 1:
                    args = converted_args[0]
                else:
                    args = converted_args
        
            print(f"[GUI {_ts()}] Queueing command: {cmd_name} with args: {args}", flush=True)
            self._worker.queue_command(cmd_name, args)

    def _setup_ui(self):
            # 使用 ScrollArea 确保小屏幕也能显示
            scroll = QScrollArea()
            scroll.setWidgetResizable(True)
            scroll.setFrameShape(QFrame.NoFrame)
        
            content_widget = QWidget()
            main_layout = QVBoxLayout(content_widget)
            main_layout.setSpacing(15)
        
            # 1. 真空规读数显示 (顶部)
            gauge_group = QGroupBox("真空规读数")
            gauge_layout = QHBoxLayout(gauge_group)
            self.gauges = {}
            for gauge_conf in VACUUM_CONTROL_CONFIG["gauges"]:
                container = QFrame()
                container.setFrameStyle(QFrame.StyledPanel | QFrame.Sunken)
                container.setMinimumHeight(60) # Increase height
            
                # Use HBox for single line merged display
                h_layout = QHBoxLayout(container)
                h_layout.setContentsMargins(10, 5, 10, 5)
                h_layout.setSpacing(10)
            
                lbl_name = QLabel(gauge_conf["name"])
                lbl_name.setAlignment(Qt.AlignVCenter | Qt.AlignLeft)
                lbl_name.setStyleSheet("font-weight: bold; color: #8fa6c5;")
            
                val_display = ValueDisplay("---", gauge_conf["unit"])
                val_display.set_large(True)
            
                h_layout.addWidget(lbl_name)
                h_layout.addStretch()
                h_layout.addWidget(val_display)
            
                gauge_layout.addWidget(container)
                self.gauges[gauge_conf["id"]] = val_display
            
            main_layout.addWidget(gauge_group)
        
            # 2. 系统控制组
            sys_group = QGroupBox("系统控制")
            sys_layout = QHBoxLayout(sys_group)
            for ctrl in VACUUM_CONTROL_CONFIG["system_controls"]:
                btn = ControlButton(ctrl["name"])
                btn.setProperty("role", ctrl["role"])
                # 连接信号
                if "command" in ctrl:
                    probe = _ClickProbe(ctrl.get("name", "system_btn"), self)
                    btn.installEventFilter(probe)
                    self._probes.append(probe)
                    btn.clicked.connect(lambda checked, c=ctrl["command"]: self._handle_command(c))
                sys_layout.addWidget(btn)
            main_layout.addWidget(sys_group)
        
            # 3. 中间区域：前级泵阀组 (左) + 主阀及放气阀组 (右)
            mid_container = QWidget()
            mid_layout = QHBoxLayout(mid_container)
            mid_layout.setContentsMargins(0, 0, 0, 0)
            mid_layout.setSpacing(15)
        
            # 左侧：前级泵阀组
            fore_group = QGroupBox(VACUUM_CONTROL_CONFIG["foreline_group"]["name"])
            fore_layout = QGridLayout(fore_group)
            self._build_device_grid(fore_layout, VACUUM_CONTROL_CONFIG["foreline_group"]["devices"])
            mid_layout.addWidget(fore_group, 1) # Stretch factor 1
        
            # 右侧：主阀及放气阀组 (包含状态显示)
            main_vent_group = QGroupBox(VACUUM_CONTROL_CONFIG["main_vent_group"]["name"])
            mv_layout = QVBoxLayout(main_vent_group)
        
            # 上半部分：设备控制
            dev_frame = QFrame()
            dev_grid = QGridLayout(dev_frame)
            dev_grid.setContentsMargins(0, 0, 0, 0)
            self._build_device_grid(dev_grid, VACUUM_CONTROL_CONFIG["main_vent_group"]["devices"], show_water=False)
            mv_layout.addWidget(dev_frame)
        
            # 分割线 (Separator)
            line = QFrame()
            line.setFrameShape(QFrame.HLine)
            line.setFrameShadow(QFrame.Sunken)
            line.setStyleSheet("background-color: #1c3146; margin: 10px 0;")
            mv_layout.addWidget(line)
        
            # 下半部分：状态显示 (并排显示)
            status_container = QWidget()
            status_layout = QHBoxLayout(status_container)
            status_layout.setContentsMargins(0, 0, 0, 0)
        
            # 仅显示项 (真空计手动阀)
            display_frame = QFrame()
            disp_layout = QVBoxLayout(display_frame)
            disp_layout.setContentsMargins(0, 0, 0, 0)
            for item in VACUUM_CONTROL_CONFIG["main_vent_group"]["display_only"]:
                status_text = "开" if item['status'] == "OPEN" else "关"
                lbl = QLabel(f"{item['name']}: {status_text}")
                lbl.setStyleSheet("color: #8fa6c5; font-weight: bold;")
                disp_layout.addWidget(lbl)
            status_layout.addWidget(display_frame)
        
            # 系统状态 (允许抽真空/放气)
            sys_stat_frame = QFrame()
            sys_stat_layout = QVBoxLayout(sys_stat_frame)
            sys_stat_layout.setContentsMargins(0, 0, 0, 0)
            for item in VACUUM_CONTROL_CONFIG["main_vent_group"]["system_status"]:
                h = QHBoxLayout()
                lbl = QLabel(item["name"])
                indicator = StatusIndicator() # 默认为灰色
                h.addWidget(lbl)
                h.addWidget(indicator)
                h.addStretch()
                sys_stat_layout.addLayout(h)
            status_layout.addWidget(sys_stat_frame)
        
            mv_layout.addWidget(status_container)
            mid_layout.addWidget(main_vent_group, 1) # Stretch factor 1
        
            main_layout.addWidget(mid_container)
        
            # 4. 分子泵阀组 (3组并列)
            mol_container = QWidget()
            mol_layout = QHBoxLayout(mol_container)
            mol_layout.setContentsMargins(0, 0, 0, 0)
            mol_layout.setSpacing(15)
        
            for group_conf in VACUUM_CONTROL_CONFIG["molecular_groups"]:
                g_box = QGroupBox(group_conf["name"])
                g_grid = QGridLayout(g_box)
                self._build_device_grid(g_grid, group_conf["devices"])
                mol_layout.addWidget(g_box)
            
            main_layout.addWidget(mol_container)
        
            # 底部弹簧
            main_layout.addStretch()
        
            scroll.setWidget(content_widget)
        
            final_layout = QVBoxLayout(self)
            final_layout.setContentsMargins(0, 0, 0, 0)
            final_layout.addWidget(scroll)

    def _build_device_grid(self, layout, devices, show_water=True):
            """构建设备控制网格: 名称 | 状态(指示灯+文字+水冷) | 操作"""
            # Headers
            layout.addWidget(QLabel("设备名称"), 0, 0)
            layout.addWidget(QLabel("状态"), 0, 1)
            layout.addWidget(QLabel("操作"), 0, 2)
        
            for i, dev in enumerate(devices, 1):
                # Name
                layout.addWidget(QLabel(dev["name"]), i, 0)
            
                # Status Container
                status_widget = QWidget()
                status_layout = QHBoxLayout(status_widget)
                status_layout.setContentsMargins(0, 0, 0, 0)
                status_layout.setSpacing(8)
            
                indicator = StatusIndicator()
                status_layout.addWidget(indicator)
            
                # Info Container (Text + Water + Freq)
                info_container = QWidget()
                # Use VBox for Molecular Pumps to stack Water/Freq, HBox for others if simple
                if "molecular" in dev.get("type", ""):
                    info_layout = QVBoxLayout(info_container)
                    info_layout.setSpacing(2)
                else:
                    info_layout = QHBoxLayout(info_container)
                    info_layout.setSpacing(8)
                info_layout.setContentsMargins(0, 0, 0, 0)
            
                status_text = QLabel("关闭")
                status_text.setStyleSheet("color: gray;")
                info_layout.addWidget(status_text)
            
                # Water Status (if applicable)
                if show_water and "water" in dev.get("type", ""):
                    water_lbl = QLabel("水冷:正常")
                    water_lbl.setStyleSheet("color: #39e072; font-weight: bold; font-size: 11px;") # Green
                    info_layout.addWidget(water_lbl)
            
                # Frequency Status (if applicable)
                freq_lbl = None
                if "attr_freq" in dev:
                    freq_lbl = QLabel("0Hz")
                    freq_lbl.setStyleSheet("color: #29d6ff; font-weight: bold; font-size: 11px;") # Cyan
                    info_layout.addWidget(freq_lbl)

                # Store widgets for update
                widgets = {
                    "indicator": indicator,
                    "status_text": status_text,
                    "config": dev # Store config for attribute names
                }
                # Need to capture the local variables
                if show_water and "water" in dev.get("type", ""):
                    widgets["water"] = water_lbl
                if freq_lbl:
                    widgets["freq"] = freq_lbl
            
                self.device_widgets[dev["id"]] = widgets

                status_layout.addWidget(info_container)
                status_layout.addStretch()
                layout.addWidget(status_widget, i, 1)
            
                # Control
                btn_layout = QHBoxLayout()
                btn_layout.setContentsMargins(0, 0, 0, 0)
                if "pump" in dev["type"]:
                    btn_start = ControlButton("启动")
                    btn_stop = ControlButton("停止")
                    btn_stop.setProperty("role", "stop")
                
                    if "cmd_start" in dev:
                        probe = _ClickProbe(f"{dev['name']}-start", self)
                        btn_start.installEventFilter(probe)
                        self._probes.append(probe)
                        btn_start.clicked.connect(lambda checked, c=dev["cmd_start"]: self._handle_command(c))
                    if "cmd_stop" in dev:
                        probe = _ClickProbe(f"{dev['name']}-stop", self)
                        btn_stop.installEventFilter(probe)
                        self._probes.append(probe)
                        btn_stop.clicked.connect(lambda checked, c=dev["cmd_stop"]: self._handle_command(c))
                
                    btn_layout.addWidget(btn_start)
                    btn_layout.addWidget(btn_stop)
                else: # valve
                    btn_open = ControlButton("打开")
                    btn_close = ControlButton("关闭")
                    btn_close.setProperty("role", "stop")
                
                    if "cmd_open" in dev:
                        probe = _ClickProbe(f"{dev['name']}-open", self)
                        btn_open.installEventFilter(probe)
                        self._probes.append(probe)
                        btn_open.clicked.connect(lambda checked, c=dev["cmd_open"]: self._handle_command(c))
                    if "cmd_close" in dev:
                        probe = _ClickProbe(f"{dev['name']}-close", self)
                        btn_close.installEventFilter(probe)
                        self._probes.append(probe)
                        btn_close.clicked.connect(lambda checked, c=dev["cmd_close"]: self._handle_command(c))
                
                    btn_layout.addWidget(btn_open)
                    btn_layout.addWidget(btn_close)
            
                container = QWidget()
                container.setLayout(btn_layout)
                layout.addWidget(container, i, 2)

# --- Digital Twin Components ---

class TwinDeviceItem(QGraphicsItem):
    def __init__(self, name, x, y, w, h, dev_type="pump", has_water=False):
        super().__init__()
        self.name = name
        self.dev_type = dev_type
        self.rect = QRectF(x, y, w, h)
        self.is_on = False
        self.hz_val = 0
        self.status_text = "关闭"
        self.water_ok = True
        self.has_water = has_water
        self.attr_name = None  # 对应的 Tango 属性名
        self.command_callback = None  # 命令回调函数
        
        self.setAcceptHoverEvents(True)
        
        # Status Labels (Child Items)
        # 1. On/Off Status
        self.lbl_status = QGraphicsTextItem(self.status_text, self)
        self.lbl_status.setDefaultTextColor(QColor("gray"))
        self.lbl_status.setFont(QFont("Arial", 9))
        self.lbl_status.setPos(x + w + 5, y - 5)
        
        # 2. Water Status (if applicable)
        self.lbl_water = None
        if self.has_water:
            self.lbl_water = QGraphicsTextItem("水冷:正常", self)
            self.lbl_water.setDefaultTextColor(QColor("#39e072"))
            self.lbl_water.setFont(QFont("Arial", 8))
            self.lbl_water.setPos(x + w + 5, y + 15)
            
        # 3. Frequency (if applicable)
        self.lbl_hz = None
        if "pump" in dev_type and "valve" not in dev_type:
            self.lbl_hz = QGraphicsTextItem("0Hz", self)
            self.lbl_hz.setDefaultTextColor(QColor("#29d6ff"))
            self.lbl_hz.setFont(QFont("Arial", 8, QFont.Bold))
            # Adjust position based on water label presence
            y_offset = 30 if self.has_water else 15
            self.lbl_hz.setPos(x + w + 5, y + y_offset)

    def boundingRect(self):
        return self.rect

    def paint(self, painter, option, widget):
        # Draw Connection Lines/Ports if needed
        # ...
        
        # Draw Body
        pen = QPen(QColor("#1c3146"), 2)
        if self.is_on:
            brush = QBrush(QColor("#39e072")) # Green
        else:
            brush = QBrush(QColor("#4b5c74")) # Gray
            
        painter.setPen(pen)
        painter.setBrush(brush)
        
        if self.dev_type == "valve":
            # Draw Bowtie/Valve shape
            path = QPainterPath()
            cx, cy = self.rect.center().x(), self.rect.center().y()
            rx, ry = self.rect.width()/2, self.rect.height()/2
            # Left Triangle
            path.moveTo(self.rect.left(), self.rect.top())
            path.lineTo(self.rect.left(), self.rect.bottom())
            path.lineTo(cx, cy)
            # Right Triangle
            path.lineTo(self.rect.right(), self.rect.bottom())
            path.lineTo(self.rect.right(), self.rect.top())
            path.lineTo(cx, cy)
            path.closeSubpath()
            painter.drawPath(path)
            
            # --- Symbol Distinction ---
            painter.setPen(QPen(QColor("white"), 1))
            if "电磁" in self.name: # Solenoid Valve
                # Draw a small box (coil) on top center
                coil_w, coil_h = 14, 10
                coil_rect = QRectF(cx - coil_w/2, self.rect.top() + 2, coil_w, coil_h)
                painter.setBrush(QBrush(QColor("#1c3146"))) # Dark fill
                painter.drawRect(coil_rect)
                painter.drawLine(coil_rect.bottomLeft(), coil_rect.topRight()) # Diagonal
            elif "放气" in self.name: # Vent Valve
                # Draw a 'V' mark in the corner
                painter.setPen(QPen(QColor("#29d6ff"), 2))
                painter.drawText(QRectF(self.rect.left()+4, self.rect.top(), 15, 15), Qt.AlignLeft, "V")
            elif "闸板" in self.name: # Gate Valve
                # Draw a vertical line in center (Gate)
                painter.drawLine(int(cx), int(self.rect.top()+5), int(cx), int(self.rect.bottom()-5))
                
        elif "pump" in self.dev_type:
            # Draw Circle
            painter.drawEllipse(self.rect)
            # Draw Symbol inside
            painter.setPen(QPen(QColor("white"), 2))
            if "分子" in self.name: # Turbo
                # Draw Turbine blades
                cx, cy = self.rect.center().x(), self.rect.center().y()
                r = min(self.rect.width(), self.rect.height()) / 2 * 0.7
                painter.drawLine(int(cx-r), int(cy), int(cx+r), int(cy))
                painter.drawLine(int(cx), int(cy-r), int(cx), int(cy+r))
                painter.drawEllipse(int(cx-r/2), int(cy-r/2), int(r), int(r))
            elif "罗茨" in self.name: # Roots
                # Draw Figure 8
                cx, cy = self.rect.center().x(), self.rect.center().y()
                r = min(self.rect.width(), self.rect.height()) / 4
                painter.drawEllipse(int(cx-r), int(cy-r), int(2*r), int(2*r))
                painter.drawEllipse(int(cx-r), int(cy+r-2*r), int(2*r), int(2*r)) # Simplified
            elif "螺杆" in self.name: # Screw
                # Draw Screw symbol (simplified)
                cx, cy = self.rect.center().x(), self.rect.center().y()
                painter.drawLine(int(self.rect.left()+5), int(cy), int(self.rect.right()-5), int(cy))
                painter.drawLine(int(cx), int(self.rect.top()+5), int(cx), int(self.rect.bottom()-5))
        else:
            painter.drawRect(self.rect)

        # Draw Name
        painter.setPen(QColor("white"))
        painter.drawText(self.rect, Qt.AlignCenter, self.name)
        
    def set_state(self, on):
        self.is_on = on
        self.status_text = "开启" if on else "关闭"
        self.lbl_status.setPlainText(self.status_text)
        self.lbl_status.setDefaultTextColor(QColor("#39e072") if on else QColor("gray"))
        self.update()
            
    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.show_context_menu(event.screenPos())
            
    def show_context_menu(self, pos):
        menu = QMenu()
        action_on = menu.addAction("开启")
        action_off = menu.addAction("关闭")
        
        action = menu.exec_(pos)
        if action == action_on:
            if self.command_callback:
                self.command_callback(self.attr_name, True)
            self.set_state(True)
        elif action == action_off:
            if self.command_callback:
                self.command_callback(self.attr_name, False)
            self.set_state(False)

class DigitalTwinTab(QWidget):
    """数字孪生 Tab - 使用定时器拉取状态"""
    
    def __init__(self, parent=None, shared_worker=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        
        self.view = QGraphicsView()
        self.scene = QGraphicsScene()
        self.view.setScene(self.scene)
        self.view.setBackgroundBrush(QBrush(QColor("#040a13")))
        
        layout.addWidget(self.view)
        
        self.items_map = {} # Map attribute name -> TwinDeviceItem
        self.gauge_items = {} # Map gauge attr -> (value_text_item)
        self._worker = shared_worker
        
        self._init_scene()
        self._setup_item_callbacks()
        
        # 使用 QTimer 定时拉取状态
        self._update_timer = QTimer(self)
        self._update_timer.timeout.connect(self._pull_status)
        self._update_timer.start(300)  # 每 300ms 拉取一次
        
        # 只连接命令完成信号
        if self._worker:
            self._worker.command_done.connect(self._on_command_done)
    
    def set_worker(self, worker):
        """设置 Worker（延迟设置）"""
        if self._worker:
            return  # 已设置，忽略
        self._worker = worker
        if self._worker:
            self._worker.command_done.connect(self._on_command_done)

    def get_required_attrs(self):
        """返回此 Tab 需要读取的属性列表"""
        attrs = list(self.items_map.keys()) + list(self.gauge_items.keys())
        # 添加速度和水冷属性
        for attr in list(attrs):
            if "Power" in attr:
                attrs.append(attr.replace("Power", "Speed"))
                if "screw" in attr:
                    attrs.append("screwPumpWaterFault")
                else:
                    attrs.append(attr.replace("Power", "WaterFault"))
        return attrs
        
    def _pull_status(self):
        """定时从 Worker 拉取状态数据并更新 UI"""
        if not self._worker:
            return
        
        status_data = self._worker.get_cached_status()
        if status_data:
            self._apply_status_to_ui(status_data)
    
    def _apply_status_to_ui(self, status_data: dict):
        """实际更新 UI"""
        # 真空规读数
        for attr, val in status_data.items():
            if attr in self.gauge_items:
                text_item = self.gauge_items[attr]
                text_item.setPlainText(f"{self._format_sci(val)} Pa")
        
        for attr_name, item in self.items_map.items():
            if attr_name in status_data:
                item.set_state(bool(status_data[attr_name]))
            
            # 更新频率
            if "pump" in item.dev_type and "valve" not in item.dev_type:
                speed_attr = attr_name.replace("Power", "Speed")
                if speed_attr in status_data and item.lbl_hz:
                    item.lbl_hz.setPlainText(f"{int(status_data[speed_attr])}Hz")
            
            # 更新水冷状态
            if item.has_water:
                water_attr = attr_name.replace("Power", "WaterFault")
                if "screw" in attr_name:
                    water_attr = "screwPumpWaterFault"
                if water_attr in status_data and item.lbl_water:
                    fault = status_data[water_attr]
                    item.lbl_water.setPlainText("水冷:异常" if fault else "水冷:正常")
                    item.lbl_water.setDefaultTextColor(QColor("red") if fault else QColor("#39e072"))
                    
    def _on_command_done(self, cmd_name: str, success: bool, message: str):
        """命令执行完成回调"""
        if success:
            print(f"[DigitalTwin] Command {cmd_name} succeeded: {message}", flush=True)
        else:
            print(f"[DigitalTwin] Command {cmd_name} failed: {message}", flush=True)

    def _setup_item_callbacks(self):
        """为所有 TwinDeviceItem 设置命令回调"""
        for attr_name, item in self.items_map.items():
            item.attr_name = attr_name
            item.command_callback = self._send_command

    def _send_command(self, attr_name, state):
        """发送命令到服务器（通过后台线程）"""
        print(f"[DigitalTwin] _send_command called: attr={attr_name}, state={state}", flush=True)
        
        if not self._worker:
            print("[DigitalTwin] Worker not running!", flush=True)
            return
            
        cmd_name, args = self._get_command_for_attr(attr_name, state)
        if cmd_name:
            print(f"[DigitalTwin] Queueing command: {cmd_name} with args: {args}", flush=True)
            self._worker.queue_command(cmd_name, args)

    @staticmethod
    def _format_sci(val):
        try:
            return f"{float(val):.3e}"
        except Exception:
            return str(val)

    def _get_command_for_attr(self, attr_name, state):
        """根据属性名获取对应的命令和参数"""
        state_val = 1 if state else 0
        
        # 闸板阀: gateValve1Open -> setGateValve [1, 1/0]
        if attr_name.startswith("gateValve") and "Open" in attr_name:
            idx = int(attr_name.replace("gateValve", "").replace("Open", ""))
            return "setGateValve", [idx, state_val]
        
        # 电磁阀: electromagneticValve1Open -> setElectromagneticValve [1, 1/0]
        if attr_name.startswith("electromagneticValve") and "Open" in attr_name:
            idx = int(attr_name.replace("electromagneticValve", "").replace("Open", ""))
            return "setElectromagneticValve", [idx, state_val]
        
        # 放气阀: ventValve1Open -> setVentValve [1, 1/0]
        if attr_name.startswith("ventValve") and "Open" in attr_name:
            idx = int(attr_name.replace("ventValve", "").replace("Open", ""))
            return "setVentValve", [idx, state_val]
        
        # 分子泵: molecularPump1Power -> setMolecularPumpPower [1, 1/0]
        if attr_name.startswith("molecularPump") and "Power" in attr_name:
            idx = int(attr_name.replace("molecularPump", "").replace("Power", ""))
            return "setMolecularPumpPower", [idx, state_val]
        
        # 罗茨泵: rootsPumpPower -> setRootsPumpPower
        if attr_name == "rootsPumpPower":
            return "setRootsPumpPower", state_val
        
        # 螺杆泵: screwPumpPower -> setScrewPumpPower  
        if attr_name == "screwPumpPower":
            return "setScrewPumpPower", state_val
        
        return None, None
    
    def _init_scene(self):
        # --- Topology Layout based on Reference Image ---
        
        # 1. Vacuum Chamber (Top Center) - Extended Left
        chamber = QGraphicsRectItem(100, 50, 700, 120)
        chamber.setPen(QPen(QColor("#c3cede"), 2))
        chamber.setBrush(QBrush(QColor("#1c3146")))
        self.scene.addItem(chamber)
        
        text = self.scene.addText("真空腔室")
        text.setDefaultTextColor(QColor("white"))
        text.setFont(QFont("Arial", 16, QFont.Bold))
        text.setPos(400, 90)
        
        # 2. Gauges & Manual Valves (Top of Chamber)
        # Gauge 1 (Left)
        self._add_gauge_node(350, 10, "主真空计1", "vacuumGauge2")
        self._add_manual_valve(350, 35, "手动阀1")
        
        # Gauge 2 (Right)
        self._add_gauge_node(650, 10, "主真空计2", "vacuumGauge3")
        self._add_manual_valve(650, 35, "手动阀2")
        
        # Top Vent Valve (Parallel to Gauges - Center) - Moved Left of Gauge 1
        self.valve_vent_top = TwinDeviceItem("放气阀1", 250, 10, 60, 40, "valve") # Shifted to x=250
        self.scene.addItem(self.valve_vent_top)
        self.items_map["ventValve1Open"] = self.valve_vent_top
        self._draw_line(280, 50, 280, 70) # Connect into chamber
        
        # 3. Main Valve & Vent (Sides of Chamber)
        # Main Gate Valve (Right Side - Gate Valve 5)
        self.valve_main = TwinDeviceItem("闸板阀5", 820, 90, 60, 40, "valve")
        self.scene.addItem(self.valve_main)
        self.items_map["gateValve5Open"] = self.valve_main
        self._draw_line(800, 110, 820, 110) # Connect to chamber
        
        # 4. Molecular Pump Groups (Middle Row)
        # Group 1 (Left)
        self._add_mol_group(250, 250, "1")
        
        # Group 2 (Center)
        self._add_mol_group(500, 250, "2")
        
        # Group 3 (Right)
        self._add_mol_group(750, 250, "3")
        
        # 5. Foreline System (Bottom Row)
        # Common Foreline Pipe
        self._draw_line(250, 400, 750, 400) # Horizontal collector
        self._draw_line(250, 380, 250, 400) # From Mol 1
        self._draw_line(500, 380, 500, 400) # From Mol 2
        self._draw_line(750, 380, 750, 400) # From Mol 3
        
        # Gauge 3 (On Foreline - Right)
        self._add_gauge_node(625, 410, "真空计3", "vacuumGauge1")
        self._draw_line(640, 400, 640, 410) # Connect to pipe
        
        # Bottom Vent Valve (On Foreline - Left) - Moved ABOVE Foreline to avoid crossing Roughing Line
        self.valve_vent_bot = TwinDeviceItem("放气阀2", 345, 340, 60, 40, "valve") # Moved up to y=340
        self.scene.addItem(self.valve_vent_bot)
        self.items_map["ventValve2Open"] = self.valve_vent_bot
        self._draw_line(375, 380, 375, 400) # Connect down to pipe
        
        # Roughing Line (Bypass) - Separate Line from Far Left
        # Path: Chamber Left (150, 170) -> Down -> Valve 4 -> Down -> Right -> Roots Input
        
        self._draw_line(150, 170, 150, 430) # Vertical Down (Far Left)
        
        self.valve_rough = TwinDeviceItem("闸板阀4", 120, 280, 60, 40, "valve") # On vertical line
        self.scene.addItem(self.valve_rough)
        self.items_map["gateValve4Open"] = self.valve_rough
        
        # Horizontal to Roots (Below Mol Collector)
        self._draw_line(150, 430, 500, 430) 
        # Connect to Roots Feed (Roots Feed is 500, 400 -> 500, 450)
        # This joins the feed line at y=430
        
        # Pumps
        self._draw_line(500, 400, 500, 450) # Down to Roots (Main Feed)
        
        self.roots = TwinDeviceItem("罗茨泵", 460, 450, 80, 50)
        self.scene.addItem(self.roots)
        self.items_map["rootsPumpPower"] = self.roots
        
        self._draw_line(500, 500, 500, 530) # Roots to Screw
        
        self.screw = TwinDeviceItem("螺杆泵", 460, 530, 80, 50, has_water=True)
        self.scene.addItem(self.screw)
        self.items_map["screwPumpPower"] = self.screw
        
        # Tail Valve (Horizontal Layout)
        self._draw_line(500, 580, 500, 620) # Down from Screw
        self._draw_line(500, 620, 660, 620) # Right horizontal line
        
        self.valve_tail = TwinDeviceItem("电磁阀4", 550, 600, 60, 40, "valve") # On horizontal line
        self.scene.addItem(self.valve_tail)
        self.items_map["electromagneticValve4Open"] = self.valve_tail
        
        text_tail = self.scene.addText("尾气")
        text_tail.setPos(670, 610)
        text_tail.setDefaultTextColor(QColor("white"))

    def _add_gauge_node(self, x, y, name, attr_name):
        ellipse = QGraphicsEllipseItem(x, y, 30, 30)
        ellipse.setPen(QPen(QColor("#c3cede"), 2))
        ellipse.setBrush(QBrush(QColor("#0b1524")))
        self.scene.addItem(ellipse)
        
        text = self.scene.addText("P")
        text.setDefaultTextColor(QColor("#c3cede"))
        text.setFont(QFont("Arial", 12, QFont.Bold))
        text.setPos(x+8, y+4)
        
        # Value Display
        val_text = self.scene.addText("--- Pa")
        val_text.setDefaultTextColor(QColor("#90EE90"))
        val_text.setFont(QFont("Arial", 9))
        val_text.setPos(x-15, y-35)
        self.gauge_items[attr_name] = val_text
        
        lbl = self.scene.addText(name)
        lbl.setDefaultTextColor(QColor("#8fa6c5"))
        lbl.setPos(x-10, y-20)
        
        # Connection line
        self._draw_line(x+15, y+30, x+15, y+50)

    def _add_manual_valve(self, x, y, name):
        # Improved Manual Valve Symbol (Bowtie + Handle)
        
        # 1. Valve Body (Bowtie)
        # Center at x+15, y+30. Size 30x20
        p1 = QPointF(x, y+20)
        p2 = QPointF(x, y+40)
        p3 = QPointF(x+30, y+40)
        p4 = QPointF(x+30, y+20)
        center = QPointF(x+15, y+30)
        
        # Draw as two triangles to ensure the "bowtie" look
        # Left Triangle
        poly_left = QPolygonF([p1, p2, center])
        item_left = QGraphicsPolygonItem(poly_left)
        item_left.setPen(QPen(QColor("#8fa6c5"), 1))
        item_left.setBrush(QBrush(QColor("#1c3146")))
        self.scene.addItem(item_left)
        
        # Right Triangle
        poly_right = QPolygonF([p4, p3, center])
        item_right = QGraphicsPolygonItem(poly_right)
        item_right.setPen(QPen(QColor("#8fa6c5"), 1))
        item_right.setBrush(QBrush(QColor("#1c3146")))
        self.scene.addItem(item_right)
        
        # 2. Handle (T-shape)
        # Stem
        stem = QGraphicsLineItem(x+15, y+30, x+15, y+15)
        stem.setPen(QPen(QColor("#8fa6c5"), 1))
        self.scene.addItem(stem)
        
        # Handle Bar
        bar = QGraphicsLineItem(x+10, y+15, x+20, y+15)
        bar.setPen(QPen(QColor("#8fa6c5"), 2))
        self.scene.addItem(bar)
        
        lbl = self.scene.addText(name)
        lbl.setDefaultTextColor(QColor("gray"))
        lbl.setPos(x+35, y+15)
        
        # Status
        status = self.scene.addText("开")
        status.setDefaultTextColor(QColor("#39e072"))
        status.setPos(x+35, y+30)
        
        # Connection to chamber
        self._draw_line(x+15, y+40, x+15, y+60) # Into chamber

    def _add_mol_group(self, x, y, idx):
        # Gate Valve (Top)
        valve_gate = TwinDeviceItem(f"闸板阀{idx}", x-30, y-60, 60, 30, "valve")
        self.scene.addItem(valve_gate)
        self.items_map[f"gateValve{idx}Open"] = valve_gate
        
        # Connection from Chamber
        self._draw_line(x, 170, x, y-60)
        
        # Mol Pump (Middle)
        mol = TwinDeviceItem(f"分子泵{idx}", x-40, y, 80, 50, has_water=True)
        self.scene.addItem(mol)
        self.items_map[f"molecularPump{idx}Power"] = mol
        
        # Connection Gate -> Mol
        self._draw_line(x, y-30, x, y)
        
        # Solenoid Valve (Bottom)
        valve_mag = TwinDeviceItem(f"电磁阀{idx}", x-30, y+70, 60, 30, "valve")
        self.scene.addItem(valve_mag)
        self.items_map[f"electromagneticValve{idx}Open"] = valve_mag
        
        # Connection Mol -> Mag
        self._draw_line(x, y+50, x, y+70)
        
        # Connection Mag -> Foreline
        self._draw_line(x, y+100, x, y+130) # Down to collector

    def _draw_line(self, x1, y1, x2, y2):
        line = QGraphicsLineItem(x1, y1, x2, y2)
        line.setPen(QPen(QColor("#8fa6c5"), 2))
        self.scene.addItem(line)

    # def _add_water_label(self, x, y):
    #     lbl = self.scene.addText("水冷:正常")
    #     lbl.setDefaultTextColor(QColor("#39e072"))
    #     lbl.setPos(x, y)
    #     lbl.setScale(0.8)

class AlarmRecordTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        
        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(["时间", "报警内容", "报警设备"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        layout.addWidget(self.table)
        
        # 模拟数据生成 (验证功能)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._add_mock_alarm)
        self.timer.start(5000) # 每5秒生成一条
        
    def _add_mock_alarm(self):
        row = self.table.rowCount()
        self.table.insertRow(row)
        import datetime
        now = datetime.datetime.now().strftime("%H:%M:%S")
        self.table.setItem(row, 0, QTableWidgetItem(now))
        self.table.setItem(row, 1, QTableWidgetItem("模拟报警: 水压过低"))
        self.table.setItem(row, 2, QTableWidgetItem("螺杆泵"))

class HistoryRecordTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        
        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(["时间", "操作类型", "设备", "详情"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        layout.addWidget(self.table)
        
        # 模拟数据生成 (验证功能)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._add_mock_history)
        self.timer.start(8000)
        
    def _add_mock_history(self):
        row = self.table.rowCount()
        self.table.insertRow(row)
        import datetime
        now = datetime.datetime.now().strftime("%H:%M:%S")
        self.table.setItem(row, 0, QTableWidgetItem(now))
        self.table.setItem(row, 1, QTableWidgetItem("自动操作"))
        self.table.setItem(row, 2, QTableWidgetItem("系统"))
        self.table.setItem(row, 3, QTableWidgetItem("趋势数据已记录")) # 确认趋势数据关联

class TrendCurveTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        
        if HAS_PYQTGRAPH:
            # 配置 PyQtGraph 样式
            pg.setConfigOption('background', '#0b1524')
            pg.setConfigOption('foreground', '#c3cede')
            
            self.plot_widget = pg.PlotWidget()
            self.plot_widget.setTitle("真空度趋势", color='#c3cede', size='12pt')
            self.plot_widget.setLabel('left', "压力 (Pa)")
            self.plot_widget.setLabel('bottom', "时间 (s)")
            self.plot_widget.showGrid(x=True, y=True, alpha=0.3)
            
            # 模拟数据
            self.curve1 = self.plot_widget.plot(pen=pg.mkPen('#29d6ff', width=2), name="前级规")
            self.curve2 = self.plot_widget.plot(pen=pg.mkPen('#39e072', width=2), name="主规1")
            
            layout.addWidget(self.plot_widget)
            
            # 模拟更新
            self.data_x = list(range(100))
            self.data_y1 = [1000 * (0.95 ** x) for x in self.data_x] # Decay
            self.data_y2 = [1e-5 for _ in self.data_x]
            
            self.timer = QTimer(self)
            self.timer.timeout.connect(self.update_plot)
            self.timer.start(1000)
            
        else:
            lbl = QLabel("未检测到 pyqtgraph 库，无法显示趋势曲线。\n请安装: pip install pyqtgraph")
            lbl.setAlignment(Qt.AlignCenter)
            layout.addWidget(lbl)
            
    def update_plot(self):
        if not HAS_PYQTGRAPH:
            return
        # 简单模拟滚动
        self.data_y1 = self.data_y1[1:] + [self.data_y1[-1] * 0.99]
        self.curve1.setData(self.data_x, self.data_y1)

