#!/bin/bash
# WSL环境部署脚本 - 启动所有服务

# 不立即退出，允许某些命令失败（如Tango检测）
set +e

echo "=== Double Cone Shooting Control System - WSL部署 ==="
echo ""

# 检查构建目录
BUILD_DIR="build-linux"
if [ ! -d "$BUILD_DIR" ]; then
    echo "错误: 构建目录不存在，请先运行编译: ./wsl_build.sh"
    exit 1
fi

# 检查可执行文件
cd "$BUILD_DIR"

# 函数: 启动MySQL/MariaDB
start_mysql() {
    echo "检查MySQL/MariaDB服务..."
    
    # 检查是否已有MySQL进程在运行
    if pgrep -f "mysqld\|mariadb" > /dev/null; then
        echo "  ✓ MySQL/MariaDB进程已在运行"
        return 0
    fi
    
    # 检查是否是WSL环境（没有systemd）
    if [ ! -d /run/systemd/system ] && [ ! -S /run/systemd/private ]; then
        echo "  检测到WSL环境，尝试直接启动MySQL..."
        
        # 查找MySQL可执行文件
        MYSQL_EXE=""
        POSSIBLE_PATHS=(
            "/usr/sbin/mysqld"
            "/usr/bin/mysqld_safe"
            "/usr/sbin/mariadbd"
            "$(which mysqld 2>/dev/null)"
        )
        
        for path in "${POSSIBLE_PATHS[@]}"; do
            if [ -f "$path" ] && [ -x "$path" ]; then
                MYSQL_EXE="$path"
                break
            fi
        done
        
        if [ -z "$MYSQL_EXE" ]; then
            echo "  ⚠ 未找到MySQL可执行文件"
            echo "  提示: 请确保MySQL/MariaDB已安装"
            return 1
        fi
        
        echo "  启动MySQL: $MYSQL_EXE"
        # MySQL通常需要特定用户和配置，直接启动可能失败
        # 建议用户手动启动或使用service命令
        echo "  ⚠ 在WSL中直接启动MySQL较复杂"
        echo "  建议手动启动: sudo service mysql start"
        echo "  或: sudo mysqld_safe --user=mysql &"
        return 1
    else
        # 使用systemd启动
        if systemctl list-unit-files 2>/dev/null | grep -qE "mysql\.service|mariadb\.service"; then
            if systemctl is-active --quiet mysql 2>/dev/null || systemctl is-active --quiet mariadb 2>/dev/null; then
                echo "  ✓ MySQL/MariaDB服务已在运行"
                return 0
            else
                echo "  启动MySQL/MariaDB服务..."
                if sudo systemctl start mysql 2>/dev/null || sudo systemctl start mariadb 2>/dev/null; then
                    echo "  ✓ MySQL/MariaDB服务已启动"
                    sleep 2
                    return 0
                else
                    echo "  ✗ 无法启动MySQL/MariaDB服务"
                    return 1
                fi
            fi
        fi
    fi
    
    return 1
}

# 函数: 启动Tango数据库
start_tango_db() {
    if ! command -v tango_admin &> /dev/null; then
        echo "⚠ Tango工具未安装，跳过数据库启动"
        return 1
    fi
    
    # 先检查数据库是否已经在运行
    if tango_admin --ping-database >/dev/null 2>&1; then
        echo "✓ Tango数据库已在运行"
        return 0
    fi
    
    echo "启动Tango数据库..."
    
    # 先启动MySQL（Tango数据库需要MySQL）
    if ! start_mysql; then
        echo "  ⚠ MySQL/MariaDB未运行，Tango数据库可能无法启动"
        echo "  提示: Tango数据库需要MySQL/MariaDB支持"
        echo "  请手动启动MySQL: sudo service mysql start"
    fi
    
    # 检查是否是WSL环境（没有systemd）
    if [ ! -d /run/systemd/system ] && [ ! -S /run/systemd/private ]; then
        echo "  检测到WSL环境（无systemd），使用直接启动方式..."
        
        # 查找Tango数据库可执行文件
        TANGO_DB_EXE=""
        POSSIBLE_PATHS=(
            "/usr/lib/tango/Databaseds"
            "/usr/bin/Databaseds"
            "/usr/local/bin/Databaseds"
            "$(which Databaseds 2>/dev/null)"
        )
        
        for path in "${POSSIBLE_PATHS[@]}"; do
            if [ -f "$path" ] && [ -x "$path" ]; then
                TANGO_DB_EXE="$path"
                break
            fi
        done
        
        if [ -z "$TANGO_DB_EXE" ]; then
            echo "  ✗ 未找到Tango数据库可执行文件"
            echo "  提示: 请确保Tango数据库已正确安装"
            return 1
        fi
        
        # 检查是否已有进程在运行
        if pgrep -f "Databaseds" > /dev/null; then
            echo "  ✓ Tango数据库进程已在运行"
        else
            echo "  启动Tango数据库: $TANGO_DB_EXE"
            
            # 获取TANGO_HOST环境变量
            TANGO_HOST="${TANGO_HOST:-localhost:10000}"
            if [ -f /etc/tangorc ]; then
                source /etc/tangorc 2>/dev/null || true
            fi
            
            # 在后台启动数据库
            nohup "$TANGO_DB_EXE" 2 -ORBendPoint "giop:tcp:${TANGO_HOST}" > /tmp/tango_db.log 2>&1 &
            TANGO_DB_PID=$!
            echo "  Tango数据库进程PID: $TANGO_DB_PID"
            sleep 3
            
            # 检查进程是否还在运行
            if ps -p $TANGO_DB_PID > /dev/null 2>&1; then
                echo "  ✓ Tango数据库进程已启动"
            else
                echo "  ⚠ Tango数据库进程可能启动失败"
                echo "  查看日志: tail -20 /tmp/tango_db.log"
                return 1
            fi
        fi
    else
        # 使用systemd（非WSL环境）
        if systemctl list-unit-files 2>/dev/null | grep -q "tango-db.service"; then
            if systemctl is-active --quiet tango-db 2>/dev/null; then
                echo "  ✓ Tango数据库服务已在运行"
            else
                echo "  使用systemctl启动Tango数据库服务..."
                if sudo systemctl start tango-db 2>/dev/null; then
                    echo "  ✓ 服务启动命令已执行"
                    sleep 3
                else
                    echo "  ✗ 无法启动服务"
                    return 1
                fi
            fi
        else
            echo "  ⚠ 未找到tango-db systemd服务"
            return 1
        fi
    fi
    
    # 等待数据库就绪并测试连接
    echo "  等待数据库就绪..."
    local max_retries=10
    local retry_count=0
    
    while [ $retry_count -lt $max_retries ]; do
        if tango_admin --ping-database >/dev/null 2>&1; then
            echo "  ✓ Tango数据库连接正常"
            return 0
        fi
        retry_count=$((retry_count + 1))
        sleep 1
    done
    
    echo "  ⚠ Tango数据库启动超时，但继续尝试..."
    return 1
}

# 检查并启动Tango数据库 (如果使用Tango)
USE_TANGO=false
if command -v tango_admin &> /dev/null; then
    # 先尝试启动数据库
    start_tango_db
    
    # 检查Tango数据库连接
    echo "检查Tango数据库连接..."
    tango_admin --ping-database >/dev/null 2>&1
    TANGO_EXIT_CODE=$?
    
    if [ $TANGO_EXIT_CODE -eq 0 ]; then
        echo "✓ Tango数据库连接正常"
        USE_TANGO=true
    else
        echo "⚠ Tango数据库未运行或无法连接（退出码: $TANGO_EXIT_CODE）"
        echo "  提示: 运行模拟模式不需要Tango"
        echo ""
        echo "  故障排除:"
        echo "  1. 确保MySQL/MariaDB正在运行:"
        echo "     sudo service mysql start"
        echo "     或: sudo systemctl start mariadb"
        echo ""
        echo "  2. 检查MySQL连接:"
        echo "     sudo mysql -u root -e 'SELECT 1;'"
        echo ""
        echo "  3. 确保Tango数据库用户存在:"
        echo "     sudo mysql -e \"CREATE USER IF NOT EXISTS 'tango'@'localhost';\""
        echo "     sudo mysql -e \"CREATE DATABASE IF NOT EXISTS tango;\""
        echo "     sudo mysql -e \"GRANT ALL PRIVILEGES ON tango.* TO 'tango'@'localhost';\""
        echo ""
        echo "  4. 手动启动Tango数据库:"
        echo "     /usr/lib/tango/Databaseds 2 -ORBendPoint giop:tcp:localhost:10000 &"
        USE_TANGO=false
    fi
else
    echo "⚠ Tango工具未安装，将使用模拟模式"
fi

# 选择运行模式
MODE="sim"
if [ "$USE_TANGO" = true ]; then
    read -p "选择运行模式 [s]im模拟 / [r]eal真实: " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Rr]$ ]]; then
        MODE="real"
    fi
else
    # 如果Tango不可用，自动使用模拟模式
    echo "Tango不可用，自动使用模拟模式"
    MODE="sim"
fi

echo ""
echo "运行模式: $([ "$MODE" == "sim" ] && echo "模拟模式" || echo "真实模式")"
echo ""

# 函数: 注册设备到Tango数据库
register_devices() {
    if [ "$MODE" == "real" ] && [ "$USE_TANGO" = true ]; then
        echo "注册设备到Tango数据库..."
        cd ..
        if python3 scripts/register_devices.py; then
            echo "✓ 设备注册完成"
        else
            echo "⚠ 设备注册失败，但继续启动..."
        fi
        cd "$BUILD_DIR"
    fi
}

# 函数: 启动设备服务器
start_servers() {
    if [ "$MODE" == "real" ] && [ "$USE_TANGO" = true ]; then
        echo "启动Tango设备服务器..."
        cd ..
        
        # 在后台启动服务器脚本，但捕获输出
        python3 scripts/start_servers.py > /tmp/device_servers.log 2>&1 &
        SERVER_PID=$!
        echo "  设备服务器管理进程PID: $SERVER_PID"
        
        # 等待更长时间让服务器完全启动
        echo "  等待设备服务器启动..."
        sleep 5
        
        # 检查设备服务器进程是否在运行
        RUNNING_SERVERS=$(pgrep -f "large_stroke_server|six_dof_server|vacuum_server|interlock_server|multi_axis_server" | wc -l)
        if [ "$RUNNING_SERVERS" -gt 0 ]; then
            echo "  ✓ 检测到 $RUNNING_SERVERS 个设备服务器进程正在运行"
        else
            echo "  ⚠ 未检测到设备服务器进程，可能启动失败"
            echo "  查看日志: tail -f /tmp/device_servers.log"
        fi
        
        cd "$BUILD_DIR"
    else
        echo "模拟模式: 跳过设备服务器启动"
    fi
}

# 函数: 启动GUI
start_gui() {
    GUI_EXE="main_controller"
    if [ ! -f "$GUI_EXE" ]; then
        echo "✗ GUI可执行文件不存在: $GUI_EXE"
        return 1
    fi
    
    echo "启动GUI界面..."
    
    # 设置中文编码环境变量
    export LANG=zh_CN.UTF-8 2>/dev/null || export LANG=C.UTF-8
    export LC_ALL=${LANG}
    export LC_CTYPE=${LANG}
    
    if [ "$MODE" == "sim" ]; then
        echo "  运行命令: ./$GUI_EXE --sim"
        env LANG=${LANG} LC_ALL=${LC_ALL} ./$GUI_EXE --sim &
    else
        echo "  运行命令: ./$GUI_EXE"
        env LANG=${LANG} LC_ALL=${LC_ALL} ./$GUI_EXE &
    fi
    GUI_PID=$!
    echo "  GUI进程PID: $GUI_PID"
    return 0
}

# 函数: 清理进程
cleanup() {
    echo ""
    echo "正在停止所有服务..."
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
    fi
    if [ ! -z "$GUI_PID" ]; then
        kill $GUI_PID 2>/dev/null || true
    fi
    # 停止所有设备服务器进程
    pkill -f "large_stroke_server\|six_dof_server\|vacuum_server\|interlock_server\|multi_axis_server" 2>/dev/null || true
    echo "所有服务已停止"
    exit 0
}

# 注册清理函数
trap cleanup SIGINT SIGTERM

# 重新启用错误退出（对于关键操作）
set -e

# 注册设备（在启动服务器之前）
register_devices

# 启动服务
start_servers
start_gui
GUI_START_RESULT=$?

if [ $GUI_START_RESULT -eq 0 ]; then
    echo ""
    echo "=== 部署完成! ==="
    echo ""
    echo "服务正在运行:"
    [ ! -z "$SERVER_PID" ] && echo "  - 设备服务器: PID $SERVER_PID"
    [ ! -z "$GUI_PID" ] && echo "  - GUI界面: PID $GUI_PID"
    echo ""
    echo "按 Ctrl+C 停止所有服务"
    echo ""
    
    # 等待
    wait
else
    echo "✗ 部署失败"
    cleanup
    exit 1
fi

