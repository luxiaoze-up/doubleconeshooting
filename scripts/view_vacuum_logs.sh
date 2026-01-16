#!/bin/bash
# 查看真空系统日志

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
LOG_FILE="$PROJECT_ROOT/logs/vacuum_system.log"

# 解析命令行参数
FOLLOW=false
LINES=50
while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--follow)
            FOLLOW=true
            shift
            ;;
        -n|--lines)
            LINES="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -f, --follow      实时跟踪日志（类似 tail -f）"
            echo "  -n, --lines N     显示最后 N 行（默认: 50）"
            echo "  --help, -h        显示此帮助信息"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

if [ ! -f "$LOG_FILE" ]; then
    echo "Log file not found: $LOG_FILE"
    echo "Server may not be running yet."
    exit 1
fi

if [ "$FOLLOW" = true ]; then
    echo "Following log file: $LOG_FILE"
    echo "Press Ctrl+C to stop"
    echo "=========================================="
    tail -f "$LOG_FILE"
else
    echo "Last $LINES lines of log file: $LOG_FILE"
    echo "=========================================="
    tail -n "$LINES" "$LOG_FILE"
    echo ""
    echo "To follow logs in real-time, use: $0 -f"
fi

