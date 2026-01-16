#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
六自由度调试GUI 打包脚本

使用 PyInstaller 将应用打包为 Windows 可执行程序

用法:
    python build_exe.py          # 打包为文件夹模式（推荐，启动快）
    python build_exe.py --onefile    # 打包为单个 exe 文件
    python build_exe.py --noconsole  # 打包为无控制台窗口版本
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path

def main():
    # 获取当前目录和项目根目录
    current_dir = Path(__file__).parent
    project_root = current_dir.parent.parent
    os.chdir(current_dir)
    
    print("=" * 60)
    print("六自由度调试GUI - 打包工具")
    print("=" * 60)
    print()
    
    # 优先使用虚拟环境（避免中文路径问题）
    venv_python = project_root / "venv_32bit" / "Scripts" / "python.exe"
    if venv_python.exists():
        print("✓ 发现虚拟环境，使用虚拟环境打包（避免中文路径问题）")
        sys.executable = str(venv_python)
        print(f"   使用Python: {venv_python}")
        print()
    else:
        # 检查是否使用32位Python
        python_arch = __import__('platform').architecture()[0]
        if python_arch != '32bit':
            print("⚠ 警告: 当前Python不是32位版本")
            print(f"   当前架构: {python_arch}")
            print("   建议使用32位Python打包（因为LTSMC.dll是32位）")
            print("   或创建虚拟环境: py -3.14-32 -m venv venv_32bit")
            print()
            response = input("是否继续? (y/n): ")
            if response.lower() != 'y':
                print("已取消")
                return
    
    # 检查 PyInstaller 是否安装
    try:
        import PyInstaller
        print(f"✓ PyInstaller 版本: {PyInstaller.__version__}")
    except ImportError:
        print("✗ PyInstaller 未安装，正在安装...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pyinstaller"])
        print("✓ PyInstaller 安装完成")
    
    # 检查DLL文件是否存在
    dll_path = project_root / "lib" / "LTSMC.dll"
    if not dll_path.exists():
        print(f"⚠ 警告: 未找到LTSMC.dll: {dll_path}")
        print("   打包后的程序需要此文件才能运行")
        print()
    
    # 解析命令行参数
    noconsole = "--noconsole" in sys.argv or "-w" in sys.argv
    onefile = "--onefile" in sys.argv or "-F" in sys.argv
    
    # 构建 PyInstaller 命令
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--name=SixDofDebugGUI",  # 确保使用正确的名称
        "--clean",
        "--noconfirm",
    ]
    
    # 添加项目路径
    cmd.append("--paths=../..")
    
    # 收集所有子模块（确保所有包和子模块都被包含）
    cmd.append("--collect-submodules=gui.six_dof_debug_gui")
    
    # 添加运行时钩子（确保Qt环境在PyQt5导入前设置）
    rthook_path = current_dir / "rthook_pyqt5.py"
    if rthook_path.exists():
        cmd.append(f"--runtime-hook={rthook_path}")
        print("→ 使用运行时钩子确保Qt初始化顺序正确")
    
    # 添加隐藏导入（基础依赖）
    hidden_imports = [
        # PyQt5
        "PyQt5",
        "PyQt5.QtWidgets",
        "PyQt5.QtCore",
        "PyQt5.QtGui",
        "PyQt5.sip",
        # numpy
        "numpy",
        "numpy.core._methods",
        "numpy.lib.format",
        # 标准库
        "json",
        "logging",
        "threading",
        "queue",
        "ctypes",
        "socket",
        "pathlib",
    ]
    
    for imp in hidden_imports:
        cmd.append(f"--hidden-import={imp}")
    
    # 添加数据文件
    # 注意：DLL文件需要在运行时从lib目录加载，所以不打包进exe
    # 但可以添加配置文件模板
    if (current_dir / "config.json").exists():
        cmd.append(f"--add-data={current_dir / 'config.json'};.")
    
    # 排除不需要的模块（减小体积）
    excludes = [
        "matplotlib",
        "scipy",
        "pandas",
        "tkinter",
        "_tkinter",
        "test",
        "unittest",
        "xmlrpc",
        "pydoc",
        "doctest",
        "IPython",
        "jupyter",
        "notebook",
    ]
    
    for exc in excludes:
        cmd.append(f"--exclude-module={exc}")
    
    # 添加自定义hook路径（解决中文路径问题）
    hook_path = current_dir / "hook-PyQt5.QtWidgets.py"
    if hook_path.exists():
        cmd.append("--additional-hooks-dir")
        cmd.append(str(current_dir))
        print("→ 使用自定义hook解决Qt插件路径问题")
    
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
        print("→ 输出: 文件夹模式 (推荐，启动更快)")
    
    # 添加入口脚本
    cmd.append("main.py")
    
    print()
    print("=" * 60)
    print("开始打包...")
    print("=" * 60)
    print(f"命令: {' '.join(cmd)}\n")
    
    # 执行打包
    try:
        # 设置环境变量，避免中文路径问题
        env = os.environ.copy()
        # 使用短路径名（8.3格式）来避免中文路径问题
        import locale
        env['PYTHONIOENCODING'] = 'utf-8'
        
        subprocess.check_call(cmd, env=env)
        print("\n" + "=" * 60)
        print("✓ 打包成功!")
        print("=" * 60)
        
        if onefile:
            exe_path = current_dir / "dist" / "SixDofDebugGUI.exe"
        else:
            exe_path = current_dir / "dist" / "SixDofDebugGUI" / "SixDofDebugGUI.exe"
        
        if exe_path.exists():
            size_mb = exe_path.stat().st_size / 1024 / 1024
            print(f"\n可执行文件位置: {exe_path}")
            print(f"文件大小: {size_mb:.1f} MB")
            
            # 复制DLL文件到dist目录（必须复制，程序依赖此文件）
            if dll_path.exists():
                if onefile:
                    # 单文件模式：创建lib目录并复制DLL
                    lib_dir = current_dir / "dist" / "lib"
                    lib_dir.mkdir(exist_ok=True)
                    shutil.copy2(dll_path, lib_dir / "LTSMC.dll")
                    print(f"\n✓ 已复制LTSMC.dll到: {lib_dir / 'LTSMC.dll'}")
                else:
                    # 文件夹模式：复制到exe同目录的lib子目录（这是程序查找的位置）
                    exe_dir = exe_path.parent
                    lib_dir = exe_dir / "lib"
                    lib_dir.mkdir(exist_ok=True)
                    shutil.copy2(dll_path, lib_dir / "LTSMC.dll")
                    print(f"\n✓ 已复制LTSMC.dll到: {lib_dir / 'LTSMC.dll'}")
                    print(f"  程序将从该位置加载DLL")
            else:
                print("\n✗ 错误: 未找到LTSMC.dll！")
                print(f"  期望位置: {dll_path}")
                print("  打包失败：DLL文件是必需的")
                sys.exit(1)
        
        print("\n部署说明:")
        print("  1. 将 dist/SixDofDebugGUI 文件夹（或exe文件）复制到目标主机")
        print("  2. 确保 lib/LTSMC.dll 文件存在")
        print("  3. 双击运行 SixDofDebugGUI.exe")
        print("\n详细部署要求请查看: 部署要求.md")
        
    except subprocess.CalledProcessError as e:
        print(f"\n✗ 打包失败: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
