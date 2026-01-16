#!/bin/bash
# 查找服务器配置文件

echo "=== Searching for server config files ==="

# 常见的配置文件位置
LOCATIONS=(
    "/etc/tango/servers"
    "/usr/local/etc/tango/servers"
    "$HOME/.tango/servers"
    "/mnt/d/00.My_workspace/DoubleConeShooting"
)

for loc in "${LOCATIONS[@]}"; do
    if [ -d "$loc" ]; then
        echo ""
        echo "Checking: $loc"
        find "$loc" -name "*auxiliary*" -o -name "*.ini" -o -name "*.conf" 2>/dev/null | head -20
    fi
done

echo ""
echo "=== Checking if server is running ==="
if pgrep -f auxiliary_support_server > /dev/null; then
    echo "Server is running"
    echo "Process info:"
    ps aux | grep auxiliary_support_server | grep -v grep
else
    echo "Server is not running"
fi

