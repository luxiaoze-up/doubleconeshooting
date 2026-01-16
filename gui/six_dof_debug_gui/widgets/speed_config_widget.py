"""
速度参数配置控件
Speed Configuration Widget
配置启动速度、最大速度、加速度时间等参数
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QLabel, QDoubleSpinBox, QPushButton, QComboBox,
    QMessageBox
)
from PyQt5.QtCore import Qt

# 使用完整包路径导入（开发和打包环境一致）
from gui.six_dof_debug_gui.hardware.smc_controller import SMCController


class SpeedConfigWidget(QWidget):
    """速度参数配置控件"""
    
    def __init__(self, smc_controller: SMCController = None, parent=None):
        super().__init__(parent)
        self.smc_controller = smc_controller
        
        self._init_ui()
    
    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        
        # 轴选择
        axis_group = QGroupBox("轴选择")
        axis_layout = QHBoxLayout()
        axis_layout.addWidget(QLabel("轴号:"))
        self.axis_combo = QComboBox()
        self.axis_combo.addItems(["全部", "轴0", "轴1", "轴2", "轴3", "轴4", "轴5"])
        axis_layout.addWidget(self.axis_combo)
        axis_layout.addStretch()
        axis_group.setLayout(axis_layout)
        layout.addWidget(axis_group)
        
        # 速度参数
        speed_group = QGroupBox("速度参数")
        speed_layout = QVBoxLayout()
        
        # 启动速度
        start_layout = QHBoxLayout()
        start_layout.addWidget(QLabel("启动速度:"))
        self.start_speed_spinbox = QDoubleSpinBox()
        self.start_speed_spinbox.setRange(0.0, 10000.0)
        self.start_speed_spinbox.setDecimals(1)
        start_layout.addWidget(self.start_speed_spinbox)
        speed_layout.addLayout(start_layout)
        
        # 最大速度
        max_layout = QHBoxLayout()
        max_layout.addWidget(QLabel("最大速度:"))
        self.max_speed_spinbox = QDoubleSpinBox()
        self.max_speed_spinbox.setRange(0.0, 10000.0)
        self.max_speed_spinbox.setDecimals(1)
        max_layout.addWidget(self.max_speed_spinbox)
        speed_layout.addLayout(max_layout)
        
        # 加速度时间
        acc_layout = QHBoxLayout()
        acc_layout.addWidget(QLabel("加速度时间(s):"))
        self.acc_time_spinbox = QDoubleSpinBox()
        self.acc_time_spinbox.setRange(0.0, 10.0)
        self.acc_time_spinbox.setDecimals(3)
        acc_layout.addWidget(self.acc_time_spinbox)
        speed_layout.addLayout(acc_layout)
        
        # 减速度时间
        dec_layout = QHBoxLayout()
        dec_layout.addWidget(QLabel("减速度时间(s):"))
        self.dec_time_spinbox = QDoubleSpinBox()
        self.dec_time_spinbox.setRange(0.0, 10.0)
        self.dec_time_spinbox.setDecimals(3)
        dec_layout.addWidget(self.dec_time_spinbox)
        speed_layout.addLayout(dec_layout)
        
        # 停止速度
        stop_layout = QHBoxLayout()
        stop_layout.addWidget(QLabel("停止速度:"))
        self.stop_speed_spinbox = QDoubleSpinBox()
        self.stop_speed_spinbox.setRange(0.0, 10000.0)
        self.stop_speed_spinbox.setDecimals(1)
        stop_layout.addWidget(self.stop_speed_spinbox)
        speed_layout.addLayout(stop_layout)
        
        speed_group.setLayout(speed_layout)
        layout.addWidget(speed_group)
        
        # 应用按钮
        self.apply_btn = QPushButton("应用")
        self.apply_btn.clicked.connect(self._on_apply_clicked)
        layout.addWidget(self.apply_btn)
        
        layout.addStretch()
    
    def set_default_values(self, start_speed: float, max_speed: float,
                          acc_time: float, dec_time: float, stop_speed: float):
        """设置默认值"""
        self.start_speed_spinbox.setValue(start_speed)
        self.max_speed_spinbox.setValue(max_speed)
        self.acc_time_spinbox.setValue(acc_time)
        self.dec_time_spinbox.setValue(dec_time)
        self.stop_speed_spinbox.setValue(stop_speed)
    
    def _on_apply_clicked(self):
        """应用按钮点击"""
        if not self.smc_controller or not self.smc_controller.is_connected():
            QMessageBox.warning(
                self, 
                "未连接", 
                "请先连接设备\n\n"
                "在配置速度参数之前，需要先连接到运动控制器。"
            )
            return
        
        try:
            # 获取参数值
            start_speed = self.start_speed_spinbox.value()
            max_speed = self.max_speed_spinbox.value()
            acc_time = self.acc_time_spinbox.value()
            dec_time = self.dec_time_spinbox.value()
            stop_speed = self.stop_speed_spinbox.value()
            
            # 获取选中的轴
            axis_index = self.axis_combo.currentIndex()
            
            # 应用配置
            success_count = 0
            if axis_index == 0:
                # 全部轴
                for axis in range(6):
                    if self.smc_controller.set_speed_profile(
                        axis, start_speed, max_speed, acc_time, dec_time, stop_speed
                    ):
                        success_count += 1
                
                if success_count == 6:
                    QMessageBox.information(self, "成功", "速度参数已应用到所有轴")
                else:
                    QMessageBox.warning(
                        self, 
                        "部分失败", 
                        f"部分轴配置失败\n\n"
                        f"成功：{success_count}/6 个轴\n\n"
                        f"请检查：\n"
                        f"• 设备连接是否正常\n"
                        f"• 控制器是否响应\n"
                        f"• 尝试重新连接设备"
                    )
            else:
                # 单个轴（axis_index - 1 因为0是"全部"）
                axis = axis_index - 1
                if self.smc_controller.set_speed_profile(
                    axis, start_speed, max_speed, acc_time, dec_time, stop_speed
                ):
                    QMessageBox.information(self, "成功", f"速度参数已应用到轴{axis}")
                else:
                    QMessageBox.critical(
                        self, 
                        "配置失败", 
                        f"轴{axis}配置失败\n\n"
                        f"无法将速度参数应用到轴{axis}。\n\n"
                        f"请检查：\n"
                        f"• 设备连接是否正常\n"
                        f"• 控制器是否响应\n"
                        f"• 参数值是否在允许范围内"
                    )
        
        except Exception as e:
            QMessageBox.critical(
                self, 
                "配置失败", 
                f"应用速度参数时发生错误\n\n"
                f"程序在配置速度参数时遇到问题。\n\n"
                f"错误信息：{str(e)}\n\n"
                f"建议：\n"
                f"• 检查参数值是否合理\n"
                f"• 确认设备连接正常\n"
                f"• 尝试重新连接设备"
            )
    
    def set_smc_controller(self, smc_controller: SMCController):
        """设置SMC控制器"""
        self.smc_controller = smc_controller
