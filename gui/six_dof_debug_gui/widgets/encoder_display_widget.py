"""
编码器显示控件
Encoder Display Widget
实时显示6个轴的编码器数据
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QLabel, QTableWidget, QTableWidgetItem, QHeaderView
)
from PyQt5.QtCore import Qt, QTimer

# 使用完整包路径导入（开发和打包环境一致）
from gui.six_dof_debug_gui.hardware.encoder_collector import EncoderCollector


class EncoderDisplayWidget(QWidget):
    """编码器显示控件"""
    
    def __init__(self, encoder_collector: EncoderCollector = None, parent=None):
        super().__init__(parent)
        self.encoder_collector = encoder_collector
        self.update_timer = QTimer(self)
        self.update_timer.timeout.connect(self._update_display)
        
        self._init_ui()
        
        # 启动定时更新（默认100ms）
        self.update_timer.start(100)
    
    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        
        # 标题
        title = QLabel("编码器数据")
        title.setStyleSheet("font-size: 14px; font-weight: bold;")
        layout.addWidget(title)
        
        # 表格
        self.table = QTableWidget(6, 4, self)
        self.table.setHorizontalHeaderLabels(["轴号", "通道", "原始值", "位置(mm)"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.verticalHeader().setVisible(False)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.setAlternatingRowColors(True)
        
        # 初始化表格行
        axis_names = ["轴1", "轴2", "轴3", "轴4", "轴5", "轴6"]
        for i in range(6):
            # 轴号
            self.table.setItem(i, 0, QTableWidgetItem(axis_names[i]))
            # 通道
            self.table.setItem(i, 1, QTableWidgetItem(str(i)))
            # 原始值
            self.table.setItem(i, 2, QTableWidgetItem("0"))
            # 位置
            self.table.setItem(i, 3, QTableWidgetItem("0.000"))
        
        layout.addWidget(self.table)
    
    def set_encoder_collector(self, encoder_collector: EncoderCollector):
        """设置编码器采集器实例"""
        self.encoder_collector = encoder_collector
    
    def _update_display(self):
        """更新显示"""
        if not self.encoder_collector or not self.encoder_collector.is_connected():
            # 显示未连接状态
            for i in range(6):
                self.table.setItem(i, 2, QTableWidgetItem("---"))
                self.table.setItem(i, 3, QTableWidgetItem("---"))
            return
        
        # 获取所有通道的读数
        readings = self.encoder_collector.get_all_readings()
        
        for i in range(6):
            reading = readings.get(i)
            if reading:
                raw_value, timestamp, position_mm = reading
                # 更新原始值
                self.table.setItem(i, 2, QTableWidgetItem(str(raw_value)))
                # 更新位置（保留3位小数）
                self.table.setItem(i, 3, QTableWidgetItem(f"{position_mm:.3f}"))
            else:
                # 无数据
                self.table.setItem(i, 2, QTableWidgetItem("---"))
                self.table.setItem(i, 3, QTableWidgetItem("---"))
    
    def set_update_interval(self, interval_ms: int):
        """设置更新间隔（毫秒）"""
        self.update_timer.setInterval(interval_ms)
    
    def start_update(self):
        """开始更新"""
        self.update_timer.start()
    
    def stop_update(self):
        """停止更新"""
        self.update_timer.stop()
