#!/bin/bash
# Configure omniORB on Ubuntu 24.04 to avoid reverse lookup timeouts.

set -e

echo "=========================================="
echo " omniORB Configuration Setup (Ubuntu 24.04)"
echo "=========================================="

CONFIG_FILE="/etc/omniORB.cfg"

echo ""
echo "Creating omniORB configuration..."

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

echo ""
echo "=========================================="
echo " Configuration Complete"
echo "=========================================="
echo ""
echo "omniORB config ($CONFIG_FILE):"
cat $CONFIG_FILE
echo ""
echo "Configuration effect:"
echo "  - resolveNamesForTransportRules=0 tells omniORB to SKIP all reverse DNS queries"
echo "  - /etc/resolv.conf is left under Ubuntu/systemd-resolved management"
echo ""
echo "Please restart your device servers for changes to take effect."
