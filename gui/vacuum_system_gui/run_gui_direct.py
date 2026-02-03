#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
真空系统 GUI 启动脚本 - Direct PLC 模式

直接连接真实PLC，不通过Tango
"""

import sys
import os

# 确保模块路径正确
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

# 强制添加 --direct 参数
if '--direct' not in sys.argv:
    sys.argv.append('--direct')

from main_window import main

if __name__ == "__main__":
    main()
