#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
真空系统 GUI 打包脚本

使用 PyInstaller 将应用打包为 Windows 可执行程序

用法:
    python build_exe.py          # 打包（默认带控制台窗口，方便调试）
    python build_exe.py --noconsole  # 打包（无控制台窗口，纯 GUI）
    python build_exe.py --onefile    # 打包为单个 exe 文件
"""

import os
import sys
import subprocess
import shutil

def main():
    # 获取当前目录
    current_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(current_dir)
    
    # 检查 PyInstaller 是否安装
    try:
        import PyInstaller
        print(f"✓ PyInstaller 版本: {PyInstaller.__version__}")
    except ImportError:
        print("✗ PyInstaller 未安装，正在安装...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pyinstaller"])
        print("✓ PyInstaller 安装完成")
    
    # 解析命令行参数
    noconsole = "--noconsole" in sys.argv or "-w" in sys.argv
    onefile = "--onefile" in sys.argv or "-F" in sys.argv
    
    # 构建 PyInstaller 命令
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--name=VacuumSystemGUI",
        "--icon=NONE",  # 可以替换为实际的 .ico 文件路径
        "--clean",
        "--noconfirm",
    ]
    
    # 添加隐藏导入（确保所有模块都被打包）
    hidden_imports = [
        "PyQt5",
        "PyQt5.QtWidgets",
        "PyQt5.QtCore",
        "PyQt5.QtGui",
        "pyqtgraph",
        "numpy",
        "json",
        "logging",
        "threading",
        "queue",
        "time",
        "datetime",
        "re",
        "enum",
        "dataclasses",
    ]
    
    for imp in hidden_imports:
        cmd.append(f"--hidden-import={imp}")
    
    # 添加数据文件（如果有配置文件等）
    # cmd.append("--add-data=config.json;.")
    
    # 排除不需要的模块（减小体积）
    excludes = [
        "matplotlib",
        "scipy",
        "pandas",
        "tkinter",
        "test",
        "unittest",
    ]
    
    for exc in excludes:
        cmd.append(f"--exclude-module={exc}")
    
    # 根据参数添加选项
    if noconsole:
        cmd.append("--noconsole")
        print("→ 模式: 无控制台窗口 (纯 GUI)")
    else:
        cmd.append("--console")
        print("→ 模式: 带控制台窗口 (方便调试)")
    
    if onefile:
        cmd.append("--onefile")
        print("→ 输出: 单个 exe 文件")
    else:
        cmd.append("--onedir")
        print("→ 输出: 文件夹模式")
    
    # 添加入口脚本
    cmd.append("run_gui.py")
    
    print("\n" + "=" * 60)
    print("开始打包...")
    print("=" * 60)
    print(f"命令: {' '.join(cmd)}\n")
    
    # 执行打包
    try:
        subprocess.check_call(cmd)
        print("\n" + "=" * 60)
        print("✓ 打包成功!")
        print("=" * 60)
        
        if onefile:
            exe_path = os.path.join(current_dir, "dist", "VacuumSystemGUI.exe")
        else:
            exe_path = os.path.join(current_dir, "dist", "VacuumSystemGUI", "VacuumSystemGUI.exe")
        
        if os.path.exists(exe_path):
            print(f"\n可执行文件位置: {exe_path}")
            print(f"文件大小: {os.path.getsize(exe_path) / 1024 / 1024:.1f} MB")
        
        print("\n运行方式:")
        print("  1. 双击 exe 文件启动（正常模式，需要 Tango 环境）")
        print("  2. 命令行运行: VacuumSystemGUI.exe --mock （模拟模式）")
        
    except subprocess.CalledProcessError as e:
        print(f"\n✗ 打包失败: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

