#!/bin/bash
# WSL环境设置脚本 - 安装依赖项和配置环境
# 注意: 此脚本专为Tango开发环境设计，直接安装包到系统Python

set -e  # 遇到错误立即退出

echo "=== Double Cone Shooting Control System - WSL环境设置 ==="
echo ""

# 检测WSL版本
if [ -f /proc/version ]; then
    echo "检测到WSL环境"
    cat /proc/version
    echo ""
fi

# 更新包管理器
echo "1. 更新包管理器..."
sudo apt-get update

# 安装基础编译工具
echo ""
echo "2. 安装基础编译工具..."
sudo apt-get install -y \
    build-essential \
    cmake \
    g++ \
    make \
    pkg-config \
    git

# 安装Qt5开发库
echo ""
echo "3. 安装Qt5开发库..."
sudo apt-get install -y \
    qtbase5-dev \
    qtbase5-dev-tools \
    qttools5-dev \
    qttools5-dev-tools \
    qt5-qmake \
    libqt5widgets5 \
    libqt5core5a

# 尝试安装Tango Controls (可选)
echo ""
echo "4. 检查Tango Controls..."
if dpkg -l | grep -q libtango; then
    echo "  ✓ Tango Controls已安装"
else
    echo "  ⚠ Tango Controls未安装"
    echo "  提示: 如果需要Tango功能，请手动安装Tango Controls"
    echo "  参考: https://www.tango-controls.org/developers/downloads/"
    read -p "  是否现在尝试安装Tango? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "  请按照Tango官方文档安装..."
        # 这里可以添加Tango安装命令
    fi
fi

# 安装Python3和pip (用于启动脚本)
echo ""
echo "5. 安装Python3和依赖..."
sudo apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    python3-full

# 安装Python依赖
echo ""
echo "6. 安装Python依赖..."
if [ -f requirements.txt ]; then
    echo "  检测到requirements.txt，准备安装Python包..."
    echo "  注意: 这是Tango开发专用环境，将使用 --break-system-packages"
    
    # 直接安装到系统Python (开发环境专用)
    pip3 install -r requirements.txt --user --break-system-packages
    
    echo "  ✓ Python依赖已安装到用户目录"
    echo "  提示: 这是开发环境，包已安装到 ~/.local/lib/python3.*/site-packages"
else
    echo "  ⚠ requirements.txt 不存在，跳过Python依赖安装"
fi

# 设置环境变量
echo ""
echo "7. 配置环境变量..."
if ! grep -q "QTFRAMEWORK_BYPASS_LICENSE_CHECK" ~/.bashrc; then
    echo 'export QTFRAMEWORK_BYPASS_LICENSE_CHECK=1' >> ~/.bashrc
    echo "  ✓ 已添加Qt许可证检查绕过"
fi

# 检查Tango数据库
echo ""
echo "8. 检查Tango数据库配置..."
if command -v tango_admin &> /dev/null; then
    echo "  ✓ Tango工具已安装"
    echo "  提示: 运行 'tango_admin --ping-database' 检查数据库连接"
else
    echo "  ⚠ Tango工具未安装"
fi

# 创建必要的目录
echo ""
echo "9. 创建必要的目录..."
mkdir -p build-linux
mkdir -p config
mkdir -p logs

# 检查配置文件
if [ ! -f scripts/devices.json ]; then
    echo "  创建默认设备配置文件..."
    mkdir -p scripts
    cat > scripts/devices.json << 'EOF'
{
    "devices": {
        "large_stroke": "sys/large_stroke/1",
        "six_dof": "sys/six_dof/1",
        "auxiliary_support": "sys/auxiliary/1",
        "backlight_system": "sys/backlight/1",
        "vacuum": "sys/vacuum/1",
        "interlock": "sys/interlock/1"
    },
    "simulation": {
        "enabled": false
    }
}
EOF
fi

echo ""
echo "=== 环境设置完成! ==="
echo ""
echo "下一步:"
echo "1. 运行编译: ./wsl_build.sh"
echo "2. 或运行完整构建和测试: ./build_and_test.sh"
echo ""
echo "注意: 如果使用Tango，请确保Tango数据库服务正在运行"

