@echo off
REM 运行自动化测试脚本 (Windows)

echo ==========================================
echo   真空系统自动化测试
echo   GUI-Server-PLC通信测试
echo ==========================================
echo.

REM 检查pytest是否安装
where pytest >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo 错误: pytest未安装
    echo 请运行: pip install pytest pytest-html pytest-cov
    exit /b 1
)

REM 创建报告目录
if not exist reports mkdir reports

REM 运行测试
echo 开始运行测试...
echo.

pytest tests\ ^
    --tb=short ^
    --color=yes ^
    --html=reports\report.html ^
    --self-contained-html ^
    --cov=. ^
    --cov-report=html ^
    --cov-report=term-missing

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ==========================================
    echo   所有测试通过！
    echo ==========================================
) else (
    echo.
    echo ==========================================
    echo   部分测试失败
    echo ==========================================
)

exit /b %ERRORLEVEL%

