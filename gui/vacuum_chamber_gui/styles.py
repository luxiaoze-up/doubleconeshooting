"""
真空腔体系统控制 GUI - 样式定义
QSS Styles for Vacuum Chamber System Control GUI
"""

# 基础字体大小
BASE_FONT_SIZE = 13

# 颜色定义
COLORS = {
    "background": "#040a13",
    "panel_bg": "#0b1524",
    "panel_border": "#1c3146",
    "text_primary": "#c3cede",
    "text_secondary": "#8fa6c5",
    "text_muted": "#4b5c74",
    "accent": "#1f6feb",
    "accent_hover": "#32e6c7",
    "success": "#39e072",
    "warning": "#f2c95c",
    "error": "#ff7b72",
    "highlight": "#29d6ff",
    "input_bg": "#0c1724",
    "input_border": "#1c3146",
    "button_gradient_start": "#1f6feb",
    "button_gradient_end": "#32e6c7",
}

VACUUM_CHAMBER_THEME = f"""
/* ============================================
   Main Window & Base
   ============================================ */
QMainWindow, QWidget {{
    background-color: {COLORS["background"]};
    color: {COLORS["text_primary"]};
    font-family: "Segoe UI Emoji", "Microsoft YaHei UI", "Segoe UI", "PingFang SC", sans-serif;
    font-size: {BASE_FONT_SIZE}px;
}}

/* ============================================
   Panels & Frames
   ============================================ */
QFrame[role="panel"] {{
    background-color: {COLORS["panel_bg"]};
    border: 1px solid {COLORS["panel_border"]};
    border-radius: 8px;
}}

QFrame[role="status_panel"] {{
    background-color: {COLORS["panel_bg"]};
    border-left: 1px solid {COLORS["panel_border"]};
}}

/* ============================================
   Group Box
   ============================================ */
QGroupBox {{
    background-color: {COLORS["panel_bg"]};
    border: 1px solid {COLORS["panel_border"]};
    border-radius: 8px;
    margin-top: 24px;
    padding-top: 16px;
    font-weight: 600;
}}

QGroupBox::title {{
    subcontrol-origin: margin;
    margin-left: 12px;
    padding: 6px 16px;
    background-color: #08101c;
    border: 1px solid {COLORS["accent"]};
    border-radius: 6px;
    color: {COLORS["highlight"]};
    font-weight: 700;
}}

/* ============================================
   Labels
   ============================================ */
QLabel {{
    color: {COLORS["text_secondary"]};
    background: transparent;
}}

QLabel[role="title"] {{
    color: {COLORS["highlight"]};
    font-size: 16px;
    font-weight: 700;
}}

QLabel[role="subtitle"] {{
    color: {COLORS["text_primary"]};
    font-size: 14px;
    font-weight: 600;
}}

QLabel[role="value"] {{
    color: {COLORS["highlight"]};
    font-family: "Consolas", "Monaco", monospace;
    font-weight: 600;
}}

QLabel[role="unit"] {{
    color: {COLORS["text_muted"]};
    padding-left: 4px;
}}

/* Status Indicators */
QLabel[status="ok"] {{ color: {COLORS["success"]}; }}
QLabel[status="warn"] {{ color: {COLORS["warning"]}; }}
QLabel[status="error"] {{ color: {COLORS["error"]}; }}
QLabel[status="offline"] {{ color: {COLORS["text_muted"]}; }}

/* ============================================
   Input Fields
   ============================================ */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {{
    background-color: {COLORS["input_bg"]};
    border: 1px solid {COLORS["input_border"]};
    border-radius: 4px;
    padding: 6px 10px;
    color: #f2f5ff;
    min-height: 24px;
}}

QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {{
    border: 1px solid {COLORS["accent_hover"]};
}}

QLineEdit:read-only {{
    background-color: #070e16;
    color: {COLORS["text_secondary"]};
    border: 1px solid #162436;
}}

/* ============================================
   Buttons
   ============================================ */
QPushButton {{
    background-color: #1b2536;
    border: 1px solid #2c3a52;
    border-radius: 6px;
    padding: 8px 16px;
    color: {COLORS["text_primary"]};
    font-weight: 600;
    min-height: 32px;
}}

QPushButton:hover {{
    background-color: #25334d;
    border-color: {COLORS["accent"]};
    color: #ffffff;
}}

QPushButton:pressed {{
    background-color: #141d2b;
    padding-top: 9px;
    padding-bottom: 7px;
}}

QPushButton:disabled {{
    background-color: #161e2b;
    border-color: #1f293a;
    color: #4b5c74;
}}

/* Primary Action Button */
QPushButton[role="primary"] {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 {COLORS["button_gradient_start"]}, stop:1 {COLORS["button_gradient_end"]});
    border: 1px solid {COLORS["highlight"]};
    color: #ffffff;
}}

QPushButton[role="primary"]:hover {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2a82ff, stop:1 #47ffd4);
}}

/* Success/Confirm Button (e.g., Absolute Move) */
QPushButton[role="success"] {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 {COLORS["success"]}, stop:1 {COLORS["accent_hover"]});
    border: 1px solid {COLORS["highlight"]};
    color: #ffffff;
}}

QPushButton[role="success"]:hover {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 {COLORS["accent_hover"]}, stop:1 {COLORS["success"]});
}}

/* Move Buttons (NO gradients): Relative / Absolute */
QPushButton[role="move_relative"] {{
    background-color: {COLORS["accent"]};
    border: 1px solid {COLORS["highlight"]};
    color: #ffffff;
}}

QPushButton[role="move_relative"]:hover {{
    background-color: {COLORS["highlight"]};
}}

QPushButton[role="move_absolute"] {{
    background-color: {COLORS["success"]};
    border: 1px solid {COLORS["highlight"]};
    color: #ffffff;
}}

QPushButton[role="move_absolute"]:hover {{
    background-color: {COLORS["accent_hover"]};
}}

/* Stop/Emergency Button */
QPushButton[role="stop"] {{
    background-color: #da3633;
    border: 1px solid #f85149;
    color: #ffffff;
}}

QPushButton[role="stop"]:hover {{
    background-color: #f85149;
}}

/* ============================================
   Navigation Sidebar
   ============================================ */
#sidebar {{
    background-color: {COLORS["panel_bg"]};
    border-right: 1px solid {COLORS["panel_border"]};
}}

QPushButton[role="nav_item"] {{
    background-color: transparent;
    border: none;
    border-radius: 0;
    border-left: 3px solid transparent;
    padding: 12px 20px;
    color: {COLORS["text_secondary"]};
    text-align: left;
    font-size: 14px;
}}

QPushButton[role="nav_item"]:hover {{
    background-color: #1b2b3f;
    color: {COLORS["text_primary"]};
}}

QPushButton[role="nav_item"]:checked {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1f6feb33, stop:1 transparent);
    border-left: 3px solid {COLORS["accent"]};
    color: {COLORS["highlight"]};
    font-weight: 700;
}}

/* ============================================
   Tables
   ============================================ */
QTableWidget, QTableView {{
    background-color: #070e16;
    border: 1px solid {COLORS["panel_border"]};
    border-radius: 6px;
    gridline-color: {COLORS["panel_border"]};
}}

QHeaderView::section {{
    background-color: {COLORS["panel_bg"]};
    color: {COLORS["highlight"]};
    font-weight: 600;
    padding: 8px;
    border: none;
    border-bottom: 2px solid {COLORS["accent"]};
}}

/* ============================================
   Scroll Area
   ============================================ */
QScrollArea {{
    background: transparent;
    border: none;
}}

QScrollBar:vertical {{
    background-color: #0f1927;
    width: 10px;
    border-radius: 5px;
}}

QScrollBar::handle:vertical {{
    background-color: #2c3a52;
    border-radius: 5px;
}}

QScrollBar::handle:vertical:hover {{
    background-color: {COLORS["accent"]};
}}

/* ============================================
   Tab Widget
   ============================================ */
QTabWidget::pane {{
    border: 1px solid {COLORS["panel_border"]};
    background: {COLORS["background"]};
    border-radius: 4px;
}}

QTabBar::tab {{
    background: #0b1524;
    color: {COLORS["text_secondary"]};
    padding: 8px 20px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
    min-width: 80px;
}}

QTabBar::tab:selected {{
    background: {COLORS["accent"]};
    color: #ffffff;
    font-weight: bold;
}}

QTabBar::tab:hover:!selected {{
    background: #1b2b3f;
    color: {COLORS["text_primary"]};
}}
"""
