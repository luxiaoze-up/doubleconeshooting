"""
真空腔体系统控制 GUI - 启动入口
Entry point for Vacuum Chamber System Control GUI
"""

import sys
import os

# 添加项目根目录到 Python 路径
current_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(os.path.dirname(current_dir))
if project_root not in sys.path:
    sys.path.insert(0, project_root)

from PyQt5.QtWidgets import QApplication
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QIcon

# 导入主窗口
from gui.vacuum_chamber_gui import VacuumChamberMainWindow

def main():
    # 启用高DPI支持
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps)
    
    app = QApplication(sys.argv)
    
    # 设置应用信息
    app.setApplicationName("VacuumChamberControl")
    app.setOrganizationName("DoubleConeShooting")
    
    # 设置全局应用图标 (使用绝对路径)
    logo_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logo.png")
    app.setWindowIcon(QIcon(logo_path))
    
    # 创建并显示主窗口
    window = VacuumChamberMainWindow()
    window.show()
    
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()
