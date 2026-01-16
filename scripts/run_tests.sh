#!/bin/bash
# 运行自动化测试脚本

set -e

echo "=========================================="
echo "  真空系统自动化测试"
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

# 测试选项
TEST_OPTS=""
REPORT_OPTS=""

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --unit)
            TEST_OPTS="$TEST_OPTS -m unit"
            shift
            ;;
        --integration)
            TEST_OPTS="$TEST_OPTS -m integration"
            shift
            ;;
        --e2e)
            TEST_OPTS="$TEST_OPTS -m e2e"
            shift
            ;;
        --html)
            REPORT_OPTS="$REPORT_OPTS --html=reports/report.html --self-contained-html"
            shift
            ;;
        --cov)
            REPORT_OPTS="$REPORT_OPTS --cov=. --cov-report=html --cov-report=term-missing"
            shift
            ;;
        --verbose|-v)
            TEST_OPTS="$TEST_OPTS -v"
            shift
            ;;
        *)
            echo "未知参数: $1"
            exit 1
            ;;
    esac
done

# 创建报告目录
mkdir -p reports

# 运行测试
echo "开始运行测试..."
echo "使用命令: $PYTEST_CMD"
echo ""

# 如果指定了--html或--cov，添加到REPORT_OPTS
if [[ "$*" == *"--html"* ]] || [[ "$*" == *"--cov"* ]]; then
    # 用户已经指定了报告选项，使用用户指定的
    $PYTEST_CMD tests/ \
        $TEST_OPTS \
        --tb=short \
        --color=yes \
        "$@"
else
    # 使用脚本默认的报告选项
    $PYTEST_CMD tests/ \
        $TEST_OPTS \
        $REPORT_OPTS \
        --tb=short \
        --color=yes
fi

TEST_RESULT=$?

echo ""
echo "=========================================="
if [ $TEST_RESULT -eq 0 ]; then
    echo "  所有测试通过！"
else
    echo "  部分测试失败"
fi
echo "=========================================="

exit $TEST_RESULT

