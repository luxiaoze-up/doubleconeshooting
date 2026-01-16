#!/usr/bin/env python3
"""
最小化测试脚本 - 用于诊断 GUI 阻塞问题
运行方式: python -m vacuum_chamber_gui.test_blocking
"""

import sys
import os
import datetime

# 添加项目根目录
current_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(os.path.dirname(current_dir))
if project_root not in sys.path:
    sys.path.insert(0, project_root)

from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QPushButton, QLabel
from PyQt5.QtCore import Qt, QTimer


def _ts():
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


class MinimalTestWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Blocking Test")
        self.resize(400, 300)
        
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        
        # 心跳标签
        self.heartbeat_label = QLabel("心跳: ---")
        layout.addWidget(self.heartbeat_label)
        
        # 测试按钮
        btn1 = QPushButton("测试按钮 1 (无操作)")
        btn1.clicked.connect(lambda: print(f"[{_ts()}] Button 1 clicked", flush=True))
        layout.addWidget(btn1)
        
        btn2 = QPushButton("测试按钮 2 (模拟阻塞 2秒)")
        btn2.clicked.connect(self._simulate_block)
        layout.addWidget(btn2)
        
        # 状态标签
        self.status_label = QLabel("状态: 正常")
        layout.addWidget(self.status_label)
        
        layout.addStretch()
        
        # 心跳定时器
        self._heartbeat_count = 0
        self._heartbeat = QTimer(self)
        self._heartbeat.timeout.connect(self._on_heartbeat)
        self._heartbeat.start(200)
        
    def _on_heartbeat(self):
        self._heartbeat_count += 1
        self.heartbeat_label.setText(f"心跳: {self._heartbeat_count} [{_ts()}]")
        print(f"[HEARTBEAT {_ts()}] #{self._heartbeat_count}", flush=True)
        
    def _simulate_block(self):
        print(f"[{_ts()}] Simulating 2 second block...", flush=True)
        import time
        time.sleep(2)
        print(f"[{_ts()}] Block finished", flush=True)


def main():
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps)
    
    app = QApplication(sys.argv)
    
    window = MinimalTestWindow()
    window.show()
    
    print(f"[{_ts()}] Test window started. Watch heartbeat in console.", flush=True)
    print(f"[{_ts()}] Click buttons to test blocking behavior.", flush=True)
    
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
