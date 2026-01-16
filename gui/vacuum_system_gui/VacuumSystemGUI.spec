# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller spec 文件 - 真空系统 GUI

使用方法:
    pyinstaller VacuumSystemGUI.spec

或者使用自动打包脚本:
    python build_exe.py
"""

import sys
import os

# 获取当前目录
spec_dir = os.path.dirname(os.path.abspath(SPEC))

block_cipher = None

# 需要包含的 Python 文件
py_files = [
    'run_gui.py',
    'main_window.py',
    'main_page.py',
    'digital_twin_page.py',
    'tango_worker.py',
    'config.py',
    'widgets.py',
    'styles.py',
    'alarm_manager.py',
    'record_pages.py',
    '__init__.py',
]

# 需要包含的子目录
submodules = [
    'utils',
]

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
    ['run_gui.py'],
    pathex=[spec_dir],
    binaries=[],
    datas=[
        # 如果有额外的数据文件，在这里添加
        # ('data/*.json', 'data'),
        # ('icons/*.ico', 'icons'),
    ],
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

# ============================================================================
# 单文件模式 (onefile) - 启动稍慢但分发方便
# ============================================================================
exe_onefile = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='VacuumSystemGUI',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,  # 使用 UPX 压缩（需要安装 UPX）
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,  # True=带控制台, False=纯GUI
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    # icon='icon.ico',  # 取消注释并提供 .ico 文件路径
)

# ============================================================================
# 文件夹模式 (onedir) - 启动快但文件多
# ============================================================================
# exe_onedir = EXE(
#     pyz,
#     a.scripts,
#     [],
#     exclude_binaries=True,
#     name='VacuumSystemGUI',
#     debug=False,
#     bootloader_ignore_signals=False,
#     strip=False,
#     upx=True,
#     console=True,
#     disable_windowed_traceback=False,
#     argv_emulation=False,
#     target_arch=None,
#     codesign_identity=None,
#     entitlements_file=None,
# )
# 
# coll = COLLECT(
#     exe_onedir,
#     a.binaries,
#     a.zipfiles,
#     a.datas,
#     strip=False,
#     upx=True,
#     upx_exclude=[],
#     name='VacuumSystemGUI',
# )

