"""
真空系统数字孪生页面

功能:
- 使用 QGraphicsView 可视化显示真空系统拓扑结构
- 管道连线显示
- 阀门动作闪烁效果
- 实时状态更新
- 右键菜单控制（手动模式）
- 自动/手动模式切换
- 自动模式下手动操作联锁屏蔽
"""

from datetime import datetime
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QFrame, QLabel, QGridLayout,
    QMenu, QMessageBox, QPushButton, QGraphicsView, QGraphicsScene,
    QGraphicsItem, QGraphicsRectItem, QGraphicsEllipseItem,
    QGraphicsLineItem, QGraphicsTextItem, QGraphicsPolygonItem,
    QGroupBox, QSplitter, QCheckBox
)
from PyQt5.QtCore import Qt, QTimer, QRectF, QPointF, QPoint, QObject, QEvent
from PyQt5.QtGui import QColor, QPainter, QPen, QBrush, QFont, QPainterPath, QPolygonF

from config import (
    ValveActionState, BlinkColors, UI_CONFIG, OperationMode, SystemState,
    OPERATION_PREREQUISITES, AUTO_BRANCH_THRESHOLDS, WATER_CHANNELS, AIR_SUPPLY,
    AUTO_MOLECULAR_PUMP_CONFIG, AUTO_SEQUENCE_STEPS
)
from styles import COLORS, MODE_AREA_STYLES
from widgets import get_unicode_font
from widgets import PrerequisitePanel, StatusIndicator
from tango_worker import VacuumTangoWorker
from utils.prerequisite_checker import PrerequisiteChecker
from utils.logger import get_logger

logger = get_logger(__name__)


class TwinDeviceItem(QGraphicsItem):
    """数字孪生设备图形项"""
    
    def __init__(self, name, x, y, w, h, dev_type="pump", parent_widget=None):
        super().__init__()
        self.name = name
        self.dev_type = dev_type
        self.rect = QRectF(x, y, w, h)
        self.is_on = False
        self.action_state = 0
        self.hz_val = 0
        self.attr_name = None
        self._parent_widget = parent_widget
        self._blink_phase = False
        self._pending_click_pos = None  # 用于区分单击和双击
        
        self.setAcceptHoverEvents(True)
        self.setFlag(QGraphicsItem.ItemIsSelectable)
        
        # 设备名称标签 (下方)
        self.lbl_name = QGraphicsTextItem(self.name, self)
        self.lbl_name.setDefaultTextColor(Qt.white)
        self.lbl_name.setFont(QFont("Microsoft YaHei", 8))
        # 居中对齐处理
        name_w = self.lbl_name.boundingRect().width()
        self.lbl_name.setPos(x + (w - name_w) / 2, y + h + 2)
        
        # 状态标签 (右侧)
        self.lbl_status = QGraphicsTextItem("关闭", self)
        self.lbl_status.setDefaultTextColor(QColor("#888"))
        self.lbl_status.setFont(QFont("Microsoft YaHei", 7))
        self.lbl_status.setPos(x + w + 2, y - 5)
        
        # 频率标签 (右侧下方, 仅泵)
        self.lbl_hz = None
        if "pump" in dev_type and "valve" not in dev_type:
            self.lbl_hz = QGraphicsTextItem("0Hz", self)
            self.lbl_hz.setDefaultTextColor(QColor("#00E5FF"))
            self.lbl_hz.setFont(QFont("Microsoft YaHei", 7))
            self.lbl_hz.setPos(x + w + 2, y + 8)
            
    def boundingRect(self):
        # 包含标签在内的包围盒
        return self.rect.adjusted(-20, -5, 40, 20)
        
    def get_color(self) -> QColor:
        """获取当前颜色"""
        if self.action_state in [ValveActionState.OPEN_TIMEOUT, ValveActionState.CLOSE_TIMEOUT]:
            return QColor(BlinkColors.TIMEOUT_ON if self._blink_phase else BlinkColors.TIMEOUT_OFF)
        elif self.action_state in [ValveActionState.OPENING, ValveActionState.CLOSING]:
            return QColor(BlinkColors.OPERATING_ON if self._blink_phase else BlinkColors.OPERATING_OFF)
        elif self.is_on:
            return QColor(BlinkColors.NORMAL_OPEN)
        else:
            return QColor(BlinkColors.NORMAL_CLOSE)
        
    def paint(self, painter, option, widget):
        painter.setRenderHint(QPainter.Antialiasing)
        
        color = self.get_color()
        pen = QPen(color.darker(120), 2)
        brush = QBrush(color)
        painter.setPen(pen)
        painter.setBrush(brush)
        
        if self.dev_type == "valve":
            cx, cy = self.rect.center().x(), self.rect.center().y()
            w, h = self.rect.width(), self.rect.height()
            
            if "闸板" in self.name:
                # ============================================
                # 闸板阀 (Gate Valve) - P&ID标准符号
                # 两个三角形对尖，中间有闸板线
                # ============================================
                path = QPainterPath()
                # 上三角
                path.moveTo(self.rect.left(), self.rect.top())
                path.lineTo(self.rect.right(), self.rect.top())
                path.lineTo(cx, cy)
                path.closeSubpath()
                # 下三角
                path.moveTo(self.rect.left(), self.rect.bottom())
                path.lineTo(self.rect.right(), self.rect.bottom())
                path.lineTo(cx, cy)
                path.closeSubpath()
                painter.drawPath(path)
                # 闸板线（中间竖线）
                painter.setPen(QPen(Qt.white, 2))
                painter.drawLine(int(cx), int(cy - h*0.35), int(cx), int(cy + h*0.35))
                
            elif "电磁" in self.name:
                # ============================================
                # 电磁阀 (Solenoid Valve) - 带线圈方框
                # 两个三角形 + 顶部线圈标记
                # ============================================
                path = QPainterPath()
                # 左三角
                path.moveTo(self.rect.left(), self.rect.top() + 8)
                path.lineTo(self.rect.left(), self.rect.bottom() - 8)
                path.lineTo(cx, cy)
                path.closeSubpath()
                # 右三角
                path.moveTo(self.rect.right(), self.rect.top() + 8)
                path.lineTo(self.rect.right(), self.rect.bottom() - 8)
                path.lineTo(cx, cy)
                path.closeSubpath()
                painter.drawPath(path)
                # 线圈方框（顶部）
                painter.setPen(QPen(QColor("#4FC3F7"), 2))
                painter.setBrush(QBrush(QColor("#1c3146")))
                coil_rect = QRectF(cx - 8, self.rect.top() - 2, 16, 12)
                painter.drawRect(coil_rect)
                # S标记
                painter.setPen(QPen(QColor("#4FC3F7"), 1))
                painter.setFont(QFont("Arial", 7, QFont.Bold))
                painter.drawText(coil_rect, Qt.AlignCenter, "S")
                
            elif "放气" in self.name:
                # ============================================
                # 放气阀/针阀 (Vent/Needle Valve) - 带箭头
                # 两个三角形 + V标记
                # ============================================
                path = QPainterPath()
                # 左三角
                path.moveTo(self.rect.left(), self.rect.top())
                path.lineTo(self.rect.left(), self.rect.bottom())
                path.lineTo(cx, cy)
                path.closeSubpath()
                # 右三角
                path.moveTo(self.rect.right(), self.rect.top())
                path.lineTo(self.rect.right(), self.rect.bottom())
                path.lineTo(cx, cy)
                path.closeSubpath()
                painter.drawPath(path)
                # V标记（放气）
                painter.setPen(QPen(QColor("#69F0AE"), 2))
                painter.setFont(QFont("Arial", 10, QFont.Bold))
                painter.drawText(QRectF(cx - 8, self.rect.top() - 15, 16, 14), Qt.AlignCenter, "V")
            else:
                # 通用阀门 - 蝶形
                path = QPainterPath()
                path.moveTo(self.rect.left(), self.rect.top())
                path.lineTo(self.rect.left(), self.rect.bottom())
                path.lineTo(cx, cy)
                path.lineTo(self.rect.right(), self.rect.bottom())
                path.lineTo(self.rect.right(), self.rect.top())
                path.lineTo(cx, cy)
                path.closeSubpath()
                painter.drawPath(path)
                
        elif "pump" in self.dev_type:
            # 泵 - 圆形
            painter.drawEllipse(self.rect)
            
            # 绘制内部符号
            painter.setPen(QPen(Qt.white, 2))
            cx, cy = self.rect.center().x(), self.rect.center().y()
            r = min(self.rect.width(), self.rect.height()) / 2 * 0.6
            
            if "分子" in self.name:
                # 分子泵 - 叶轮
                painter.drawLine(int(cx-r), int(cy), int(cx+r), int(cy))
                painter.drawLine(int(cx), int(cy-r), int(cx), int(cy+r))
            else:
                # 其他泵 - P
                painter.setFont(QFont("Arial", 12, QFont.Bold))
                painter.drawText(self.rect, Qt.AlignCenter, "P")
        else:
            painter.drawRect(self.rect)
            
    def set_state(self, on, action_state=0):
        self.is_on = on
        self.action_state = action_state
        
        if action_state == ValveActionState.OPENING:
            status = "开启中"
            color = QColor("#2196F3")
        elif action_state == ValveActionState.CLOSING:
            status = "关闭中"
            color = QColor("#2196F3")
        elif action_state == ValveActionState.OPEN_TIMEOUT:
            status = "开超时!"
            color = QColor("#F44336")
        elif action_state == ValveActionState.CLOSE_TIMEOUT:
            status = "关超时!"
            color = QColor("#F44336")
        else:
            status = "开启" if on else "关闭"
            color = QColor("#4CAF50") if on else QColor("#888")
            
        self.lbl_status.setPlainText(status)
        self.lbl_status.setDefaultTextColor(color)
        self.update()
        
    def set_speed(self, hz):
        self.hz_val = hz
        if self.lbl_hz:
            self.lbl_hz.setPlainText(f"{hz}Hz")
            
    def toggle_blink(self):
        if self.action_state in [ValveActionState.OPENING, ValveActionState.CLOSING,
                                  ValveActionState.OPEN_TIMEOUT, ValveActionState.CLOSE_TIMEOUT]:
            self._blink_phase = not self._blink_phase
            self.update()
            
    def hoverEnterEvent(self, event):
        """鼠标悬停时记录对应的操作"""
        if not self.attr_name or not self._parent_widget:
            return
            
        op_key = None
        attr_lower = self.attr_name.lower()
        if "power" in attr_lower:
            if "screw" in attr_lower: op_key = "ScrewPump_Start"
            elif "roots" in attr_lower: op_key = "RootsPump_Start"
            elif "molecular" in attr_lower:
                # 分子泵1-3启动需要对应的电磁阀和闸板阀分别开启
                import re
                match = re.search(r'(\d+)', self.attr_name)
                if match:
                    idx = int(match.group(1))
                    op_key = f"MolecularPump{idx}_Start"  # 使用单独条件，检查对应的电磁阀和闸板阀
                else:
                    op_key = "MolecularPump_Start"  # 兼容性：批量配置
        elif "open" in attr_lower:
            if "gatevalve" in attr_lower:
                # 从 gateValve1Open 提取数字
                import re
                match = re.search(r'(\d+)', self.attr_name)
                if match:
                    idx = int(match.group(1))
                    if idx <= 3: op_key = f"GateValve{idx}_Open"  # 使用单独条件，检查对应的电磁阀
                    elif idx == 4: op_key = "GateValve4_Open"
                    else: op_key = "GateValve5_Open"
            elif "ventvalve" in attr_lower:
                import re
                match = re.search(r'(\d+)', self.attr_name)
                if match:
                    idx = int(match.group(1))
                    op_key = f"VentValve{idx}_Open"
            elif "electromagnetic" in attr_lower:
                import re
                match = re.search(r'(\d+)', self.attr_name)
                if match:
                    idx = int(match.group(1))
                    if idx <= 3: op_key = "ElectromagneticValve123_Open"
                    else: op_key = "ElectromagneticValve4_Open"
                
        if op_key:
            self._parent_widget._last_selected_op = op_key
            # 强制触发一次更新
            status = self._parent_widget._worker.get_cached_status()
            if status:
                self._parent_widget._update_prerequisites(status)
                
        super().hoverEnterEvent(event)

    def mousePressEvent(self, event):
        """鼠标按下 - 使用定时器区分单击和双击"""
        if event.button() == Qt.LeftButton:
            # 保存点击位置，延迟处理单击（等待可能的双击）
            self._pending_click_pos = event.screenPos()
            # 使用 250ms 延迟，足够区分单击和双击
            QTimer.singleShot(250, self._handle_single_click)
            event.accept()
        elif event.button() == Qt.RightButton:
            # 右键直接显示菜单
            self._show_context_menu(event.screenPos())
            event.accept()
        else:
            super().mousePressEvent(event)
    
    def mouseDoubleClickEvent(self, event):
        """双击直接切换状态"""
        if event.button() == Qt.LeftButton:
            # 取消待处理的单击
            self._pending_click_pos = None
            # 执行双击操作
            self._toggle_device_state()
            event.accept()
        else:
            super().mouseDoubleClickEvent(event)
    
    def _handle_single_click(self):
        """处理单击（延迟执行，确认不是双击后才触发）"""
        if self._pending_click_pos is not None:
            pos = self._pending_click_pos
            self._pending_click_pos = None
            self._show_context_menu(pos)
            
    def _toggle_device_state(self):
        """双击切换设备状态"""
        if not self._parent_widget:
            return
            
        # 检查是否可操作
        if not self._parent_widget._check_manual_mode():
            return
        
        # 根据当前状态决定新状态
        new_state = not self.is_on
        self._parent_widget._send_device_command(self.attr_name, new_state)
            
    def _show_context_menu(self, pos):
        if not self._parent_widget:
            return
            
        # 检查是否可操作
        if not self._parent_widget._check_manual_mode():
            return
            
        menu = QMenu()
        menu.setStyleSheet(f"""
            QMenu {{
                background-color: {COLORS['surface']};
                color: {COLORS['text_primary']};
                border: 1px solid {COLORS['border']};
            }}
            QMenu::item:selected {{
                background-color: {COLORS['primary']};
            }}
        """)
        
        if "pump" in self.dev_type:
            action_on = menu.addAction("启动")
            action_off = menu.addAction("停止")
        else:
            action_on = menu.addAction("开启")
            action_off = menu.addAction("关闭")
            
        # pos 可能是 QPointF 或 QPoint，转为整数坐标
        if hasattr(pos, 'toPoint'):
            screen_pos = pos.toPoint()
        elif hasattr(pos, 'x') and hasattr(pos, 'y'):
            screen_pos = QPoint(int(pos.x()), int(pos.y()))
        else:
            screen_pos = pos
        action = menu.exec_(screen_pos)
        if action == action_on:
            self._parent_widget._send_device_command(self.attr_name, True)
        elif action == action_off:
            self._parent_widget._send_device_command(self.attr_name, False)


class DigitalTwinPage(QWidget):
    """数字孪生页面"""
    
    def __init__(self, worker: VacuumTangoWorker, parent=None):
        super().__init__(parent)
        
        self._worker = worker
        self._current_mode = OperationMode.MANUAL
        self._system_state = SystemState.IDLE
        self._last_selected_op = None
        self._tango_connected = False
        self._plc_connected = False
        
        self._devices = {}  # attr_name -> TwinDeviceItem
        self._gauge_items = {}  # attr_name -> QGraphicsTextItem
        
        self._init_ui()
        self._init_scene()
        self._connect_signals()
        
        # 初始同步连接状态
        self._on_connection_changed(self._worker.device is not None)
        self._on_plc_connection_changed(self._worker._last_plc_connected)
        
        self._start_polling()
        
    def _init_ui(self):
        """初始化 UI - 与控制面板布局一致"""
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(8, 6, 8, 6)
        main_layout.setSpacing(6)
        
        # 1. 顶部状态栏
        main_layout.addWidget(self._create_status_bar())
        
        # 2. 自动流程进度 Banner
        main_layout.addWidget(self._create_progress_banner())
        
        # 3. 主内容区：三栏布局（参考控制面板）
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
        
        # 左侧：自动控制面板（变窄）
        self._control_panel = self._create_control_panel()
        self._control_panel.setMinimumWidth(220)  # 设置最小宽度，让出空间给数字孪生栏
        splitter.addWidget(self._control_panel)
        
        # 中间：拓扑图（使用 QGroupBox 保持边框风格一致）
        # 使用紫色边框，与自动操作区（青色）、手动操作区（黄色）、先决条件面板（绿色）区分
        twin_group = QGroupBox("数字孪生")
        twin_group.setStyleSheet(f"""
            QGroupBox {{
                background-color: rgba(156, 39, 176, 0.05);
                border: 2px solid #9C27B0;
                border-radius: 12px;
                padding: 0px;  /* 移除QGroupBox默认的padding */
            }}
            QGroupBox::title {{
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 15px;
                padding: 0 10px;
                color: #9C27B0;
                font-weight: bold;
                font-size: 14px;
            }}
        """)
        twin_layout = QVBoxLayout(twin_group)
        twin_layout.setContentsMargins(0, 20, 0, 0)  # 只保留顶部标题空间，其他边距全部为0，最大化内部矩形区域
        twin_layout.setSpacing(0)  # 移除布局间距
        
        self._view = QGraphicsView()
        self._scene = QGraphicsScene()
        self._view.setScene(self._scene)
        self._view.setRenderHint(QPainter.Antialiasing)
        self._view.setBackgroundBrush(QBrush(QColor("#0a1520")))
        # 启用滚动条，确保可以滚动查看完整内容
        self._view.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self._view.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        # 设置视图的变换锚点，确保缩放和滚动正常工作
        self._view.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)
        self._view.setResizeAnchor(QGraphicsView.AnchorViewCenter)
        # 确保视图可以正确显示和滚动完整场景
        self._view.setDragMode(QGraphicsView.NoDrag)  # 禁用拖拽，避免干扰滚动
        twin_layout.addWidget(self._view)
        
        splitter.addWidget(twin_group)
        
        # 右侧：操作先决条件面板
        self._prereq_panel = PrerequisitePanel("操作先决条件")
        self._prereq_panel.setMinimumWidth(240)  # 减小最小宽度，让出空间
        splitter.addWidget(self._prereq_panel)
        
        # 设置各栏宽度比例：控制面板 250（变窄），拓扑图 750（加宽，保证完整显示），先决条件面板 240（变窄）
        splitter.setSizes([250, 750, 240])
        main_layout.addWidget(splitter, 1)
        
        # 初始化底部栏的标签（会在场景中显示）
        self._interlock_label = None  # 将在场景中创建
        self._time_label = None  # 将在场景中创建
    
    def _create_status_bar(self) -> QWidget:
        """创建顶部状态栏 - 与控制面板一致"""
        frame = QFrame()
        frame.setStyleSheet(f"""
            QFrame {{
                background-color: {COLORS['surface']};
                border-bottom: 2px solid {COLORS['border']};
            }}
        """)
        frame.setFixedHeight(50)
        
        layout = QHBoxLayout(frame)
        layout.setContentsMargins(15, 5, 15, 5)
        layout.setSpacing(20)
        
        # --- 组1: 系统状态 ---
        sys_group = QHBoxLayout()
        sys_group.setSpacing(15)
        
        # 连接状态指示器
        self._top_conn_indicator = StatusIndicator("连接")
        sys_group.addWidget(self._top_conn_indicator)
        
        # 模式
        mode_box = QHBoxLayout()
        mode_box.setSpacing(5)
        mode_box.addWidget(QLabel("模式:"))
        self._top_mode_label = QLabel("手动")
        self._top_mode_label.setStyleSheet(f"color: {COLORS['warning']}; font-weight: bold; font-size: 14px;")
        mode_box.addWidget(self._top_mode_label)
        sys_group.addLayout(mode_box)
        
        # 状态
        state_box = QHBoxLayout()
        state_box.setSpacing(5)
        state_box.addWidget(QLabel("状态:"))
        self._top_state_label = QLabel("空闲")
        self._top_state_label.setStyleSheet(f"color: {COLORS['success']}; font-weight: bold; font-size: 14px;")
        state_box.addWidget(self._top_state_label)
        sys_group.addLayout(state_box)
        
        layout.addLayout(sys_group)
        layout.addWidget(self._create_v_separator())
        
        # --- 组2: 真空计参数 ---
        param_group = QHBoxLayout()
        param_group.setSpacing(15)
        
        # 前级真空计
        v1_box = QHBoxLayout()
        v1_box.setSpacing(4)
        v1_lbl = QLabel("前级:")
        v1_lbl.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")
        v1_box.addWidget(v1_lbl)
        self._top_vacuum1 = QLabel("--- Pa")
        self._top_vacuum1.setStyleSheet(f"color: #00E5FF; font-family: Consolas; font-size: 12px; font-weight: bold;")
        v1_box.addWidget(self._top_vacuum1)
        param_group.addLayout(v1_box)
        
        # 真空计1
        v2_box = QHBoxLayout()
        v2_box.setSpacing(4)
        v2_lbl = QLabel("真空计1:")
        v2_lbl.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")
        v2_box.addWidget(v2_lbl)
        self._top_vacuum2 = QLabel("--- Pa")
        self._top_vacuum2.setStyleSheet(f"color: #00E5FF; font-family: Consolas; font-size: 12px; font-weight: bold;")
        v2_box.addWidget(self._top_vacuum2)
        param_group.addLayout(v2_box)
        
        # 真空计2
        v3_box = QHBoxLayout()
        v3_box.setSpacing(4)
        v3_lbl = QLabel("真空计2:")
        v3_lbl.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")
        v3_box.addWidget(v3_lbl)
        self._top_vacuum3 = QLabel("--- Pa")
        self._top_vacuum3.setStyleSheet(f"color: #00E5FF; font-family: Consolas; font-size: 12px; font-weight: bold;")
        v3_box.addWidget(self._top_vacuum3)
        param_group.addLayout(v3_box)
        
        layout.addLayout(param_group)
        layout.addWidget(self._create_v_separator())
        
        # --- 组3: 辅助设施（4路水路 + 气源）---
        aux_group = QHBoxLayout()
        aux_group.setSpacing(8)
        
        # 4路水路指示器
        self._top_water_indicators = []
        for i in range(4):
            water_ind = StatusIndicator(f"水{i+1}")
            water_ind.setMinimumWidth(40)
            water_ind.setToolTip(f"冷却水路{i+1}")
            self._top_water_indicators.append(water_ind)
            aux_group.addWidget(water_ind)
        
        aux_group.addWidget(self._create_v_separator())
        
        self._top_air_indicator = StatusIndicator("气源")
        self._top_air_indicator.setMinimumWidth(45)
        aux_group.addWidget(self._top_air_indicator)
        
        # 气压值
        self._top_air_pressure = QLabel("0.50 MPa")
        self._top_air_pressure.setStyleSheet(f"color: #00E5FF; font-family: Consolas; font-size: 11px;")
        aux_group.addWidget(self._top_air_pressure)
        
        layout.addLayout(aux_group)
        layout.addWidget(self._create_v_separator())
        
        # --- 组4: 报警状态 ---
        self._top_alarm_indicator = StatusIndicator("报警")
        self._top_alarm_indicator.setMinimumWidth(50)
        layout.addWidget(self._top_alarm_indicator)
        
        layout.addStretch()
        
        return frame
    
    def _create_progress_banner(self) -> QWidget:
        """创建自动流程进度 Banner - 与控制面板一致"""
        frame = QFrame()
        frame.setStyleSheet(f"""
            QFrame {{
                background-color: rgba(0, 229, 255, 0.05);
                border: 1px solid {COLORS['border']};
                border-radius: 4px;
            }}
        """)
        frame.setFixedHeight(32)
        
        layout = QHBoxLayout(frame)
        layout.setContentsMargins(15, 0, 15, 0)
        layout.setSpacing(20)
        
        # 1. 当前步骤
        step_layout = QHBoxLayout()
        step_layout.setSpacing(6)
        lbl_step = QLabel("当前步骤:")
        lbl_step.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")
        self._auto_progress_label = QLabel("空闲")
        self._auto_progress_label.setStyleSheet(f"color: {COLORS['primary']}; font-size: 12px; font-weight: bold;")
        step_layout.addWidget(lbl_step)
        step_layout.addWidget(self._auto_progress_label)
        layout.addLayout(step_layout)
        
        layout.addWidget(self._create_v_separator())
        
        # 2. 流程状态
        status_layout = QHBoxLayout()
        status_layout.setSpacing(6)
        lbl_status = QLabel("流程:")
        lbl_status.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")
        self._auto_status_label = QLabel("手动模式")
        self._auto_status_label.setStyleSheet(f"color: #ECEFF1; font-size: 11px;")
        status_layout.addWidget(lbl_status)
        status_layout.addWidget(self._auto_status_label)
        layout.addLayout(status_layout)
        
        layout.addWidget(self._create_v_separator())
        
        # 3. 分支/互锁信息
        self._top_branch_label = QLabel("")
        self._top_branch_label.setStyleSheet(f"color: #00E5FF; font-size: 11px;")
        layout.addWidget(self._top_branch_label)
        
        self._top_interlock_label = QLabel("")
        self._top_interlock_label.setStyleSheet(f"color: {COLORS['warning']}; font-size: 11px; font-weight: bold;")
        layout.addWidget(self._top_interlock_label)
        
        layout.addStretch()
        
        return frame
    
    def _create_v_separator(self) -> QFrame:
        """创建垂直分隔线"""
        sep = QFrame()
        sep.setFrameShape(QFrame.VLine)
        sep.setStyleSheet(f"background-color: {COLORS['divider']}; max-width: 1px;")
        return sep
        
    def _create_control_panel(self) -> QWidget:
        """创建自动控制面板 - 精简版（状态信息已移至顶部）"""
        group = QGroupBox("自动操作区")
        group.setStyleSheet(MODE_AREA_STYLES['auto_area'])
        
        layout = QVBoxLayout(group)
        layout.setContentsMargins(12, 16, 12, 12)
        layout.setSpacing(12)  # 与控制面板一致，更紧凑
        
        # =====================================================================
        # 1. 模式切换按钮
        # =====================================================================
        mode_frame = QFrame()
        mode_frame.setStyleSheet("background: transparent; border: none;")
        mode_layout = QHBoxLayout(mode_frame)
        mode_layout.setContentsMargins(0, 0, 0, 0)
        mode_layout.setSpacing(8)
        
        self._btn_auto = QPushButton("自动模式")
        # 使用更明显的青色背景和深色文字，确保可见性
        self._btn_auto.setStyleSheet(f"""
            QPushButton {{
                background-color: {COLORS['primary']};
                color: {COLORS['background']};
                border: 2px solid {COLORS['primary']};
                font-weight: bold;
                border-radius: 6px;
                font-size: 13px;
            }}
            QPushButton:hover {{
                background-color: {COLORS['primary_dark']};
                border-color: {COLORS['primary_dark']};
            }}
            QPushButton:disabled {{
                background-color: {COLORS['surface_light']};
                border: 1px solid {COLORS['border']};
                color: {COLORS['text_secondary']};
                opacity: 0.5;
            }}
        """)
        self._btn_auto.setMinimumHeight(40)  # 设置最小高度，提高可见性
        self._btn_auto.setCursor(Qt.PointingHandCursor)  # 添加鼠标指针样式
        self._btn_auto.clicked.connect(self._on_switch_to_auto)
        mode_layout.addWidget(self._btn_auto)
        
        self._btn_manual = QPushButton("手动模式")
        self._btn_manual.setMinimumHeight(40)  # 设置最小高度，提高可见性
        self._btn_manual.setCursor(Qt.PointingHandCursor)  # 添加鼠标指针样式
        # 为手动模式按钮设置更明显的样式（黄色背景和深色文字，确保可见性）
        self._btn_manual.setStyleSheet(f"""
            QPushButton {{
                background-color: {COLORS['warning']};
                color: {COLORS['background']};
                border: 2px solid {COLORS['warning']};
                font-weight: bold;
                border-radius: 6px;
                font-size: 13px;
            }}
            QPushButton:hover {{
                background-color: #E6D200;
                border-color: #E6D200;
            }}
            QPushButton:disabled {{
                background-color: {COLORS['surface_light']};
                border: 1px solid {COLORS['border']};
                color: {COLORS['text_secondary']};
                opacity: 0.5;
            }}
        """)
        self._btn_manual.clicked.connect(self._on_switch_to_manual)
        mode_layout.addWidget(self._btn_manual)
        
        layout.addWidget(mode_frame)
        
        # 分隔线
        layout.addWidget(self._create_h_separator())
        
        # =====================================================================
        # 2. 一键操作按钮 (Grid 2x2 布局)
        # =====================================================================
        ops_layout = QGridLayout()
        ops_layout.setSpacing(12)  # 与控制面板一致
        
        self._btn_vacuum_start = QPushButton("一键抽真空")
        self._btn_vacuum_start.setMinimumHeight(55)  # 稍微增加高度，更美观
        self._btn_vacuum_start.setProperty("class", "success")
        self._btn_vacuum_start.setProperty("operation", "Auto_OneKeyVacuumStart")
        self._btn_vacuum_start.installEventFilter(self)
        self._btn_vacuum_start.clicked.connect(self._on_vacuum_start)
        self._btn_vacuum_start.setEnabled(False)
        ops_layout.addWidget(self._btn_vacuum_start, 0, 0)
        
        self._btn_vacuum_stop = QPushButton("一键停机")
        self._btn_vacuum_stop.setMinimumHeight(55)
        self._btn_vacuum_stop.setProperty("class", "warning")
        self._btn_vacuum_stop.setProperty("operation", "Auto_OneKeyVacuumStop")
        self._btn_vacuum_stop.installEventFilter(self)
        self._btn_vacuum_stop.clicked.connect(self._on_vacuum_stop)
        self._btn_vacuum_stop.setEnabled(False)
        ops_layout.addWidget(self._btn_vacuum_stop, 0, 1)
        
        self._btn_vent = QPushButton("腔室放气")
        self._btn_vent.setMinimumHeight(55)
        self._btn_vent.setEnabled(False)
        self._btn_vent.setProperty("operation", "Auto_ChamberVent")
        self._btn_vent.installEventFilter(self)
        self._btn_vent.clicked.connect(self._on_chamber_vent)
        ops_layout.addWidget(self._btn_vent, 1, 0)
        
        self._btn_fault_reset = QPushButton("故障复位")
        self._btn_fault_reset.setMinimumHeight(55)
        self._btn_fault_reset.setProperty("class", "danger")
        self._btn_fault_reset.setEnabled(False)
        self._btn_fault_reset.setProperty("operation", "Auto_FaultReset")
        self._btn_fault_reset.installEventFilter(self)
        self._btn_fault_reset.clicked.connect(self._on_fault_reset)
        ops_layout.addWidget(self._btn_fault_reset, 1, 1)
        
        layout.addLayout(ops_layout)
        
        # 分隔线
        layout.addWidget(self._create_h_separator())
        
        # =====================================================================
        # 3. 紧急停止按钮（放在分子泵配置之前，与控制面板一致）
        # =====================================================================
        self._btn_emergency = QPushButton("⚠ 紧急停止")
        # 设置支持 Unicode 符号的字体，确保图标正确显示
        from widgets import get_unicode_font
        self._btn_emergency.setFont(get_unicode_font(14))
        self._btn_emergency.setMinimumHeight(60)  # 与控制面板接近
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
        
        # =====================================================================
        # 4. 分子泵配置区
        # =====================================================================
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
    
    def _create_h_separator(self) -> QFrame:
        """创建水平分隔线"""
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setStyleSheet(f"background-color: {COLORS['divider']}; max-height: 1px; border: none;")
        return line
    
    def _on_mp_config_changed(self, pump_index: int, state: int):
        """分子泵配置变更"""
        enabled = (state == Qt.Checked)
        AUTO_MOLECULAR_PUMP_CONFIG[f'molecularPump{pump_index}Enabled'] = enabled
        
        # 写入Tango属性，同步到设备端和PLC
        if hasattr(self._worker, 'set_molecular_pump_enabled'):
            self._worker.set_molecular_pump_enabled(pump_index, enabled)
        
        logger.info(f"分子泵{pump_index} 自动流程参与状态: {'启用' if enabled else '停用'}")
        
        
    def _init_scene(self):
        """初始化场景 - 绘制拓扑图（参照实际系统布局，紧凑版）"""
        # 清空场景
        self._scene.clear()
        self._devices.clear()
        self._gauge_items.clear()
        
        # 设置场景尺寸（扩大以增加横竖方向的使用率）
        # 增加场景尺寸，充分利用数字孪生栏的空间
        # 宽度：从950增加到1100，增加横向空间利用率
        # 高度：从680增加到800，增加纵向空间利用率
        self._scene.setSceneRect(0, 0, 1100, 800)
        
        # =====================================================================
        # 真空腔室 (主体，紧凑布局)
        # =====================================================================
        chamber = QGraphicsRectItem(220, 130, 600, 80)
        chamber.setPen(QPen(QColor("#546E7A"), 2))
        chamber.setBrush(QBrush(QColor("#1c3146")))
        self._scene.addItem(chamber)
        
        text = self._scene.addText("真空腔室")
        text.setDefaultTextColor(Qt.white)
        text.setFont(QFont("Microsoft YaHei", 12, QFont.Bold))
        text.setPos(470, 155)
        
        # =====================================================================
        # 真空计 (腔室上方，圆形带P标识)
        # =====================================================================
        # 真空计1 (主真空计1)
        self._add_gauge(350, 30, "主真空计1", "vacuumGauge2")
        self._draw_line(372, 74, 372, 130)  # 连入腔室顶部
        
        # 真空计2 (主真空计2)
        self._add_gauge(620, 30, "主真空计2", "vacuumGauge3")
        self._draw_line(642, 74, 642, 130)  # 连入腔室顶部
        
        # =====================================================================
        # 放气阀1 (腔室上方左侧)
        # =====================================================================
        self._add_device(270, 75, 50, 35, "放气阀1", "ventValve1Open", "valve")
        self._draw_line(295, 110, 295, 130)  # 连入腔室顶部
        
        # =====================================================================
        # 闸板阀5 (右侧，直接与腔室交接)
        # =====================================================================
        self._add_device(820, 153, 50, 35, "闸板阀5", "gateValve5Open", "valve")
        
        # =====================================================================
        # 闸板阀4 (粗抽，直接从腔室底部连线)
        # =====================================================================
        self._add_device(220, 245, 50, 35, "闸板阀4", "gateValve4Open", "valve")
        # 从腔室底部直接连线到闸板阀4
        self._draw_line(245, 210, 245, 245)  # 腔室底部 -> 闸板阀4
        self._draw_line(245, 280, 245, 420)  # 闸板阀4 -> 前级总管
        
        # =====================================================================
        # 分子泵组 (闸板阀 + 分子泵 + 电磁阀 + 水路状态)
        # 水路对应关系：分子泵1→水路2，分子泵2→水路3，分子泵3→水路4
        # =====================================================================
        group_x = [320, 520, 720]  # 水平间距
        self._water_status_items = {}  # 存储水路状态显示项
        
        for i, x in enumerate(group_x):
            idx = i + 1
            water_idx = idx + 1  # 分子泵1→水路2，分子泵2→水路3，分子泵3→水路4
            
            # 连线：腔室底部 -> 闸板阀
            self._draw_line(x + 25, 210, x + 25, 245)
            
            # 闸板阀
            self._add_device(x, 245, 50, 35, f"闸板阀{idx}", f"gateValve{idx}Open", "valve")
            # 连线：闸板阀 -> 分子泵
            self._draw_line(x + 25, 280, x + 25, 300)
            
            # 分子泵
            self._add_device(x - 5, 300, 60, 45, f"分子泵{idx}", f"molecularPump{idx}Power", "pump")
            
            # 水路状态指示器（分子泵右侧，避开状态和频率标签）
            # 注意：使用 waterValve{water_idx}State 作为键，因为这是 Device Server 的实际属性
            self._add_water_status(x + 90, 310, f"水冷{water_idx}", f"waterValve{water_idx}State")
            
            # 连线：分子泵 -> 电磁阀
            self._draw_line(x + 25, 345, x + 25, 370)
            
            # 电磁阀
            self._add_device(x, 370, 50, 35, f"电磁阀{idx}", f"electromagneticValve{idx}Open", "valve")
            # 连线：电磁阀 -> 前级总管
            self._draw_line(x + 25, 405, x + 25, 420)
            
        # =====================================================================
        # 放气阀2 (分子泵1和分子泵2之间，连接前级总管)
        # =====================================================================
        self._add_device(420, 370, 50, 35, "放气阀2", "ventValve2Open", "valve")
        self._draw_line(445, 405, 445, 420)  # 连入前级总管
        
        # =====================================================================
        # 前级总管 (水平，从闸板阀4连接)
        # =====================================================================
        self._draw_line(245, 420, 780, 420)
        
        # =====================================================================
        # 前级真空计 (前级总管右侧)
        # =====================================================================
        self._add_gauge(620, 430, "前级真空计", "vacuumGauge1")
        self._draw_line(642, 420, 642, 430)  # 连入前级总管
        
        # =====================================================================
        # 罗茨泵
        # =====================================================================
        self._add_device(460, 455, 60, 45, "罗茨泵", "rootsPumpPower", "pump")
        self._draw_line(490, 420, 490, 455)  # 从前级总管连入
        self._draw_line(490, 500, 490, 525)  # 连入螺杆泵
        
        # =====================================================================
        # 螺杆泵 + 水路1状态
        # =====================================================================
        self._add_device(460, 525, 60, 45, "螺杆泵", "screwPumpPower", "pump")
        # 水路1状态指示器（螺杆泵右侧，避开状态和频率标签）
        # 注意：使用 waterValve1State 作为键，因为这是 Device Server 的实际属性
        self._add_water_status(555, 535, "水冷1", "waterValve1State")
        
        # 螺杆泵下方：竖线 -> 90度折弯 -> 横线 -> 电磁阀4 -> 尾气
        self._draw_line(490, 570, 490, 600)  # 竖线向下
        self._draw_line(490, 600, 560, 600)  # 90度折弯，横线向右
        
        # =====================================================================
        # 电磁阀4 (尾气阀，在水平线上)
        # =====================================================================
        self._add_device(560, 582, 50, 35, "电磁阀4", "electromagneticValve4Open", "valve")
        # 电磁阀4右侧继续水平连向尾气
        self._draw_line(610, 600, 700, 600)  # 水平连向尾气
        
        self._add_text(710, 592, "→ 尾气", "#AAA")
        
        # 添加图例 (竖排，放在右下角，可与孪生图重叠)
        # 位置：右下角，向左向上移动更多
        scene_width = 1100  # 更新为新的场景宽度
        scene_height = 800  # 更新为新的场景高度
        legend_width = 120  # 竖排图例宽度
        legend_height = 130  # 竖排图例高度（4个图例项 + 提示文字）
        legend_x = scene_width - legend_width - 110  # 右下角，向左移动（60px + 50px = 110px边距）
        legend_y = scene_height - legend_height - 110  # 右下角，向上移动（60px + 50px = 110px边距）
        self._add_legend(legend_x, legend_y)
        
        # 确保场景尺寸包含所有元素（包括标签和边距）
        # 计算实际需要的场景尺寸
        items_rect = self._scene.itemsBoundingRect()
        if items_rect.isValid():
            # 添加一些边距，确保所有元素都可见
            margin = 20
            self._scene.setSceneRect(
                items_rect.x() - margin,
                items_rect.y() - margin,
                items_rect.width() + margin * 2,
                items_rect.height() + margin * 2
            )
    
    def _add_legend(self, x, y):
        """在场景中添加图例 - 竖排，放在右下角，可与孪生图重叠"""
        legends = [
            (BlinkColors.NORMAL_OPEN, "开启/运行"),
            (BlinkColors.NORMAL_CLOSE, "关闭/停止"),
            (BlinkColors.OPERATING_ON, "正在操作"),
            (BlinkColors.TIMEOUT_ON, "超时/故障"),
        ]
        
        # 计算图例所需尺寸（竖排，紧凑）
        item_height = 20  # 每个图例项的高度
        item_width = 100  # 图例项宽度（颜色块 + 文字）
        legend_width = item_width + 20  # 总宽度 + 边距
        legend_height = len(legends) * item_height + 10  # 总高度 + 边距
        
        # 背景框（竖排，半透明，允许与孪生图重叠）
        bg = QGraphicsRectItem(x - 10, y - 5, legend_width, legend_height)
        bg.setPen(QPen(QColor(COLORS['border']), 1))
        bg.setBrush(QBrush(QColor("#0a1520")))
        bg.setOpacity(0.85)  # 半透明，允许看到背后的孪生图
        self._scene.addItem(bg)
        
        # 竖排显示图例项
        offset_y = y
        for color, text in legends:
            # 颜色块
            box = QGraphicsRectItem(x, offset_y, 12, 12)
            box.setPen(QPen(Qt.NoPen))
            box.setBrush(QBrush(QColor(color)))
            self._scene.addItem(box)
            
            # 文字（在颜色块右侧）
            lbl = self._scene.addText(text)
            lbl.setDefaultTextColor(QColor(COLORS['text_secondary']))
            lbl.setFont(QFont("Microsoft YaHei", 8))
            lbl.setPos(x + 16, offset_y - 1)
            
            offset_y += item_height
        
        # 提示文字（在图例下方）
        hint = self._scene.addText("单击设备弹出菜单 / 双击切换状态")
        hint.setDefaultTextColor(QColor(COLORS['text_secondary']))
        hint.setFont(QFont("Microsoft YaHei", 7))
        # 计算提示文字的位置，使其在图例项下方居中
        hint_rect = hint.boundingRect()
        hint_x = x - 10 + (legend_width - hint_rect.width()) / 2  # 居中
        hint_y = offset_y + 5  # 图例项下方留5px间距
        hint.setPos(hint_x, hint_y)
        
        # 调整背景框高度以包含提示文字
        final_height = hint_y + hint_rect.height() - y + 10
        bg.setRect(x - 10, y - 5, legend_width, final_height)
        
    def _add_device(self, x, y, w, h, name, attr_name, dev_type):
        """添加设备"""
        item = TwinDeviceItem(name, x, y, w, h, dev_type, self)
        item.attr_name = attr_name
        self._scene.addItem(item)
        self._devices[attr_name] = item
        
    def _add_gauge(self, x, y, name, attr_name):
        """添加真空计 - 圆形带P标识样式"""
        radius = 22
        
        # 圆形背景
        circle = QGraphicsEllipseItem(x, y, radius * 2, radius * 2)
        circle.setPen(QPen(QColor("#4FC3F7"), 2))
        circle.setBrush(QBrush(QColor("#1c3146")))
        self._scene.addItem(circle)
        
        # P 标识（圆内中心）
        p_text = self._scene.addText("P")
        p_text.setDefaultTextColor(QColor("#4FC3F7"))
        p_text.setFont(QFont("Arial", 16, QFont.Bold))
        p_text.setPos(x + radius - 8, y + radius - 14)
        
        # 名称（圆形右侧）
        name_text = self._scene.addText(name)
        name_text.setDefaultTextColor(QColor(COLORS['text_secondary']))
        name_text.setFont(QFont("Microsoft YaHei", 8))
        name_text.setPos(x + radius * 2 + 5, y - 2)
        
        # 数值（名称下方）
        val_text = self._scene.addText("--- Pa")
        val_text.setDefaultTextColor(QColor("#00E5FF"))
        val_text.setFont(QFont("Consolas", 10, QFont.Bold))
        val_text.setPos(x + radius * 2 + 5, y + 14)
        self._gauge_items[attr_name] = val_text
        
    def _add_text(self, x, y, text, color):
        """添加文本"""
        t = self._scene.addText(text)
        t.setDefaultTextColor(QColor(color))
        t.setFont(QFont("Microsoft YaHei", 9))
        t.setPos(x, y)
        
    def _add_water_status(self, x, y, name, attr_name):
        """添加水路状态指示器（小圆点 + 文字）"""
        # 状态指示圆点
        circle = QGraphicsEllipseItem(x, y, 12, 12)
        circle.setPen(QPen(QColor("#546E7A"), 1))
        circle.setBrush(QBrush(QColor("#4CAF50")))  # 默认绿色
        self._scene.addItem(circle)
        
        # 名称标签
        label = self._scene.addText(name)
        label.setDefaultTextColor(QColor("#4FC3F7"))
        label.setFont(QFont("Microsoft YaHei", 8))
        label.setPos(x + 14, y - 2)
        
        # 存储引用以便更新状态
        self._water_status_items[attr_name] = circle
    
    def _add_utility_status_display(self, x, y):
        """添加水路/气路状态显示区"""
        # 外框
        rect = QGraphicsRectItem(x, y, 130, 180)
        rect.setPen(QPen(QColor(COLORS['border']), 1))
        rect.setBrush(QBrush(QColor(COLORS['surface_light'])))
        self._scene.addItem(rect)
        
        # 标题
        title = self._scene.addText("水路/气路")
        title.setDefaultTextColor(QColor(COLORS['text_secondary']))
        title.setFont(QFont("Microsoft YaHei", 10, QFont.Bold))
        title.setPos(x + 15, y + 5)
        
        # 水路状态 (4路)
        self._water_scene_items = []
        for i in range(1, 5):
            row_y = y + 35 + (i - 1) * 28
            
            # 水路名称
            name_text = self._scene.addText(f"水路{i}")
            name_text.setDefaultTextColor(QColor(COLORS['text_secondary']))
            name_text.setFont(QFont("Microsoft YaHei", 9))
            name_text.setPos(x + 10, row_y)
            
            # 状态指示 (圆形)
            indicator = QGraphicsEllipseItem(x + 75, row_y + 5, 12, 12)
            indicator.setPen(QPen(QColor("#555"), 1))
            indicator.setBrush(QBrush(QColor(COLORS['success'])))
            self._scene.addItem(indicator)
            self._water_scene_items.append(indicator)
            
            # 状态文字
            status_text = self._scene.addText("有流")
            status_text.setDefaultTextColor(QColor(COLORS['success']))
            status_text.setFont(QFont("Microsoft YaHei", 8))
            status_text.setPos(x + 92, row_y)
            self._water_scene_items.append(status_text)
        
        # 分隔线
        sep_y = y + 145
        sep_line = QGraphicsLineItem(x + 10, sep_y, x + 120, sep_y)
        sep_line.setPen(QPen(QColor(COLORS['border']), 1))
        self._scene.addItem(sep_line)
        
        # 气路状态
        air_y = sep_y + 5
        air_name = self._scene.addText("气源")
        air_name.setDefaultTextColor(QColor(COLORS['text_secondary']))
        air_name.setFont(QFont("Microsoft YaHei", 9))
        air_name.setPos(x + 10, air_y)
        
        self._air_scene_indicator = QGraphicsEllipseItem(x + 75, air_y + 5, 12, 12)
        self._air_scene_indicator.setPen(QPen(QColor("#555"), 1))
        self._air_scene_indicator.setBrush(QBrush(QColor(COLORS['success'])))
        self._scene.addItem(self._air_scene_indicator)
        
        self._air_scene_status = self._scene.addText("正常")
        self._air_scene_status.setDefaultTextColor(QColor(COLORS['success']))
        self._air_scene_status.setFont(QFont("Microsoft YaHei", 8))
        self._air_scene_status.setPos(x + 92, air_y)
        
    def _draw_line(self, x1, y1, x2, y2):
        """绘制管道线"""
        line = QGraphicsLineItem(x1, y1, x2, y2)
        line.setPen(QPen(QColor("#546E7A"), 2))
        self._scene.addItem(line)
        
    def _connect_signals(self):
        """连接信号"""
        self._worker.connection_changed.connect(self._on_connection_changed)
        self._worker.plc_connection_changed.connect(self._on_plc_connection_changed)
        
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
        self._top_conn_indicator.set_state(overall_connected)
        
    def _start_polling(self):
        """启动轮询"""
        self._poll_timer = QTimer(self)
        self._poll_timer.timeout.connect(self._pull_status)
        self._poll_timer.start(UI_CONFIG['poll_interval_ms'])
        
        self._blink_timer = QTimer(self)
        self._blink_timer.timeout.connect(self._toggle_blinks)
        self._blink_timer.start(UI_CONFIG['blink_interval_ms'])
        
        self._time_timer = QTimer(self)
        self._time_timer.timeout.connect(self._update_time)
        self._time_timer.start(1000)
        self._update_time()
        
    def _update_time(self):
        """更新时间显示 - 在场景中显示"""
        now = datetime.now()
        if hasattr(self, '_scene_time_text') and self._scene_time_text:
            self._scene_time_text.setPlainText(now.strftime("%Y-%m-%d %H:%M:%S"))

    def eventFilter(self, obj, event):
        """事件过滤器 - 鼠标悬停在按钮上时显示先决条件"""
        from PyQt5.QtCore import QEvent

        if event.type() == QEvent.Enter:
            operation = obj.property("operation")
            if operation:
                self._last_selected_op = operation
                # 立即更新先决条件（即使状态为空也显示）
                status = self._worker.get_cached_status()
                if not status:
                    status = {}  # 使用空字典，先决条件会显示为未满足
                self._update_prerequisites(status)

        return super().eventFilter(obj, event)
        
    def _pull_status(self):
        """拉取状态"""
        status = self._worker.get_cached_status()
        if not status:
            logger.warning("[WARNING] _pull_status: 状态为空，跳过更新")
            return
            
        # 更新模式 - 强制从缓存中读取最新值
        mode_raw = status.get('operationMode')
        if mode_raw is None:
            logger.warning(f"[WARNING] _pull_status: operationMode 不存在于状态中，使用默认值1")
            mode = 1
        else:
            # 确保是整数类型
            mode = int(mode_raw)
            
        self._current_mode = mode
        self._update_mode_display(mode)
        
        # 更新系统状态
        state = status.get('systemState', 0)
        self._system_state = state
        self._update_state_display(state, status.get('autoSequenceStep', 0))
        
        # 更新按钮状态
        is_auto = (mode == OperationMode.AUTO)
        self._btn_auto.setChecked(is_auto)
        self._btn_manual.setChecked(not is_auto)
        self._btn_vacuum_start.setEnabled(is_auto and state == SystemState.IDLE)
        
        # 检查是否有任何设备正在运行
        any_pump_running = any([
            status.get('screwPumpPower', False),
            status.get('rootsPumpPower', False),
            status.get('molecularPump1Power', False),
            status.get('molecularPump2Power', False),
            status.get('molecularPump3Power', False)
        ])
        
        # 一键停机：只要系统不是空闲状态（IDLE），或有任何泵运行，均允许执行停机序列
        self._btn_vacuum_stop.setEnabled(state != SystemState.IDLE or any_pump_running)
        
        # P0-1: 腔室放气互锁屏蔽增强 (允许在 IDLE 状态下手动/自动执行)
        vent_allowed = state == SystemState.IDLE
        self._btn_vent.setEnabled(vent_allowed)
        
        if state == SystemState.PUMPING:
            self._btn_vent.setToolTip("抽真空进行中：腔室放气已互锁屏蔽")
            self._btn_vent.setStyleSheet(f"""
                QPushButton {{
                    background-color: #555;
                    color: #888;
                    border: 2px dashed {COLORS['warning']};
                    border-radius: 4px;
                }}
            """)
        elif state == SystemState.STOPPING:
            self._btn_vent.setToolTip("停机进行中：腔室放气已互锁屏蔽")
            self._btn_vent.setStyleSheet(f"""
                QPushButton {{
                    background-color: #555;
                    color: #888;
                    border: 2px dashed {COLORS['warning']};
                    border-radius: 4px;
                }}
            """)
        else:
            self._btn_vent.setToolTip("")
            # 恢复默认样式
            self._btn_vent.setStyleSheet("")
            self._btn_vent.setProperty("class", "")
        
        # 更新联锁提示
        interlock_msgs = []
        if is_auto:
            interlock_msgs.append("⚠ 自动模式：手动操作已锁定")
        if state == SystemState.PUMPING:
            interlock_msgs.append("⚠ 腔室放气已互锁")
        interlock_text = " | ".join(interlock_msgs) if interlock_msgs else ""
        self._top_interlock_label.setText(interlock_text)
        
        # 故障复位：在故障/急停状态下启用，或者有活跃报警时也启用
        has_active_alarm = status.get('activeAlarmCount', 0) > 0 or status.get('hasUnacknowledgedAlarm', False)
        fault_reset_enabled = state in [SystemState.FAULT, SystemState.EMERGENCY_STOP] or has_active_alarm
        self._btn_fault_reset.setEnabled(fault_reset_enabled)
        
        # P2-1: 更新分支判据显示
        self._update_branch_info(status, state)
        
        # 更新设备状态
        for attr_name, item in self._devices.items():
            if "Power" in attr_name:
                is_on = status.get(attr_name, False)
                item.set_state(is_on)
                # 更新速度
                speed_attr = attr_name.replace("Power", "Speed")
                if speed_attr in status:
                    item.set_speed(int(status[speed_attr]))
            elif "Open" in attr_name:
                is_open = status.get(attr_name, False)
                # 获取动作状态
                action_attr = attr_name.replace("Open", "ActionState")
                action_state = status.get(action_attr, 0)
                item.set_state(is_open, action_state)
                
        # 更新真空计 - 全部使用科学记数法
        for attr_name, text_item in self._gauge_items.items():
            val = status.get(attr_name, 0)
            if isinstance(val, (int, float)) and val > 0:
                text_item.setPlainText(f"{val:.2e} Pa")
            else:
                text_item.setPlainText("--- Pa")
        
        # 更新顶部状态栏真空计
        self._update_top_vacuum_displays(status)
        
        # 更新水路/气路状态
        self._update_utility_status(status)
        
        # 更新分子泵启用配置复选框状态（从设备端同步）
        for i, cb in enumerate(self._mp_checkboxes, 1):
            enabled = status.get(f'molecularPump{i}Enabled', AUTO_MOLECULAR_PUMP_CONFIG.get(f'molecularPump{i}Enabled', True))
            # 临时断开信号，避免触发写入
            cb.blockSignals(True)
            cb.setChecked(enabled)
            cb.blockSignals(False)
                
        # 更新先决条件面板 (如果当前有选择的操作)
        self._update_prerequisites(status)
                
    def _update_mode_display(self, mode):
        """更新模式显示"""
        # 确保是整数类型
        mode = int(mode) if mode is not None else 1
        is_auto = (mode == OperationMode.AUTO)
        # 更新按钮启用状态（按钮不再使用 checked 状态）
        self._btn_auto.setEnabled(not is_auto)
        self._btn_manual.setEnabled(is_auto)
        
        # 更新顶部模式标签
        if is_auto:
            self._top_mode_label.setText("自动")
            self._top_mode_label.setStyleSheet(f"color: {COLORS['primary']}; font-weight: bold; font-size: 14px;")
            self._auto_status_label.setText("自动流程就绪")
        else:
            self._top_mode_label.setText("手动")
            self._top_mode_label.setStyleSheet(f"color: {COLORS['warning']}; font-weight: bold; font-size: 14px;")
            self._auto_status_label.setText("手动模式")
            
    def _update_state_display(self, state, step):
        """更新状态显示 - 使用顶部状态栏"""
        state_names = {
            0: ("空闲", COLORS['success']),
            1: ("抽真空中", "#00E5FF"),
            2: ("停机中", COLORS['warning']),
            3: ("放气中", "#2196F3"),
            4: ("故障", COLORS['error']),
            5: ("急停", COLORS['error']),
        }
        name, color = state_names.get(state, ("未知", "#888"))
        
        # 更新顶部状态标签
        self._top_state_label.setText(name)
        self._top_state_label.setStyleSheet(f"color: {color}; font-weight: bold; font-size: 14px;")
        
        # 更新进度 Banner 的步骤显示
        step_text = "空闲"
        if state in AUTO_SEQUENCE_STEPS:
            state_steps = AUTO_SEQUENCE_STEPS[state]
            step_text = state_steps.get(step, f"步骤 {step}")
        elif step > 0:
            step_text = f"步骤 {step}"
            
        self._auto_progress_label.setText(step_text)
        
    def _toggle_blinks(self):
        for item in self._devices.values():
            item.toggle_blink()
    
    def _update_branch_info(self, status: dict, state: int):
        """P2-1: 更新自动抽真空分支判据显示 - 使用顶部 Banner"""
        if state != SystemState.PUMPING:
            self._top_branch_label.setText("")
            return
        
        vacuum2 = status.get('vacuumGauge2', 101325)
        threshold = AUTO_BRANCH_THRESHOLDS.get("粗抽分支阈值Pa", 3000)
        
        if vacuum2 >= threshold:
            self._top_branch_label.setText(f"分支: 粗抽 ({vacuum2:.0f} Pa)")
            self._top_branch_label.setStyleSheet(f"color: #FF9800; font-size: 11px;")
        else:
            self._top_branch_label.setText(f"分支: 精抽 ({vacuum2:.0f} Pa)")
            self._top_branch_label.setStyleSheet(f"color: #00E5FF; font-size: 11px;")
    
    def _update_top_vacuum_displays(self, status: dict):
        """更新顶部状态栏真空计显示 - 使用科学记数法"""
        def format_vacuum(val):
            if val is None or not isinstance(val, (int, float)) or val <= 0:
                return "---"
            return f"{val:.2e}"
        
        v1 = status.get('vacuumGauge1', None)
        v2 = status.get('vacuumGauge2', None)
        v3 = status.get('vacuumGauge3', None)
        
        self._top_vacuum1.setText(format_vacuum(v1))
        self._top_vacuum2.setText(format_vacuum(v2))
        self._top_vacuum3.setText(format_vacuum(v3))
    
    def _update_utility_status(self, status: dict):
        """更新水路/气路状态 - 使用顶部状态栏"""
        # 水路状态（4路独立显示）
        for i in range(4):
            water_flow_ok = status.get(f'waterFlow{i+1}', False)
            self._top_water_indicators[i].set_state(water_flow_ok, is_alarm=not water_flow_ok)
            if water_flow_ok:
                self._top_water_indicators[i].setToolTip(f"冷却水路{i+1}: 正常")
            else:
                self._top_water_indicators[i].setToolTip(f"冷却水路{i+1}: 无水流!")
            
            # 更新拓扑图中的水路状态指示器
            # 注意：拓扑图中使用的键是 waterValve{i+1}State，需要匹配
            attr_name = f'waterValve{i+1}State'
            if attr_name in self._water_status_items:
                circle = self._water_status_items[attr_name]
                if water_flow_ok:
                    circle.setBrush(QBrush(QColor("#4CAF50")))  # 绿色 - 正常
                else:
                    circle.setBrush(QBrush(QColor("#F44336")))  # 红色 - 故障
        
        # 气路状态
        air_ok = status.get('airSupplyOk', False)
        air_pressure = status.get('airPressure', 0)
        self._top_air_indicator.set_state(air_ok, is_alarm=not air_ok)
        
        # 报警状态
        has_alarm = status.get('hasUnacknowledgedAlarm', False) or status.get('activeAlarmCount', 0) > 0
        self._top_alarm_indicator.set_state(has_alarm, is_alarm=True)
        
        # 气源压力显示
        self._top_air_pressure.setText(f"{air_pressure:.2f} MPa")
        if air_ok:
            self._top_air_pressure.setStyleSheet(f"color: #00E5FF; font-family: Consolas; font-size: 11px;")
        else:
            self._top_air_pressure.setStyleSheet(f"color: {COLORS['error']}; font-family: Consolas; font-size: 11px;")
        
        # 更新场景中的水路/气路状态显示 - 已移除
        # if hasattr(self, '_water_scene_items') and self._water_scene_items:
        #     for i in range(1, 5):
        #         idx = (i - 1) * 2
        #         flow_ok = status.get(f'waterFlow{i}', False)
        #         indicator = self._water_scene_items[idx]
        #         status_text = self._water_scene_items[idx + 1]
        #         
        #         if flow_ok:
        #             indicator.setBrush(QBrush(QColor(COLORS['success'])))
        #             status_text.setPlainText("有流")
        #             status_text.setDefaultTextColor(QColor(COLORS['success']))
        #         else:
        #             indicator.setBrush(QBrush(QColor(COLORS['error'])))
        #             status_text.setPlainText("无流")
        #             status_text.setDefaultTextColor(QColor(COLORS['error']))
        # 
        # if hasattr(self, '_air_scene_indicator'):
        #     if air_ok:
        #         self._air_scene_indicator.setBrush(QBrush(QColor(COLORS['success'])))
        #         self._air_scene_status.setPlainText("正常")
        #         self._air_scene_status.setDefaultTextColor(QColor(COLORS['success']))
        #     else:
        #         self._air_scene_indicator.setBrush(QBrush(QColor(COLORS['error'])))
        #         self._air_scene_status.setPlainText("不足")
        #         self._air_scene_status.setDefaultTextColor(QColor(COLORS['error']))
            
    def _update_prerequisites(self, status: dict):
        """更新先决条件面板"""
        if not hasattr(self, '_last_selected_op') or not self._last_selected_op:
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
        op_name = operation_names.get(self._last_selected_op, self._last_selected_op)
            
        prereqs = OPERATION_PREREQUISITES.get(self._last_selected_op, [])
        if not prereqs:
            self._prereq_panel.set_prerequisites([], op_name)
            return
            
        # 使用新的详细检查方法
        results = PrerequisiteChecker.check_all_with_details(self._last_selected_op, status)
        self._prereq_panel.set_prerequisites(results, op_name)

    def _check_prerequisites_before_action(self, operation: str) -> bool:
        """执行动作前检查"""
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

        # 关键：应用模态，阻止与主窗口交互
        msg_box.setWindowModality(Qt.ApplicationModal)
        msg_box.setModal(True)

        # Windows 下避免被主窗口“抢前台”盖住：强制置顶
        msg_box.setWindowFlags(msg_box.windowFlags() | Qt.WindowStaysOnTopHint)

        # 额外保险：弹窗存在时吞掉主窗口的鼠标/键盘事件
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
            msg_box.show()
            msg_box.raise_()
            msg_box.activateWindow()
            return msg_box.exec_()
        finally:
            main_win.removeEventFilter(blocker)

    def _check_manual_mode(self) -> bool:
        """检查是否可以手动操作"""
        if self._current_mode != OperationMode.MANUAL:
            self._show_centered_message(QMessageBox.Warning, "操作受限", 
                "当前为自动模式，手动操作已锁定。\n请先切换到手动模式。")
            return False
        return True
        
    def _send_device_command(self, attr_name, state):
        """发送设备控制命令 - 严格检查先决条件"""
        if not attr_name:
            return
        
        import re
        attr_lower = attr_name.lower()
        match = re.search(r'(\d+)', attr_name)
        idx = int(match.group(1)) if match else 0
            
        # 确定操作 key (用于检查先决条件) - 开启/关闭、启动/停止都要检查
        # 注意：关闭操作使用"对应的"设备编号，开启操作使用整体编号
        op_key = None
        if "power" in attr_lower:
            suffix = "Start" if state else "Stop"
            if "screw" in attr_lower: op_key = f"ScrewPump_{suffix}"
            elif "roots" in attr_lower: op_key = f"RootsPump_{suffix}"
            elif "molecular" in attr_lower:
                # 分子泵1-3启动需要对应的电磁阀和闸板阀分别开启
                if state and idx >= 1 and idx <= 3:
                    op_key = f"MolecularPump{idx}_Start"  # 使用单独条件，检查对应的电磁阀和闸板阀
                else:
                    op_key = f"MolecularPump_{suffix}"  # 停止或兼容性：使用批量配置
        elif "open" in attr_lower:
            if "gatevalve" in attr_lower:
                if idx <= 3:
                    # 闸板阀1-3：开启和关闭都使用对应的单独条件（检查对应的电磁阀）
                    if state:
                        op_key = f"GateValve{idx}_Open"  # 使用单独条件，检查对应的电磁阀
                    else:
                        op_key = f"GateValve{idx}_Close"  # 使用单独条件
                elif idx == 4:
                    op_key = f"GateValve4_{'Open' if state else 'Close'}"
                else:
                    op_key = f"GateValve5_{'Open' if state else 'Close'}"
            elif "ventvalve" in attr_lower:
                op_key = f"VentValve{idx}_{'Open' if state else 'Close'}"
            elif "electromagnetic" in attr_lower:
                if idx <= 3:
                    # 电磁阀1-3：开启用整体条件，关闭用对应的单独条件
                    if state:
                        op_key = "ElectromagneticValve123_Open"
                    else:
                        op_key = f"ElectromagneticValve{idx}_Close"  # 使用单独条件
                else:
                    op_key = f"ElectromagneticValve4_{'Open' if state else 'Close'}"
                
        # 严格检查先决条件（所有操作都必须检查）
        if op_key:
            if not self._check_prerequisites_before_action(op_key):
                return
        # 如果没有找到 op_key，说明是未在配置中定义的操作，允许执行但记录日志

        # 根据属性名确定命令并发送
        if "gatevalve" in attr_lower:
            self._worker.set_gate_valve(idx, state)
        elif "electromagneticvalve" in attr_lower:
            self._worker.set_electromagnetic_valve(idx, state)
        elif "ventvalve" in attr_lower:
            self._worker.set_vent_valve(idx, state)
        elif "molecularpump" in attr_lower:
            self._worker.set_molecular_pump_power(idx, state)
        elif "rootspump" in attr_lower:
            self._worker.set_roots_pump_power(state)
        elif "screwpump" in attr_lower:
            self._worker.set_screw_pump_power(state)
            
    # ===== 自动控制回调 =====
    
    def _on_switch_to_auto(self):
        res = self._show_centered_message(QMessageBox.Question, "确认",
            "确定要切换到自动模式吗？\n切换后手动操作将被锁定。",
            QMessageBox.Yes | QMessageBox.No)
        if res == QMessageBox.Yes:
            self._worker.switch_to_auto()
            
    def _on_switch_to_manual(self):
        res = self._show_centered_message(QMessageBox.Question, "确认",
            "确定要切换到手动模式吗？\n自动流程将被中止。",
            QMessageBox.Yes | QMessageBox.No)
        if res == QMessageBox.Yes:
            self._worker.switch_to_manual()
            
    def _on_vacuum_start(self):
        if not self._check_prerequisites_before_action("Auto_OneKeyVacuumStart"):
            return
        res = self._show_centered_message(QMessageBox.Question, "确认",
            "确定要启动一键抽真空流程吗？",
            QMessageBox.Yes | QMessageBox.No)
        if res == QMessageBox.Yes:
            self._worker.one_key_vacuum_start()
            
    def _on_vacuum_stop(self):
        if not self._check_prerequisites_before_action("Auto_OneKeyVacuumStop"):
            return
        res = self._show_centered_message(QMessageBox.Question, "确认",
            "确定要执行一键停机吗？",
            QMessageBox.Yes | QMessageBox.No)
        if res == QMessageBox.Yes:
            self._worker.one_key_vacuum_stop()
            
    def _on_chamber_vent(self):
        if not self._btn_vent.isEnabled():
            self._show_centered_message(QMessageBox.Information, "操作受限", "抽真空流程进行中：腔室放气已互锁屏蔽。")
            return
        if not self._check_prerequisites_before_action("Auto_ChamberVent"):
            return
        res = self._show_centered_message(QMessageBox.Question, "确认",
            "确定要进行腔室放气吗？",
            QMessageBox.Yes | QMessageBox.No)
        if res == QMessageBox.Yes:
            self._worker.chamber_vent()

    def _on_fault_reset(self):
        """故障复位"""
        if not self._check_prerequisites_before_action("Auto_FaultReset"):
            return
        self._worker.fault_reset()
            
    def _on_emergency_stop(self):
        res = self._show_centered_message(QMessageBox.Warning, "紧急停止确认",
            "确定要执行紧急停止吗？\n\n这将立即关闭所有泵和阀门！",
            QMessageBox.Yes | QMessageBox.No)
        if res == QMessageBox.Yes:
            self._worker.queue_command("EmergencyStop")
            self._show_centered_message(QMessageBox.Information, "紧急停止", "已发送紧急停止命令")
    
    def closeEvent(self, event):
        """页面关闭时清理资源"""
        # 停止所有定时器
        if hasattr(self, '_poll_timer') and self._poll_timer.isActive():
            self._poll_timer.stop()
        if hasattr(self, '_blink_timer') and self._blink_timer.isActive():
            self._blink_timer.stop()
        if hasattr(self, '_time_timer') and self._time_timer.isActive():
            self._time_timer.stop()
        super().closeEvent(event)
