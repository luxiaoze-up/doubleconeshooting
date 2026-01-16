#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
真空系统 GUI 启动脚本 - Mock 模式专用

打包后默认以模拟模式运行，无需 Tango 环境
"""

import sys
import os

# 确保模块路径正确
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

# 强制添加 --mock 参数
if '--mock' not in sys.argv:
    sys.argv.append('--mock')

from main_window import main

if __name__ == "__main__":
    main()

