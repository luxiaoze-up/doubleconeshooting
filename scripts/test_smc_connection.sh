#!/bin/bash
#
# 编译并运行 SMC 控制器连接测试
#

cd "$(dirname "$0")"

echo "=== 编译测试程序 ==="
g++ -o test_smc_connection test_smc_connection.cpp \
    -L../lib -lLTSMC \
    -Wl,-rpath,$(pwd)/../lib \
    -I../include/drivers

if [ $? -ne 0 ]; then
    echo "编译失败!"
    exit 1
fi

echo ""
echo "=== 运行测试 ==="
export LD_LIBRARY_PATH=$(pwd)/../lib:$LD_LIBRARY_PATH
./test_smc_connection

echo ""
echo "=== 清理 ==="
rm -f test_smc_connection

