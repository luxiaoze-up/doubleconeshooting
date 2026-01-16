@echo off
REM 使用32位Python运行GUI应用
REM Run GUI application with 32-bit Python

cd /d "%~dp0\..\.."

echo ========================================
echo 六自由度机器人调试GUI - 32位Python启动
echo ========================================
echo.

REM 尝试找到32位Python
echo [1/3] 查找32位Python...

set PYTHON32=
set PYTHON32_CMD=

REM 方法1: 使用py launcher（优先尝试3.14，然后3.13，最后3.11）
py -3.14-32 --version >nul 2>&1
if not errorlevel 1 (
    set PYTHON32_CMD=py -3.14-32
    echo   ✓ 使用Python 3.14 32位
    goto :found_py_launcher
)
py -3.13-32 --version >nul 2>&1
if not errorlevel 1 (
    set PYTHON32_CMD=py -3.13-32
    echo   ✓ 使用Python 3.13 32位
    goto :found_py_launcher
)
py -3.11-32 --version >nul 2>&1
if not errorlevel 1 (
    set PYTHON32_CMD=py -3.11-32
    echo   ✓ 使用Python 3.11 32位
    goto :found_py_launcher
)
for /f "tokens=*" %%i in ('py --list 2^>nul ^| findstr /i "32"') do (
    echo   找到: %%i
    for /f "tokens=1" %%j in ("%%i") do (
        set PYTHON32_CMD=py -%%j-32
        goto :found_py_launcher
    )
)

:found_py_launcher
if not "%PYTHON32_CMD%"=="" (
    echo   ✓ 使用py launcher: %PYTHON32_CMD%
    goto :check_deps
)

REM 方法2: 检查常见路径
if exist "C:\Python311-32\python.exe" (
    set PYTHON32=C:\Python311-32\python.exe
    goto :found_direct
)
if exist "C:\Python312-32\python.exe" (
    set PYTHON32=C:\Python312-32\python.exe
    goto :found_direct
)
if exist "C:\Python313-32\python.exe" (
    set PYTHON32=C:\Python313-32\python.exe
    goto :found_direct
)
if exist "C:\Program Files (x86)\Python311\python.exe" (
    set PYTHON32=C:\Program Files (x86)\Python311\python.exe
    goto :found_direct
)
if exist "C:\Program Files (x86)\Python312\python.exe" (
    set PYTHON32=C:\Program Files (x86)\Python312\python.exe
    goto :found_direct
)
if exist "C:\Program Files (x86)\Python313\python.exe" (
    set PYTHON32=C:\Program Files (x86)\Python313\python.exe
    goto :found_direct
)

REM 未找到
echo   ✗ 未找到32位Python
echo.
echo ========================================
echo 需要安装32位Python
echo ========================================
echo.
echo 请运行安装助手:
echo   gui\six_dof_debug_gui\setup_32bit_python.ps1
echo.
echo 或手动安装:
echo   1. 访问: https://www.python.org/downloads/windows/
echo   2. 下载 "Windows installer (32-bit)"
echo   3. 安装时勾选 "Add Python to PATH"
echo.
pause
exit /b 1

:found_direct
echo   ✓ 找到32位Python: %PYTHON32%
set PYTHON32_CMD=%PYTHON32%

:check_deps
echo.
echo [2/3] 检查依赖库...

if "%PYTHON32_CMD:~0,2%"=="py" (
    %PYTHON32_CMD% -c "import PyQt5" >nul 2>&1
    if errorlevel 1 (
        echo   缺少PyQt5，正在安装...
        %PYTHON32_CMD% -m pip install PyQt5
    ) else (
        echo   ✓ PyQt5 已安装
    )
    
    %PYTHON32_CMD% -c "import numpy" >nul 2>&1
    if errorlevel 1 (
        echo   缺少numpy，正在安装...
        %PYTHON32_CMD% -m pip install numpy
    ) else (
        echo   ✓ numpy 已安装
    )
) else (
    "%PYTHON32%" -c "import PyQt5" >nul 2>&1
    if errorlevel 1 (
        echo   缺少PyQt5，正在安装...
        "%PYTHON32%" -m pip install PyQt5
    ) else (
        echo   ✓ PyQt5 已安装
    )
    
    "%PYTHON32%" -c "import numpy" >nul 2>&1
    if errorlevel 1 (
        echo   缺少numpy，正在安装...
        "%PYTHON32%" -m pip install numpy
    ) else (
        echo   ✓ numpy 已安装
    )
)

echo.
echo [3/3] 启动GUI应用...
echo ========================================
echo.

if "%PYTHON32_CMD:~0,2%"=="py" (
    %PYTHON32_CMD% -m gui.six_dof_debug_gui.main
) else (
    "%PYTHON32%" -m gui.six_dof_debug_gui.main
)

if errorlevel 1 (
    echo.
    echo ========================================
    echo 启动失败
    echo ========================================
    echo.
    echo 请检查:
    echo 1. 32位Python是否正确安装
    echo 2. 依赖库是否已安装
    echo 3. 运行诊断工具: %PYTHON32_CMD% gui\six_dof_debug_gui\diagnose_dll.py
    echo.
    pause
)
