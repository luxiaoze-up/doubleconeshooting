#!/bin/bash
# 查找辅助支撑服务器的配置文件

echo "=== Checking Tango server configuration files ==="
echo ""

# 检查常见的配置文件位置
CONFIG_LOCATIONS=(
    "/etc/tango/servers/auxiliary_support_server.ini"
    "/etc/tango/servers/auxiliary.ini"
    "/usr/local/etc/tango/servers/auxiliary_support_server.ini"
    "$HOME/.tango/servers/auxiliary_support_server.ini"
)

FOUND=false
for loc in "${CONFIG_LOCATIONS[@]}"; do
    if [ -f "$loc" ]; then
        echo "✓ Found: $loc"
        echo "---"
        cat "$loc"
        echo ""
        FOUND=true
    fi
done

if [ "$FOUND" = false ]; then
    echo "⚠ No server config file found in common locations"
    echo ""
    echo "Trying to find via tango_admin..."
    if command -v tango_admin &> /dev/null; then
        echo "Listing servers:"
        tango_admin --list-server
        echo ""
        echo "Try: tango_admin --server auxiliary_support_server/auxiliary --info"
    fi
fi

echo ""
echo "=== Checking database server definition ==="
python3 << 'EOF'
import tango
try:
    db = tango.Database()
    # Try to get server info
    try:
        info = db.get_server_info("auxiliary_support_server/auxiliary")
        print("Server info object:")
        print(f"  Type: {type(info)}")
        print(f"  Attributes: {[a for a in dir(info) if not a.startswith('_')]}")
    except Exception as e:
        print(f"Could not get server info: {e}")
    
    # List all devices for this server
    all_devices = db.get_device_name("*", "*")
    server_devices = []
    for dev in all_devices:
        try:
            dev_info = db.get_device_info(dev)
            if "auxiliary" in dev_info.ds_full_name:
                server_devices.append((dev, dev_info.ds_full_name))
        except:
            pass
    
    print(f"\nDevices found for auxiliary_support_server:")
    for dev, server in sorted(server_devices):
        print(f"  {dev} -> {server}")
except Exception as e:
    print(f"Error: {e}")
EOF

