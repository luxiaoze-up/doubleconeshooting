#!/bin/bash
# WSL hosts 文件修复工具
# 主要功能：
# 1. 更新主机映射（Windows 主机、WSL 主机名）
# 2. 更新网络接口映射（DNS 反向解析）
# 3. 配置 omniORB（禁用反向 DNS 查询）

set -e

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
print_success() { echo -e "${GREEN}✓ $1${NC}"; }
print_warning() { echo -e "${YELLOW}⚠️  $1${NC}"; }
print_error() { echo -e "${RED}❌ $1${NC}"; }

# 获取系统信息
get_system_info() {
    WSL_HOSTNAME=$(hostname)
    WINDOWS_IP=$(ip route show | grep -i default | awk '{ print $3}' 2>/dev/null)
    if command -v hostname.exe &> /dev/null; then
        WINDOWS_HOSTNAME=$(hostname.exe)
    else
        WINDOWS_HOSTNAME="$WSL_HOSTNAME"
    fi
}


# 更新主机映射
update_host_mappings() {
    echo "[1/4] 更新主机映射..."
    
    get_system_info
    
    if [ -z "$WINDOWS_IP" ]; then
        print_error "无法获取 Windows 主机 IP"
        return 1
    fi
    
    echo "  系统信息："
    echo "    WSL 主机名: $WSL_HOSTNAME"
    echo "    Windows 主机 IP: $WINDOWS_IP"
    echo "    Windows 主机名: $WINDOWS_HOSTNAME"
    echo ""
    
    # 备份 hosts 文件
    BACKUP_FILE="/etc/hosts.bak.$(date +%Y%m%d_%H%M%S)"
    sudo cp /etc/hosts "$BACKUP_FILE"
    print_success "已备份 hosts 文件到: $BACKUP_FILE"
    
    # 更新 Windows 主机映射
    sudo sed -i "/[[:space:]]*${WINDOWS_HOSTNAME}[[:space:]]*$/d" /etc/hosts
    sudo sed -i "/[[:space:]]*${WINDOWS_HOSTNAME}\.mshome\.net[[:space:]]*$/d" /etc/hosts
    
    if ! grep -q "^${WINDOWS_IP}[[:space:]]" /etc/hosts; then
        echo "$WINDOWS_IP    $WINDOWS_HOSTNAME" | sudo tee -a /etc/hosts > /dev/null
        echo "$WINDOWS_IP    ${WINDOWS_HOSTNAME}.mshome.net" | sudo tee -a /etc/hosts > /dev/null
        print_success "已添加 Windows 主机映射: $WINDOWS_IP -> $WINDOWS_HOSTNAME"
    else
        # 如果 IP 已存在，更新对应的主机名
        sudo sed -i "s|^${WINDOWS_IP}[[:space:]]*.*|${WINDOWS_IP}    ${WINDOWS_HOSTNAME} ${WINDOWS_HOSTNAME}.mshome.net|" /etc/hosts
        print_success "已更新 Windows 主机映射: $WINDOWS_IP -> $WINDOWS_HOSTNAME"
    fi
    
    # 确保 WSL 主机名映射存在
    if ! grep -q "^127\.0\.0\.1[[:space:]]*${WSL_HOSTNAME}" /etc/hosts; then
        echo "127.0.0.1    $WSL_HOSTNAME" | sudo tee -a /etc/hosts > /dev/null
        print_success "已添加 WSL 主机名 IPv4 映射"
    fi
    
    if ! grep -q "^::1[[:space:]]*${WSL_HOSTNAME}" /etc/hosts; then
        echo "::1          $WSL_HOSTNAME" | sudo tee -a /etc/hosts > /dev/null
        print_success "已添加 WSL 主机名 IPv6 映射"
    fi
    
    echo ""
}

# 更新网络接口映射（DNS 反向解析）
update_network_hosts() {
    echo "[2/4] 更新网络接口映射（DNS 反向解析）..."
    
    HOSTS_FILE="/etc/hosts"
    
    echo "  检测网络接口..."
    
    # 获取所有网络接口及其 IP
    declare -A INTERFACE_IPS
    declare -A HOSTNAME_MAP
    
    while IFS= read -r line; do
        if [[ $line =~ ^[0-9]+:[[:space:]]+([^:]+): ]]; then
            INTERFACE_NAME="${BASH_REMATCH[1]}"
            if [[ "$INTERFACE_NAME" == "lo" ]]; then
                continue
            fi
        elif [[ $line =~ inet[[:space:]]+([0-9.]+) ]]; then
            IP="${BASH_REMATCH[1]}"
            if [ -n "$INTERFACE_NAME" ] && [ -n "$IP" ]; then
                INTERFACE_IPS["$INTERFACE_NAME"]="$IP"
            fi
        fi
    done < <(ip -4 addr show)
    
    # 添加 Windows 主机 IP
    WINDOWS_IP=$(ip route show | grep -i default | awk '{ print $3}')
    if [ -n "$WINDOWS_IP" ]; then
        INTERFACE_IPS["wsl-dns"]="$WINDOWS_IP"
    fi
    
    echo "  检测到的网络接口："
    for iface in "${!INTERFACE_IPS[@]}"; do
        echo "    $iface -> ${INTERFACE_IPS[$iface]}"
    done
    
    # 确定映射名称
    for iface in "${!INTERFACE_IPS[@]}"; do
        IP="${INTERFACE_IPS[$iface]}"
        case "$iface" in
            eth0)
                HOSTNAME_MAP["$IP"]="wsl-eth0"
                ;;
            eth1)
                if [[ "$IP" =~ ^198\.18\. ]]; then
                    HOSTNAME_MAP["$IP"]="wsl-hyper"
                else
                    HOSTNAME_MAP["$IP"]="wsl-eth1"
                fi
                ;;
            wsl-dns)
                HOSTNAME_MAP["$IP"]="wsl-dns"
                ;;
            *)
                if [[ "$IP" =~ ^192\.168\. ]]; then
                    if [ -z "${HOSTNAME_MAP[$IP]}" ]; then
                        HOSTNAME_MAP["$IP"]="wsl-eth0-new"
                    fi
                elif [[ "$IP" =~ ^198\.18\. ]]; then
                    HOSTNAME_MAP["$IP"]="wsl-hyper"
                elif [[ "$IP" =~ ^10\. ]]; then
                    HOSTNAME_MAP["$IP"]="wsl-internal"
                else
                    HOSTNAME_MAP["$IP"]="wsl-${iface}"
                fi
                ;;
        esac
    done
    
    # 删除旧的 wsl-* 映射
    sudo sed -i '/^[0-9.]*[[:space:]]*wsl-/d' "$HOSTS_FILE"
    
    # 添加新的映射
    echo "" | sudo tee -a "$HOSTS_FILE" > /dev/null
    echo "# WSL 网络接口映射 - 自动生成于 $(date)" | sudo tee -a "$HOSTS_FILE" > /dev/null
    echo "# 用于解决 DNS 反向解析超时问题" | sudo tee -a "$HOSTS_FILE" > /dev/null
    
    for ip in "${!HOSTNAME_MAP[@]}"; do
        hostname="${HOSTNAME_MAP[$ip]}"
        echo "$ip    $hostname" | sudo tee -a "$HOSTS_FILE" > /dev/null
        print_success "已添加: $ip -> $hostname"
    done
    
    echo ""
}

# 配置 omniORB
configure_omniORB() {
    echo "[3/4] 配置 omniORB..."
    
    OMNIORB_SCRIPT="$SCRIPT_DIR/setup_omniORB.sh"
    if [ -f "$OMNIORB_SCRIPT" ]; then
        bash "$OMNIORB_SCRIPT"
        print_success "omniORB 配置完成"
    else
        print_warning "未找到 setup_omniORB.sh，跳过 omniORB 配置"
    fi
    echo ""
}

# 主程序：执行所有主要功能
main() {
    echo "=========================================="
    echo " WSL hosts 文件修复工具"
    echo "=========================================="
    echo ""
    echo "执行以下操作："
    echo "  1. 更新主机映射（Windows 主机、WSL 主机名）"
    echo "  2. 更新网络接口映射（DNS 反向解析）"
    echo "  3. 配置 omniORB（禁用反向 DNS 查询）"
    echo ""
    
    # 执行所有功能
    update_host_mappings
    update_network_hosts
    configure_omniORB
    
    echo "[4/4] 验证配置..."
    echo ""
    echo "当前 /etc/hosts 内容："
    echo "----------------------------------------"
    cat /etc/hosts
    echo "----------------------------------------"
    echo ""
    
    echo "=========================================="
    echo " 修复完成！"
    echo "=========================================="
    echo ""
    print_info "提示："
    echo "  - 主机映射已更新"
    echo "  - 网络接口映射已更新（用于 DNS 反向解析）"
    echo "  - omniORB 已配置（禁用反向 DNS 查询）"
    echo ""
    echo "如果网络环境变化，重新运行此脚本即可。"
    echo ""
}

# 执行主程序
main
