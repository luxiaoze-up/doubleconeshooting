"""
位置控制控件
Position Control Widget
X/Y/Z轴位置控制和相对/绝对模式
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QLabel, QDoubleSpinBox, QPushButton, QRadioButton,
    QButtonGroup, QMessageBox
)
from PyQt5.QtCore import Qt, pyqtSignal

# 使用完整包路径导入（开发和打包环境一致）
from gui.six_dof_debug_gui.hardware.smc_controller import SMCController
from gui.six_dof_debug_gui.hardware.pulse_calculator import PulseCalculator
from gui.six_dof_debug_gui.kinematics.stewart_kinematics import StewartPlatformKinematics, Pose


class PositionControlWidget(QWidget):
    """位置控制控件"""
    
    # 信号
    move_started = pyqtSignal()
    move_completed = pyqtSignal(bool, str)  # success, message
    
    def __init__(self, smc_controller: SMCController = None,
                 pulse_calculator: PulseCalculator = None,
                 kinematics: StewartPlatformKinematics = None,
                 parent=None):
        super().__init__(parent)
        self.smc_controller = smc_controller
        self.pulse_calculator = pulse_calculator
        self.kinematics = kinematics
        
        # 位置模式：True=绝对，False=相对
        self.absolute_mode = False
        
        self._init_ui()
    
    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        
        # 模式选择
        mode_group = QGroupBox("位置模式")
        mode_layout = QVBoxLayout()
        
        self.mode_button_group = QButtonGroup(self)
        self.relative_radio = QRadioButton("相对位置")
        self.absolute_radio = QRadioButton("绝对位置")
        self.relative_radio.setChecked(True)
        
        self.mode_button_group.addButton(self.relative_radio, 0)
        self.mode_button_group.addButton(self.absolute_radio, 1)
        self.mode_button_group.buttonClicked.connect(self._on_mode_changed)
        
        mode_layout.addWidget(self.relative_radio)
        mode_layout.addWidget(self.absolute_radio)
        mode_group.setLayout(mode_layout)
        layout.addWidget(mode_group)
        
        # 位置输入
        position_group = QGroupBox("位置设定 (mm)")
        position_layout = QVBoxLayout()
        
        # X轴
        x_layout = QHBoxLayout()
        x_layout.addWidget(QLabel("X:"))
        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setRange(-1000.0, 1000.0)
        self.x_spinbox.setDecimals(3)
        self.x_spinbox.setSuffix(" mm")
        x_layout.addWidget(self.x_spinbox)
        position_layout.addLayout(x_layout)
        
        # Y轴
        y_layout = QHBoxLayout()
        y_layout.addWidget(QLabel("Y:"))
        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setRange(-1000.0, 1000.0)
        self.y_spinbox.setDecimals(3)
        self.y_spinbox.setSuffix(" mm")
        y_layout.addWidget(self.y_spinbox)
        position_layout.addLayout(y_layout)
        
        # Z轴
        z_layout = QHBoxLayout()
        z_layout.addWidget(QLabel("Z:"))
        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setRange(-1000.0, 1000.0)
        self.z_spinbox.setDecimals(3)
        self.z_spinbox.setSuffix(" mm")
        z_layout.addWidget(self.z_spinbox)
        position_layout.addLayout(z_layout)
        
        position_group.setLayout(position_layout)
        layout.addWidget(position_group)
        
        # 控制按钮
        button_layout = QHBoxLayout()
        
        self.execute_btn = QPushButton("执行")
        self.execute_btn.clicked.connect(self._on_execute_clicked)
        button_layout.addWidget(self.execute_btn)
        
        self.stop_btn = QPushButton("停止")
        self.stop_btn.clicked.connect(self._on_stop_clicked)
        button_layout.addWidget(self.stop_btn)
        
        layout.addLayout(button_layout)
        layout.addStretch()
    
    def _on_mode_changed(self, button):
        """模式改变"""
        self.absolute_mode = (button == self.absolute_radio)
    
    def _on_execute_clicked(self):
        """执行按钮点击"""
        if not self.smc_controller or not self.smc_controller.is_connected():
            QMessageBox.warning(
                self, 
                "未连接", 
                "请先连接设备\n\n"
                "在执行运动命令之前，需要先连接到运动控制器。"
            )
            return
        
        if not self.pulse_calculator:
            QMessageBox.warning(
                self, 
                "未初始化", 
                "脉冲计算器未初始化\n\n"
                "请检查系统配置是否正确。"
            )
            return
        
        if not self.kinematics:
            QMessageBox.warning(
                self, 
                "未初始化", 
                "运动学计算器未初始化\n\n"
                "请检查系统配置是否正确。"
            )
            return
        
        try:
            # 获取输入的位置
            x = self.x_spinbox.value()
            y = self.y_spinbox.value()
            z = self.z_spinbox.value()
            
            # 创建目标位姿（旋转角度为0，只做平移）
            target_pose = Pose(x=x, y=y, z=z, rx=0.0, ry=0.0, rz=0.0)
            
            # 转换为铰点面位姿
            platform_pose = self.kinematics.convert_target_pose_to_platform_pose(target_pose)
            
            # 计算Z轴投影位移（6个轴的位移）
            z_displacements = self.kinematics.calculate_z_axis_displacement(platform_pose)
            
            if z_displacements is None:
                QMessageBox.critical(
                    self, 
                    "超出范围", 
                    "目标位置超出可达范围\n\n"
                    "请输入一个在机器人工作空间内的位置。\n\n"
                    "请检查：\n"
                    "• X、Y、Z坐标是否在允许范围内\n"
                    "• 是否超出了机械限位"
                )
                return
            
            # 发送运动命令
            success_count = 0
            for axis in range(6):
                displacement_mm = z_displacements[axis]
                
                # 转换为脉冲数
                if self.absolute_mode:
                    # 绝对模式：需要获取当前位置，计算绝对脉冲数
                    # 这里简化处理：使用相对位移
                    pulses = self.pulse_calculator.mm_to_pulses(displacement_mm)
                    if self.smc_controller.move_relative(axis, pulses):
                        success_count += 1
                else:
                    # 相对模式：直接使用位移
                    pulses = self.pulse_calculator.mm_to_pulses(displacement_mm)
                    if self.smc_controller.move_relative(axis, pulses):
                        success_count += 1
            
            if success_count == 6:
                self.move_started.emit()
                QMessageBox.information(self, "成功", "运动命令已发送")
            else:
                QMessageBox.warning(
                    self, 
                    "部分失败", 
                    f"部分轴命令发送失败\n\n"
                    f"成功：{success_count}/6 个轴\n\n"
                    f"请检查：\n"
                    f"• 设备连接是否正常\n"
                    f"• 控制器是否响应\n"
                    f"• 尝试重新连接设备"
                )
        
        except Exception as e:
            QMessageBox.critical(
                self, 
                "执行失败", 
                f"执行运动时发生错误\n\n"
                f"程序在执行运动命令时遇到问题。\n\n"
                f"错误信息：{str(e)}\n\n"
                f"建议：\n"
                f"• 检查输入参数是否正确\n"
                f"• 确认设备连接正常\n"
                f"• 尝试重新连接设备"
            )
    
    def _on_stop_clicked(self):
        """停止按钮点击"""
        if not self.smc_controller or not self.smc_controller.is_connected():
            return
        
        # 停止所有轴
        for axis in range(6):
            self.smc_controller.stop_move(axis)
        
        QMessageBox.information(self, "停止", "已发送停止命令")
    
    def set_smc_controller(self, smc_controller: SMCController):
        """设置SMC控制器"""
        self.smc_controller = smc_controller
    
    def set_pulse_calculator(self, pulse_calculator: PulseCalculator):
        """设置脉冲计算器"""
        self.pulse_calculator = pulse_calculator
    
    def set_kinematics(self, kinematics: StewartPlatformKinematics):
        """设置运动学计算器"""
        self.kinematics = kinematics
