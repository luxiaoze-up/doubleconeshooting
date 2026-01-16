#!/bin/bash
# 启动 GUI 的脚本，处理显示问题

# 设置 TANGO_HOST
export TANGO_HOST=192.168.80.98:10000

# 检查是否有 DISPLAY 设置
if [ -z "$DISPLAY" ]; then
    echo "警告: 未检测到 DISPLAY 环境变量"
    echo ""
    echo "要运行 GUI，你需要："
    echo "1. 在 Windows 上安装 X 服务器（如 VcXsrv 或 Xming）"
    echo "2. 在容器启动时添加: -e DISPLAY=host.docker.internal:0"
    echo ""
    echo "或者使用 offscreen 渲染（无显示）："
    export QT_QPA_PLATFORM=offscreen
    echo "已设置 QT_QPA_PLATFORM=offscreen"
fi

python3 gui/vacuum_chamber_gui/main.py
