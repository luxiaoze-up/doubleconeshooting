"""
真空腔体系统控制 GUI - 主窗口
Main Window for Vacuum Chamber System Control GUI
"""

from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QStackedWidget, QPushButton, QButtonGroup, QLabel,
    QFrame, QSplitter, QCheckBox
)
from PyQt5.QtCore import Qt, QSize, QTimer, QDateTime
from PyQt5.QtGui import QIcon, QPixmap

from .config import NAVIGATION, UI_SETTINGS, TARGET_POSITIONING_CONFIG, REFLECTION_IMAGING_CONFIG, AUXILIARY_SUPPORT_CONFIG, DEVICES
from .styles import VACUUM_CHAMBER_THEME
from .pages import (
    TargetPositioningPage,
    ReflectionImagingPage,
    AuxiliarySupportPage,
    VacuumControlPage
)
from .simulation_mode_manager import SimulationModeManager


class VacuumChamberMainWindow(QMainWindow):
    """
    主应用程序窗口
    
    布局结构:
    ┌────────────┬─────────────────────────────┐
    │            │                             │
    │  Sidebar   │       Content Area          │
    │  (nav)     │    (QStackedWidget)         │
    │            │   (包含各自的状态面板)      │
    │            │                             │
    └────────────┴─────────────────────────────┘
    
    注意：状态面板已整合到各自的功能页面中
    """
    
    def __init__(self):
        super().__init__()
        
        self.setWindowTitle(UI_SETTINGS["window_title"])
        self.resize(*UI_SETTINGS["window_size"])
        
        # 设置窗口图标 (使用绝对路径)
        import os
        current_dir = os.path.dirname(os.path.abspath(__file__))
        self.logo_path = os.path.join(current_dir, "logo.png")
        self.setWindowIcon(QIcon(self.logo_path))
        
        # 应用样式
        self.setStyleSheet(VACUUM_CHAMBER_THEME)
        
        # 页面实例
        self._pages = {}
        self._nav_buttons = {}
        
        # 全局模拟模式管理器
        self.sim_mode_manager = SimulationModeManager.instance()
        # 注册所有支持模拟模式的设备
        self._register_sim_mode_devices()
        
        self._setup_ui()
        
    def _setup_ui(self):
        # 中央部件
        central = QWidget()
        self.setCentralWidget(central)
        
        # 主水平布局
        main_layout = QHBoxLayout(central)
        main_layout.setSpacing(0)
        main_layout.setContentsMargins(0, 0, 0, 0)
        
        # 1. 左侧导航栏
        sidebar = self._create_sidebar()
        main_layout.addWidget(sidebar)
        
        # 2. 中间内容堆栈（状态面板已整合到各自页面中）
        self.content_stack = QStackedWidget()
        self.content_stack.setObjectName("contentStack")
        main_layout.addWidget(self.content_stack)
        
        # 创建所有页面
        self._create_pages()
        
        # 默认显示第一页
        if NAVIGATION:
            self._switch_to_page(NAVIGATION[0]["id"])
            
    def _create_sidebar(self) -> QWidget:
        """创建左侧导航栏"""
        sidebar = QWidget()
        sidebar.setObjectName("sidebar")
        sidebar.setFixedWidth(UI_SETTINGS["sidebar_width"])
        
        layout = QVBoxLayout(sidebar)
        layout.setSpacing(4)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # 顶部标题/Logo区域
        header = QFrame()
        header.setStyleSheet("background-color: #08101c; border-bottom: 1px solid #1c3146;")
        header.setFixedHeight(60)
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(10, 0, 10, 0)
        header_layout.setSpacing(10)
        
        # Logo Image
        logo_lbl = QLabel()
        # Use the absolute path defined in __init__
        logo_pixmap = QPixmap(self.logo_path)
        if not logo_pixmap.isNull():
            logo_lbl.setPixmap(logo_pixmap.scaled(32, 32, Qt.KeepAspectRatio, Qt.SmoothTransformation))
            header_layout.addWidget(logo_lbl)
        
        title = QLabel("打靶控制系统")
        title.setStyleSheet("color: #29d6ff; font-size: 16px; font-weight: 700;")
        # header_layout.addWidget(title, 0, Qt.AlignCenter) # Removed AlignCenter to flow with logo
        header_layout.addWidget(title)
        header_layout.addStretch() # Push everything to left
        
        layout.addWidget(header)
        
        # 导航按钮组
        self.nav_group = QButtonGroup(self)
        self.nav_group.setExclusive(True)
        
        # 创建导航按钮
        for nav_item in NAVIGATION:
            btn = self._create_nav_button(nav_item)
            layout.addWidget(btn)
        
        # 模拟模式切换区域（在导航按钮下方）
        sim_mode_container = self._create_sim_mode_control()
        layout.addWidget(sim_mode_container)
            
        layout.addStretch()
        
        # 底部功能区
        bottom_container = QWidget()
        bottom_layout = QVBoxLayout(bottom_container)
        bottom_layout.setSpacing(10)
        bottom_layout.setContentsMargins(10, 10, 10, 10)
        
        # 1. 全部停止按钮
        btn_stop_all = QPushButton("全部停止")
        btn_stop_all.setCursor(Qt.PointingHandCursor)
        btn_stop_all.setStyleSheet("""
            QPushButton {
                background-color: #e74c3c;
                color: white;
                border: none;
                padding: 10px;
                border-radius: 4px;
                font-weight: bold;
                font-size: 14px;
            }
            QPushButton:hover { background-color: #c0392b; }
            QPushButton:pressed { background-color: #a93226; }
        """)
        # TODO: Connect to actual stop all function
        bottom_layout.addWidget(btn_stop_all)
        
        # 2. 系统时间显示
        self.time_label = QLabel()
        self.time_label.setStyleSheet("color: #29d6ff; font-size: 14px; font-weight: bold;")
        self.time_label.setAlignment(Qt.AlignCenter)
        bottom_layout.addWidget(self.time_label)
        
        # 启动定时器更新时间
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._update_time)
        self.timer.start(1000)
        self._update_time() # Initial update
        
        layout.addWidget(bottom_container)
        
        # 底部版本信息
        version = QLabel("v2.0")
        version.setStyleSheet("color: #4b5c74; font-size: 11px; padding: 5px;")
        version.setAlignment(Qt.AlignCenter)
        layout.addWidget(version)
        
        return sidebar

    def _register_sim_mode_devices(self):
        """注册支持模拟模式的设备（所有设备）"""
        # 注册所有支持 simSwitch 命令的设备
        sim_mode_devices = [
            DEVICES.get("large_stroke"),      # 大行程
            DEVICES.get("six_dof"),            # 六自由度
            DEVICES.get("auxiliary_1"),        # 辅助支撑设备1 (M14)
            DEVICES.get("auxiliary_2"),        # 辅助支撑设备2 (M15)
            DEVICES.get("auxiliary_3"),        # 辅助支撑设备3 (M16)
            DEVICES.get("auxiliary_4"),        # 辅助支撑设备4 (M17)
            DEVICES.get("auxiliary_5"),        # 辅助支撑设备5 (M18)
            DEVICES.get("reflection"),         # 反射光成像
            DEVICES.get("vacuum"),             # 真空系统
            # 注意：如果某些设备不支持 simSwitch，会在切换时失败但不影响其他设备
        ]
        for device_name in sim_mode_devices:
            if device_name:
                self.sim_mode_manager.register_device(device_name)
    
    def _create_sim_mode_control(self) -> QWidget:
        """创建模拟模式切换控件"""
        container = QFrame()
        container.setStyleSheet("""
            QFrame {
                background-color: #0c1724;
                border: 1px solid #1c3146;
                border-radius: 4px;
                margin: 8px 4px;
            }
        """)
        layout = QVBoxLayout(container)
        layout.setSpacing(8)
        layout.setContentsMargins(10, 10, 10, 10)
        
        # 标题
        title = QLabel("运行模式")
        title.setStyleSheet("color: #29d6ff; font-size: 12px; font-weight: bold;")
        layout.addWidget(title)
        
        # 当前模式显示
        self.sim_mode_label = QLabel()
        self.sim_mode_label.setAlignment(Qt.AlignCenter)
        self._update_sim_mode_display()
        layout.addWidget(self.sim_mode_label)
        
        # 单个切换按钮（显示当前模式，点击切换）
        self.sim_mode_btn = QPushButton()
        self.sim_mode_btn.setCursor(Qt.PointingHandCursor)
        self._update_switch_button()
        self.sim_mode_btn.clicked.connect(self._on_toggle_sim_mode)
        layout.addWidget(self.sim_mode_btn)
        
        # 连接模式变化信号
        self.sim_mode_manager.mode_changed.connect(self._on_sim_mode_changed)
        
        return container
    
    def _update_sim_mode_display(self):
        """更新模拟模式显示"""
        status_text = self.sim_mode_manager.get_mode_status_text()
        self.sim_mode_label.setText(status_text)
        
        # 根据模式设置颜色
        if self.sim_mode_manager.get_sim_mode():
            # 模拟模式 - 橙色
            self.sim_mode_label.setStyleSheet("""
                QLabel {
                    color: white;
                    background-color: #f39c12;
                    padding: 6px;
                    border-radius: 3px;
                    font-size: 11px;
                    font-weight: bold;
                }
            """)
        else:
            # 真实模式 - 蓝色
            self.sim_mode_label.setStyleSheet("""
                QLabel {
                    color: white;
                    background-color: #3498db;
                    padding: 6px;
                    border-radius: 3px;
                    font-size: 11px;
                    font-weight: bold;
                }
            """)
    
    def _update_switch_button(self):
        """更新切换按钮文本和样式"""
        is_sim = self.sim_mode_manager.get_sim_mode()
        if is_sim:
            # 模拟模式 - 橙色
            self.sim_mode_btn.setText("切换到真实模式")
            self.sim_mode_btn.setStyleSheet("""
                QPushButton {
                    background-color: #f39c12;
                    color: white;
                    border: none;
                    padding: 10px;
                    border-radius: 4px;
                    font-weight: bold;
                    font-size: 12px;
                }
                QPushButton:hover { background-color: #e67e22; }
                QPushButton:pressed { background-color: #d35400; }
            """)
        else:
            # 真实模式 - 蓝色
            self.sim_mode_btn.setText("切换到模拟模式")
            self.sim_mode_btn.setStyleSheet("""
                QPushButton {
                    background-color: #3498db;
                    color: white;
                    border: none;
                    padding: 10px;
                    border-radius: 4px;
                    font-weight: bold;
                    font-size: 12px;
                }
                QPushButton:hover { background-color: #2980b9; }
                QPushButton:pressed { background-color: #21618c; }
            """)
    
    def _on_toggle_sim_mode(self):
        """切换模拟模式"""
        new_mode = not self.sim_mode_manager.get_sim_mode()
        success = self.sim_mode_manager.set_sim_mode(new_mode)
        if success:
            print(f"[MainWindow] Sim mode switched to: {self.sim_mode_manager.get_mode_text()}")
        else:
            from .utils import show_warning
            show_warning("切换失败", "模拟模式切换失败，请检查设备连接。")
    
    def _on_sim_mode_changed(self, sim_mode: bool):
        """模拟模式变化回调（来自管理器信号）"""
        # 更新显示和按钮
        self._update_sim_mode_display()
        self._update_switch_button()
        
        print(f"[MainWindow] Sim mode changed to: {'模拟模式' if sim_mode else '真实模式'}")
    
    def _update_time(self):
        """更新系统时间显示"""
        current_time = QDateTime.currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        self.time_label.setText(current_time)
        
    def _create_nav_button(self, nav_item: dict) -> QPushButton:
        """创建单个导航按钮"""
        btn = QPushButton(f"{nav_item['icon']}  {nav_item['name']}")
        btn.setProperty("role", "nav_item")
        btn.setCheckable(True)
        btn.setCursor(Qt.PointingHandCursor)
        btn.setToolTip(nav_item.get("description", ""))
        
        page_id = nav_item["id"]
        self._nav_buttons[page_id] = btn
        self.nav_group.addButton(btn)
        
        # 连接点击事件
        btn.clicked.connect(lambda checked, pid=page_id: self._switch_to_page(pid))
        
        return btn
        
    def _create_pages(self):
        """初始化所有页面（Workers 延迟启动）"""
        
        # 1. 靶定位页面
        self._pages["target_positioning"] = TargetPositioningPage()
        self.content_stack.addWidget(self._pages["target_positioning"])
        
        # 2. 反射光成像页面
        self._pages["reflection_imaging"] = ReflectionImagingPage()
        self.content_stack.addWidget(self._pages["reflection_imaging"])
        
        # 3. 辅助支撑页面
        self._pages["auxiliary_support"] = AuxiliarySupportPage()
        self.content_stack.addWidget(self._pages["auxiliary_support"])
        
        # 4. 真空控制页面
        self._pages["vacuum_control"] = VacuumControlPage()
        self.content_stack.addWidget(self._pages["vacuum_control"])
        
        # 延迟启动 Workers（等 GUI 完全显示后，顺序启动避免资源争抢）
        self._start_workers_delayed()
    
    def _start_workers_delayed(self):
        """延迟并顺序启动所有页面的 Workers"""
        # 使用 QTimer.singleShot 延迟启动，让 GUI 先完全显示
        # 顺序启动各个 Worker，每个间隔 500ms，避免同时连接设备导致阻塞
        page_ids = ["target_positioning", "reflection_imaging", "auxiliary_support", "vacuum_control"]
        
        for i, page_id in enumerate(page_ids):
            delay_ms = 200 + i * 500  # 200ms 后开始，每个间隔 500ms
            page = self._pages.get(page_id)
            if page and hasattr(page, 'start_worker'):
                QTimer.singleShot(delay_ms, page.start_worker)
        
    def _switch_to_page(self, page_id: str):
        """切换到指定页面"""
        if page_id in self._pages:
            self.content_stack.setCurrentWidget(self._pages[page_id])
            
            # 更新按钮状态
            if page_id in self._nav_buttons:
                self._nav_buttons[page_id].setChecked(True)
            
            # 注意：状态面板已整合到各自页面中，不再需要在这里配置
                
    def closeEvent(self, event):
        """关闭窗口时的清理工作"""
        # 停止所有页面的后台任务
        for page in self._pages.values():
            if hasattr(page, 'cleanup'):
                page.cleanup()
                
        # 注意：状态面板已整合到各自页面中，页面清理时会自动处理
            
        event.accept()
