#!/usr/bin/env python3
"""
Device server startup configuration for Double Cone Shooting Control System
"""

import os
import subprocess
import time
import sys
import signal
import atexit
import threading

# 全局变量用于存储进程列表，供信号处理和 atexit 使用
_all_processes = []
_output_threads = []  # 输出处理线程

# Device server configurations
# 三台运动控制器：ctrl1(192.168.1.11), ctrl2(192.168.1.12), ctrl3(192.168.1.13)
DEVICE_SERVERS = [
    # ========== 运动控制器 (3台) ==========
    # {
    #     'name': 'MotionController-1',
    #     'executable': 'motion_controller_server',
    #     'instance': 'ctrl1',
    #     'device': 'sys/motion/1'
    # },
    {
        'name': 'MotionController-2',
        'executable': 'motion_controller_server',
        'instance': 'ctrl2',
        'device': 'sys/motion/2'
    },
    {
        'name': 'MotionController-3',
        'executable': 'motion_controller_server',
        'instance': 'ctrl3',
        'device': 'sys/motion/3'
    },
    # ========== 编码器 ==========
    {
        'name': 'Encoder',
        'executable': 'encoder_server',
        'instance': 'main',
        'device': 'sys/encoder/1'
    },
    # ========== 大行程 (控制器1) ==========
    {
        'name': 'LargeStroke',
        'executable': 'large_stroke_server',
        'instance': 'large_stroke',
        'device': 'sys/large_stroke/1'
    },
    # ========== 辅助支撑 (控制器1) ==========
    {
        'name': 'AuxiliarySupport',
        'executable': 'auxiliary_support_server',
        'instance': 'auxiliary',
        'device': 'sys/auxiliary/1'
    },
    # ========== 六自由度 (控制器2) ==========
    {
        'name': 'SixDof',
        'executable': 'six_dof_server',
        'instance': 'six_dof',
        'device': 'sys/six_dof/1'
    },
    # ========== 反射光成像表征 (控制器3) ==========
    {
        'name': 'ReflectionImaging',
        'executable': 'reflection_imaging_server',
        'instance': 'reflection',
        'device': 'sys/reflection/1'
    },
    # ========== 真空系统 ==========
    # {
    #     'name': 'Vacuum',
    #     'executable': 'vacuum_server',
    #     'instance': 'vacuum',
    #     'device': 'sys/vacuum/1'
    # },
    # {
    #     'name': 'VacuumSystem',
    #     'executable': 'vacuum_system_server',
    #     'instance': 'vacuum2',
    #     'device': 'sys/vacuum/2'
    # },
    # ========== 联锁服务 ==========
    {
        'name': 'Interlock',
        'executable': 'interlock_server',
        'instance': 'interlock',
        'device': 'sys/interlock/1'
    }
]

def output_handler(pipe, log_file, name, verbose):
    """
    线程函数：读取进程输出，写入日志文件，可选输出到终端
    """
    try:
        for line in iter(pipe.readline, b''):
            if not line:
                break
            try:
                decoded = line.decode('utf-8', errors='replace')
            except:
                decoded = str(line)
            # 写入日志文件
            log_file.write(decoded)
            log_file.flush()
            # 如果是 verbose 模式，也输出到终端
            if verbose:
                sys.stdout.write(f"[{name}] {decoded}")
                sys.stdout.flush()
    except Exception as e:
        pass  # 进程退出时管道关闭，忽略错误
    finally:
        try:
            pipe.close()
        except:
            pass


def start_device_servers():
    """Start all C++ device servers in sequential order"""
    global _output_threads
    processes = []
    
    print("Starting C++ device servers...")
    
    # Check if we are in scripts dir
    base_path = ".." if os.path.basename(os.getcwd()) == "scripts" else "."
    
    # Create log directory
    log_dir = os.path.join(base_path, "logs")
    os.makedirs(log_dir, exist_ok=True)
    
    # 定义启动顺序：motion controller -> encoder -> 大行程 -> 六自由度 -> 辅助支撑 -> 反射光成像 -> 联锁服务
    startup_order = [
        'MotionController-1',
        'MotionController-2',
        'MotionController-3',
        'Encoder',
        'LargeStroke',
        'SixDof',
        'AuxiliarySupport',
        'ReflectionImaging',
        'Interlock'
    ]
    
    # 创建名称到服务器配置的映射
    server_map = {server['name']: server for server in DEVICE_SERVERS}
    
    # 按顺序启动服务器
    for server_name in startup_order:
        if server_name not in server_map:
            continue
        
        server = server_map[server_name]
        
        # Try build-linux first (WSL/Linux), then build (Windows)
        exe_path = os.path.join(base_path, "build", server['executable'])
        if not os.path.exists(exe_path):
            exe_path = os.path.join(base_path, "build", server['executable'])
        
        # Check if executable exists
        if not os.path.exists(exe_path):
            print(f"  ✗ {server['name']}: executable not found, skipping...")
            continue
        
        # ORB parameters are now in /etc/omniORB.cfg
        # motion_controller_server 使用 -v4 (DEBUG)，其他服务使用 -v1 (ERROR only) 减少日志噪音
        if server['executable'] == 'motion_controller_server':
            verbose_level = "-v4"
            verbose_output = True  # motion_controller 输出到终端
        else:
            verbose_level = "-v1"  # 只显示错误
            verbose_output = False  # 其他服务只写日志
        cmd = [exe_path, server['instance'], verbose_level]
        
        try:
            log_file_path = os.path.join(log_dir, f"{server['name'].lower()}.log")
            log_file = open(log_file_path, "w")
            
            # 直接启动进程，不使用 shell=True
            # 使用 PIPE 捕获输出，通过线程处理
            process = subprocess.Popen(
                cmd, cwd=base_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0  # 无缓冲
            )
            
            # 创建输出处理线程
            output_thread = threading.Thread(
                target=output_handler,
                args=(process.stdout, log_file, server['name'], verbose_output),
                daemon=True
            )
            output_thread.start()
            _output_threads.append(output_thread)
            
            processes.append({
                'process': process,
                'name': server['name'],
                'device': server['device'],
                'server_name': f"{server['executable']}/{server['instance']}",
                'log_file': log_file,
                'log_file_path': log_file_path
            })
            print(f"  → {server['name']} starting... (PID: {process.pid})")
            
            # 每个服务启动后等待2秒
            time.sleep(1)
            
        except Exception as e:
            print(f"  ✗ Failed to start {server['name']}: {e}")
    
    # 检查所有服务器状态
    if processes:
        print("\nServer status:")
        running = 0
        for item in processes:
            process = item['process']
            name = item['name']
            
            if process.poll() is not None:
                # Process has exited
                if item.get('log_file'):
                    item['log_file'].close()
                print(f"  ✗ {name} exited!")
                # Show error from log
                try:
                    with open(item['log_file_path'], "r") as lf:
                        error = lf.read()[:300]
                        if error:
                            print(f"    Error: {error.strip()}")
                except:
                    pass
            else:
                print(f"  ✓ {name} running (Device: {item['device']})")
                running += 1
        
        print(f"\n✓ {running}/{len(processes)} servers running")
    
    return processes

# def start_python_servers():
#     """Start Python device servers as backup"""
#     python_servers = [
#         {
#             'name': 'SixDof (Python)',
#             'file': '../backup/python_implementation/src/device_services/six_dof_device.py',
#             'instance': 'six_dof'
#         }
#     ]
    
#     processes = []
    
#     for server in python_servers:
#         if os.path.exists(server['file']):
#             print(f"Starting {server['name']} device server...")
#             cmd = ['python3', server['file'], server['instance']]
            
#             try:
#                 process = subprocess.Popen(cmd)
#                 processes.append(process)
#                 time.sleep(2)
#                 print(f"{server['name']} started successfully (PID: {process.pid})")
#             except Exception as e:
#                 print(f"Failed to start {server['name']}: {e}")
    
#     return processes

def cleanup_on_exit():
    """atexit handler to ensure all processes are terminated"""
    global _all_processes
    if _all_processes:
        stop_device_servers(_all_processes)
        _all_processes = []

def signal_handler(signum, frame):
    """Handle SIGTERM and SIGINT signals"""
    sig_name = signal.Signals(signum).name if hasattr(signal, 'Signals') else str(signum)
    print(f"\nReceived signal {sig_name}...")
    global _all_processes
    if _all_processes:
        stop_device_servers(_all_processes)
        _all_processes = []
    sys.exit(0)

def stop_device_servers(processes):
    """Stop all device servers"""
    print("Stopping device servers...")
    for item in processes:
        try:
            if isinstance(item, dict):
                process = item['process']
                name = item.get('name', 'Unknown')
                log_file = item.get('log_file')
                if log_file:
                    try:
                        log_file.close()
                    except:
                        pass
            else:
                process = item
                name = 'Unknown'
            
            # 检查进程是否还在运行
            if process.poll() is not None:
                print(f"  ✓ {name} already exited")
                continue
            
            # 直接终止进程（不再需要处理进程组）
            try:
                process.terminate()
                process.wait(timeout=5)
                print(f"  ✓ {name} (PID: {process.pid}) stopped")
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)
                print(f"  ✓ {name} (PID: {process.pid}) killed")
        except ProcessLookupError:
            print(f"  ✓ {name} already exited")
        except Exception as e:
            print(f"  ✗ Error stopping {name}: {e}")

def main():
    global _all_processes
    
    if len(sys.argv) > 1 and sys.argv[1] == "stop":
        print("Stopping device servers...")
        # Implementation for stopping servers would go here
        return
    
    # 注册信号处理器（捕获 SIGTERM 和 SIGINT）
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    
    # 注册 atexit 处理器作为最后的清理保障
    atexit.register(cleanup_on_exit)
    
    print("=== Double Cone Shooting Control System ===")
    print("Starting device servers...")
    
    # Start C++ servers
    cpp_processes = start_device_servers()
    
    # # Start Python servers if needed
    # python_processes = start_python_servers()
    
    # all_processes = cpp_processes + python_processes
    
    all_processes = cpp_processes
    _all_processes = all_processes  # 保存到全局变量供信号处理器使用
    
    if not all_processes:
        print("\n✗ No device servers started!")
        return
    
    # Count running servers
    running_count = 0
    for item in all_processes:
        if isinstance(item, dict):
            process = item['process']
        else:
            process = item
        if process.poll() is None:  # Still running
            running_count += 1
    
    print(f"\n✓ Started {running_count}/{len(all_processes)} device server(s)")
    
    if running_count < len(all_processes):
        print("⚠ Some servers failed to start. Check the output above for errors.")
    
    print("\nDevice servers running. Press Ctrl+C to stop.")
    
    try:
        while True:
            time.sleep(1)
            # Check if any server has died
            for item in all_processes:
                if isinstance(item, dict):
                    process = item['process']
                    name = item.get('name', 'Unknown')
                else:
                    process = item
                    name = 'Unknown'
                
                if process.poll() is not None:
                    print(f"\n⚠ {name} server has stopped unexpectedly!")
    except KeyboardInterrupt:
        print("\nReceived Ctrl+C...")
        stop_device_servers(all_processes)
        _all_processes = []  # 清空，防止 atexit 重复清理
        print("All device servers stopped.")

if __name__ == "__main__":
    main()
