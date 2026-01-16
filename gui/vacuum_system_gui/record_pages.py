"""
真空系统记录页面

包含:
- 报警记录页
- 操作历史页
- 趋势曲线页
"""

from datetime import datetime
from typing import List, Dict
from collections import deque

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTableWidget, QTableWidgetItem,
    QPushButton, QLabel, QHeaderView, QFrame, QSplitter, QGroupBox,
    QCheckBox, QComboBox, QDateTimeEdit
)
from PyQt5.QtCore import Qt, QTimer, QDateTime
from PyQt5.QtGui import QColor

from config import TREND_CONFIG, UI_CONFIG, ALARM_TYPES
from styles import COLORS
from styles import ALARM_STYLES
from alarm_manager import AlarmManager, AlarmRecord
from tango_worker import VacuumTangoWorker
from utils.logger import get_logger

logger = get_logger(__name__)

# 尝试导入 pyqtgraph
try:
    import pyqtgraph as pg
    HAS_PYQTGRAPH = True
except ImportError:
    HAS_PYQTGRAPH = False
    import logging
    logging.warning("pyqtgraph 未安装，趋势曲线功能不可用")


class AlarmRecordPage(QWidget):
    """报警记录页面"""
    
    def __init__(self, alarm_manager: AlarmManager, parent=None):
        super().__init__(parent)
        
        self._alarm_manager = alarm_manager
        
        self._init_ui()
        self._connect_signals()
        self._refresh_table()
        
    def _init_ui(self):
        """初始化 UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)
        
        # 工具栏
        toolbar = QHBoxLayout()
        
        self._btn_ack_all = QPushButton("确认所有")
        self._btn_ack_all.clicked.connect(self._on_ack_all)
        toolbar.addWidget(self._btn_ack_all)
        
        self._btn_clear = QPushButton("清除历史")
        self._btn_clear.clicked.connect(self._on_clear_history)
        toolbar.addWidget(self._btn_clear)
        
        toolbar.addStretch()
        
        self._status_label = QLabel("当前报警: 0 | 未确认: 0")
        toolbar.addWidget(self._status_label)
        
        layout.addLayout(toolbar)
        
        # 分隔 - 活跃报警 和 历史记录
        splitter = QSplitter(Qt.Vertical)
        
        # 活跃报警
        active_group = QGroupBox("活跃报警")
        active_layout = QVBoxLayout(active_group)
        
        self._active_table = QTableWidget()
        self._active_table.setColumnCount(5)
        self._active_table.setHorizontalHeaderLabels([
            "时间", "报警码", "设备", "描述", "状态"
        ])
        self._active_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.Stretch)
        self._active_table.setSelectionBehavior(QTableWidget.SelectRows)
        active_layout.addWidget(self._active_table)
        
        splitter.addWidget(active_group)
        
        # 历史报警
        history_group = QGroupBox("历史记录")
        history_layout = QVBoxLayout(history_group)
        
        self._history_table = QTableWidget()
        self._history_table.setColumnCount(5)
        self._history_table.setHorizontalHeaderLabels([
            "时间", "报警码", "设备", "描述", "状态"
        ])
        # 设置列宽
        self._history_table.setColumnWidth(0, 180)  # 时间列进一步加宽
        self._history_table.setColumnWidth(1, 80)
        self._history_table.setColumnWidth(2, 80)
        
        self._history_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.Stretch)
        self._history_table.setSelectionBehavior(QTableWidget.SelectRows)
        history_layout.addWidget(self._history_table)
        
        splitter.addWidget(history_group)
        
        splitter.setSizes([300, 400])
        layout.addWidget(splitter)
        
    def _connect_signals(self):
        """连接信号"""
        self._alarm_manager.alarms_changed.connect(self._refresh_table)
        
    def _refresh_table(self):
        """刷新表格"""
        # 活跃报警
        active_alarms = self._alarm_manager.get_active_alarms()
        self._active_table.setRowCount(len(active_alarms))
        
        for row, alarm in enumerate(active_alarms):
            self._set_table_row(self._active_table, row, alarm)
            
            # 未确认的高亮
            if not alarm.acknowledged:
                for col in range(5):
                    item = self._active_table.item(row, col)
                    if item:
                        item.setBackground(QColor("#4A1C1C"))
                        
        # 历史记录
        history_alarms = self._alarm_manager.get_history_alarms()
        self._history_table.setRowCount(len(history_alarms))
        
        for row, alarm in enumerate(reversed(history_alarms)):  # 最新的在前
            self._set_table_row(self._history_table, row, alarm)
            
        # 更新状态
        unack_count = len(self._alarm_manager.get_unacknowledged_alarms())
        self._status_label.setText(
            f"当前报警: {len(active_alarms)} | 未确认: {unack_count}"
        )
        
    def _set_table_row(self, table: QTableWidget, row: int, alarm: AlarmRecord):
        """设置表格行"""
        # 格式化时间
        try:
            dt = datetime.fromisoformat(alarm.timestamp)
            time_str = dt.strftime("%Y-%m-%d %H:%M:%S")
        except (ValueError, AttributeError) as e:
            logger.warning(f"时间格式解析失败: {e}, 使用原始值: {alarm.timestamp}")
            time_str = str(alarm.timestamp)
            
        items = [
            time_str,
            str(alarm.alarm_code),
            alarm.device_name,
            alarm.description,
            "已确认" if alarm.acknowledged else "未确认"
        ]
        
        for col, text in enumerate(items):
            item = QTableWidgetItem(text)
            item.setFlags(item.flags() & ~Qt.ItemIsEditable)
            table.setItem(row, col, item)
            
    def _on_ack_all(self):
        """确认所有"""
        self._alarm_manager.acknowledge_all()
        
    def _on_clear_history(self):
        """清除历史"""
        self._alarm_manager.clear_history()
        self._refresh_table()


class OperationHistoryPage(QWidget):
    """操作历史页面"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        self._history: List[Dict] = []
        
        self._init_ui()
        
    def _init_ui(self):
        """初始化 UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)
        
        # 筛选栏
        filter_layout = QHBoxLayout()
        
        filter_layout.addWidget(QLabel("时间范围:"))
        
        self._start_time = QDateTimeEdit()
        self._start_time.setDateTime(QDateTime.currentDateTime().addDays(-1))
        filter_layout.addWidget(self._start_time)
        
        filter_layout.addWidget(QLabel("至"))
        
        self._end_time = QDateTimeEdit()
        self._end_time.setDateTime(QDateTime.currentDateTime())
        filter_layout.addWidget(self._end_time)
        
        self._btn_filter = QPushButton("筛选")
        self._btn_filter.clicked.connect(self._on_filter)
        filter_layout.addWidget(self._btn_filter)
        
        filter_layout.addStretch()
        
        layout.addLayout(filter_layout)
        
        # 操作表格
        self._table = QTableWidget()
        self._table.setColumnCount(4)
        self._table.setHorizontalHeaderLabels([
            "时间", "操作类型", "设备", "描述"
        ])
        # 设置列宽
        self._table.setColumnWidth(0, 180)  # 时间列进一步加宽
        self._table.setColumnWidth(1, 100)
        self._table.setColumnWidth(2, 80)
        
        self._table.horizontalHeader().setSectionResizeMode(3, QHeaderView.Stretch)
        self._table.setSelectionBehavior(QTableWidget.SelectRows)
        
        layout.addWidget(self._table)
        
    def add_operation(self, operation_type: str, device: str, description: str):
        """添加操作记录"""
        record = {
            "timestamp": datetime.now().isoformat(),
            "type": operation_type,
            "device": device,
            "description": description
        }
        self._history.append(record)
        self._refresh_table()
        
    def _refresh_table(self):
        """刷新表格"""
        self._table.setRowCount(len(self._history))
        
        for row, record in enumerate(reversed(self._history)):
            try:
                dt = datetime.fromisoformat(record["timestamp"])
                time_str = dt.strftime("%Y-%m-%d %H:%M:%S")
            except (ValueError, AttributeError, KeyError) as e:
                logger.warning(f"时间格式解析失败: {e}, 使用原始值: {record.get('timestamp', 'N/A')}")
                time_str = str(record.get("timestamp", "N/A"))
                
            items = [
                time_str,
                record["type"],
                record["device"],
                record["description"]
            ]
            
            for col, text in enumerate(items):
                item = QTableWidgetItem(text)
                item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                self._table.setItem(row, col, item)
                
    def _on_filter(self):
        """筛选"""
        # 简单实现，实际应根据时间范围过滤
        self._refresh_table()


class TrendCurvePage(QWidget):
    """趋势曲线页面"""
    
    def __init__(self, worker: VacuumTangoWorker, parent=None):
        super().__init__(parent)
        
        self._worker = worker
        
        # 数据存储
        self._max_points = TREND_CONFIG['max_points']
        self._data_buffers = {}
        self._time_buffer = deque(maxlen=self._max_points)
        
        for channel in TREND_CONFIG['channels']:
            self._data_buffers[channel['attr']] = deque(maxlen=self._max_points)
            
        self._init_ui()
        self._start_update()
        
    def _init_ui(self):
        """初始化 UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        
        # 控制栏
        control_layout = QHBoxLayout()
        
        # 通道选择
        control_layout.addWidget(QLabel("显示通道:"))
        
        self._channel_checks = {}
        for channel in TREND_CONFIG['channels']:
            check = QCheckBox(channel['name'])
            check.setChecked(True)
            check.stateChanged.connect(self._update_curves)
            self._channel_checks[channel['attr']] = check
            control_layout.addWidget(check)
            
        control_layout.addStretch()
        
        # 时间范围
        control_layout.addWidget(QLabel("时间范围:"))
        self._time_range = QComboBox()
        self._time_range.addItems(["1分钟", "5分钟", "10分钟", "30分钟"])
        self._time_range.setCurrentIndex(1)
        control_layout.addWidget(self._time_range)
        
        layout.addLayout(control_layout)
        
        # 图表区域
        if HAS_PYQTGRAPH:
            # 使用 pyqtgraph
            pg.setConfigOptions(antialias=True)
            
            self._plot_widget = pg.PlotWidget()
            self._plot_widget.setBackground(COLORS['surface'])
            self._plot_widget.showGrid(x=True, y=True, alpha=0.3)
            self._plot_widget.setLabel('left', '数值')
            self._plot_widget.setLabel('bottom', '时间 (s)')
            
            # 添加图例
            self._plot_widget.addLegend()
            
            # 创建曲线
            self._curves = {}
            for channel in TREND_CONFIG['channels']:
                pen = pg.mkPen(color=channel['color'], width=2)
                curve = self._plot_widget.plot([], [], pen=pen, name=channel['name'])
                self._curves[channel['attr']] = curve
                
            layout.addWidget(self._plot_widget, 1)
        else:
            # 无 pyqtgraph
            placeholder = QLabel("趋势曲线功能需要安装 pyqtgraph 库")
            placeholder.setAlignment(Qt.AlignCenter)
            placeholder.setStyleSheet(f"""
                background-color: {COLORS['surface']};
                color: {COLORS['text_secondary']};
                padding: 40px;
                border-radius: 8px;
            """)
            layout.addWidget(placeholder, 1)
            
    def _start_update(self):
        """启动更新"""
        self._update_timer = QTimer(self)
        self._update_timer.timeout.connect(self._update_data)
        self._update_timer.start(TREND_CONFIG['update_interval_ms'])
        
        self._start_time = datetime.now()
        
    def _update_data(self):
        """更新数据"""
        status = self._worker.get_cached_status()
        if not status:
            return
            
        # 计算相对时间
        elapsed = (datetime.now() - self._start_time).total_seconds()
        self._time_buffer.append(elapsed)
        
        # 更新各通道数据
        for channel in TREND_CONFIG['channels']:
            attr = channel['attr']
            value = status.get(attr, 0)
            self._data_buffers[attr].append(value)
            
        # 更新曲线
        self._update_curves()
        
    def _update_curves(self):
        """更新曲线显示"""
        if not HAS_PYQTGRAPH:
            return
            
        time_data = list(self._time_buffer)
        
        for channel in TREND_CONFIG['channels']:
            attr = channel['attr']
            curve = self._curves.get(attr)
            check = self._channel_checks.get(attr)
            
            if curve and check:
                if check.isChecked():
                    curve.setData(time_data, list(self._data_buffers[attr]))
                    curve.show()
                else:
                    curve.hide()
    
    def closeEvent(self, event):
        """页面关闭时清理资源"""
        # 停止更新定时器
        if hasattr(self, '_update_timer') and self._update_timer.isActive():
            self._update_timer.stop()
        super().closeEvent(event)

