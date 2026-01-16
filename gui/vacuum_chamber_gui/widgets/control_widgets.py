"""
真空腔体系统控制 - 通用控制组件
Common Control Widgets
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QLabel, QDoubleSpinBox, QFrame
)
from PyQt5.QtCore import Qt, pyqtSignal


class ControlButton(QPushButton):
    """通用控制按钮"""
    
    def __init__(self, text: str, role: str = "normal", icon: str = None, parent=None):
        if icon:
            super().__init__(f"{icon} {text}", parent)
        else:
            super().__init__(text, parent)
            
        self.setProperty("role", role)
        self.setCursor(Qt.PointingHandCursor)


class ParameterInput(QWidget):
    """参数输入组件"""
    
    value_changed = pyqtSignal(float)
    
    def __init__(self, label: str, unit: str = "", 
                 min_val: float = -9999.0, max_val: float = 9999.0, 
                 precision: float = 0.01, parent=None):
        super().__init__(parent)
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(8)
        
        if label:
            layout.addWidget(QLabel(label))
            
        self.spin_box = QDoubleSpinBox()
        self.spin_box.setRange(min_val, max_val)
        self.spin_box.setSingleStep(precision)
        
        # 设置小数位数
        decimals = 0
        if precision < 1:
            decimals = len(str(precision).split(".")[1])
        self.spin_box.setDecimals(decimals)
        
        layout.addWidget(self.spin_box)
        
        if unit:
            unit_label = QLabel(unit)
            unit_label.setProperty("role", "unit")
            layout.addWidget(unit_label)
            
        self.spin_box.valueChanged.connect(self.value_changed.emit)
        
    def value(self) -> float:
        return self.spin_box.value()
        
    def set_value(self, val: float):
        self.spin_box.setValue(val)


class AxisControlGroup(QFrame):
    """单轴控制组"""
    
    move_requested = pyqtSignal(str, float, bool)  # axis_id, value, is_absolute
    stop_requested = pyqtSignal(str)  # axis_id
    
    def __init__(self, axis_id: str, name: str, unit: str, 
                 min_val: float, max_val: float, precision: float, parent=None):
        super().__init__(parent)
        self.axis_id = axis_id
        self.setProperty("role", "panel")
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(12, 8, 12, 8)
        layout.setSpacing(16)
        
        # 轴名称
        name_label = QLabel(name)
        name_label.setFixedWidth(60)
        name_label.setProperty("role", "subtitle")
        layout.addWidget(name_label)
        
        # 当前位置显示
        self.pos_display = QLabel("0.000")
        self.pos_display.setFixedWidth(80)
        self.pos_display.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.pos_display.setProperty("role", "value")
        layout.addWidget(self.pos_display)
        
        layout.addWidget(QLabel(unit))
        
        # 目标位置输入
        self.target_input = ParameterInput("", unit, min_val, max_val, precision)
        layout.addWidget(self.target_input)
        
        # 运动按钮
        self.btn_abs = ControlButton("绝对运动", role="move_absolute")
        self.btn_rel = ControlButton("相对运动", role="move_relative")
        self.btn_stop = ControlButton("停止", role="stop")
        
        layout.addWidget(self.btn_abs)
        layout.addWidget(self.btn_rel)
        layout.addWidget(self.btn_stop)
        
        # 连接信号
        self.btn_abs.clicked.connect(self._on_abs_move)
        self.btn_rel.clicked.connect(self._on_rel_move)
        self.btn_stop.clicked.connect(lambda: self.stop_requested.emit(self.axis_id))
        
    def _on_abs_move(self):
        self.move_requested.emit(self.axis_id, self.target_input.value(), True)
        
    def _on_rel_move(self):
        self.move_requested.emit(self.axis_id, self.target_input.value(), False)
        
    def update_position(self, pos: float):
        self.pos_display.setText(f"{pos:.3f}")
