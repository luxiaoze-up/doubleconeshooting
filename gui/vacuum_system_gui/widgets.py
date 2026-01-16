"""
真空系统 GUI 组件
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QFrame, QGridLayout,
    QPushButton, QScrollArea, QDialog, QGroupBox
)
from PyQt5.QtCore import Qt, QTimer, QPropertyAnimation, QEasingCurve, pyqtProperty
from PyQt5.QtGui import QColor, QPainter, QBrush, QPen, QFont

from config import ValveActionState, BlinkColors, UI_CONFIG
from styles import COLORS, STATUS_INDICATOR_STYLES


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


class StatusIndicator(QWidget):
    """状态指示器"""
    
    def __init__(self, label: str = "", parent=None):
        super().__init__(parent)
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        
        # 指示灯
        self._light = QFrame()
        self._light.setFixedSize(20, 20)
        self._light.setStyleSheet(STATUS_INDICATOR_STYLES['off'])
        layout.addWidget(self._light)
        
        # 标签
        if label:
            self._label = QLabel(label)
            layout.addWidget(self._label)
            
        self._state = False
        self._is_blinking = False
        self._blink_timer = QTimer(self)
        self._blink_timer.timeout.connect(self._toggle_blink_internal)
        self._blink_phase = False
        self._blink_type = "normal"  # normal, timeout
        
    def set_state(self, state: bool, is_alarm: bool = False):
        """设置状态"""
        self._state = state
        self.stop_blink()
        
        if is_alarm:
            if state:
                self._light.setStyleSheet(STATUS_INDICATOR_STYLES['error'])
            else:
                self._light.setStyleSheet(STATUS_INDICATOR_STYLES['off'])
        else:
            if state:
                self._light.setStyleSheet(STATUS_INDICATOR_STYLES['on'])
            else:
                self._light.setStyleSheet(STATUS_INDICATOR_STYLES['off'])
                
    def start_blink(self, blink_type: str = "normal"):
        """开始闪烁
        
        Args:
            blink_type: "normal" 蓝色闪烁 (正在操作), "timeout" 红色闪烁 (超时)
        """
        self._blink_type = blink_type
        self._is_blinking = True
        if not self._blink_timer.isActive():
            self._blink_phase = False
            self._blink_timer.start(UI_CONFIG['blink_interval_ms'])
        
    def stop_blink(self):
        """停止闪烁"""
        self._is_blinking = False
        self._blink_timer.stop()
        
    def toggle_blink(self):
        """外部触发切换闪烁状态（用于统一闪烁节奏）"""
        if self._is_blinking:
            self._toggle_blink_internal()
        
    def _toggle_blink_internal(self):
        """切换闪烁状态"""
        self._blink_phase = not self._blink_phase
        
        if self._blink_type == "timeout":
            if self._blink_phase:
                self._light.setStyleSheet(STATUS_INDICATOR_STYLES['blink_red_on'])
            else:
                self._light.setStyleSheet(STATUS_INDICATOR_STYLES['blink_red_off'])
        else:
            if self._blink_phase:
                self._light.setStyleSheet(STATUS_INDICATOR_STYLES['blink_blue_on'])
            else:
                self._light.setStyleSheet(STATUS_INDICATOR_STYLES['blink_blue_off'])


class ValueDisplay(QWidget):
    """数值显示组件 - 模拟工业仪表发光管效果"""
    
    def __init__(self, label: str, unit: str = "", parent=None):
        super().__init__(parent)
        
        self._unit = unit  # 保存单位信息，用于判断是否为真空计
        
        layout = QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(2)
        
        self._label = QLabel(label)
        self._label.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px; text-transform: uppercase;")
        self._label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self._label)
        
        # 数值显示区域
        self._value_frame = QFrame()
        self._value_frame.setStyleSheet(f"""
            QFrame {{
                background-color: #000000;
                border: 1px solid {COLORS['border']};
                border-radius: 4px;
                padding: 4px 6px;
            }}
        """)
        value_layout = QHBoxLayout(self._value_frame)
        value_layout.setContentsMargins(4, 2, 4, 2)
        value_layout.setSpacing(3)
        
        # 根据单位决定字体大小：真空计（Pa）使用较小字体以容纳科学计数法
        is_vacuum_gauge = (unit == "Pa")
        font_size = 11 if is_vacuum_gauge else 14  # 真空计使用 11px，其他使用 14px
        
        self._value_label = QLabel("--")
        self._value_label.setStyleSheet(f"""
            color: {COLORS['primary']};
            font-family: 'Consolas', 'Courier New', monospace;
            font-weight: bold;
            font-size: {font_size}px;
        """)
        self._value_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        self._value_label.setMinimumWidth(75 if is_vacuum_gauge else 70)  # 真空计需要更宽以显示科学计数法
        value_layout.addWidget(self._value_label, 1)  # 给数值标签分配更多空间
        
        if unit:
            self._unit_label = QLabel(unit)
            unit_font_size = 9 if is_vacuum_gauge else 10  # 真空计单位字体也稍小
            self._unit_label.setStyleSheet(f"color: {COLORS['primary']}; font-size: {unit_font_size}px;")
            self._unit_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
            value_layout.addWidget(self._unit_label, 0)  # 单位标签不拉伸
            
        layout.addWidget(self._value_frame)
            
    def set_value(self, value, precision: int = 2):
        """设置值 - 真空计使用科学计数法，其他智能格式化"""
        # 处理 None 或无效值
        if value is None:
            self._value_label.setText("--")
            return
            
        # 处理非数值类型
        if not isinstance(value, (int, float)):
            try:
                value = float(value)
            except (ValueError, TypeError):
                self._value_label.setText("--")
                return
        
        # 处理 NaN 或无穷大
        import math
        if math.isnan(value) or math.isinf(value):
            self._value_label.setText("--")
            return
        
        # 真空计（Pa单位）始终使用科学计数法
        if self._unit == "Pa":
            if value == 0:
                self._value_label.setText("0.00e+00")
            else:
                self._value_label.setText(f"{value:.2e}")
        else:
            # 其他单位：智能格式化显示
            if value == 0:
                # 0 值直接显示为 0
                self._value_label.setText("0.00")
            elif abs(value) >= 1000:
                # 大于等于 1000，使用科学计数法
                self._value_label.setText(f"{value:.2e}")
            elif abs(value) < 0.01 and value != 0:
                # 非常小的值（小于 0.01 且不为 0），使用科学计数法
                self._value_label.setText(f"{value:.2e}")
            else:
                # 正常范围的值，使用普通小数格式
                self._value_label.setText(f"{value:.{precision}f}")


class PrerequisitePanel(QGroupBox):
    """操作先决条件面板 - 单列条目式布局，支持滚动"""
    
    def __init__(self, title: str = "操作先决条件", parent=None):
        super().__init__(title, parent)
        
        # 使用与自动/手动操作区类似的边界线样式
        self.setStyleSheet(f"""
            QGroupBox {{
                background-color: rgba(0, 230, 118, 0.05);
                border: 2px solid {COLORS['success']};
                border-radius: 12px;
            }}
            QGroupBox::title {{
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 15px;
                padding: 0 10px;
                color: {COLORS['success']};
                font-weight: bold;
                font-size: 14px;
            }}
        """)
        self.setMinimumHeight(80)
        
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(12, 20, 12, 10)
        main_layout.setSpacing(6)
        
        # 操作名称标签（显示在标题下方）
        self._operation_label = QLabel("")
        self._operation_label.setStyleSheet(f"color: {COLORS['warning']}; font-weight: bold; font-size: 11px;")
        main_layout.addWidget(self._operation_label)
        
        # 滚动区域
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setStyleSheet("background: transparent;")
        
        content = QWidget()
        content.setStyleSheet("background: transparent;")
        # 改为单列布局，每行一条，不强制铺满栏目
        self._list_layout = QVBoxLayout(content)
        self._list_layout.setContentsMargins(0, 5, 0, 5)
        self._list_layout.setSpacing(8)
        # 不设置 stretch，让条目按内容自然排列，不强制铺满
        
        scroll.setWidget(content)
        main_layout.addWidget(scroll)
        
        self._items = []
        self.clear_prerequisites()

    def set_operation_name(self, name: str):
        self._operation_label.setText(f" » {name}" if name else "")
        
    def set_prerequisites(self, prerequisites: list, operation_name: str = ""):
        self.clear_prerequisites()
        self.set_operation_name(operation_name)
                
        if not prerequisites:
            # 修改：不调用 clear，因为我们要自定义 placeholder
            self._remove_all()
            placeholder = QLabel("✓ 当前操作无特殊条件")
            placeholder.setStyleSheet(f"color: {COLORS['success']}; font-size: 12px; font-weight: bold;")
            placeholder.setAlignment(Qt.AlignCenter)
            self._list_layout.addWidget(placeholder)
            return
            
        # 移除默认占位符
        self._remove_all()
        
        # 单列布局，每行一条，依次往下排列
        # 条目不设置 stretch，按内容自然排列，不强制铺满栏目
        for prereq in prerequisites:
            item = PrerequisiteItem(
                prereq['description'], 
                prereq.get('is_met', False),
                prereq.get('current_value'),
                prereq.get('expected_value')
            )
            self._items.append(item)
            # 不设置 stretch，让条目按内容自然显示
            self._list_layout.addWidget(item)
        
        # 在最后添加 stretch，让条目靠上排列，不铺满栏目
        self._list_layout.addStretch()
            
    def clear_prerequisites(self):
        self._remove_all()
        self._operation_label.setText("")
        
        # 优化占位符显示
        placeholder = QLabel("请将鼠标悬停在操作按钮上查看先决条件")
        placeholder.setStyleSheet(f"color: {COLORS['text_secondary']}; font-style: italic; background: transparent; font-size: 11px;")
        placeholder.setAlignment(Qt.AlignCenter)
        placeholder.setMinimumHeight(40)
        self._list_layout.addWidget(placeholder)
        # 添加 stretch，让占位符不铺满栏目
        self._list_layout.addStretch()

    def _remove_all(self):
        """内部方法：彻底清空布局"""
        while self._list_layout.count():
            item = self._list_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self._items = []


class PrerequisiteItem(QFrame):
    """单个先决条件项 - 条目式显示"""
    
    def __init__(self, description: str, is_met: bool, current_value=None, expected_value=None, parent=None):
        super().__init__(parent)
        
        # 使用唯一 objectName 避免样式冲突
        self.setObjectName("prereqItem")
        
        # 根据状态设置背景色 - 条目式，更紧凑
        if is_met:
            bg_color = "rgba(76, 175, 80, 0.25)"
            border_color = COLORS['success']
            text_color = "#E8F5E9"  # 浅绿色文字
        else:
            bg_color = "rgba(244, 67, 54, 0.25)"
            border_color = COLORS['error']
            text_color = "#FFEBEE"  # 浅红色文字
        
        self.setStyleSheet(f"""
            #prereqItem {{
                background-color: {bg_color};
                border: 1px solid {border_color};
                border-radius: 4px;
            }}
        """)
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 6, 8, 6)
        layout.setSpacing(8)
        
        # 状态图标
        status_label = QLabel("✓" if is_met else "✗")
        status_label.setFixedWidth(16)
        status_label.setStyleSheet(f"""
            color: {COLORS['success'] if is_met else COLORS['error']};
            font-weight: bold;
            font-size: 14px;
            background: transparent;
        """)
        layout.addWidget(status_label)
        
        # 描述和值的容器 - 单列紧凑布局
        text_layout = QVBoxLayout()
        text_layout.setSpacing(2)
        text_layout.setContentsMargins(0, 0, 0, 0)
        
        # 描述
        desc_label = QLabel(description)
        desc_label.setWordWrap(True)
        desc_label.setStyleSheet(f"""
            color: {text_color};
            font-size: 11px;
            font-weight: {'normal' if is_met else 'bold'};
            background: transparent;
        """)
        text_layout.addWidget(desc_label)
        
        # 当前值/期望值（如果有）
        if current_value is not None or expected_value is not None:
            value_parts = []
            if current_value is not None:
                value_parts.append(f"当前: {current_value}")
            if expected_value is not None:
                value_parts.append(f"期望: {expected_value}")
            
            value_text = "  |  ".join(value_parts)
            value_label = QLabel(value_text)
            value_label.setStyleSheet(f"""
                color: {'#A5D6A7' if is_met else '#FFAB91'};
                font-size: 10px;
                background: transparent;
            """)
            text_layout.addWidget(value_label)
        
        layout.addLayout(text_layout, 1)


class DeviceIcon(QWidget):
    """设备图标（用于数字孪生页面）"""
    
    def __init__(self, device_type: str, device_name: str, parent=None):
        super().__init__(parent)
        
        self.device_type = device_type
        self.device_name = device_name
        self.is_active = False
        self.action_state = ValveActionState.IDLE
        
        self.setFixedSize(80, 80)
        
        self._blink_timer = QTimer(self)
        self._blink_timer.timeout.connect(self._toggle_blink)
        self._blink_phase = False
        
    def set_state(self, is_active: bool, action_state: int = 0):
        """设置设备状态"""
        self.is_active = is_active
        self.action_state = action_state
        
        # 处理闪烁
        if action_state in [ValveActionState.OPENING, ValveActionState.CLOSING]:
            if not self._blink_timer.isActive():
                self._blink_timer.start(UI_CONFIG['blink_interval_ms'])
        elif action_state in [ValveActionState.OPEN_TIMEOUT, ValveActionState.CLOSE_TIMEOUT]:
            if not self._blink_timer.isActive():
                # 超时时使用更快的闪烁间隔（正常间隔的60%）
                fast_interval = int(UI_CONFIG['blink_interval_ms'] * 0.6)
                self._blink_timer.start(fast_interval)
        else:
            self._blink_timer.stop()
            
        self.update()
        
    def _toggle_blink(self):
        """切换闪烁"""
        self._blink_phase = not self._blink_phase
        self.update()
        
    def paintEvent(self, event):
        """绘制设备图标"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # 确定颜色
        if self.action_state in [ValveActionState.OPEN_TIMEOUT, ValveActionState.CLOSE_TIMEOUT]:
            # 超时 - 红色闪烁
            if self._blink_phase:
                color = QColor(BlinkColors.TIMEOUT_ON)
            else:
                color = QColor(BlinkColors.TIMEOUT_OFF)
        elif self.action_state in [ValveActionState.OPENING, ValveActionState.CLOSING]:
            # 操作中 - 蓝色闪烁
            if self._blink_phase:
                color = QColor(BlinkColors.OPERATING_ON)
            else:
                color = QColor(BlinkColors.OPERATING_OFF)
        elif self.is_active:
            color = QColor(BlinkColors.NORMAL_OPEN)
        else:
            color = QColor(BlinkColors.NORMAL_CLOSE)
            
        # 绘制背景圆
        painter.setBrush(QBrush(color))
        painter.setPen(QPen(color.darker(120), 2))
        
        margin = 5
        rect = self.rect().adjusted(margin, margin, -margin, -margin)
        
        if self.device_type == "pump":
            # 泵 - 圆形
            painter.drawEllipse(rect)
        elif self.device_type == "gate_valve":
            # 闸板阀 - 菱形
            center = rect.center()
            size = min(rect.width(), rect.height()) // 2
            points = [
                center + self._point(0, -size),
                center + self._point(size, 0),
                center + self._point(0, size),
                center + self._point(-size, 0)
            ]
            from PyQt5.QtGui import QPolygon
            from PyQt5.QtCore import QPoint
            polygon = QPolygon([QPoint(int(p.x()), int(p.y())) for p in points])
            painter.drawPolygon(polygon)
        elif self.device_type == "valve":
            # 电磁阀/放气阀 - 三角形
            center = rect.center()
            size = min(rect.width(), rect.height()) // 2
            points = [
                center + self._point(0, -size),
                center + self._point(size, size),
                center + self._point(-size, size)
            ]
            from PyQt5.QtGui import QPolygon
            from PyQt5.QtCore import QPoint
            polygon = QPolygon([QPoint(int(p.x()), int(p.y())) for p in points])
            painter.drawPolygon(polygon)
        else:
            # 默认 - 矩形
            painter.drawRoundedRect(rect, 6, 6)
            
        # 绘制名称
        painter.setPen(QPen(Qt.white))
        painter.drawText(self.rect(), Qt.AlignCenter, self.device_name)
        
    def _point(self, x, y):
        """辅助方法创建点"""
        from PyQt5.QtCore import QPointF
        return QPointF(x, y)


class AlarmPopup(QDialog):
    """报警弹窗 - 使用 QDialog 确保交互可靠"""
    
    def __init__(self, alarm_data: dict, parent=None):
        super().__init__(parent)
        self.setObjectName("alarmPopup")
        self.setFixedSize(420, 260)
        self.setModal(False)  # 非模态，不阻塞主界面
        
        # 使用 Dialog 标志 + 无边框 + 置顶，但不阻止主窗口关闭
        self.setWindowFlags(Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setAttribute(Qt.WA_DeleteOnClose)
        self.setAttribute(Qt.WA_QuitOnClose, False)  # 关闭此弹窗不会退出应用
        
        # 样式 - 增强边框效果代替阴影（阴影会干扰点击事件）
        self.setStyleSheet(f"""
            QDialog#alarmPopup {{
                background-color: #1A1F2E;
                border: 4px solid {COLORS['error']};
                border-radius: 12px;
            }}
            QLabel {{
                background: transparent;
                color: #ECEFF1;
            }}
        """)
        
        layout = QVBoxLayout(self)
        layout.setContentsMargins(30, 25, 30, 25)
        layout.setSpacing(15)
        
        # 标题
        title = QLabel("⚠️ 系统报警")
        # 设置支持 Unicode 符号的字体，确保图标正确显示
        title_font = get_unicode_font(24)
        title_font.setBold(True)
        title.setFont(title_font)
        title.setStyleSheet(f"color: {COLORS['error']}; font-weight: bold;")
        title.setAlignment(Qt.AlignCenter)
        layout.addWidget(title)
        
        # 故障内容
        desc = alarm_data.get('description', '未知报警')
        device = alarm_data.get('device_name', '未知设备')
        
        content = QLabel(desc)
        content.setStyleSheet("color: #FFFFFF; font-size: 17px; font-weight: bold;")
        content.setAlignment(Qt.AlignCenter)
        content.setWordWrap(True)
        layout.addWidget(content)
        
        dev_lbl = QLabel(f"来源: {device}")
        dev_lbl.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 13px;")
        dev_lbl.setAlignment(Qt.AlignCenter)
        layout.addWidget(dev_lbl)
        
        layout.addStretch()
        
        # 确认按钮
        self.btn_ack = QPushButton("确认 (已知晓)")
        self.btn_ack.setCursor(Qt.PointingHandCursor)
        self.btn_ack.setMinimumHeight(50)
        self.btn_ack.setStyleSheet(f"""
            QPushButton {{
                background-color: {COLORS['error']};
                color: white;
                border: none;
                border-radius: 10px;
                font-size: 16px;
                font-weight: bold;
            }}
            QPushButton:hover {{
                background-color: #FF5252;
                border: 2px solid white;
            }}
            QPushButton:pressed {{
                background-color: #B71C1C;
            }}
        """)
        self.btn_ack.clicked.connect(self._on_acknowledge)
        layout.addWidget(self.btn_ack)
        
        self._alarm_code = alarm_data.get('alarm_code', 0)
        self._callback = None
        
    def set_acknowledge_callback(self, callback):
        self._callback = callback
        
    def _on_acknowledge(self):
        """确认按钮点击"""
        if self._callback:
            self._callback(self._alarm_code)
        self.accept()  # 使用 QDialog 的标准关闭方式
    
    def keyPressEvent(self, event):
        """按 ESC 或 Enter 键也可关闭弹窗"""
        if event.key() in (Qt.Key_Escape, Qt.Key_Return, Qt.Key_Enter):
            self._on_acknowledge()
        else:
            super().keyPressEvent(event)
        
    def show_centered(self, parent_widget):
        """强制居中并显示 - 确保在任何情况下都可交互"""
        if parent_widget:
            window = parent_widget.window()
            
            # 如果主窗口被最小化，先恢复它
            if window.isMinimized():
                window.showNormal()
            
            # 激活主窗口（确保应用程序在前台）
            window.activateWindow()
            window.raise_()
            
            # 使用 frameGeometry 获取包含标题栏的完整窗口区域
            geom = window.frameGeometry()
            x = geom.x() + (geom.width() - self.width()) // 2
            y = geom.y() + (geom.height() - self.height()) // 2
            self.move(x, y)
        
        # 先显示再置顶
        self.show()
        
        # 强制置顶并激活
        self.setWindowFlags(self.windowFlags() | Qt.WindowStaysOnTopHint)
        self.show()  # 修改 windowFlags 后需要重新 show
        self.raise_()
        self.activateWindow()
        
        # 确保按钮可以接收焦点
        self.btn_ack.setFocus()
