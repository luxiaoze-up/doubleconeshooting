"""
连接配置控件
Connection Configuration Widget
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QLabel, QLineEdit, QPushButton, QCheckBox, QMessageBox
)
from PyQt5.QtCore import Qt, pyqtSignal

# 使用完整包路径导入（开发和打包环境一致）
from gui.six_dof_debug_gui.hardware.smc_controller import SMCController
from gui.six_dof_debug_gui.hardware.encoder_collector import EncoderCollector
from gui.six_dof_debug_gui.config import Config


class ConnectionWidget(QWidget):
    """连接配置控件"""
    
    # 信号
    connected = pyqtSignal(bool)  # 连接状态变化
    brake_state_changed = pyqtSignal(bool)  # 抱闸状态变化
    
    def __init__(self, config: Config, parent=None):
        super().__init__(parent)
        self.config = config
        self.smc_controller: SMCController = None
        self.encoder_collector: EncoderCollector = None
        self.is_connected = False
        self.brake_released = False
        
        self._init_ui()
        self._load_config()
    
    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        layout.setSpacing(16)
        
        # 运动控制器配置组
        motion_group = QGroupBox("运动控制器配置")
        motion_layout = QVBoxLayout()
        
        # IP地址
        ip_layout = QHBoxLayout()
        ip_layout.addWidget(QLabel("IP地址:"))
        self.motion_ip_edit = QLineEdit()
        self.motion_ip_edit.setPlaceholderText("192.168.1.13")
        ip_layout.addWidget(self.motion_ip_edit)
        motion_layout.addLayout(ip_layout)
        
        # 卡ID
        card_layout = QHBoxLayout()
        card_layout.addWidget(QLabel("卡ID:"))
        self.motion_card_id_edit = QLineEdit()
        self.motion_card_id_edit.setPlaceholderText("0")
        self.motion_card_id_edit.setMaximumWidth(80)
        card_layout.addWidget(self.motion_card_id_edit)
        card_layout.addStretch()
        motion_layout.addLayout(card_layout)
        
        # 连接按钮
        self.connect_btn = QPushButton("连接")
        self.connect_btn.clicked.connect(self._on_connect_clicked)
        motion_layout.addWidget(self.connect_btn)
        
        # 连接状态
        self.connection_status_label = QLabel("未连接")
        self.connection_status_label.setAlignment(Qt.AlignCenter)
        motion_layout.addWidget(self.connection_status_label)
        
        motion_group.setLayout(motion_layout)
        layout.addWidget(motion_group)
        
        # 编码器采集器配置组
        encoder_group = QGroupBox("编码器采集器配置")
        encoder_layout = QVBoxLayout()
        
        # IP地址和端口
        encoder_ip_layout = QHBoxLayout()
        encoder_ip_layout.addWidget(QLabel("IP地址:"))
        self.encoder_ip_edit = QLineEdit()
        self.encoder_ip_edit.setPlaceholderText("192.168.1.199")
        encoder_ip_layout.addWidget(self.encoder_ip_edit)
        encoder_layout.addLayout(encoder_ip_layout)
        
        encoder_port_layout = QHBoxLayout()
        encoder_port_layout.addWidget(QLabel("端口:"))
        self.encoder_port_edit = QLineEdit()
        self.encoder_port_edit.setPlaceholderText("5000")
        self.encoder_port_edit.setMaximumWidth(100)
        encoder_port_layout.addWidget(self.encoder_port_edit)
        encoder_port_layout.addStretch()
        encoder_layout.addLayout(encoder_port_layout)
        
        encoder_group.setLayout(encoder_layout)
        layout.addWidget(encoder_group)
        
        # 抱闸控制组
        brake_group = QGroupBox("抱闸控制")
        brake_layout = QVBoxLayout()
        
        self.brake_checkbox = QCheckBox("释放抱闸")
        self.brake_checkbox.stateChanged.connect(self._on_brake_changed)
        brake_layout.addWidget(self.brake_checkbox)
        
        self.brake_status_label = QLabel("状态: 启用")
        self.brake_status_label.setAlignment(Qt.AlignCenter)
        brake_layout.addWidget(self.brake_status_label)
        
        brake_group.setLayout(brake_layout)
        layout.addWidget(brake_group)
        
        layout.addStretch()
    
    def _load_config(self):
        """加载配置"""
        # 运动控制器配置
        motion_config = self.config.get_motion_controller_config()
        self.motion_ip_edit.setText(motion_config.get("ip", "192.168.1.13"))
        self.motion_card_id_edit.setText(str(motion_config.get("card_id", 0)))
        
        # 编码器采集器配置
        encoder_config = self.config.get_encoder_collector_config()
        self.encoder_ip_edit.setText(encoder_config.get("ip", "192.168.1.199"))
        self.encoder_port_edit.setText(str(encoder_config.get("port", 5000)))
    
    def _on_connect_clicked(self):
        """连接按钮点击事件"""
        if self.is_connected:
            self._disconnect()
        else:
            self._connect()
    
    def _connect(self):
        """连接设备"""
        try:
            # 获取配置
            motion_ip = self.motion_ip_edit.text().strip()
            try:
                motion_card_id = int(self.motion_card_id_edit.text().strip())
            except ValueError:
                QMessageBox.warning(
                    self, 
                    "输入错误", 
                    "卡ID格式不正确\n\n"
                    "请输入一个有效的整数（例如：0、1、2）"
                )
                return
            
            encoder_ip = self.encoder_ip_edit.text().strip()
            try:
                encoder_port = int(self.encoder_port_edit.text().strip())
            except ValueError:
                QMessageBox.warning(
                    self, 
                    "输入错误", 
                    "端口号格式不正确\n\n"
                    "请输入一个有效的整数（例如：8080、9090）"
                )
                return
            
            # 连接运动控制器
            try:
                self.smc_controller = SMCController()
            except Exception as e:
                error_msg = str(e)
                if "WinError 193" in error_msg or "无法加载" in error_msg or "未找到" in error_msg:
                    # 友好的DLL加载错误提示
                    user_msg = (
                        "无法加载运动控制器驱动\n\n"
                        "程序无法初始化运动控制器。\n\n"
                        "请检查以下事项：\n"
                        "1. 确认 lib/LTSMC.dll 文件存在\n"
                        "2. 检查驱动文件版本是否匹配\n"
                        "3. 安装 Microsoft Visual C++ Redistributable 运行库\n"
                        "4. 确认驱动文件完整未损坏\n\n"
                        "如果问题持续，请联系技术支持。"
                    )
                    QMessageBox.critical(self, "驱动加载失败", f"{user_msg}\n\n详细信息：{error_msg}")
                else:
                    QMessageBox.critical(
                        self, 
                        "初始化失败", 
                        f"无法初始化运动控制器\n\n"
                        f"程序在初始化控制器时遇到问题。\n\n"
                        f"错误信息：{error_msg}\n\n"
                        f"建议：\n"
                        f"• 检查程序安装是否完整\n"
                        f"• 尝试重新启动程序\n"
                        f"• 联系技术支持"
                    )
                self.smc_controller = None
                return
            
            try:
                if not self.smc_controller.connect(motion_ip, motion_card_id):
                    QMessageBox.critical(
                        self, 
                        "连接失败", 
                        f"无法连接到运动控制器\n\n"
                        f"IP地址：{motion_ip}\n\n"
                        f"请检查以下事项：\n"
                        f"1. IP地址是否正确\n"
                        f"2. 控制器是否已开机并在线\n"
                        f"3. 网络连接是否正常\n"
                        f"4. 防火墙是否阻止了连接\n"
                        f"5. 控制器是否被其他程序占用"
                    )
                    self.smc_controller.disconnect()
                    self.smc_controller = None
                    return
            except RuntimeError as e:
                # DLL加载错误已在connect方法中抛出RuntimeError
                error_msg = str(e)
                user_msg = (
                    "无法加载运动控制器驱动\n\n"
                    "程序无法初始化运动控制器驱动。\n\n"
                    "请检查以下事项：\n"
                    "1. 确认 lib/LTSMC.dll 文件存在\n"
                    "2. 检查驱动文件版本是否匹配\n"
                    "3. 安装 Microsoft Visual C++ Redistributable 运行库\n"
                    "4. 确认驱动文件完整未损坏\n\n"
                    "如果问题持续，请联系技术支持。"
                )
                QMessageBox.critical(self, "驱动加载失败", f"{user_msg}\n\n详细信息：{error_msg}")
                self.smc_controller = None
                return
            
            # 连接编码器采集器
            encoder_channels = self.config.get_encoder_collector_config().get("channels", [0, 1, 2, 3, 4, 5])
            encoder_resolution = self.config.get_encoder_collector_config().get("encoder_resolution", 0.001)
            
            self.encoder_collector = EncoderCollector(
                encoder_ip, encoder_port, encoder_channels, encoder_resolution
            )
            if not self.encoder_collector.connect():
                QMessageBox.critical(
                    self, 
                    "连接失败", 
                    f"无法连接到编码器采集器\n\n"
                    f"地址：{encoder_ip}:{encoder_port}\n\n"
                    f"请检查以下事项：\n"
                    f"1. IP地址和端口号是否正确\n"
                    f"2. 编码器采集器是否已开机并在线\n"
                    f"3. 网络连接是否正常\n"
                    f"4. 防火墙是否阻止了连接"
                )
                self.smc_controller.disconnect()
                self.smc_controller = None
                self.encoder_collector = None
                return
            
            # 更新状态
            self.is_connected = True
            self.connect_btn.setText("断开")
            self.connection_status_label.setText("已连接")
            self.connection_status_label.setStyleSheet("color: #39e072;")
            
            # 保存配置
            self._save_config()
            
            # 发送信号
            self.connected.emit(True)
            
        except Exception as e:
            QMessageBox.critical(
                self, 
                "连接失败", 
                f"连接过程中发生错误\n\n"
                f"程序在连接设备时遇到问题。\n\n"
                f"错误信息：{str(e)}\n\n"
                f"建议操作：\n"
                f"• 检查设备是否正常\n"
                f"• 确认网络连接正常\n"
                f"• 尝试重新连接\n"
                f"• 如果问题持续，请联系技术支持"
            )
            if self.smc_controller:
                self.smc_controller.disconnect()
                self.smc_controller = None
            if self.encoder_collector:
                self.encoder_collector.disconnect()
                self.encoder_collector = None
            self.is_connected = False
    
    def _disconnect(self):
        """断开连接"""
        if self.smc_controller:
            self.smc_controller.disconnect()
            self.smc_controller = None
        
        if self.encoder_collector:
            self.encoder_collector.disconnect()
            self.encoder_collector = None
        
        self.is_connected = False
        self.connect_btn.setText("连接")
        self.connection_status_label.setText("未连接")
        self.connection_status_label.setStyleSheet("color: #ff7b72;")
        
        # 发送信号
        self.connected.emit(False)
    
    def _on_brake_changed(self, state):
        """抱闸状态改变"""
        if not self.is_connected or not self.smc_controller:
            self.brake_checkbox.setChecked(False)
            QMessageBox.warning(self, "警告", "请先连接设备")
            return
        
        brake_released = (state == Qt.Checked)
        brake_config = self.config.get_brake_control_config()
        brake_port = brake_config.get("port", 3)
        
        # 逻辑值：1=释放，0=启用
        logical_value = 1 if brake_released else 0
        
        if self.smc_controller.write_io(brake_port, logical_value):
            self.brake_released = brake_released
            if brake_released:
                self.brake_status_label.setText("状态: 已释放")
                self.brake_status_label.setStyleSheet("color: #39e072;")
            else:
                self.brake_status_label.setText("状态: 已启用")
                self.brake_status_label.setStyleSheet("color: #ff7b72;")
            
            self.brake_state_changed.emit(brake_released)
        else:
            # 恢复复选框状态
            self.brake_checkbox.blockSignals(True)
            self.brake_checkbox.setChecked(not brake_released)
            self.brake_checkbox.blockSignals(False)
            QMessageBox.critical(
                self, 
                "操作失败", 
                "抱闸控制失败\n\n"
                "无法发送抱闸控制命令。\n\n"
                "请检查：\n"
                "• 设备连接是否正常\n"
                "• 控制器是否响应\n"
                "• 尝试重新连接设备"
            )
    
    def _save_config(self):
        """保存配置到文件"""
        try:
            self.config.set("motion_controller.ip", self.motion_ip_edit.text().strip())
            self.config.set("motion_controller.card_id", int(self.motion_card_id_edit.text().strip()))
            self.config.set("encoder_collector.ip", self.encoder_ip_edit.text().strip())
            self.config.set("encoder_collector.port", int(self.encoder_port_edit.text().strip()))
            self.config.save()
        except Exception as e:
            print(f"保存配置失败: {e}")
    
    def get_smc_controller(self) -> SMCController:
        """获取SMC控制器实例"""
        return self.smc_controller
    
    def get_encoder_collector(self) -> EncoderCollector:
        """获取编码器采集器实例"""
        return self.encoder_collector
    
    def is_device_connected(self) -> bool:
        """检查设备是否已连接"""
        return self.is_connected
    
    def closeEvent(self, event):
        """窗口关闭事件"""
        if self.is_connected:
            self._disconnect()
        event.accept()
