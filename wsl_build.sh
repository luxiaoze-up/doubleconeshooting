#!/bin/bash
# WSL环境专用编译脚本

set -e  # 遇到错误立即退出

echo "=== Double Cone Shooting Control System - WSL编译 ==="
echo ""

# 设置环境变量
export QTFRAMEWORK_BYPASS_LICENSE_CHECK=1

# 检查是否在项目根目录
if [ ! -f "CMakeLists.txt" ]; then
    echo "错误: 请在项目根目录运行此脚本"
    exit 1
fi

# 选择构建目录
BUILD_DIR="build-linux"
if [ "$1" == "clean" ]; then
    echo "清理旧的构建文件..."
    rm -rf "$BUILD_DIR"
    shift
fi

# 如果构建目录存在但MOC文件有问题，也清理autogen目录
if [ -d "$BUILD_DIR/main_controller_autogen" ]; then
    echo "清理Qt MOC生成文件..."
    rm -rf "$BUILD_DIR/main_controller_autogen"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置CMake（确保环境变量传递）
echo "配置CMake..."
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Debug"
if [ "$1" == "release" ]; then
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release"
    echo "  构建类型: Release"
else
    echo "  构建类型: Debug"
fi

# 使用env确保环境变量传递到CMake和MOC
env QTFRAMEWORK_BYPASS_LICENSE_CHECK=1 cmake .. $CMAKE_ARGS

# 检查配置结果
if [ $? -ne 0 ]; then
    echo "✗ CMake配置失败!"
    exit 1
fi

# 获取CPU核心数
CORES=$(nproc)
echo ""
echo "使用 $CORES 个CPU核心进行编译..."

# 编译（确保环境变量传递）
echo "开始编译..."
echo "环境变量 QTFRAMEWORK_BYPASS_LICENSE_CHECK=$QTFRAMEWORK_BYPASS_LICENSE_CHECK"
env QTFRAMEWORK_BYPASS_LICENSE_CHECK=1 make -j$CORES

# 检查编译结果
if [ $? -ne 0 ]; then
    echo "✗ 编译失败!"
    exit 1
fi

echo ""
echo "=== 编译成功! ==="
echo ""

# 列出生成的可执行文件
echo "生成的可执行文件:"
echo "---"
find . -maxdepth 1 -type f -executable ! -name "*.sh" ! -name "*.so" ! -name "*.a" | while read exe; do
    if [ -x "$exe" ]; then
        echo "  ✓ $(basename $exe)"
    fi
done

# 特别检查反射光成像服务器
if [ -f "reflection_imaging_server" ]; then
    echo ""
    echo "=== 反射光成像服务器编译成功 ==="
    ls -lh reflection_imaging_server
fi

echo ""
echo "可执行文件位置: $(pwd)"
echo ""
echo "下一步:"
echo "1. 运行测试: ./test_devices"
echo "2. 启动设备服务器: cd .. && python3 scripts/start_servers.py"
echo "3. 测试反射光成像: cd .. && python3 scripts/test_image_api.py"
echo "4. 启动GUI (模拟模式): ./main_controller --sim"
echo "5. 启动GUI (真实模式): ./main_controller"

