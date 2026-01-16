"""
真空系统 GUI 主窗口

设备: sys/vacuum/2
"""

import sys
import os
import ctypes
from datetime import datetime
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QStatusBar, QLabel, QFrame, QMessageBox
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont, QIcon

from config import VACUUM_SYSTEM_DEVICE, UI_CONFIG
from styles import MAIN_STYLE, COLORS
from tango_worker import VacuumTangoWorker, MockTangoWorker
from alarm_manager import AlarmManager, AlarmIntegration
from main_page import VacuumSystemMainPage
from digital_twin_page import DigitalTwinPage
from record_pages import AlarmRecordPage, OperationHistoryPage, TrendCurvePage


def get_unicode_font(size: int = 9) -> QFont:
    """获取支持 Unicode 符号的字体
    
    用于确保 Unicode 图标字符（如 ●、⚠）能正确显示
    
    Args:
        size: 字体大小，默认 9
        
    Returns:
        支持 Unicode 符号的字体对象
    """
    # 按优先级尝试支持 Unicode 符号的字体
    font_families = [
        "Segoe UI Symbol",      # Windows 10+ 自带，支持常用符号
        "Arial Unicode MS",     # 支持大量 Unicode 字符
        "Segoe UI",             # Windows 标准字体，部分支持
        "Microsoft YaHei",      # 中文字体，部分支持
    ]
    
    for family in font_families:
        font = QFont(family, size)
        if font.exactMatch():
            return font
    
    # 如果都不存在，返回系统默认字体
    return QFont()


class OperationLogger:
    """操作记录器 - 统一记录所有操作"""
    
    def __init__(self, history_page: OperationHistoryPage):
        self._history_page = history_page
        
    def log(self, operation_type: str, device: str, description: str):
        """记录操作"""
        self._history_page.add_operation(operation_type, device, description)


class VacuumSystemMainWindow(QMainWindow):
    """真空系统主窗口"""
    
    def __init__(self, use_mock: bool = False):
        super().__init__()
        
        self._use_mock = use_mock
        self._tango_connected = False
        self._plc_connected = False
        
        self._init_worker()
        self._init_alarm_manager()
        self._init_ui()
        self._init_operation_logger()
        self._connect_signals()
        
        # 初始同步连接状态（处理 Worker 已经连接的情况）
        self._on_connection_changed(self._worker.device is not None)
        self._on_plc_connection_changed(self._worker._last_plc_connected)
        
        self._start_time_update()
        
    def _init_worker(self):
        """初始化 Tango Worker"""
        if self._use_mock:
            self._worker = MockTangoWorker(VACUUM_SYSTEM_DEVICE, self)
        else:
            self._worker = VacuumTangoWorker(VACUUM_SYSTEM_DEVICE, self)
            
        self._worker.start()
        
    def _init_alarm_manager(self):
        """初始化报警管理器"""
        self._alarm_manager = AlarmManager(self)
        self._alarm_manager.set_popup_parent(self)
        
        # 集成
        self._alarm_integration = AlarmIntegration(self._worker, self._alarm_manager)
        
    def _init_operation_logger(self):
        """初始化操作记录器"""
        self._operation_logger = OperationLogger(self._history_page)
        
        # 连接命令结果信号
        self._worker.command_result.connect(self._on_command_for_logging)
        
    def _init_ui(self):
        """初始化 UI"""
        self.setWindowTitle("真空系统控制 - sys/vacuum/2")
        self.setMinimumSize(UI_CONFIG['window_min_width'], UI_CONFIG['window_min_height'])
        self.resize(1400, 900)
        
        # 应用样式
        self.setStyleSheet(MAIN_STYLE)
        
        # 中心部件
        central = QWidget()
        self.setCentralWidget(central)
        
        layout = QVBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        
        # 标题栏
        layout.addWidget(self._create_title_bar())
        
        # 主选项卡
        self._tabs = QTabWidget()
        self._tabs.setDocumentMode(True)
        
        # 添加页面
        self._main_page = VacuumSystemMainPage(self._worker, self)
        self._tabs.addTab(self._main_page, "控制面板")
        
        self._twin_page = DigitalTwinPage(self._worker, self)
        self._tabs.addTab(self._twin_page, "数字孪生")
        
        self._alarm_page = AlarmRecordPage(self._alarm_manager, self)
        self._tabs.addTab(self._alarm_page, "报警记录")
        
        self._history_page = OperationHistoryPage(self)
        self._tabs.addTab(self._history_page, "操作历史")
        
        self._trend_page = TrendCurvePage(self._worker, self)
        self._tabs.addTab(self._trend_page, "趋势曲线")
        
        layout.addWidget(self._tabs, 1)
        
        # 状态栏
        self._status_bar = self._create_status_bar()
        self.setStatusBar(self._status_bar)
        
    def _create_title_bar(self) -> QWidget:
        """创建标题栏"""
        frame = QFrame()
        frame.setStyleSheet(f"""
            QFrame {{
                background-color: {COLORS['surface']};
                border-bottom: 2px solid {COLORS['primary']};
            }}
        """)
        frame.setFixedHeight(60)
        
        layout = QHBoxLayout(frame)
        layout.setContentsMargins(20, 0, 20, 0)
        
        # 标题
        title = QLabel("真空系统控制台")
        title.setFont(QFont("Microsoft YaHei", 18, QFont.Bold))
        title.setStyleSheet(f"color: {COLORS['primary_light']};")
        layout.addWidget(title)
        
        layout.addStretch()
        
        # 设备名
        device_label = QLabel(f"设备: {VACUUM_SYSTEM_DEVICE}")
        device_label.setStyleSheet(f"color: {COLORS['text_secondary']};")
        layout.addWidget(device_label)
        
        # 模式指示
        if self._use_mock:
            mock_label = QLabel("[ 模拟模式 ]")
            mock_label.setStyleSheet(f"color: {COLORS['warning']}; font-weight: bold;")
            layout.addWidget(mock_label)
            
        return frame
        
    def _create_status_bar(self) -> QStatusBar:
        """创建状态栏"""
        status_bar = QStatusBar()
        status_bar.setStyleSheet(f"""
            QStatusBar {{
                background-color: {COLORS['surface']};
                color: {COLORS['text_secondary']};
                border-top: 1px solid {COLORS['border']};
            }}
        """)
        
        # 连接状态
        self._conn_status = QLabel()
        self._update_conn_status_label()
        # 设置支持 Unicode 符号的字体，确保图标正确显示
        self._conn_status.setFont(get_unicode_font(9))
        status_bar.addWidget(self._conn_status)
        
        # 分隔
        sep = QFrame()
        sep.setFrameShape(QFrame.VLine)
        sep.setStyleSheet(f"background-color: {COLORS['border']};")
        status_bar.addWidget(sep)
        
        # 报警状态
        self._alarm_status = QLabel("报警: 0")
        # 设置支持 Unicode 符号的字体，确保图标正确显示
        self._alarm_status.setFont(get_unicode_font(9))
        status_bar.addWidget(self._alarm_status)
        
        # 分隔
        sep2 = QFrame()
        sep2.setFrameShape(QFrame.VLine)
        sep2.setStyleSheet(f"background-color: {COLORS['border']};")
        status_bar.addWidget(sep2)
        
        # 时间显示
        self._time_label = QLabel("")
        self._time_label.setStyleSheet(f"color: {COLORS['primary_light']};")
        status_bar.addWidget(self._time_label)
        
        # 右侧永久消息
        status_bar.addPermanentWidget(QLabel("版本 2.0.0"))
        
        return status_bar
        
    def _start_time_update(self):
        """启动时间更新"""
        self._time_timer = QTimer(self)
        self._time_timer.timeout.connect(self._update_time)
        self._time_timer.start(1000)
        self._update_time()
        
    def _update_time(self):
        """更新时间显示"""
        now = datetime.now()
        self._time_label.setText(now.strftime("%Y-%m-%d %H:%M:%S"))
        
    def _connect_signals(self):
        """连接信号"""
        self._worker.connection_changed.connect(self._on_connection_changed)
        self._worker.plc_connection_changed.connect(self._on_plc_connection_changed)
        self._alarm_manager.alarms_changed.connect(self._on_alarms_changed)
        
    def _on_connection_changed(self, connected: bool):
        """Tango 连接状态变化"""
        self._tango_connected = connected
        self._update_conn_status_label()
            
    def _on_plc_connection_changed(self, connected: bool):
        """PLC 连接状态变化"""
        self._plc_connected = connected
        self._update_conn_status_label()
        
    def _update_conn_status_label(self):
        """更新总连接状态标签"""
        # Tango 状态
        tango_text = "Tango: ●"
        tango_color = COLORS['success'] if self._tango_connected else COLORS['error']
        
        # PLC 状态
        plc_text = "PLC: ●"
        plc_color = COLORS['success'] if self._plc_connected else COLORS['error']
        
        # 如果 Tango 都没连上，PLC 肯定也没连上
        if not self._tango_connected:
            plc_color = COLORS['error']
            
        self._conn_status.setText(f"{tango_text}  {plc_text}")
        
        # 这种复杂的富文本 QStatusBar 可能不支持直接设置样式，使用 QLabel 的 setStyleSheet
        # 简单的方案是分别创建两个 Label，或者使用一个 Label 配合 HTML
        self._conn_status.setText(f'Tango: <span style="color: {tango_color}">●</span>  '
                                 f'PLC: <span style="color: {plc_color}">●</span>')
            
    def _on_alarms_changed(self):
        """报警状态变化"""
        active_count = len(self._alarm_manager.get_active_alarms())
        unack_count = len(self._alarm_manager.get_unacknowledged_alarms())
        
        if unack_count > 0:
            self._alarm_status.setText(f"⚠ 报警: {active_count} (未确认: {unack_count})")
            self._alarm_status.setStyleSheet(f"color: {COLORS['error']}; font-weight: bold;")
        elif active_count > 0:
            self._alarm_status.setText(f"报警: {active_count}")
            self._alarm_status.setStyleSheet(f"color: {COLORS['warning']};")
        else:
            self._alarm_status.setText("报警: 0")
            self._alarm_status.setStyleSheet(f"color: {COLORS['text_secondary']};")
            
    def _on_command_for_logging(self, cmd_name: str, success: bool, message: str):
        """记录命令执行到操作历史"""
        # 命令名称映射
        cmd_descriptions = {
            "SwitchToAuto": ("模式切换", "系统", "切换到自动模式"),
            "SwitchToManual": ("模式切换", "系统", "切换到手动模式"),
            "OneKeyVacuumStart": ("自动操作", "系统", "一键抽真空启动"),
            "OneKeyVacuumStop": ("自动操作", "系统", "一键停机"),
            "ChamberVent": ("自动操作", "系统", "腔室放气"),
            "FaultReset": ("故障处理", "系统", "故障复位"),
            "SetScrewPumpPower": ("泵控制", "螺杆泵", "电源控制"),
            "SetRootsPumpPower": ("泵控制", "罗茨泵", "电源控制"),
            "SetMolecularPumpPower": ("泵控制", "分子泵", "启停控制"),
            "SetGateValve": ("阀门控制", "闸板阀", "开关控制"),
            "SetElectromagneticValve": ("阀门控制", "电磁阀", "开关控制"),
            "SetVentValve": ("阀门控制", "放气阀", "开关控制"),
            "SetWaterValve": ("阀门控制", "水电磁阀", "开关控制"),
            "SetAirMainValve": ("阀门控制", "气主电磁阀", "开关控制"),
            "EmergencyStop": ("紧急操作", "系统", "紧急停止"),
        }
        
        if cmd_name in cmd_descriptions:
            op_type, device, desc = cmd_descriptions[cmd_name]
            status = "成功" if success else f"失败: {message}"
            self._operation_logger.log(op_type, device, f"{desc} - {status}")
            
    def closeEvent(self, event):
        """关闭事件"""
        # 先清理所有报警弹窗，防止阻塞退出
        self._alarm_manager.acknowledge_all()
        self._alarm_manager.clear_all_active()
        
        # 停止 worker
        self._worker.stop()
        event.accept()


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description="真空系统控制 GUI")
    parser.add_argument("--mock", action="store_true", help="使用模拟模式")
    args = parser.parse_args()
    
    # ---------------------------------------------------------------------
    # High DPI / 缩放兼容（exe 环境 vs python 运行时的 DPI awareness 可能不同）
    # 在创建 QApplication 前设置，避免 exe 下控件比例失调/字体异常缩放。
    # ---------------------------------------------------------------------
    try:
        # Per-monitor DPI aware（Win10+）
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        try:
            # 旧接口兜底
            ctypes.windll.user32.SetProcessDPIAware()
        except Exception:
            pass

    # Qt 高 DPI 支持
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
    try:
        QApplication.setHighDpiScaleFactorRoundingPolicy(
            Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
        )
    except Exception:
        pass

    # Windows 下原生对话框有概率出现 Z-order 异常（被主窗口盖住/点击后跑到后面）
    # 强制使用 Qt 自绘对话框，确保模态/置顶行为可控。
    QApplication.setAttribute(Qt.AA_DontUseNativeDialogs, True)
    app = QApplication(sys.argv)
    
    # 设置应用信息
    app.setApplicationName("VacuumSystemGUI")
    app.setOrganizationName("DoubleConeShooting")
    
    # ---------------------------------------------------------------------
    # 设置应用程序默认字体，确保打包后字体一致
    # ---------------------------------------------------------------------
    default_font = QFont()
    # 优先使用支持 Unicode 符号的字体，确保图标正确显示
    # Segoe UI Symbol 和 Arial Unicode MS 都支持常用 Unicode 符号
    font_families = ["Segoe UI", "Segoe UI Symbol", "Microsoft YaHei", "Arial Unicode MS", "SimHei", "Arial", "Sans-Serif"]
    for family in font_families:
        default_font.setFamily(family)
        if default_font.exactMatch() or QFont(family).exactMatch():
            break
    default_font.setPointSize(9)
    app.setFont(default_font)
    
    # 创建主窗口
    window = VacuumSystemMainWindow(use_mock=args.mock)
    window.show()
    
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
