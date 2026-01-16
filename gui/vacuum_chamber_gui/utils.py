"""
真空腔体系统控制 GUI - 工具函数
Utils for Vacuum Chamber System Control GUI
"""

from PyQt5.QtWidgets import QApplication, QMessageBox


def get_main_window():
    """
    获取主窗口实例
    遍历所有顶层窗口，查找 VacuumChamberMainWindow
    """
    app = QApplication.instance()
    if app:
        # 优先使用当前活动窗口
        active = app.activeWindow()
        if active and active.__class__.__name__ == 'VacuumChamberMainWindow':
            return active
        
        # 如果活动窗口不是主窗口，遍历查找
        for widget in app.topLevelWidgets():
            if widget.__class__.__name__ == 'VacuumChamberMainWindow':
                return widget
    return None


def _center_message_box(msg_box, parent):
    """
    手动将消息框居中于父窗口或屏幕中心
    """
    # 先计算消息框的尺寸
    msg_box.adjustSize()
    
    if parent and parent.isVisible():
        # 相对于父窗口居中
        parent_geo = parent.frameGeometry()
        box_width = msg_box.sizeHint().width()
        box_height = msg_box.sizeHint().height()
        
        x = parent_geo.x() + (parent_geo.width() - box_width) // 2
        y = parent_geo.y() + (parent_geo.height() - box_height) // 2
        
        # 确保不会超出屏幕
        screen = QApplication.primaryScreen()
        if screen:
            screen_geo = screen.availableGeometry()
            x = max(screen_geo.x(), min(x, screen_geo.right() - box_width))
            y = max(screen_geo.y(), min(y, screen_geo.bottom() - box_height))
        
        msg_box.move(x, y)
    else:
        # 相对于屏幕居中
        screen = QApplication.primaryScreen()
        if screen:
            screen_geo = screen.availableGeometry()
            box_width = msg_box.sizeHint().width()
            box_height = msg_box.sizeHint().height()
            msg_box.move(
                screen_geo.x() + (screen_geo.width() - box_width) // 2,
                screen_geo.y() + (screen_geo.height() - box_height) // 2
            )


def show_warning(title: str, message: str, parent=None):
    """
    显示警告消息框（居中于主窗口）
    
    Args:
        title: 对话框标题
        message: 消息内容
        parent: 可选的父窗口，如果不指定则使用主窗口
    """
    if parent is None:
        parent = get_main_window()
    
    msg_box = QMessageBox(parent)
    msg_box.setIcon(QMessageBox.Warning)
    msg_box.setWindowTitle(title)
    msg_box.setText(message)
    msg_box.setStandardButtons(QMessageBox.Ok)
    
    # 确保弹出框居中
    _center_message_box(msg_box, parent)
    
    msg_box.exec_()


def show_info(title: str, message: str, parent=None):
    """
    显示信息消息框（居中于主窗口）
    
    Args:
        title: 对话框标题
        message: 消息内容
        parent: 可选的父窗口，如果不指定则使用主窗口
    """
    if parent is None:
        parent = get_main_window()
    
    msg_box = QMessageBox(parent)
    msg_box.setIcon(QMessageBox.Information)
    msg_box.setWindowTitle(title)
    msg_box.setText(message)
    msg_box.setStandardButtons(QMessageBox.Ok)
    
    # 确保弹出框居中
    _center_message_box(msg_box, parent)
    
    msg_box.exec_()


def show_error(title: str, message: str, parent=None):
    """
    显示错误消息框（居中于主窗口）
    
    Args:
        title: 对话框标题
        message: 消息内容
        parent: 可选的父窗口，如果不指定则使用主窗口
    """
    if parent is None:
        parent = get_main_window()
    
    msg_box = QMessageBox(parent)
    msg_box.setIcon(QMessageBox.Critical)
    msg_box.setWindowTitle(title)
    msg_box.setText(message)
    msg_box.setStandardButtons(QMessageBox.Ok)
    
    # 确保弹出框居中
    _center_message_box(msg_box, parent)
    
    msg_box.exec_()


def show_question(title: str, message: str, parent=None) -> bool:
    """
    显示询问消息框（居中于主窗口）
    
    Args:
        title: 对话框标题
        message: 消息内容
        parent: 可选的父窗口，如果不指定则使用主窗口
        
    Returns:
        bool: 用户点击"是"返回 True，点击"否"返回 False
    """
    if parent is None:
        parent = get_main_window()
    
    msg_box = QMessageBox(parent)
    msg_box.setIcon(QMessageBox.Question)
    msg_box.setWindowTitle(title)
    msg_box.setText(message)
    msg_box.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
    msg_box.setDefaultButton(QMessageBox.No)
    
    # 确保弹出框居中
    _center_message_box(msg_box, parent)
    
    return msg_box.exec_() == QMessageBox.Yes

