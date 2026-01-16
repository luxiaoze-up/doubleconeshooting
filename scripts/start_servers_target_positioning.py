#!/usr/bin/env python3
"""
靶定位页面调试专用 - 设备服务器启动脚本
Target Positioning Page Debug - Device Server Startup Script

仅启动靶定位页面所需的服务：
- MotionController-1 (ctrl1) - 大行程设备需要
- MotionController-2 (ctrl2) - 六自由度设备需要
- LargeStroke server - 大行程控制
- SixDof server - 六自由度控制
"""

import os
import subprocess
import time
import sys

# 靶定位页面所需的设备服务器配置
DEVICE_SERVERS = [
    # ========== 运动控制器 (2台) ==========
    {
        'name': 'MotionController-1',
        'executable': 'motion_controller_server',
        'instance': 'ctrl1',
        'device': 'sys/motion/1'
    },
    {
        'name': 'MotionController-2',
        'executable': 'motion_controller_server',
        'instance': 'ctrl2',
        'device': 'sys/motion/2'
    },
    # ========== 大行程 (控制器1) ==========
    {
        'name': 'LargeStroke',
        'executable': 'large_stroke_server',
        'instance': 'large_stroke',
        'device': 'sys/large_stroke/1'
    },
    # ========== 六自由度 (控制器2) ==========
    {
        'name': 'SixDof',
        'executable': 'six_dof_server',
        'instance': 'six_dof',
        'device': 'sys/six_dof/1'
    }
]

def start_device_servers():
    """启动所有C++设备服务器（并行启动以提高速度）"""
    processes = []
    
    print("=" * 60)
    print("靶定位页面调试模式 - 启动设备服务器")
    print("Target Positioning Page Debug Mode - Starting Device Servers")
    print("=" * 60)
    print()
    
    # 检查是否在scripts目录
    base_path = ".." if os.path.basename(os.getcwd()) == "scripts" else "."
    
    # 创建日志目录
    log_dir = os.path.join(base_path, "logs")
    os.makedirs(log_dir, exist_ok=True)
    
    # 阶段1：并行启动所有服务器（不等待）
    print("阶段1: 并行启动设备服务器...")
    for server in DEVICE_SERVERS:
        # 优先尝试 build-linux (WSL/Linux)，然后尝试 build (Windows)
        exe_path = os.path.join(base_path, "build-linux", server['executable'])
        if not os.path.exists(exe_path):
            exe_path = os.path.join(base_path, "build", server['executable'])
        
        # 检查可执行文件是否存在
        if not os.path.exists(exe_path):
            print(f"  ✗ {server['name']}: 可执行文件未找到，跳过...")
            print(f"     路径: {exe_path}")
            continue
        
        # ORB参数现在在 /etc/omniORB.cfg 中，只添加 -v4 用于调试输出
        cmd = [exe_path, server['instance'], "-v4"]
        
        try:
            log_file_path = os.path.join(log_dir, f"{server['name'].lower()}.log")
            log_file = open(log_file_path, "w")
            
            # 在 Linux/WSL 下使用 stdbuf 禁用缓冲 + tee
            if sys.platform != 'win32':
                tee_cmd = f"stdbuf -oL -eL {' '.join(cmd)} 2>&1 | tee -a {log_file_path}"
                process = subprocess.Popen(tee_cmd, shell=True, cwd=base_path)
            else:
                process = subprocess.Popen(
                    cmd, cwd=base_path, stdout=log_file, stderr=subprocess.STDOUT
                )
            
            processes.append({
                'process': process,
                'name': server['name'],
                'device': server['device'],
                'server_name': f"{server['executable']}/{server['instance']}",
                'log_file': log_file,
                'log_file_path': log_file_path
            })
            print(f"  → {server['name']} 启动中... (PID: {process.pid})")
            print(f"     设备: {server['device']}")
            print(f"     日志: {log_file_path}")
            
        except Exception as e:
            print(f"  ✗ 启动 {server['name']} 失败: {e}")
    
    # 阶段2：等待所有服务器初始化，然后检查状态
    if processes:
        print()
        print(f"阶段2: 等待 {len(processes)} 个服务器初始化...")
        time.sleep(2)  # 统一等待所有服务器（从每个服务器3秒减少到2秒）
        
        print()
        print("服务器状态检查:")
        print("-" * 60)
        running = 0
        for item in processes:
            process = item['process']
            name = item['name']
            
            if process.poll() is not None:
                # 进程已退出
                if item.get('log_file'):
                    item['log_file'].close()
                print(f"  ✗ {name} 已退出!")
                # 显示日志中的错误
                try:
                    with open(item['log_file_path'], "r", encoding='utf-8', errors='ignore') as lf:
                        error = lf.read()[:300]
                        if error:
                            print(f"     错误信息: {error.strip()}")
                except:
                    pass
            else:
                print(f"  ✓ {name} 运行中")
                print(f"     设备: {item['device']}")
                print(f"     PID: {process.pid}")
                running += 1
        
        print("-" * 60)
        print(f"\n✓ {running}/{len(processes)} 个服务器正在运行")
        
        if running < len(processes):
            print("⚠ 部分服务器启动失败。请检查上方的错误信息。")
    
    return processes

def stop_device_servers(processes):
    """停止所有设备服务器"""
    print()
    print("=" * 60)
    print("停止设备服务器...")
    print("=" * 60)
    for item in processes:
        try:
            if isinstance(item, dict):
                process = item['process']
                name = item.get('name', 'Unknown')
                log_file = item.get('log_file')
                if log_file:
                    log_file.close()
            else:
                process = item
                name = 'Unknown'
            
            process.terminate()
            process.wait(timeout=5)
            print(f"  ✓ {name} (PID: {process.pid}) 已停止")
        except subprocess.TimeoutExpired:
            process.kill()
            print(f"  ✓ {name} (PID: {process.pid}) 已强制终止")
        except Exception as e:
            print(f"  ✗ 停止 {name} 时出错: {e}")

def main():
    if len(sys.argv) > 1 and sys.argv[1] == "stop":
        print("停止设备服务器...")
        # 停止服务器的实现将在这里
        return
    
    print()
    print("=" * 60)
    print("靶定位页面调试模式")
    print("Target Positioning Page Debug Mode")
    print("=" * 60)
    print()
    print("此脚本仅启动靶定位页面所需的服务：")
    print("  - MotionController-1 (ctrl1) - 大行程设备需要")
    print("  - MotionController-2 (ctrl2) - 六自由度设备需要")
    print("  - LargeStroke server - 大行程控制")
    print("  - SixDof server - 六自由度控制")
    print()
    
    # 启动C++服务器
    cpp_processes = start_device_servers()
    
    all_processes = cpp_processes
    if not all_processes:
        print("\n✗ 没有启动任何设备服务器!")
        return
    
    # 统计运行中的服务器数量
    running_count = 0
    for item in all_processes:
        if isinstance(item, dict):
            process = item['process']
        else:
            process = item
        if process.poll() is None:  # 仍在运行
            running_count += 1
    
    print()
    print("=" * 60)
    print(f"✓ 已启动 {running_count}/{len(all_processes)} 个设备服务器")
    print("=" * 60)
    
    if running_count < len(all_processes):
        print("⚠ 部分服务器启动失败。请检查上方的错误信息。")
        print("   查看日志文件获取详细信息：")
        for item in all_processes:
            if isinstance(item, dict):
                print(f"   - {item['name']}: {item['log_file_path']}")
    
    print()
    print("设备服务器正在运行。按 Ctrl+C 停止。")
    print()
    
    try:
        while True:
            time.sleep(1)
            # 检查是否有服务器意外退出
            for item in all_processes:
                if isinstance(item, dict):
                    process = item['process']
                    name = item.get('name', 'Unknown')
                else:
                    process = item
                    name = 'Unknown'
                
                if process.poll() is not None:
                    print(f"\n⚠ {name} 服务器意外停止!")
                    print(f"   请查看日志文件: {item.get('log_file_path', 'N/A')}")
    except KeyboardInterrupt:
        print("\n收到中断信号...")
        stop_device_servers(all_processes)
        print()
        print("所有设备服务器已停止。")

if __name__ == "__main__":
    main()

