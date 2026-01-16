@echo off
chcp 65001 >nul
echo ============================================================
echo    六自由度调试GUI - 打包工具
echo ============================================================
echo.

:: 切换到脚本所在目录
cd /d "%~dp0"
set PROJECT_ROOT=%~dp0..\..

:: 优先使用虚拟环境（避免中文路径问题）
echo [1/6] 检查虚拟环境...
if exist "%PROJECT_ROOT%\venv_32bit\Scripts\python.exe" (
    echo   ✓ 发现虚拟环境，使用虚拟环境打包（推荐）
    set PYTHON_CMD=%PROJECT_ROOT%\venv_32bit\Scripts\python.exe
    goto :use_venv
)

:: 检查是否使用32位Python
echo   未找到虚拟环境，检查系统Python...
for /f "tokens=*" %%i in ('py --list 2^>nul ^| findstr /i "32"') do (
    echo   找到32位Python: %%i
    set PYTHON_CMD=py -3.14-32
    goto :check_deps
)
echo   ⚠ 未找到32位Python
echo   建议创建虚拟环境: py -3.14-32 -m venv venv_32bit
set PYTHON_CMD=py -3.14-32
goto :check_deps

:use_venv
echo   使用虚拟环境: %PYTHON_CMD%
goto :check_deps

:check_deps
echo.

:: 检查PyInstaller
echo [2/6] 检查PyInstaller...
%PYTHON_CMD% -c "import PyInstaller" >nul 2>&1
if errorlevel 1 (
    echo   安装PyInstaller...
    %PYTHON_CMD% -m pip install pyinstaller --quiet
) else (
    echo   ✓ PyInstaller 已安装
)
echo.

:: 检查依赖
echo [3/6] 检查依赖库...
%PYTHON_CMD% -c "import PyQt5, numpy" >nul 2>&1
if errorlevel 1 (
    echo   安装依赖库...
    %PYTHON_CMD% -m pip install PyQt5 numpy --quiet
) else (
    echo   ✓ 依赖库已安装
)
echo.

:: 选择打包模式
echo [4/5] 选择打包模式:
echo   1. 文件夹模式 (推荐，启动快，文件多)
echo   2. 单文件模式 (启动慢，单个exe)
echo.
set /p mode=请选择 (1 或 2，默认1): 
if "%mode%"=="" set mode=1
if "%mode%"=="2" (
    set mode_arg=--onefile
    echo   选择: 单文件模式
) else (
    set mode_arg=--onedir
    echo   选择: 文件夹模式
)
echo.

:: 开始打包
echo [5/6] 开始打包...
echo.
%PYTHON_CMD% build_exe.py %mode_arg%

:: 复制DLL文件
echo.
echo [6/6] 复制DLL文件...
if exist "%PROJECT_ROOT%\lib\LTSMC.dll" (
    if not exist "dist\SixDofDebugGUI\lib" mkdir "dist\SixDofDebugGUI\lib"
    copy "%PROJECT_ROOT%\lib\LTSMC.dll" "dist\SixDofDebugGUI\lib\LTSMC.dll" >nul
    if exist "dist\SixDofDebugGUI\lib\LTSMC.dll" (
        echo   ✓ DLL已复制
    ) else (
        echo   ⚠ DLL复制失败，请手动复制
    )
) else (
    echo   ⚠ 未找到LTSMC.dll，请手动复制到lib目录
)
echo.

echo.
if exist "dist\SixDofDebugGUI.exe" (
    echo ============================================================
    echo [成功] 打包完成!
    echo ============================================================
    echo.
    echo 可执行文件位置: %~dp0dist\SixDofDebugGUI.exe
    echo.
    echo 部署说明:
    echo   1. 将 dist\SixDofDebugGUI 文件夹复制到目标主机
    echo   2. 确保 lib\LTSMC.dll 文件存在
    echo   3. 双击运行 SixDofDebugGUI.exe
    echo.
) else if exist "dist\SixDofDebugGUI\SixDofDebugGUI.exe" (
    echo ============================================================
    echo [成功] 打包完成!
    echo ============================================================
    echo.
    echo 可执行文件位置: %~dp0dist\SixDofDebugGUI\SixDofDebugGUI.exe
    echo.
    echo 部署说明:
    echo   1. 将 dist\SixDofDebugGUI 整个文件夹复制到目标主机
    echo   2. 确保 lib\LTSMC.dll 文件存在
    echo   3. 双击运行 SixDofDebugGUI.exe
    echo.
) else (
    echo [错误] 打包失败，请检查错误信息
)

echo.
pause
