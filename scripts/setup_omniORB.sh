#!/bin/bash
# Setup omniORB and system DNS configuration to avoid reverse lookup timeouts
# This is necessary in WSL where dynamic IP addresses cause DNS timeout
#
# 解决方案分两层：
# 1. omniORB 层：禁用反向 DNS 查询（根本解决）
# 2. 系统 DNS 层：缩短超时时间（双重保障）

set -e

echo "=========================================="
echo " omniORB & DNS Configuration Setup"
echo "=========================================="

# ============================================
# 1. omniORB Configuration (根本解决方案)
# ============================================
CONFIG_FILE="/etc/omniORB.cfg"

echo ""
echo "[1/2] Creating omniORB configuration..."

sudo bash -c "cat > $CONFIG_FILE << 'EOF'
# omniORB configuration for Tango Controls
# Created by setup_omniORB.sh

# CRITICAL: Disable reverse DNS lookup for transport rules
# This completely skips DNS reverse queries, regardless of IP changes
# 这是根本解决方案：完全禁用反向 DNS 查询
resolveNamesForTransportRules = 0

# Set reasonable client timeouts (milliseconds)
clientCallTimeOutPeriod = 5000
clientConnectTimeOutPeriod = 3000

# Server call timeout
serverCallTimeOutPeriod = 5000
EOF"

echo "  ✓ Created $CONFIG_FILE"

# ============================================
# 2. System DNS Configuration (双重保障)
# ============================================
echo ""
echo "[2/2] Configuring system DNS timeout..."

# Check if resolv.conf is managed by WSL
if grep -q "generateResolvConf" /etc/wsl.conf 2>/dev/null; then
    echo "  Note: /etc/resolv.conf is managed by WSL config"
fi

# Add timeout options if not already present
RESOLV_CONF="/etc/resolv.conf"
if ! grep -q "options.*timeout" "$RESOLV_CONF" 2>/dev/null; then
    # WSL 的 resolv.conf 可能是自动生成的，尝试添加选项
    if [ -w "$RESOLV_CONF" ] || sudo test -w "$RESOLV_CONF"; then
        # 备份原文件
        sudo cp "$RESOLV_CONF" "${RESOLV_CONF}.bak" 2>/dev/null || true
        # 添加 DNS 超时选项：1秒超时，1次尝试
        echo "options timeout:1 attempts:1" | sudo tee -a "$RESOLV_CONF" > /dev/null
        echo "  ✓ Added DNS timeout options to $RESOLV_CONF"
    else
        echo "  ! Cannot modify $RESOLV_CONF (read-only), skipping DNS timeout config"
        echo "    This is fine - omniORB config alone is sufficient"
    fi
else
    echo "  ✓ DNS timeout options already configured"
fi

# ============================================
# Summary
# ============================================
echo ""
echo "=========================================="
echo " Configuration Complete"
echo "=========================================="
echo ""
echo "omniORB config ($CONFIG_FILE):"
cat $CONFIG_FILE
echo ""
echo "Why this is a PERMANENT solution:"
echo "  - resolveNamesForTransportRules=0 tells omniORB to SKIP all reverse DNS queries"
echo "  - This works regardless of IP changes or network environment"
echo "  - No need to update /etc/hosts when IP changes"
echo ""
echo "Please restart your device servers for changes to take effect."

