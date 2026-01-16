"""
PyInstaller 运行时钩子
在主程序运行之前设置 Qt 环境变量和属性

这个钩子解决了 "QWidget: Must construct a QApplication before a QWidget" 错误
该错误发生在 PyInstaller 打包环境中，因为模块加载顺序与开发环境不同
"""

import os
import sys

def setup_qt_env():
    """设置 Qt 环境（在任何 PyQt5 导入之前）"""
    # 禁用 Qt 的某些早期检查
    os.environ['QT_AUTO_SCREEN_SCALE_FACTOR'] = '1'
    
    # 找到 PyQt5 的路径并设置插件路径
    try:
        # 使用 importlib 来查找路径，避免触发完整导入
        import importlib.util
        spec = importlib.util.find_spec('PyQt5')
        if spec and spec.origin:
            pyqt5_base = os.path.dirname(spec.origin)
            qt5_plugins = os.path.join(pyqt5_base, 'Qt5', 'plugins')
            if os.path.exists(qt5_plugins):
                os.environ['QT_PLUGIN_PATH'] = qt5_plugins
    except Exception:
        pass

# 执行设置
setup_qt_env()
