@echo off
chcp 65001 >nul
echo ============================================================
echo    真空系统 GUI 打包工具
echo ============================================================
echo.
echo 请选择打包模式:
echo   1. 正常版本 (需要 Tango 环境)
echo   2. Mock 版本 (模拟模式，无需 Tango)
echo.
set /p choice=请输入选项 (1 或 2): 

:: 切换到脚本所在目录
cd /d "%~dp0"

:: 检查虚拟环境是否存在
if not exist "venv\Scripts\python.exe" (
    echo [1/4] 创建虚拟环境...
    python -m venv venv
    if errorlevel 1 (
        echo [错误] 创建虚拟环境失败
        pause
        exit /b 1
    )
)

:: 激活虚拟环境
echo [2/4] 激活虚拟环境...
call venv\Scripts\activate.bat

:: 安装依赖
echo [3/4] 检查依赖...
pip install pyinstaller pyqt5 pyqtgraph numpy --quiet

echo [4/4] 开始打包...
echo.

:: 根据选择使用不同的 spec 文件
if "%choice%"=="2" (
    echo 打包 Mock 版本...
    python -m PyInstaller VacuumSystemGUI_Mock.spec --clean --noconfirm
) else (
    echo 打包正常版本...
    python -m PyInstaller VacuumSystemGUI.spec --clean --noconfirm
)

echo.
if exist "dist\VacuumSystemGUI.exe" (
    echo ============================================================
    echo [成功] 打包完成!
    echo ============================================================
    echo.
    echo 可执行文件位置: %~dp0dist\VacuumSystemGUI.exe
    echo.
    echo 运行方式:
    echo   1. 双击 VacuumSystemGUI.exe 启动
    echo   2. 命令行: VacuumSystemGUI.exe --mock  (模拟模式)
    echo.
) else (
    echo [错误] 打包失败，请检查错误信息
)

echo.
pause

