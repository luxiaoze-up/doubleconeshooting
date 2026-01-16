#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
真空系统 GUI 启动脚本

用法:
    python run_gui.py          # 正常模式（需要 Tango 环境）
    python run_gui.py --mock   # 模拟模式（无需 Tango）
"""

import sys
import os

# 确保模块路径正确
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

from main_window import main

if __name__ == "__main__":
    main()

