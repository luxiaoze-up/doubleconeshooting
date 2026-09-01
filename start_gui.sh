#!/bin/bash
# 启动 GUI 的脚本，处理显示问题

# 设置 TANGO_HOST
export TANGO_HOST=192.168.80.98:10000

# Ubuntu 桌面通常提供 DISPLAY（X11）或 WAYLAND_DISPLAY。
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && [ "${QT_QPA_PLATFORM:-}" != "offscreen" ]; then
    echo "错误: 未检测到 Ubuntu 图形会话。"
    echo "请在桌面终端运行，或显式设置 QT_QPA_PLATFORM=offscreen 进行无界面检查。"
    exit 1
fi

python3 gui/vacuum_chamber_gui/main.py
