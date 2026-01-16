"""
真空系统 GUI 主页面

严格分区:
- 自动操作区
- 手动操作区
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QPushButton,
    QLabel, QGridLayout, QFrame, QTabWidget, QMessageBox, QSplitter,
    QCheckBox
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QObject, QEvent
from PyQt5.QtGui import QFont

from config import (
    OperationMode, SystemState, ValveActionState,
    PUMP_DEVICES, GATE_VALVES, ELECTROMAGNETIC_VALVES, VENT_VALVES,
    SENSORS, UI_CONFIG, OPERATION_PREREQUISITES, AUTO_BRANCH_THRESHOLDS,
    AUTO_MOLECULAR_PUMP_CONFIG, WATER_CHANNELS, AIR_SUPPLY,
    AUTO_SEQUENCE_STEPS
)
from styles import MAIN_STYLE, MODE_AREA_STYLES, COLORS
from widgets import StatusIndicator, ValueDisplay, PrerequisitePanel, get_unicode_font
from tango_worker import VacuumTangoWorker, MockTangoWorker
from utils.prerequisite_checker import PrerequisiteChecker
from utils.logger import get_logger

logger = get_logger(__name__)


class VacuumSystemMainPage(QWidget):
    """真空系统主控制页面"""
    
    def __init__(self, worker: VacuumTangoWorker, parent=None):
        super().__init__(parent)
        
        self._worker = worker
        self._tango_connected = False
        self._plc_connected = False
        
        self._current_mode = OperationMode.MANUAL
        self._last_selected_operation = None  # 上次选择的操作
        
        self._init_ui()
        self._connect_signals()
        
        # 初始同步连接状态
        self._on_connection_changed(self._worker.device is not None)
        self._on_plc_connection_changed(self._worker._last_plc_connected)
        
        self._start_polling()
        
    def _init_ui(self):
        """初始化 UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 10, 16, 10)
        layout.setSpacing(10)
        
        # 1. 顶部状态栏
        layout.addWidget(self._create_status_bar())
        
        # 2. 自动流程进度 Banner (新位置：横跨全屏)
        layout.addWidget(self._create_progress_banner())
        
        # 3. 主内容区 - 使用 Splitter (三栏布局)
        splitter = QSplitter(Qt.Horizontal)
        # 设置分隔条样式，避免打包后显示粗线
        splitter.setStyleSheet("""
            QSplitter::handle {
                background-color: transparent;
                width: 2px;
            }
            QSplitter::handle:horizontal {
                width: 2px;
            }
        """)
        splitter.setHandleWidth(2)
        
        # 左侧: 自动操作区 (变窄)
        splitter.addWidget(self._create_auto_area())
        
        # 中间: 手动操作区 (变窄)
        splitter.addWidget(self._create_manual_area())
        
        # 右侧: 操作先决条件面板
        self._prereq_panel = PrerequisitePanel("操作先决条件")
        # 设置最小宽度，留足一列提示信息的宽度即可
        self._prereq_panel.setMinimumWidth(280)
        splitter.addWidget(self._prereq_panel)
        
        # 设置各栏宽度比例：自动操作区 300，手动操作区 450，先决条件面板 280
        # 先决条件面板变窄，操作区有更多空间
        splitter.setSizes([300, 450, 280])
        layout.addWidget(splitter, 1)

    def _create_progress_banner(self) -> QWidget:
        """创建横向流程进度 Banner - 压缩为单行"""
        frame = QFrame()
        frame.setStyleSheet(f"""
            QFrame {{
                background-color: rgba(0, 229, 255, 0.05);
                border: 1px solid {COLORS['border']};
                border-radius: 4px;
            }}
        """)
        frame.setFixedHeight(35)
        
        layout = QHBoxLayout(frame)
        layout.setContentsMargins(15, 0, 15, 0)
        layout.setSpacing(20)
        
        # 1. 当前步骤
        step_layout = QHBoxLayout()
        step_layout.setSpacing(8)
        lbl_step = QLabel("当前自动步骤:")
        lbl_step.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 12px;")
        self._auto_progress_label = QLabel("空闲")
        self._auto_progress_label.setStyleSheet(f"color: {COLORS['primary']}; font-size: 13px; font-weight: bold;")
        step_layout.addWidget(lbl_step)
        step_layout.addWidget(self._auto_progress_label)
        layout.addLayout(step_layout)
        
        layout.addWidget(self._create_separator())
        
        # 2. 流程状态
        status_layout = QHBoxLayout()
        status_layout.setSpacing(8)
        lbl_status = QLabel("流程状态:")
        lbl_status.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 12px;")
        self._auto_status_label = QLabel("系统处于手动模式")
        self._auto_status_label.setStyleSheet(f"color: #ECEFF1; font-size: 12px;")
        status_layout.addWidget(lbl_status)
        status_layout.addWidget(self._auto_status_label)
        layout.addLayout(status_layout)
        
        layout.addWidget(self._create_separator())
        
        # 3. 动态信息 (分支/互锁)
        info_layout = QHBoxLayout()
        info_layout.setSpacing(15)
        self._branch_label = QLabel("")
        self._branch_label.setStyleSheet(f"color: #00E5FF; font-size: 12px;")
        self._interlock_label = QLabel("")
        self._interlock_label.setStyleSheet(f"color: {COLORS['warning']}; font-size: 12px; font-weight: bold;")
        info_layout.addWidget(self._branch_label)
        info_layout.addWidget(self._interlock_label)
        layout.addLayout(info_layout, 1)
        
        return frame
        
    def _create_status_bar(self) -> QWidget:
        """创建顶部状态栏 - 逻辑分组化"""
        frame = QFrame()
        frame.setStyleSheet(f"""
            QFrame {{
                background-color: {COLORS['surface']};
                border-bottom: 2px solid {COLORS['border']};
                padding: 5px;
            }}
        """)
        
        main_layout = QHBoxLayout(frame)
        main_layout.setContentsMargins(15, 5, 15, 5)
        
        # --- 组1: 系统概览 ---
        sys_group = QHBoxLayout()
        sys_group.setSpacing(15)
        
        self._conn_indicator = StatusIndicator("连接")
        sys_group.addWidget(self._conn_indicator)
        
        mode_box = QHBoxLayout()
        mode_box.addWidget(QLabel("模式:"))
        self._mode_label = QLabel("手动")
        self._mode_label.setStyleSheet(f"color: {COLORS['warning']}; font-weight: bold; font-size: 14px;")
        mode_box.addWidget(self._mode_label)
        sys_group.addLayout(mode_box)
        
        state_box = QHBoxLayout()
        state_box.addWidget(QLabel("状态:"))
        self._state_label = QLabel("空闲")
        self._state_label.setStyleSheet(f"color: {COLORS['success']}; font-weight: bold; font-size: 14px;")
        state_box.addWidget(self._state_label)
        sys_group.addLayout(state_box)
        
        main_layout.addLayout(sys_group)
        main_layout.addWidget(self._create_separator())
        
        # --- 组2: 实时参数 ---
        param_group = QHBoxLayout()
        param_group.setSpacing(12)
        
        self._vacuum1_display = ValueDisplay("前级真空计", "Pa")
        self._vacuum2_display = ValueDisplay("真空计1", "Pa")
        self._vacuum3_display = ValueDisplay("真空计2", "Pa")
        self._air_pressure_display = ValueDisplay("气源", "MPa")
        
        param_group.addWidget(self._vacuum1_display)
        param_group.addWidget(self._vacuum2_display)
        param_group.addWidget(self._vacuum3_display)
        param_group.addWidget(self._air_pressure_display)
        
        main_layout.addLayout(param_group)
        main_layout.addWidget(self._create_separator())
        
        # --- 组3: 辅助设施 ---
        aux_group = QHBoxLayout()
        aux_group.setSpacing(12)
        
        # 水路
        water_box = QHBoxLayout()
        water_box.addWidget(QLabel("水路:"))
        self._water_indicators = []
        for i in range(1, 5):
            ind = StatusIndicator(str(i))
            # 增加尺寸确保文字不被遮挡
            ind.setMinimumWidth(45)
            self._water_indicators.append(ind)
            water_box.addWidget(ind)
        aux_group.addLayout(water_box)
        
        # 气路 (物理分开)
        aux_group.addWidget(self._create_separator())
        
        air_box = QHBoxLayout()
        air_box.addWidget(QLabel("气路:"))
        self._air_indicator = StatusIndicator("")
        air_box.addWidget(self._air_indicator)
        aux_group.addLayout(air_box)
        
        main_layout.addLayout(aux_group)
        
        main_layout.addStretch()
        
        # --- 组4: 报警 ---
        self._alarm_indicator = StatusIndicator("报警")
        main_layout.addWidget(self._alarm_indicator)
        
        return frame
        
    def _create_auto_area(self) -> QWidget:
        """创建自动操作区"""
        group = QGroupBox("自动操作区")
        group.setStyleSheet(MODE_AREA_STYLES['auto_area'])
        
        layout = QVBoxLayout(group)
        layout.setSpacing(16)
        
        # 模式切换
        mode_frame = QFrame()
        mode_layout = QHBoxLayout(mode_frame)
        mode_layout.setContentsMargins(0, 0, 0, 0)
        
        self._btn_switch_auto = QPushButton("切换至自动模式")
        self._btn_switch_auto.setProperty("class", "primary")
        self._btn_switch_auto.clicked.connect(self._on_switch_to_auto)
        mode_layout.addWidget(self._btn_switch_auto)
        
        self._btn_switch_manual = QPushButton("切换至手动模式")
        self._btn_switch_manual.clicked.connect(self._on_switch_to_manual)
        mode_layout.addWidget(self._btn_switch_manual)
        
        layout.addWidget(mode_frame)
        
        # 分隔线
        layout.addWidget(self._create_h_separator())
        
        # 一键操作按钮
        ops_layout = QGridLayout()
        ops_layout.setSpacing(12)
        
        self._btn_vacuum_start = QPushButton("一键抽真空")
        self._btn_vacuum_start.setMinimumHeight(60)
        self._btn_vacuum_start.setProperty("class", "success")
        self._btn_vacuum_start.setEnabled(False)
        self._btn_vacuum_start.setProperty("operation", "Auto_OneKeyVacuumStart")
        self._btn_vacuum_start.installEventFilter(self)
        self._btn_vacuum_start.clicked.connect(self._on_vacuum_start)
        ops_layout.addWidget(self._btn_vacuum_start, 0, 0)
        
        self._btn_vacuum_stop = QPushButton("一键停机")
        self._btn_vacuum_stop.setMinimumHeight(60)
        self._btn_vacuum_stop.setProperty("class", "warning")
        self._btn_vacuum_stop.setEnabled(False)
        self._btn_vacuum_stop.setProperty("operation", "Auto_OneKeyVacuumStop")
        self._btn_vacuum_stop.installEventFilter(self)
        self._btn_vacuum_stop.clicked.connect(self._on_vacuum_stop)
        ops_layout.addWidget(self._btn_vacuum_stop, 0, 1)
        
        self._btn_chamber_vent = QPushButton("腔室放气")
        self._btn_chamber_vent.setMinimumHeight(60)
        self._btn_chamber_vent.setEnabled(False)
        self._btn_chamber_vent.setProperty("operation", "Auto_ChamberVent")
        self._btn_chamber_vent.installEventFilter(self)
        self._btn_chamber_vent.clicked.connect(self._on_chamber_vent)
        ops_layout.addWidget(self._btn_chamber_vent, 1, 0)
        
        self._btn_fault_reset = QPushButton("故障复位")
        self._btn_fault_reset.setMinimumHeight(60)
        self._btn_fault_reset.setProperty("class", "danger")
        self._btn_fault_reset.setEnabled(False)
        self._btn_fault_reset.setProperty("operation", "Auto_FaultReset")
        self._btn_fault_reset.installEventFilter(self)
        self._btn_fault_reset.clicked.connect(self._on_fault_reset)
        ops_layout.addWidget(self._btn_fault_reset, 1, 1)
        
        layout.addLayout(ops_layout)
        
        # 分隔线
        layout.addWidget(self._create_h_separator())
        
        # 紧急停止按钮
        self._btn_emergency = QPushButton("⚠ 紧急停止")
        # 设置支持 Unicode 符号的字体，确保图标正确显示
        from widgets import get_unicode_font
        self._btn_emergency.setFont(get_unicode_font(14))
        self._btn_emergency.setMinimumHeight(70)
        self._btn_emergency.setStyleSheet(f"""
            QPushButton {{
                background-color: #D32F2F;
                color: white;
                border: none;
                border-radius: 6px;
                font-size: 16px;
                font-weight: bold;
            }}
            QPushButton:hover {{
                background-color: #B71C1C;
            }}
            QPushButton:pressed {{
                background-color: #7F0000;
            }}
        """)
        self._btn_emergency.clicked.connect(self._on_emergency_stop)
        layout.addWidget(self._btn_emergency)
        
        # P1-2: 分子泵配置区
        layout.addWidget(self._create_h_separator())
        mp_config_label = QLabel("自动模式 - 分子泵启用配置")
        mp_config_label.setProperty("class", "section-title")
        layout.addWidget(mp_config_label)
        
        mp_config_frame = QFrame()
        mp_config_frame.setStyleSheet(f"""
            QFrame {{
                background-color: {COLORS['surface_light']};
                border: 1px solid {COLORS['border']};
                border-radius: 4px;
                padding: 6px;
            }}
        """)
        mp_config_layout = QHBoxLayout(mp_config_frame)
        mp_config_layout.setContentsMargins(8, 4, 8, 4)
        mp_config_layout.setSpacing(16)
        
        self._mp_checkboxes = []
        for i in range(1, 4):
            cb = QCheckBox(f"分子泵{i}")
            cb.setChecked(AUTO_MOLECULAR_PUMP_CONFIG.get(f'molecularPump{i}Enabled', True))
            cb.stateChanged.connect(lambda state, idx=i: self._on_mp_config_changed(idx, state))
            cb.setToolTip(f"勾选后分子泵{i}将参与自动抽真空流程")
            cb.setCursor(Qt.PointingHandCursor)
            self._mp_checkboxes.append(cb)
            mp_config_layout.addWidget(cb)
        
        mp_config_layout.addStretch()
        layout.addWidget(mp_config_frame)
        
        layout.addStretch()
        
        return group
        
    def _create_manual_area(self) -> QWidget:
        """创建手动操作区"""
        group = QGroupBox("手动操作区")
        group.setStyleSheet(MODE_AREA_STYLES['manual_area'])
        
        layout = QVBoxLayout(group)
        layout.setSpacing(12)
        
        # 使用选项卡组织不同设备
        self._manual_tabs = QTabWidget()
        self._manual_tabs.currentChanged.connect(self._on_tab_changed)
        
        # 泵控制
        self._manual_tabs.addTab(self._create_pump_control_tab(), "泵控制")
        
        # 闸板阀控制
        self._manual_tabs.addTab(self._create_gate_valve_tab(), "闸板阀")
        
        # 电磁阀控制
        self._manual_tabs.addTab(self._create_electromagnetic_valve_tab(), "电磁阀")
        
        # 放气阀控制
        self._manual_tabs.addTab(self._create_vent_valve_tab(), "放气阀")
        
        layout.addWidget(self._manual_tabs, 1)
        
        return group
        
    def _create_pump_control_tab(self) -> QWidget:
        """创建泵控制选项卡"""
        widget = QWidget()
        layout = QGridLayout(widget)
        layout.setSpacing(12)
        
        row = 0
        
        # 螺杆泵
        layout.addWidget(QLabel("螺杆泵"), row, 0)
        self._screw_pump_indicator = StatusIndicator("")
        layout.addWidget(self._screw_pump_indicator, row, 1)
        
        btn_screw_on = QPushButton("上电")
        btn_screw_on.setProperty("class", "success")
        btn_screw_on.clicked.connect(lambda: self._set_pump("ScrewPump", True))
        btn_screw_on.setProperty("operation", "ScrewPump_Start")
        btn_screw_on.installEventFilter(self)
        layout.addWidget(btn_screw_on, row, 2)
        
        btn_screw_off = QPushButton("断电")
        btn_screw_off.clicked.connect(lambda: self._set_pump("ScrewPump", False))
        btn_screw_off.setProperty("operation", "ScrewPump_Stop")
        btn_screw_off.installEventFilter(self)
        layout.addWidget(btn_screw_off, row, 3)
        
        row += 1
        
        # 罗茨泵
        layout.addWidget(QLabel("罗茨泵"), row, 0)
        self._roots_pump_indicator = StatusIndicator("")
        layout.addWidget(self._roots_pump_indicator, row, 1)
        
        btn_roots_on = QPushButton("上电")
        btn_roots_on.setProperty("class", "success")
        btn_roots_on.clicked.connect(lambda: self._set_pump("RootsPump", True))
        btn_roots_on.setProperty("operation", "RootsPump_Start")
        btn_roots_on.installEventFilter(self)
        layout.addWidget(btn_roots_on, row, 2)
        
        btn_roots_off = QPushButton("断电")
        btn_roots_off.clicked.connect(lambda: self._set_pump("RootsPump", False))
        btn_roots_off.setProperty("operation", "RootsPump_Stop")
        btn_roots_off.installEventFilter(self)
        layout.addWidget(btn_roots_off, row, 3)
        
        row += 1
        
        # 分子泵 1-3
        self._molecular_pump_indicators = []
        self._molecular_pump_speed_labels = []
        
        for i in range(1, 4):
            layout.addWidget(QLabel(f"分子泵{i}"), row, 0)
            
            indicator = StatusIndicator("")
            self._molecular_pump_indicators.append(indicator)
            layout.addWidget(indicator, row, 1)
            
            speed_label = QLabel("0 Hz")
            self._molecular_pump_speed_labels.append(speed_label)
            layout.addWidget(speed_label, row, 2)
            
            btn_on = QPushButton("启动")
            btn_on.setProperty("class", "success")
            btn_on.clicked.connect(lambda checked, idx=i: self._set_molecular_pump(idx, True))
            btn_on.setProperty("operation", f"MolecularPump{i}_Start")  # 使用单独条件，检查对应的电磁阀和闸板阀
            btn_on.installEventFilter(self)
            layout.addWidget(btn_on, row, 3)
            
            btn_off = QPushButton("停止")
            btn_off.clicked.connect(lambda checked, idx=i: self._set_molecular_pump(idx, False))
            btn_off.setProperty("operation", "MolecularPump_Stop")
            btn_off.installEventFilter(self)
            layout.addWidget(btn_off, row, 4)
            
            row += 1
            
        layout.setRowStretch(row, 1)
        return widget
        
    def _create_gate_valve_tab(self) -> QWidget:
        """创建闸板阀控制选项卡"""
        widget = QWidget()
        layout = QGridLayout(widget)
        layout.setSpacing(12)
        
        self._gate_valve_indicators = []
        self._gate_valve_status_labels = []
        
        for i, (key, config) in enumerate(GATE_VALVES.items()):
            layout.addWidget(QLabel(config['name']), i, 0)
            
            indicator = StatusIndicator("")
            self._gate_valve_indicators.append(indicator)
            layout.addWidget(indicator, i, 1)
            
            status_label = QLabel("关闭")
            status_label.setStyleSheet(f"color: {COLORS['text_secondary']}; min-width: 60px;")
            self._gate_valve_status_labels.append(status_label)
            layout.addWidget(status_label, i, 2)
            
            # 确定操作对应的先决条件
            # 注意：闸板阀1-3开启需要对应的电磁阀1-3分别开启，关闭操作使用"对应的"设备编号（如闸板阀1关闭只需分子泵1停止）
            if config['index'] <= 3:
                open_prereq_key = f"GateValve{config['index']}_Open"  # 使用单独条件，检查对应的电磁阀
                close_prereq_key = f"GateValve{config['index']}_Close"  # 单独的关闭条件
            elif config['index'] == 4:
                open_prereq_key = "GateValve4_Open"
                close_prereq_key = "GateValve4_Close"
            else:
                open_prereq_key = "GateValve5_Open"
                close_prereq_key = "GateValve5_Close"
            
            btn_open = QPushButton("开启")
            btn_open.setProperty("class", "success")
            btn_open.clicked.connect(lambda checked, idx=config['index']: self._set_gate_valve(idx, True))
            btn_open.setProperty("operation", open_prereq_key)
            btn_open.installEventFilter(self)
            layout.addWidget(btn_open, i, 3)
            
            btn_close = QPushButton("关闭")
            btn_close.clicked.connect(lambda checked, idx=config['index']: self._set_gate_valve(idx, False))
            btn_close.setProperty("operation", close_prereq_key)
            btn_close.installEventFilter(self)
            layout.addWidget(btn_close, i, 4)
            
        layout.setRowStretch(len(GATE_VALVES), 1)
        return widget
        
    def _create_electromagnetic_valve_tab(self) -> QWidget:
        """创建电磁阀控制选项卡"""
        widget = QWidget()
        layout = QGridLayout(widget)
        layout.setSpacing(12)
        
        self._electromagnetic_valve_indicators = []
        
        for i, (key, config) in enumerate(ELECTROMAGNETIC_VALVES.items()):
            layout.addWidget(QLabel(config['name']), i, 0)
            
            indicator = StatusIndicator("")
            self._electromagnetic_valve_indicators.append(indicator)
            layout.addWidget(indicator, i, 1)
            
            # 确定操作对应的先决条件
            # 注意：关闭操作使用"对应的"设备编号（如电磁阀1关闭只需分子泵1停止+闸板阀1关闭）
            if config['index'] <= 3:
                open_prereq_key = "ElectromagneticValve123_Open"
                close_prereq_key = f"ElectromagneticValve{config['index']}_Close"  # 单独的关闭条件
            else:
                open_prereq_key = "ElectromagneticValve4_Open"
                close_prereq_key = "ElectromagneticValve4_Close"
            
            btn_open = QPushButton("开启")
            btn_open.setProperty("class", "success")
            btn_open.clicked.connect(lambda checked, idx=config['index']: self._set_electromagnetic_valve(idx, True))
            btn_open.setProperty("operation", open_prereq_key)
            btn_open.installEventFilter(self)
            layout.addWidget(btn_open, i, 2)
            
            btn_close = QPushButton("关闭")
            btn_close.clicked.connect(lambda checked, idx=config['index']: self._set_electromagnetic_valve(idx, False))
            btn_close.setProperty("operation", close_prereq_key)
            btn_close.installEventFilter(self)
            layout.addWidget(btn_close, i, 3)
            
        layout.setRowStretch(len(ELECTROMAGNETIC_VALVES), 1)
        return widget
        
    def _create_vent_valve_tab(self) -> QWidget:
        """创建放气阀控制选项卡"""
        widget = QWidget()
        layout = QGridLayout(widget)
        layout.setSpacing(12)
        
        self._vent_valve_indicators = []
        
        for i, (key, config) in enumerate(VENT_VALVES.items()):
            layout.addWidget(QLabel(config['name']), i, 0)
            
            indicator = StatusIndicator("")
            self._vent_valve_indicators.append(indicator)
            layout.addWidget(indicator, i, 1)
            
            open_prereq_key = f"VentValve{config['index']}_Open"
            close_prereq_key = f"VentValve{config['index']}_Close"
            
            btn_open = QPushButton("开启")
            btn_open.setProperty("class", "success")
            btn_open.clicked.connect(lambda checked, idx=config['index']: self._set_vent_valve(idx, True))
            btn_open.setProperty("operation", open_prereq_key)
            btn_open.installEventFilter(self)
            layout.addWidget(btn_open, i, 2)
            
            btn_close = QPushButton("关闭")
            btn_close.clicked.connect(lambda checked, idx=config['index']: self._set_vent_valve(idx, False))
            btn_close.setProperty("operation", close_prereq_key)
            btn_close.installEventFilter(self)
            layout.addWidget(btn_close, i, 3)
            
        layout.setRowStretch(len(VENT_VALVES), 1)
        return widget
        
    def _create_separator(self) -> QFrame:
        """创建垂直分隔线"""
        sep = QFrame()
        sep.setFrameShape(QFrame.VLine)
        sep.setStyleSheet(f"background-color: {COLORS['divider']};")
        return sep
        
    def _create_h_separator(self) -> QFrame:
        """创建水平分隔线"""
        sep = QFrame()
        sep.setFrameShape(QFrame.HLine)
        sep.setStyleSheet(f"background-color: {COLORS['divider']};")
        sep.setMaximumHeight(1)
        return sep
        
    def _connect_signals(self):
        """连接信号"""
        self._worker.connection_changed.connect(self._on_connection_changed)
        self._worker.plc_connection_changed.connect(self._on_plc_connection_changed)
        self._worker.command_result.connect(self._on_command_result)
        
    def _start_polling(self):
        """启动状态轮询"""
        self._poll_timer = QTimer(self)
        self._poll_timer.timeout.connect(self._pull_status)
        self._poll_timer.start(UI_CONFIG['poll_interval_ms'])
        
        # 闪烁定时器
        self._blink_timer = QTimer(self)
        self._blink_timer.timeout.connect(self._toggle_blinks)
        self._blink_timer.start(UI_CONFIG['blink_interval_ms'])
        
    def _toggle_blinks(self):
        """切换闪烁状态"""
        for indicator in self._gate_valve_indicators:
            indicator.toggle_blink()
            
    def _pull_status(self):
        """拉取状态并更新 UI"""
        status = self._worker.get_cached_status()
        if not status:
            return
            
        # 更新模式和状态
        mode = status.get('operationMode', 1)
        self._current_mode = mode
        mode_text = "自动" if mode == OperationMode.AUTO else "手动"
        self._mode_label.setText(mode_text)
        self._mode_label.setStyleSheet(
            f"color: {COLORS['primary'] if mode == 0 else COLORS['warning']}; font-weight: bold; font-size: 14px;"
        )
        
        state = status.get('systemState', 0)
        self._state_label.setText(SystemState.to_string(state))
        
        # 根据状态设置颜色
        state_colors = {
            SystemState.IDLE: COLORS['success'],      # 空闲 - 绿色
            SystemState.PUMPING: "#00E5FF",           # 抽真空 - 青色
            SystemState.STOPPING: COLORS['warning'],  # 停机中 - 黄色
            SystemState.VENTING: "#2196F3",           # 放气中 - 蓝色
            SystemState.FAULT: COLORS['error'],       # 故障 - 红色
            SystemState.EMERGENCY_STOP: COLORS['error'],  # 急停 - 红色
        }
        state_color = state_colors.get(state, "#888888")
        self._state_label.setStyleSheet(f"color: {state_color}; font-weight: bold; font-size: 14px;")
        
        # 更新自动流程进度
        auto_step = status.get('autoSequenceStep', 0)
        
        # 使用多级字典获取步骤描述，区分 PUMPING/STOPPING/VENTING
        step_text = "空闲"
        if state in AUTO_SEQUENCE_STEPS:
            state_steps = AUTO_SEQUENCE_STEPS[state]
            step_text = state_steps.get(auto_step, f"步骤 {auto_step}")
        elif auto_step > 0:
            step_text = f"步骤 {auto_step}"
            
        self._auto_progress_label.setText(step_text)
        
        if mode == OperationMode.AUTO:
            if state == SystemState.IDLE:
                self._auto_status_label.setText("自动模式就绪，可启动抽真空")
                self._auto_status_label.setStyleSheet(f"color: {COLORS['success']};")
            elif state == SystemState.PUMPING:
                self._auto_status_label.setText("抽真空进行中...")
                self._auto_status_label.setStyleSheet(f"color: {COLORS['primary']};")
            elif state == SystemState.STOPPING:
                self._auto_status_label.setText("停机进行中...")
                self._auto_status_label.setStyleSheet(f"color: {COLORS['warning']};")
            elif state == SystemState.VENTING:
                self._auto_status_label.setText("放气进行中...")
                self._auto_status_label.setStyleSheet(f"color: #2196F3;")
        else:
            self._auto_status_label.setText("系统处于手动模式")
            self._auto_status_label.setStyleSheet(f"color: {COLORS['text_secondary']};")
        
        # P2-1: 更新自动抽真空分支判据显示
        self._update_branch_display(status, state)
        
        # 更新按钮状态
        is_auto = (mode == OperationMode.AUTO)
        self._btn_switch_auto.setEnabled(not is_auto)
        self._btn_switch_manual.setEnabled(is_auto)
        self._btn_vacuum_start.setEnabled(is_auto and state == SystemState.IDLE)
        
        # 检查是否有任何设备正在运行 (用于手动模式下一键停机的启用)
        any_pump_running = any([
            status.get('screwPumpPower', False),
            status.get('rootsPumpPower', False),
            status.get('molecularPump1Power', False),
            status.get('molecularPump2Power', False),
            status.get('molecularPump3Power', False)
        ])
        
        # 一键停机：系统非空闲，或有任何泵正在运行时，均允许执行
        self._btn_vacuum_stop.setEnabled(state != SystemState.IDLE or any_pump_running)
        
        # 腔室放气：仅允许在系统空闲时执行；抽真空过程中互锁屏蔽
        vent_allowed = state == SystemState.IDLE
        self._btn_chamber_vent.setEnabled(vent_allowed)
        
        # 更新互锁提示
        interlock_msgs = []
        if state == SystemState.PUMPING:
            interlock_msgs.append("⚠ 腔室放气已互锁：抽真空进行中")
            self._btn_chamber_vent.setToolTip("抽真空进行中：腔室放气已互锁屏蔽")
            self._btn_chamber_vent.setStyleSheet(f"""
                QPushButton {{
                    background-color: #555;
                    color: #888;
                    border: 2px dashed {COLORS['warning']};
                }}
            """)
        elif state == SystemState.STOPPING:
            interlock_msgs.append("⚠ 腔室放气已互锁：停机进行中")
            self._btn_chamber_vent.setToolTip("停机进行中：腔室放气已互锁屏蔽")
        else:
            self._btn_chamber_vent.setToolTip("")
            self._btn_chamber_vent.setStyleSheet("")  # 恢复默认样式
        
        # 显示互锁提示
        self._interlock_label.setText(" | ".join(interlock_msgs))
        
        # 故障复位：在故障/急停状态下启用，或者有活跃报警时也启用
        has_active_alarm = status.get('activeAlarmCount', 0) > 0 or status.get('hasUnacknowledgedAlarm', False)
        fault_reset_enabled = state in [SystemState.FAULT, SystemState.EMERGENCY_STOP] or has_active_alarm
        self._btn_fault_reset.setEnabled(fault_reset_enabled)
        
        # 更新传感器
        self._vacuum1_display.set_value(status.get('vacuumGauge1', 0))
        self._vacuum2_display.set_value(status.get('vacuumGauge2', 0))
        self._vacuum3_display.set_value(status.get('vacuumGauge3', 0))
        self._air_pressure_display.set_value(status.get('airPressure', 0))
        
        # 更新4路水路状态
        for i in range(4):
            water_flow_ok = status.get(f'waterFlow{i+1}', False)
            self._water_indicators[i].set_state(water_flow_ok, is_alarm=not water_flow_ok)
            if water_flow_ok:
                self._water_indicators[i].setToolTip(f"冷却水路{i+1}: 正常")
            else:
                self._water_indicators[i].setToolTip(f"冷却水路{i+1}: 无水流!")
        
        # 更新气路状态
        air_ok = status.get('airSupplyOk', False)
        air_pressure = status.get('airPressure', 0)
        self._air_indicator.set_state(air_ok, is_alarm=not air_ok)
        if air_ok:
            self._air_indicator.setToolTip(f"气源压力正常: {air_pressure:.2f} MPa")
        else:
            self._air_indicator.setToolTip(f"气源压力不足: {air_pressure:.2f} MPa (需≥0.4 MPa)")
        
        # 更新泵状态
        self._screw_pump_indicator.set_state(status.get('screwPumpPower', False))
        self._roots_pump_indicator.set_state(status.get('rootsPumpPower', False))
        
        for i in range(3):
            self._molecular_pump_indicators[i].set_state(
                status.get(f'molecularPump{i+1}Power', False)
            )
            speed = status.get(f'molecularPump{i+1}Speed', 0)
            self._molecular_pump_speed_labels[i].setText(f"{speed} Hz")
            
        # 更新闸板阀状态 (包括闪烁效果)
        for i in range(5):
            is_open = status.get(f'gateValve{i+1}Open', False)
            is_close = status.get(f'gateValve{i+1}Close', False)
            action_state = status.get(f'gateValve{i+1}ActionState', 0)
            
            indicator = self._gate_valve_indicators[i]
            status_label = self._gate_valve_status_labels[i]
            
            # 根据动作状态设置闪烁和文字
            if action_state == ValveActionState.OPENING:
                indicator.start_blink("normal")
                status_label.setText("正在开启")
                status_label.setStyleSheet(f"color: #2196F3; min-width: 60px;")
            elif action_state == ValveActionState.CLOSING:
                indicator.start_blink("normal")
                status_label.setText("正在关闭")
                status_label.setStyleSheet(f"color: #2196F3; min-width: 60px;")
            elif action_state == ValveActionState.OPEN_TIMEOUT:
                indicator.start_blink("timeout")
                status_label.setText("开超时!")
                status_label.setStyleSheet(f"color: #F44336; min-width: 60px; font-weight: bold;")
            elif action_state == ValveActionState.CLOSE_TIMEOUT:
                indicator.start_blink("timeout")
                status_label.setText("关超时!")
                status_label.setStyleSheet(f"color: #F44336; min-width: 60px; font-weight: bold;")
            else:
                indicator.stop_blink()
                indicator.set_state(is_open)
                if is_open:
                    status_label.setText("已开启")
                    status_label.setStyleSheet(f"color: #4CAF50; min-width: 60px;")
                else:
                    status_label.setText("已关闭")
                    status_label.setStyleSheet(f"color: {COLORS['text_secondary']}; min-width: 60px;")
            
        for i in range(4):
            is_open = status.get(f'electromagneticValve{i+1}Open', False)
            self._electromagnetic_valve_indicators[i].set_state(is_open)
            
        for i in range(2):
            is_open = status.get(f'ventValve{i+1}Open', False)
            self._vent_valve_indicators[i].set_state(is_open)
            
        # 更新报警指示
        has_alarm = status.get('hasUnacknowledgedAlarm', False)
        self._alarm_indicator.set_state(has_alarm, is_alarm=True)
        
        # 更新分子泵启用配置复选框状态（从设备端同步）
        for i, cb in enumerate(self._mp_checkboxes, 1):
            enabled = status.get(f'molecularPump{i}Enabled', AUTO_MOLECULAR_PUMP_CONFIG.get(f'molecularPump{i}Enabled', True))
            # 临时断开信号，避免触发写入
            cb.blockSignals(True)
            cb.setChecked(enabled)
            cb.blockSignals(False)
        
        # 更新先决条件面板
        self._update_prerequisites(status)
        
    def _update_prerequisites(self, status: dict):
        """根据当前状态更新先决条件面板"""
        if not hasattr(self, "_prereq_panel"):
            return

        if not self._last_selected_operation:
            self._prereq_panel.clear_prerequisites()
            return
        
        # 获取操作的友好名称
        operation_names = {
            "Auto_OneKeyVacuumStart": "一键抽真空",
            "Auto_OneKeyVacuumStop": "一键停机",
            "Auto_ChamberVent": "腔室放气",
            "Auto_FaultReset": "故障复位",
            "ScrewPump_Start": "螺杆泵启动",
            "ScrewPump_Stop": "螺杆泵停止",
            "RootsPump_Start": "罗茨泵启动",
            "RootsPump_Stop": "罗茨泵停止",
            "MolecularPump_Start": "分子泵启动",
            "MolecularPump_Stop": "分子泵停止",
            "MolecularPump1_Start": "分子泵1启动",
            "MolecularPump2_Start": "分子泵2启动",
            "MolecularPump3_Start": "分子泵3启动",
            "GateValve123_Open": "闸板阀1-3开启",
            "GateValve123_Close": "闸板阀1-3关闭",
            "GateValve1_Open": "闸板阀1开启",
            "GateValve1_Close": "闸板阀1关闭",
            "GateValve2_Open": "闸板阀2开启",
            "GateValve2_Close": "闸板阀2关闭",
            "GateValve3_Open": "闸板阀3开启",
            "GateValve3_Close": "闸板阀3关闭",
            "GateValve4_Open": "闸板阀4开启",
            "GateValve4_Close": "闸板阀4关闭",
            "GateValve5_Open": "闸板阀5开启",
            "GateValve5_Close": "闸板阀5关闭",
            "ElectromagneticValve123_Open": "电磁阀1-3开启",
            "ElectromagneticValve123_Close": "电磁阀1-3关闭",
            "ElectromagneticValve1_Close": "电磁阀1关闭",
            "ElectromagneticValve2_Close": "电磁阀2关闭",
            "ElectromagneticValve3_Close": "电磁阀3关闭",
            "ElectromagneticValve4_Open": "电磁阀4开启",
            "ElectromagneticValve4_Close": "电磁阀4关闭",
            "VentValve1_Open": "放气阀1开启",
            "VentValve1_Close": "放气阀1关闭",
            "VentValve2_Open": "放气阀2开启",
            "VentValve2_Close": "放气阀2关闭",
        }
        op_name = operation_names.get(self._last_selected_operation, self._last_selected_operation)
            
        prereqs = OPERATION_PREREQUISITES.get(self._last_selected_operation, [])
        if not prereqs:
            self._prereq_panel.set_prerequisites([], op_name)
            return
            
        # 检查每个先决条件，并获取详细信息
        results = PrerequisiteChecker.check_all_with_details(self._last_selected_operation, status)
        self._prereq_panel.set_prerequisites(results, op_name)
            
    def _on_tab_changed(self, index: int):
        """选项卡切换时更新先决条件"""
        # 清空先决条件显示
        self._last_selected_operation = None
        if hasattr(self, "_prereq_panel"):
            self._prereq_panel.clear_prerequisites()
        
    def eventFilter(self, obj, event):
        """事件过滤器 - 鼠标悬停时显示先决条件"""
        from PyQt5.QtCore import QEvent
        
        if event.type() == QEvent.Enter:
            operation = obj.property("operation")
            if operation:
                self._last_selected_operation = operation
                # 立即更新先决条件（即使状态为空也显示）
                status = self._worker.get_cached_status()
                if not status:
                    status = {}  # 使用空字典，先决条件会显示为未满足
                self._update_prerequisites(status)
        elif event.type() == QEvent.Leave:
            # 鼠标离开时可选择清空或保留
            pass
                    
        return super().eventFilter(obj, event)
        
    # =========================================================================
    # 事件处理
    # =========================================================================
    
    def _on_connection_changed(self, connected: bool):
        """Tango 连接状态变化"""
        self._tango_connected = connected
        self._update_conn_indicator()
        
    def _on_plc_connection_changed(self, connected: bool):
        """PLC 连接状态变化"""
        self._plc_connected = connected
        self._update_conn_indicator()
        
    def _update_conn_indicator(self):
        """更新连接指示灯 (Tango AND PLC)"""
        # 只有两者都连接才显示绿色
        overall_connected = self._tango_connected and self._plc_connected
        self._conn_indicator.set_state(overall_connected)
        
        # 更新提示
        status_text = []
        if self._tango_connected:
            status_text.append("Tango: 已连接")
        else:
            status_text.append("Tango: 未连接")
            
        if self._plc_connected:
            status_text.append("PLC: 已连接")
        else:
            status_text.append("PLC: 未连接")
            
        self._conn_indicator.setToolTip("\n".join(status_text))
        
    def _on_command_result(self, cmd_name: str, success: bool, message: str):
        """命令执行结果"""
        if not success:
            self._show_centered_message(QMessageBox.Warning, "命令执行失败", f"{cmd_name}: {message}")
            
    def _on_switch_to_auto(self):
        """切换到自动模式"""
        res = self._show_centered_message(
            QMessageBox.Question, "确认",
            "确定要切换到自动模式吗？\n切换后将禁用手动操作。",
            QMessageBox.Yes | QMessageBox.No
        )
        if res == QMessageBox.Yes:
            self._worker.switch_to_auto()
            
    def _on_switch_to_manual(self):
        """切换到手动模式"""
        res = self._show_centered_message(
            QMessageBox.Question, "确认",
            "确定要切换到手动模式吗？\n自动流程将被中止。",
            QMessageBox.Yes | QMessageBox.No
        )
        if res == QMessageBox.Yes:
            self._worker.switch_to_manual()
            
    def _on_vacuum_start(self):
        """一键抽真空"""
        if not self._check_prerequisites_before_action("Auto_OneKeyVacuumStart"):
            return
        res = self._show_centered_message(
            QMessageBox.Question, "确认",
            "确定要启动一键抽真空流程吗？",
            QMessageBox.Yes | QMessageBox.No
        )
        if res == QMessageBox.Yes:
            self._worker.one_key_vacuum_start()
            
    def _on_vacuum_stop(self):
        """一键停机"""
        if not self._check_prerequisites_before_action("Auto_OneKeyVacuumStop"):
            return
        res = self._show_centered_message(
            QMessageBox.Question, "确认",
            "确定要执行一键停机吗？",
            QMessageBox.Yes | QMessageBox.No
        )
        if res == QMessageBox.Yes:
            self._worker.one_key_vacuum_stop()
            
    def _on_chamber_vent(self):
        """腔室放气"""
        if not self._btn_chamber_vent.isEnabled():
            self._show_centered_message(QMessageBox.Information, "操作受限", "抽真空流程进行中：腔室放气已互锁屏蔽。")
            return
        if not self._check_prerequisites_before_action("Auto_ChamberVent"):
            return
        res = self._show_centered_message(
            QMessageBox.Question, "确认",
            "确定要进行腔室放气吗？",
            QMessageBox.Yes | QMessageBox.No
        )
        if res == QMessageBox.Yes:
            self._worker.chamber_vent()
            
    def _on_fault_reset(self):
        """故障复位"""
        if not self._check_prerequisites_before_action("Auto_FaultReset"):
            return
        self._worker.fault_reset()
        
    def _on_emergency_stop(self):
        """紧急停止"""
        res = self._show_centered_message(
            QMessageBox.Warning, "紧急停止确认",
            "确定要执行紧急停止吗？\n\n这将立即关闭所有泵和阀门！",
            QMessageBox.Yes | QMessageBox.No
        )
        if res == QMessageBox.Yes:
            self._worker.queue_command("EmergencyStop")
            self._show_centered_message(QMessageBox.Information, "紧急停止", "已发送紧急停止命令")
        
    def _check_manual_mode(self) -> bool:
        """检查是否为手动模式"""
        if self._current_mode != OperationMode.MANUAL:
            self._show_centered_message(QMessageBox.Warning, "操作受限", "当前处于自动模式，请先切换到手动模式。")
            return False
        return True
        
    def _check_prerequisites_before_action(self, operation: str) -> bool:
        """执行操作前检查先决条件"""
        status = self._worker.get_cached_status()
        if not status:
            return True
        
        all_met, failed = PrerequisiteChecker.check_all(operation, status)
        
        if not all_met and failed:
            msg = "操作受阻！以下先决条件未满足:\n\n"
            msg += "\n".join(f"• {f}" for f in failed)
            msg += "\n\n请先满足上述条件后再试。"
            
            self._show_centered_message(QMessageBox.Warning, "操作受限", msg)
            return False
            
        return True
        
    def _show_centered_message(self, icon_type, title, text, buttons=QMessageBox.Ok) -> int:
        """确保弹出框在主窗口中心且不会被主窗口盖住（Windows Z-order 兼容）"""
        main_win = self.window()

        # 关键：必须把主窗口作为 parent/owner，才能保证点主窗口不会把对话框压到后面
        msg_box = QMessageBox(main_win)
        msg_box.setIcon(icon_type)
        msg_box.setWindowTitle(title)
        msg_box.setText(text)
        msg_box.setStandardButtons(buttons)

        # 关键：应用模态，阻止与主窗口交互（避免“点别处导致层级变化”的感觉）
        msg_box.setWindowModality(Qt.ApplicationModal)
        msg_box.setModal(True)

        # Windows 下避免被主窗口“抢前台”盖住：强制置顶
        msg_box.setWindowFlags(msg_box.windowFlags() | Qt.WindowStaysOnTopHint)

        # 额外保险：弹窗存在时吞掉主窗口的鼠标/键盘事件（防止用户点到主窗口触发激活/层级变化）
        class _Blocker(QObject):
            def eventFilter(self, obj, event):
                if event.type() in (
                    QEvent.MouseButtonPress, QEvent.MouseButtonRelease, QEvent.MouseButtonDblClick,
                    QEvent.Wheel,
                    QEvent.KeyPress, QEvent.KeyRelease,
                ):
                    return True
                return False

        blocker = _Blocker(msg_box)
        main_win.installEventFilter(blocker)

        # 计算居中位置
        msg_box.adjustSize()
        main_geom = main_win.frameGeometry()
        x = main_geom.x() + (main_geom.width() - msg_box.sizeHint().width()) // 2
        y = main_geom.y() + (main_geom.height() - msg_box.sizeHint().height()) // 2
        msg_box.move(x, y)

        try:
            # 显示前先激活并抬升，减少首次弹出被压住的概率
            msg_box.show()
            msg_box.raise_()
            msg_box.activateWindow()
            return msg_box.exec_()
        finally:
            main_win.removeEventFilter(blocker)
        
    def _set_pump(self, pump_name: str, state: bool):
        """设置泵状态"""
        if not self._check_manual_mode():
            return
            
        # 检查先决条件
        operation = f"{pump_name}_{'Start' if state else 'Stop'}"
        if not self._check_prerequisites_before_action(operation):
            return
            
        if pump_name == "ScrewPump":
            self._worker.set_screw_pump_power(state)
        elif pump_name == "RootsPump":
            self._worker.set_roots_pump_power(state)
            
    def _set_molecular_pump(self, index: int, state: bool):
        """设置分子泵"""
        if not self._check_manual_mode():
            return
            
        if state:
            # 分子泵1-3启动需要对应的电磁阀和闸板阀分别开启
            operation = f"MolecularPump{index}_Start"
            if not self._check_prerequisites_before_action(operation):
                return
                
        self._worker.set_molecular_pump_power(index, state)
        
    def _set_gate_valve(self, index: int, open_valve: bool):
        """设置闸板阀"""
        if not self._check_manual_mode():
            return
            
        # 确定先决条件 - 闸板阀1-3开启需要对应的电磁阀1-3分别开启，关闭操作使用对应的单独条件
        if open_valve:
            if index <= 3:
                operation = f"GateValve{index}_Open"  # 使用单独条件，检查对应的电磁阀
            elif index == 4:
                operation = "GateValve4_Open"
            else:
                operation = "GateValve5_Open"
        else:
            if index <= 3:
                operation = f"GateValve{index}_Close"  # 单独的关闭条件
            elif index == 4:
                operation = "GateValve4_Close"
            else:
                operation = "GateValve5_Close"
                
        if not self._check_prerequisites_before_action(operation):
            return
                
        self._worker.set_gate_valve(index, open_valve)
        
    def _set_electromagnetic_valve(self, index: int, state: bool):
        """设置电磁阀"""
        if not self._check_manual_mode():
            return
        
        # P1-1: 电磁阀先决条件检查 - 关闭操作使用对应的单独条件
        if state:
            if index <= 3:
                operation = "ElectromagneticValve123_Open"
            else:
                operation = "ElectromagneticValve4_Open"
        else:
            if index <= 3:
                operation = f"ElectromagneticValve{index}_Close"  # 单独的关闭条件
            else:
                operation = "ElectromagneticValve4_Close"
        
        if not self._check_prerequisites_before_action(operation):
            return
            
        self._worker.set_electromagnetic_valve(index, state)
        
    def _set_vent_valve(self, index: int, state: bool):
        """设置放气阀"""
        if not self._check_manual_mode():
            return
            
        if state:
            operation = f"VentValve{index}_Open"
            if not self._check_prerequisites_before_action(operation):
                return
                
        self._worker.set_vent_valve(index, state)
    
    def _on_mp_config_changed(self, index: int, state: int):
        """P1-2: 分子泵配置改变"""
        enabled = state == Qt.Checked
        AUTO_MOLECULAR_PUMP_CONFIG[f'molecularPump{index}Enabled'] = enabled
        
        # 通知 Worker（如果支持）
        if hasattr(self._worker, 'set_molecular_pump_enabled'):
            self._worker.set_molecular_pump_enabled(index, enabled)
        
        logger.info(f"分子泵{index} 自动流程参与状态: {'启用' if enabled else '停用'}")
    
    def _update_branch_display(self, status: dict, state: int):
        """P2-1: 更新自动抽真空分支判据显示"""
        if state != SystemState.PUMPING:
            self._branch_label.setText("")
            return
        
        # 获取当前真空度
        vacuum2 = status.get('vacuumGauge2', 101325)  # 腔室真空
        vacuum1 = status.get('vacuumGauge1', 101325)  # 前级真空
        threshold = AUTO_BRANCH_THRESHOLDS.get("粗抽分支阈值Pa", 3000)
        roots_threshold = AUTO_BRANCH_THRESHOLDS.get("罗茨泵启动阈值Pa", 80000)
        
        branch_info = []
        
        # 判断当前分支
        if vacuum2 >= threshold:
            branch_info.append(f"分支: 粗抽({vacuum2:.0f}Pa)")
        else:
            branch_info.append(f"分支: 精抽({vacuum2:.0f}Pa)")
        
        # 罗茨泵启动条件
        if vacuum1 <= roots_threshold:
            branch_info.append(f"前级到位({vacuum1:.0f}Pa)")
        else:
            branch_info.append(f"前级降压中({vacuum1:.0f}Pa)")
        
        self._branch_label.setText(" | ".join(branch_info))
    
    def closeEvent(self, event):
        """页面关闭时清理资源"""
        # 停止所有定时器
        if hasattr(self, '_poll_timer') and self._poll_timer.isActive():
            self._poll_timer.stop()
        if hasattr(self, '_blink_timer') and self._blink_timer.isActive():
            self._blink_timer.stop()
        super().closeEvent(event)
