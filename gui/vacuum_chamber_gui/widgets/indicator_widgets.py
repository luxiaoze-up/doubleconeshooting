"""
真空腔体系统控制 - 通用指示器组件
Common Indicator Widgets
"""

from PyQt5.QtWidgets import (
    QWidget, QLabel, QHBoxLayout, QProgressBar
)
from PyQt5.QtCore import Qt
from numbers import Number


class StatusIndicator(QLabel):
    """状态指示灯"""
    
    def __init__(self, status: str = "offline", size: int = 12, parent=None):
        super().__init__("●", parent)
        self.setProperty("status", status)
        self.setStyleSheet(f"font-size: {size}px;")
        
    def set_status(self, status: str):
        """
        设置状态
        status: 'ok', 'warn', 'error', 'offline'
        """
        self.setProperty("status", status)
        self.style().unpolish(self)
        self.style().polish(self)


class ValueDisplay(QWidget):
    """数值显示组件"""
    
    def __init__(self, value: str = "---", unit: str = "", parent=None):
        super().__init__(parent)
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        
        self.value_label = QLabel(value)
        self.value_label.setProperty("role", "value")
        layout.addWidget(self.value_label)
        
        if unit:
            unit_label = QLabel(unit)
            unit_label.setProperty("role", "unit")
            layout.addWidget(unit_label)
            
    def set_value(self, value):
        """设置显示值，自动转换为字符串"""
        text = "---"
        if value is None:
            text = "---"
        elif isinstance(value, bool):
            text = "开" if value else "关"
        elif isinstance(value, Number):
            try:
                text = f"{float(value):.3e}"
            except Exception:
                text = str(value)
        else:
            text = str(value)
        self.value_label.setText(text)
        
    def set_large(self, is_large: bool):
        if is_large:
            self.value_label.setStyleSheet("font-size: 24px; font-weight: 700;")
        else:
            self.value_label.setStyleSheet("")


class ProgressIndicator(QWidget):
    """进度指示器"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        self.bar = QProgressBar()
        self.bar.setTextVisible(False)
        self.bar.setFixedHeight(4)
        self.bar.setStyleSheet("""
            QProgressBar {
                border: none;
                background-color: #1c3146;
                border-radius: 2px;
            }
            QProgressBar::chunk {
                background-color: #1f6feb;
                border-radius: 2px;
            }
        """)
        layout.addWidget(self.bar)
        
    def set_progress(self, value: int):
        self.bar.setValue(value)
