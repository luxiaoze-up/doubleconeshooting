#!/bin/bash
# 运行测试并生成报告脚本

set -e

echo "=========================================="
echo "  真空系统自动化测试（带报告）"
echo "  GUI-Server-PLC通信测试"
echo "=========================================="
echo ""

# 检查pytest是否安装（尝试多种方式）
PYTEST_CMD=""
if command -v pytest &> /dev/null; then
    PYTEST_CMD="pytest"
elif python3 -m pytest --version &> /dev/null; then
    PYTEST_CMD="python3 -m pytest"
elif python -m pytest --version &> /dev/null; then
    PYTEST_CMD="python -m pytest"
else
    echo "错误: pytest未安装"
    echo "请运行: pip install pytest pytest-html pytest-cov"
    exit 1
fi

# 创建报告目录
mkdir -p reports

# 运行测试并生成报告
echo "开始运行测试并生成报告..."
echo "使用命令: $PYTEST_CMD"
echo ""

$PYTEST_CMD tests/ \
    --html=reports/report.html \
    --self-contained-html \
    --cov=. \
    --cov-report=html \
    --cov-report=term-missing \
    --tb=short \
    --color=yes

TEST_RESULT=$?

echo ""
echo "=========================================="
if [ $TEST_RESULT -eq 0 ]; then
    echo "  所有测试通过！"
    echo ""
    echo "  报告位置:"
    echo "  - HTML测试报告: reports/report.html"
    echo "  - 覆盖率报告: htmlcov/index.html"
else
    echo "  部分测试失败"
    echo ""
    echo "  查看详细报告: reports/report.html"
fi
echo "=========================================="

exit $TEST_RESULT

