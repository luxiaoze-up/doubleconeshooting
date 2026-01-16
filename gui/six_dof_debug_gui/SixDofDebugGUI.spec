# -*- mode: python ; coding: utf-8 -*-
from PyInstaller.utils.hooks import collect_submodules

hiddenimports = ['PyQt5', 'PyQt5.QtWidgets', 'PyQt5.QtCore', 'PyQt5.QtGui', 'PyQt5.sip', 'numpy', 'numpy.core._methods', 'numpy.lib.format', 'json', 'logging', 'threading', 'queue', 'ctypes', 'socket', 'pathlib']
hiddenimports += collect_submodules('gui.six_dof_debug_gui')


a = Analysis(
    ['main.py'],
    pathex=['../..'],
    binaries=[],
    datas=[('D:\\00.My_workspace\\DoubleConeShooting\\gui\\six_dof_debug_gui\\config.json', '.')],
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=['D:\\00.My_workspace\\DoubleConeShooting\\gui\\six_dof_debug_gui\\rthook_pyqt5.py'],
    excludes=['matplotlib', 'scipy', 'pandas', 'tkinter', '_tkinter', 'test', 'unittest', 'xmlrpc', 'pydoc', 'doctest', 'IPython', 'jupyter', 'notebook'],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='SixDofDebugGUI',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='SixDofDebugGUI',
)
