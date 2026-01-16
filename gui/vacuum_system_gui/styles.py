"""
真空系统 GUI 样式定义
"""

# =============================================================================
# 主题色
# =============================================================================

COLORS = {
    "primary": "#00E5FF",      # 更亮的青色
    "primary_dark": "#00B8D4",
    "primary_light": "#B2EBF2",
    "accent": "#FF4081",
    "success": "#00E676",      # 鲜艳的绿色
    "warning": "#FFEA00",      # 鲜艳的黄色
    "error": "#FF1744",       # 鲜艳的红色
    "background": "#0A0E1A",   # 更深的背景色
    "surface": "#1A1F2E",      # 中层
    "surface_light": "#252B3B", # 浅层
    "text_primary": "#ECEFF1",
    "text_secondary": "#90A4AE",
    "border": "#2C3447",
    "divider": "#1F2633"
}

# =============================================================================
# 主样式表
# =============================================================================

MAIN_STYLE = f"""
QMainWindow {{
    background-color: {COLORS['background']};
}}

QWidget {{
    color: {COLORS['text_primary']};
    font-family: "Segoe UI", "Segoe UI Symbol", "Microsoft YaHei", "Arial Unicode MS", sans-serif;
    font-size: 13px;
}}

/* 分组框 - 更现代的设计 */
QGroupBox {{
    background-color: {COLORS['surface']};
    border: 1px solid {COLORS['border']};
    border-radius: 12px;
    margin-top: 15px;
    padding: 20px 15px 15px 15px;
}}

QGroupBox::title {{
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 15px;
    padding: 0 10px;
    color: {COLORS['primary']};
    font-weight: bold;
    font-size: 14px;
}}

/* 按钮美化 */
QPushButton {{
    background-color: {COLORS['surface_light']};
    border: 1px solid {COLORS['border']};
    border-radius: 6px;
    padding: 8px 16px;
    min-height: 35px;
}}

QPushButton:hover {{
    background-color: {COLORS['surface_light']};
    border-color: {COLORS['primary']};
}}

QPushButton[class="primary"] {{
    background-color: {COLORS['primary']};
    color: {COLORS['background']};
    font-weight: bold;
}}

QPushButton[class="success"] {{
    border: 1px solid {COLORS['success']};
    color: {COLORS['success']};
    background-color: rgba(0, 230, 118, 0.1);
}}
QPushButton[class="success"]:hover {{
    background-color: {COLORS['success']};
    color: {COLORS['background']};
}}

QPushButton[class="warning"] {{
    border: 1px solid {COLORS['warning']};
    color: {COLORS['warning']};
    background-color: rgba(255, 234, 0, 0.1);
}}
QPushButton[class="warning"]:hover {{
    background-color: {COLORS['warning']};
    color: {COLORS['background']};
}}

QPushButton[class="danger"] {{
    border: 1px solid {COLORS['error']};
    color: {COLORS['error']};
    background-color: rgba(255, 23, 68, 0.1);
}}
QPushButton[class="danger"]:hover {{
    background-color: {COLORS['error']};
    color: {COLORS['background']};
}}

/* 选项卡 */
QTabWidget::pane {{
    background-color: {COLORS['surface']};
    border: 1px solid {COLORS['border']};
    border-radius: 4px;
    padding: 8px;
}}

QTabBar::tab {{
    background-color: {COLORS['surface_light']};
    color: {COLORS['text_secondary']};
    border: 1px solid {COLORS['border']};
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    padding: 10px 20px;
    margin-right: 2px;
    font-size: 13px;
}}

QTabBar::tab:selected {{
    background-color: {COLORS['primary']};
    color: {COLORS['background']};
    font-weight: bold;
}}

/* 表格 */
QTableWidget {{
    background-color: {COLORS['surface']};
    border: 1px solid {COLORS['border']};
    border-radius: 4px;
    gridline-color: {COLORS['divider']};
}}

QTableWidget::item {{
    padding: 8px;
    border-bottom: 1px solid {COLORS['divider']};
}}

QTableWidget::item:selected {{
    background-color: {COLORS['primary']};
    color: {COLORS['background']};
}}

QHeaderView::section {{
    background-color: {COLORS['surface_light']};
    color: {COLORS['text_primary']};
    padding: 10px;
    border: none;
    border-bottom: 2px solid {COLORS['primary']};
    font-weight: bold;
}}

/* 滚动条 */
QScrollBar:vertical {{
    background-color: {COLORS['surface']};
    width: 10px;
    border-radius: 5px;
}}

QScrollBar::handle:vertical {{
    background-color: {COLORS['border']};
    border-radius: 5px;
    min-height: 20px;
}}

QScrollBar::handle:vertical:hover {{
    background-color: {COLORS['primary']};
}}

/* 复选框美化 */
QCheckBox {{
    spacing: 10px;
    color: {COLORS['text_primary']};
    font-size: 13px;
    padding: 5px 10px;
    background-color: rgba(255, 255, 255, 0.03);
    border-radius: 4px;
}}

QCheckBox:hover {{
    background-color: rgba(0, 229, 255, 0.08);
    border: 1px solid rgba(0, 229, 255, 0.3);
}}

QCheckBox::indicator {{
    width: 18px;
    height: 18px;
    border: 2px solid {COLORS['border']};
    border-radius: 4px;
    background-color: {COLORS['background']};
}}

QCheckBox::indicator:unchecked:hover {{
    border-color: {COLORS['primary']};
}}

QCheckBox::indicator:checked {{
    background-color: {COLORS['primary']};
    border-color: {COLORS['primary']};
    image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjMEEwRTE0IiBzdHJva2Utd2lkdGg9IjQiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCI+PHBvbHlsaW5lIHBvaW50cz0iMjAgNiA5IDE3IDQgMTIiPjwvcG9seWxpbmU+PC9zdmc+);
}}

/* 分隔线 */
QFrame[frameShape="4"] {{  /* HLine */
    background-color: {COLORS['divider']};
    max-height: 1px;
}}

QFrame[frameShape="5"] {{  /* VLine */
    background-color: {COLORS['divider']};
    max-width: 1px;
}}

/* 消息框 - 彻底解决白底白字内容不可见问题 */
QMessageBox {{
    background-color: #1A1F2E;
    border: 1px solid {COLORS['primary']};
}}

QMessageBox QLabel {{
    color: #ECEFF1 !important;
    background-color: transparent;
    font-size: 14px;
}}

QMessageBox QPushButton {{
    background-color: #252B3B;
    color: #ECEFF1;
    border: 1px solid #2C3447;
    border-radius: 4px;
    min-width: 80px;
    padding: 6px;
}}

QMessageBox QPushButton:hover {{
    background-color: {COLORS['primary']};
    color: #0A0E1A;
}}
"""

# =============================================================================
# 状态指示器样式
# =============================================================================

STATUS_INDICATOR_STYLES = {
    "on": f"""
        background-color: {COLORS['success']};
        border: 2px solid #81C784;
        border-radius: 10px;
    """,
    "off": f"""
        background-color: #424242;
        border: 2px solid #616161;
        border-radius: 10px;
    """,
    "warning": f"""
        background-color: {COLORS['warning']};
        border: 2px solid #FFB74D;
        border-radius: 10px;
    """,
    "error": f"""
        background-color: {COLORS['error']};
        border: 2px solid #EF5350;
        border-radius: 10px;
    """,
    "blink_blue_on": f"""
        background-color: #2196F3;
        border: 2px solid #64B5F6;
        border-radius: 10px;
    """,
    "blink_blue_off": f"""
        background-color: #1565C0;
        border: 2px solid #1976D2;
        border-radius: 10px;
    """,
    "blink_red_on": f"""
        background-color: #F44336;
        border: 2px solid #EF5350;
        border-radius: 10px;
    """,
    "blink_red_off": f"""
        background-color: #B71C1C;
        border: 2px solid #C62828;
        border-radius: 10px;
    """
}

# =============================================================================
# 报警样式
# =============================================================================

ALARM_STYLES = {
    "unacknowledged": f"""
        background-color: rgba(255, 23, 68, 0.2);
        border: 1px solid {COLORS['error']};
        border-radius: 4px;
        padding: 8px;
    """,
    "acknowledged": f"""
        background-color: rgba(255, 234, 0, 0.1);
        border: 1px solid {COLORS['warning']};
        border-radius: 4px;
        padding: 8px;
    """,
    "normal": f"""
        background-color: {COLORS['surface']};
        border: 1px solid {COLORS['border']};
        border-radius: 4px;
        padding: 8px;
    """
}

# =============================================================================
# 数字孪生页面样式
# =============================================================================

DIGITAL_TWIN_STYLES = {
    "device_frame": f"""
        background-color: {COLORS['surface_light']};
        border: 2px solid {COLORS['border']};
        border-radius: 8px;
        padding: 8px;
    """,
    "device_frame_hover": f"""
        background-color: {COLORS['surface_light']};
        border: 2px solid {COLORS['primary']};
        border-radius: 8px;
        padding: 8px;
    """,
    "device_frame_active": f"""
        background-color: rgba(0, 230, 118, 0.1);
        border: 2px solid {COLORS['success']};
        border-radius: 8px;
        padding: 8px;
    """,
    "device_frame_error": f"""
        background-color: rgba(255, 23, 68, 0.1);
        border: 2px solid {COLORS['error']};
        border-radius: 8px;
        padding: 8px;
    """,
    "pipe": f"""
        background-color: #2C3447;
        border: 1px solid #37474F;
    """,
    "pipe_active": f"""
        background-color: {COLORS['primary']};
        border: 1px solid {COLORS['primary_light']};
    """
}

# =============================================================================
# 自动/手动模式区域样式
# =============================================================================

MODE_AREA_STYLES = {
    "auto_area": f"""
        QGroupBox {{
            background-color: rgba(0, 229, 255, 0.05);
            border: 2px solid {COLORS['primary']};
            border-radius: 12px;
        }}
    """,
    "manual_area": f"""
        QGroupBox {{
            background-color: rgba(255, 234, 0, 0.05);
            border: 2px solid {COLORS['warning']};
            border-radius: 12px;
        }}
    """
}
