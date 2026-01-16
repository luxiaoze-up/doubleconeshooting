#!/bin/bash
# 只启动 VacuumSystemDevice 服务器

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 解析命令行参数
SHOW_LOGS=true
BACKGROUND=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-display|--quiet|-q)
            SHOW_LOGS=false
            shift
            ;;
        --background|-b)
            BACKGROUND=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --no-display, --quiet, -q   只输出到日志文件，不在终端显示"
            echo "  --background, -b             后台运行（自动启用 --no-display）"
            echo "  --help, -h                   显示此帮助信息"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "=== Starting VacuumSystemDevice Only ==="

# 检查 TANGO_HOST
if [ -z "$TANGO_HOST" ]; then
    export TANGO_HOST=localhost:10000
    echo "TANGO_HOST not set, using default: $TANGO_HOST"
fi

# 1. 注册设备
echo ""
echo "[1/2] Registering VacuumSystemDevice..."
python3 "$SCRIPT_DIR/register_devices.py" --devices vacuum_system_2 --force

if [ $? -ne 0 ]; then
    echo "✗ Failed to register device. Is Tango database running?"
    echo "  Try: sudo systemctl start tango-db"
    exit 1
fi

# 2. 启动服务器
echo ""
echo "[2/2] Starting vacuum_system_server..."
EXECUTABLE="$PROJECT_ROOT/build-linux/vacuum_system_server"

if [ ! -f "$EXECUTABLE" ]; then
    echo "✗ Executable not found: $EXECUTABLE"
    echo "  Please build the project first: cd build-linux && make"
    exit 1
fi

# 创建日志目录
mkdir -p "$PROJECT_ROOT/logs"

# 日志文件路径（带时间戳，避免覆盖）
LOG_FILE="$PROJECT_ROOT/logs/vacuum_system.log"
TIMESTAMPED_LOG="$PROJECT_ROOT/logs/vacuum_system_$(date +%Y%m%d_%H%M%S).log"

# 如果日志文件已存在且较大（>10MB），重命名为带时间戳的文件
if [ -f "$LOG_FILE" ] && [ $(stat -f%z "$LOG_FILE" 2>/dev/null || stat -c%s "$LOG_FILE" 2>/dev/null || echo 0) -gt 10485760 ]; then
    mv "$LOG_FILE" "$TIMESTAMPED_LOG"
    echo "Old log file moved to: $TIMESTAMPED_LOG"
fi

echo "Starting server with instance 'vacuum2'..."
echo "Device: sys/vacuum/2"
echo "Log file: $LOG_FILE"
echo ""
echo "To view logs in real-time, use:"
echo "  tail -f $LOG_FILE"
echo ""

# 后台模式
if [ "$BACKGROUND" = true ]; then
    SHOW_LOGS=false
    echo "Starting in background mode..."
    nohup stdbuf -oL -eL "$EXECUTABLE" vacuum2 -v4 > "$LOG_FILE" 2>&1 &
    PID=$!
    echo "Server started with PID: $PID"
    echo "Log file: $LOG_FILE"
    echo "To stop: kill $PID"
    exit 0
fi

# 前台模式：根据 SHOW_LOGS 决定是否在终端显示
if [ "$SHOW_LOGS" = true ]; then
    # 同时输出到终端和文件
    stdbuf -oL -eL "$EXECUTABLE" vacuum2 -v4 2>&1 | tee "$LOG_FILE"
else
    # 只输出到文件
    echo "Logs will only be written to file (use 'tail -f $LOG_FILE' to view)"
    stdbuf -oL -eL "$EXECUTABLE" vacuum2 -v4 > "$LOG_FILE" 2>&1
fi

