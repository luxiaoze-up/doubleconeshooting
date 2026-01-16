"""
六自由度机器人调试GUI - 主窗口
Six DOF Robot Debug GUI - Main Window
"""

import sys
from pathlib import Path
from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QStatusBar, QLabel
)
from PyQt5.QtCore import Qt

# 使用完整包路径导入（开发和打包环境一致）
from gui.six_dof_debug_gui.config import Config
from gui.six_dof_debug_gui.hardware.smc_controller import SMCController
from gui.six_dof_debug_gui.hardware.encoder_collector import EncoderCollector
from gui.six_dof_debug_gui.hardware.pulse_calculator import PulseCalculator
from gui.six_dof_debug_gui.kinematics.stewart_kinematics import StewartPlatformKinematics
from gui.six_dof_debug_gui.widgets.connection_widget import ConnectionWidget
from gui.six_dof_debug_gui.widgets.position_control_widget import PositionControlWidget
from gui.six_dof_debug_gui.widgets.rotation_control_widget import RotationControlWidget
from gui.six_dof_debug_gui.widgets.encoder_display_widget import EncoderDisplayWidget
from gui.six_dof_debug_gui.widgets.speed_config_widget import SpeedConfigWidget


class SixDofDebugMainWindow(QMainWindow):
    """六自由度机器人调试主窗口"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        # 加载配置
        self.config = Config()
        
        # 初始化硬件模块
        self.pulse_calculator = PulseCalculator(self.config.get_pulse_calculation_config())
        self.kinematics = StewartPlatformKinematics(self.config.get_kinematics_config())
        
        # 初始化UI
        self._init_ui()
        
        # 连接信号
        self._connect_signals()
        
        # 设置窗口属性
        self.setWindowTitle("六自由度机器人调试GUI")
        self.setMinimumSize(1000, 700)
    
    def _init_ui(self):
        """初始化UI"""
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(10)
        
        # 创建标签页
        self.tab_widget = QTabWidget()
        
        # 连接配置标签页
        self.connection_widget = ConnectionWidget(self.config)
        self.tab_widget.addTab(self.connection_widget, "连接配置")
        
        # 位置控制标签页
        self.position_control_widget = PositionControlWidget(
            None, self.pulse_calculator, self.kinematics
        )
        self.tab_widget.addTab(self.position_control_widget, "位置控制")
        
        # 旋转控制标签页
        self.rotation_control_widget = RotationControlWidget(
            None, self.pulse_calculator, self.kinematics
        )
        self.tab_widget.addTab(self.rotation_control_widget, "旋转控制")
        
        # 编码器显示标签页
        self.encoder_display_widget = EncoderDisplayWidget()
        self.tab_widget.addTab(self.encoder_display_widget, "编码器数据")
        
        # 速度配置标签页
        self.speed_config_widget = SpeedConfigWidget()
        speed_defaults = self.config.get_speed_defaults()
        self.speed_config_widget.set_default_values(
            speed_defaults.get("start_speed", 100),
            speed_defaults.get("max_speed", 1000),
            speed_defaults.get("acc_time", 0.1),
            speed_defaults.get("dec_time", 0.1),
            speed_defaults.get("stop_speed", 50)
        )
        self.tab_widget.addTab(self.speed_config_widget, "速度配置")
        
        main_layout.addWidget(self.tab_widget)
        
        # 状态栏
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_label = QLabel("就绪")
        self.status_bar.addWidget(self.status_label)
    
    def _connect_signals(self):
        """连接信号"""
        # 连接配置信号
        self.connection_widget.connected.connect(self._on_connection_changed)
    
    def _on_connection_changed(self, connected: bool):
        """连接状态改变"""
        if connected:
            # 获取硬件实例
            smc_controller = self.connection_widget.get_smc_controller()
            encoder_collector = self.connection_widget.get_encoder_collector()
            
            # 更新各个控件
            self.position_control_widget.set_smc_controller(smc_controller)
            self.rotation_control_widget.set_smc_controller(smc_controller)
            self.encoder_display_widget.set_encoder_collector(encoder_collector)
            self.speed_config_widget.set_smc_controller(smc_controller)
            
            self.status_label.setText("已连接")
            self.status_label.setStyleSheet("color: #39e072;")
        else:
            # 清除硬件实例
            self.position_control_widget.set_smc_controller(None)
            self.rotation_control_widget.set_smc_controller(None)
            self.encoder_display_widget.set_encoder_collector(None)
            self.speed_config_widget.set_smc_controller(None)
            
            self.status_label.setText("未连接")
            self.status_label.setStyleSheet("color: #ff7b72;")
    
    def closeEvent(self, event):
        """窗口关闭事件"""
        # 断开连接
        if self.connection_widget.is_device_connected():
            self.connection_widget._disconnect()
        event.accept()
