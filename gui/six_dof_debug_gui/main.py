"""
六自由度机器人调试GUI - 启动入口
Six DOF Robot Debug GUI - Entry Point

重要：PyQt5 要求 QApplication 必须在任何 QWidget 之前创建。
在 PyInstaller 打包环境中，模块加载顺序可能导致问题。
此入口文件确保正确的初始化顺序。
"""

import sys
import os
import traceback
from pathlib import Path


def setup_paths():
    """设置Python路径（支持打包和开发环境）"""
    # 检查是否在PyInstaller打包环境中
    if getattr(sys, 'frozen', False):
        # 打包环境：从exe所在目录确定路径
        exe_dir = Path(sys.executable).parent
        # PyInstaller会在exe同目录创建_internal文件夹
        if (exe_dir / "_internal").exists():
            # onedir模式：exe在SixDofDebugGUI目录下
            app_dir = exe_dir
            project_root = exe_dir.parent
        else:
            app_dir = exe_dir
            project_root = exe_dir
        
        # 添加应用目录到Python路径（用于导入打包的模块）
        if str(app_dir) not in sys.path:
            sys.path.insert(0, str(app_dir))
    else:
        # 开发环境：从当前文件所在目录确定路径
        current_dir = Path(__file__).parent
        project_root = current_dir.parent.parent
    
    # 添加项目根目录到Python路径
    if str(project_root) not in sys.path:
        sys.path.insert(0, str(project_root))
    
    return project_root


def setup_qt_env():
    """设置Qt环境变量（在导入PyQt5之前调用）"""
    # 设置高DPI相关环境变量
    os.environ['QT_AUTO_SCREEN_SCALE_FACTOR'] = '1'
    
    # 尝试设置Qt插件路径
    try:
        import importlib.util
        spec = importlib.util.find_spec('PyQt5')
        if spec and spec.origin:
            pyqt5_base = os.path.dirname(spec.origin)
            qt5_plugins = os.path.join(pyqt5_base, 'Qt5', 'plugins')
            if os.path.exists(qt5_plugins):
                os.environ['QT_PLUGIN_PATH'] = qt5_plugins
                return qt5_plugins
    except Exception:
        pass
    return None


def run_app():
    """
    运行应用程序
    
    这个函数包含所有PyQt5相关代码，确保在正确的时机导入
    """
    # 第1步：导入并创建 QApplication（必须最先）
    from PyQt5.QtCore import Qt
    from PyQt5.QtWidgets import QApplication
    
    # 设置Qt属性（必须在创建QApplication实例之前）
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps)
    
    # 创建 QApplication 实例
    app = QApplication(sys.argv)
    app.setApplicationName("SixDofDebugGUI")
    app.setOrganizationName("DoubleConeShooting")
    
    # 第2步：现在可以安全地导入 QWidget 相关模块
    from PyQt5.QtWidgets import QMessageBox
    from PyQt5.QtGui import QIcon
    
    # 第3步：导入主窗口（这会导入其他 widget 模块）
    try:
        from gui.six_dof_debug_gui.main_window import SixDofDebugMainWindow
    except ImportError:
        # 如果相对导入失败，尝试直接导入（打包环境）
        from main_window import SixDofDebugMainWindow
    
    # 第4步：创建并显示主窗口
    try:
        window = SixDofDebugMainWindow()
        
        # 确保窗口显示在屏幕中央
        window.show()
        window.raise_()  # 提升窗口到最前
        window.activateWindow()  # 激活窗口
    except Exception as e:
        # 友好的错误提示
        user_msg = (
            "无法启动应用程序\n\n"
            "程序在创建主窗口时遇到了问题。\n\n"
            "可能的原因：\n"
            "• 缺少必要的配置文件\n"
            "• 系统资源不足\n"
            "• 程序文件损坏\n\n"
            "建议操作：\n"
            "1. 检查程序文件是否完整\n"
            "2. 尝试重新启动程序\n"
            "3. 如果问题持续，请联系技术支持"
        )
        detail_msg = f"技术详情：{str(e)}"
        if not getattr(sys, 'frozen', False):
            detail_msg += f"\n\n{traceback.format_exc()}"
        
        QMessageBox.critical(None, "启动失败", f"{user_msg}\n\n{detail_msg}")
        sys.exit(1)
    
    # 第5步：运行应用
    sys.exit(app.exec_())


def main():
    """主函数 - 程序入口点"""
    # 第1步：设置Python路径
    setup_paths()
    
    # 第2步：设置Qt环境变量（在导入PyQt5之前）
    setup_qt_env()
    
    # 第3步：运行应用
    try:
        run_app()
    except ImportError as e:
        # 模块导入失败
        user_msg = (
            "无法启动应用程序\n\n"
            "程序缺少必要的组件或模块。\n\n"
            "可能的原因：\n"
            "• 缺少必要的Python库（如PyQt5）\n"
            "• 程序文件不完整\n"
            "• 环境配置不正确\n\n"
            "建议操作：\n"
            "1. 检查程序安装是否完整\n"
            "2. 重新安装程序\n"
            "3. 联系技术支持获取帮助"
        )
        detail_msg = f"缺少模块：{str(e)}"
        if not getattr(sys, 'frozen', False):
            detail_msg += f"\n\n{traceback.format_exc()}"
        
        # 尝试显示错误对话框
        try:
            from PyQt5.QtWidgets import QApplication, QMessageBox
            _app = QApplication(sys.argv)
            QMessageBox.critical(None, "启动失败", f"{user_msg}\n\n{detail_msg}")
        except Exception:
            # 如果无法显示对话框，输出到控制台
            print(f"{user_msg}\n\n{detail_msg}")
        
        if not getattr(sys, 'frozen', False):
            input("\n按Enter键退出...")
        sys.exit(1)
        
    except Exception as e:
        # 其他未预期的错误
        user_msg = (
            "无法启动应用程序\n\n"
            "程序在启动过程中遇到了意外错误。\n\n"
            "建议操作：\n"
            "1. 尝试重新启动程序\n"
            "2. 检查系统是否满足运行要求\n"
            "3. 如果问题持续，请联系技术支持"
        )
        detail_msg = f"错误信息：{str(e)}"
        if not getattr(sys, 'frozen', False):
            detail_msg += f"\n\n{traceback.format_exc()}"
        
        # 尝试显示错误对话框
        try:
            from PyQt5.QtWidgets import QApplication, QMessageBox
            _app = QApplication(sys.argv)
            QMessageBox.critical(None, "启动失败", f"{user_msg}\n\n{detail_msg}")
        except Exception:
            # 如果无法显示对话框，输出到控制台
            print(f"{user_msg}\n\n{detail_msg}")
        
        if not getattr(sys, 'frozen', False):
            input("\n按Enter键退出...")
        sys.exit(1)


if __name__ == "__main__":
    main()
