# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller spec 文件 - 真空系统 GUI (Mock 模式)

打包后默认以模拟模式运行，无需 Tango 环境

使用方法:
    pyinstaller VacuumSystemGUI_Mock.spec --clean --noconfirm
"""

import sys
import os

# 获取当前目录
spec_dir = os.path.dirname(os.path.abspath(SPEC))

block_cipher = None

# 隐藏导入 - 确保这些模块被打包
hidden_imports = [
    'PyQt5',
    'PyQt5.QtWidgets',
    'PyQt5.QtCore',
    'PyQt5.QtGui',
    'PyQt5.sip',
    'pyqtgraph',
    'pyqtgraph.graphicsItems',
    'pyqtgraph.graphicsItems.PlotDataItem',
    'numpy',
    'numpy.core._methods',
    'numpy.lib.format',
    'json',
    'logging',
    'logging.handlers',
    'threading',
    'queue',
    'time',
    'datetime',
    're',
    'enum',
    'dataclasses',
    'typing',
    'collections',
    'functools',
    'random',
    'ctypes',
]

# 排除不需要的模块
excludes = [
    'matplotlib',
    'scipy',
    'pandas',
    'tkinter',
    '_tkinter',
    'test',
    'unittest',
    'xmlrpc',
    'pydoc',
    'doctest',
    'IPython',
    'jupyter',
    'notebook',
]

a = Analysis(
    ['run_gui_mock.py'],  # 使用 mock 模式入口
    pathex=[spec_dir],
    binaries=[],
    datas=[],
    hiddenimports=hidden_imports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=excludes,
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

# 单文件模式 - Mock 版本
exe_onefile = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='VacuumSystemGUI_Mock',  # 文件名带 _Mock 后缀
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,  # True=带控制台, False=纯GUI
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

