"""
旋转角度控制控件
Rotation Control Widget
ThetaX/ThetaY/ThetaZ角度控制
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QLabel, QDoubleSpinBox, QPushButton, QRadioButton,
    QButtonGroup, QMessageBox, QComboBox
)
from PyQt5.QtCore import Qt, pyqtSignal

# 使用完整包路径导入（开发和打包环境一致）
from gui.six_dof_debug_gui.hardware.smc_controller import SMCController
from gui.six_dof_debug_gui.hardware.pulse_calculator import PulseCalculator
from gui.six_dof_debug_gui.kinematics.stewart_kinematics import StewartPlatformKinematics, Pose


class RotationControlWidget(QWidget):
    """旋转角度控制控件"""
    
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
        
        # 角度单位：True=度，False=弧度
        self.angle_unit_degrees = True
        
        # 位置模式：True=绝对，False=相对
        self.absolute_mode = False
        
        self._init_ui()
    
    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        
        # 角度单位选择
        unit_group = QGroupBox("角度单位")
        unit_layout = QHBoxLayout()
        
        self.unit_combo = QComboBox()
        self.unit_combo.addItems(["度 (°)", "弧度 (rad)"])
        self.unit_combo.currentIndexChanged.connect(self._on_unit_changed)
        unit_layout.addWidget(self.unit_combo)
        unit_layout.addStretch()
        unit_group.setLayout(unit_layout)
        layout.addWidget(unit_group)
        
        # 模式选择
        mode_group = QGroupBox("旋转模式")
        mode_layout = QVBoxLayout()
        
        self.mode_button_group = QButtonGroup(self)
        self.relative_radio = QRadioButton("相对旋转角")
        self.absolute_radio = QRadioButton("绝对旋转角")
        self.relative_radio.setChecked(True)
        
        self.mode_button_group.addButton(self.relative_radio, 0)
        self.mode_button_group.addButton(self.absolute_radio, 1)
        self.mode_button_group.buttonClicked.connect(self._on_mode_changed)
        
        mode_layout.addWidget(self.relative_radio)
        mode_layout.addWidget(self.absolute_radio)
        mode_group.setLayout(mode_layout)
        layout.addWidget(mode_group)
        
        # 角度输入
        angle_group = QGroupBox("旋转角度设定")
        angle_layout = QVBoxLayout()
        
        # ThetaX (Roll)
        tx_layout = QHBoxLayout()
        tx_layout.addWidget(QLabel("ThetaX (Roll):"))
        self.tx_spinbox = QDoubleSpinBox()
        self.tx_spinbox.setRange(-180.0, 180.0)
        self.tx_spinbox.setDecimals(3)
        self.tx_spinbox.setSuffix(" °")
        tx_layout.addWidget(self.tx_spinbox)
        angle_layout.addLayout(tx_layout)
        
        # ThetaY (Pitch)
        ty_layout = QHBoxLayout()
        ty_layout.addWidget(QLabel("ThetaY (Pitch):"))
        self.ty_spinbox = QDoubleSpinBox()
        self.ty_spinbox.setRange(-180.0, 180.0)
        self.ty_spinbox.setDecimals(3)
        self.ty_spinbox.setSuffix(" °")
        ty_layout.addWidget(self.ty_spinbox)
        angle_layout.addLayout(ty_layout)
        
        # ThetaZ (Yaw)
        tz_layout = QHBoxLayout()
        tz_layout.addWidget(QLabel("ThetaZ (Yaw):"))
        self.tz_spinbox = QDoubleSpinBox()
        self.tz_spinbox.setRange(-180.0, 180.0)
        self.tz_spinbox.setDecimals(3)
        self.tz_spinbox.setSuffix(" °")
        tz_layout.addWidget(self.tz_spinbox)
        angle_layout.addLayout(tz_layout)
        
        angle_group.setLayout(angle_layout)
        layout.addWidget(angle_group)
        
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
    
    def _on_unit_changed(self, index):
        """角度单位改变"""
        self.angle_unit_degrees = (index == 0)
        
        # 更新范围和后缀
        if self.angle_unit_degrees:
            # 度：-180 到 180
            self.tx_spinbox.setRange(-180.0, 180.0)
            self.ty_spinbox.setRange(-180.0, 180.0)
            self.tz_spinbox.setRange(-180.0, 180.0)
            self.tx_spinbox.setSuffix(" °")
            self.ty_spinbox.setSuffix(" °")
            self.tz_spinbox.setSuffix(" °")
        else:
            # 弧度：-π 到 π
            import math
            self.tx_spinbox.setRange(-math.pi, math.pi)
            self.ty_spinbox.setRange(-math.pi, math.pi)
            self.tz_spinbox.setRange(-math.pi, math.pi)
            self.tx_spinbox.setSuffix(" rad")
            self.ty_spinbox.setSuffix(" rad")
            self.tz_spinbox.setSuffix(" rad")
    
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
            # 获取输入的角度
            tx = self.tx_spinbox.value()
            ty = self.ty_spinbox.value()
            tz = self.tz_spinbox.value()
            
            # 转换为度（如果输入是弧度）
            if not self.angle_unit_degrees:
                import math
                tx = math.degrees(tx)
                ty = math.degrees(ty)
                tz = math.degrees(tz)
            
            # 创建目标位姿（位置为0，只做旋转）
            target_pose = Pose(x=0.0, y=0.0, z=0.0, rx=tx, ry=ty, rz=tz)
            
            # 转换为铰点面位姿
            platform_pose = self.kinematics.convert_target_pose_to_platform_pose(target_pose)
            
            # 计算Z轴投影位移（6个轴的位移）
            z_displacements = self.kinematics.calculate_z_axis_displacement(platform_pose)
            
            if z_displacements is None:
                QMessageBox.critical(
                    self, 
                    "超出范围", 
                    "目标角度超出可达范围\n\n"
                    "请输入一个在机器人工作空间内的角度。\n\n"
                    "请检查：\n"
                    "• Tx、Ty、Tz角度是否在允许范围内\n"
                    "• 是否超出了机械限位"
                )
                return
            
            # 发送运动命令
            success_count = 0
            for axis in range(6):
                displacement_mm = z_displacements[axis]
                
                # 转换为脉冲数（相对模式）
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
