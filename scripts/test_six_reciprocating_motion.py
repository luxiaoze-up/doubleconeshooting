#!/usr/bin/env python3
"""
六自由度设备往复运动测试脚本
该脚本在零位和目标位置之间进行往复运动
"""

import sys
import time
import tango

def wait_for_motion_complete(device, timeout=120, poll_interval=0.5):
    """等待运动完成
    
    Args:
        device: Tango设备代理
        timeout: 超时时间（秒）
        poll_interval: 轮询间隔（秒）
        
    Returns:
        bool: 是否成功完成运动
    """
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        try:
            current_state = device.state()
            elapsed = time.time() - start_time
            
            # 读取当前姿态
            try:
                pose = device.read_attribute("sixFreedomPose").value
                pose_str = f"[{pose[0]:.2f}, {pose[1]:.2f}, {pose[2]:.2f}, {pose[3]:.2f}, {pose[4]:.2f}, {pose[5]:.2f}]"
            except:
                pose_str = "N/A"
            
            print(f"\r  时间: {elapsed:.1f}s | 状态: {current_state} | 姿态: {pose_str}", end="", flush=True)
            
            if current_state not in [tango.DevState.MOVING, tango.DevState.RUNNING]:
                print()
                return True
            
            time.sleep(poll_interval)
        except KeyboardInterrupt:
            print("\n⚠ 用户中断")
            return False
        except Exception as e:
            print(f"\n⚠ 状态查询异常: {e}")
            return False
    
    print(f"\n⚠ 超时")
    return False


def move_to_pose(device, target_pose, pose_name="目标"):
    """移动到指定姿态
    
    Args:
        device: Tango设备代理
        target_pose: 目标姿态 [x, y, z, rx, ry, rz]
        pose_name: 位置名称（用于显示）
        
    Returns:
        bool: 是否成功移动
    """
    print(f"\n移动到 {pose_name}: {[f'{p:.2f}' for p in target_pose]}")
    
    # 将 Python list 转换为 Tango DeviceData
    pose_data = tango.DeviceData()
    pose_data.insert(tango.DevVarDoubleArray, target_pose)
    
    try:
        device.command_inout("movePoseRelative", pose_data)
        print("✓ movePoseRelative 命令已发送")
        
        # 等待运动完成
        success = wait_for_motion_complete(device)
        
        if success:
            # 读取最终姿态确认
            try:
                final_pose = device.read_attribute("sixFreedomPose").value
                print(f"✓ 到达位置: {[f'{p:.2f}' for p in final_pose]}")
                
                # 计算误差
                errors = [abs(final_pose[i] - target_pose[i]) for i in range(6)]
                max_error = max(errors)
                if max_error > 0.5:
                    print(f"⚠ 最大误差: {max_error:.4f}")
                    
            except Exception as e:
                print(f"⚠ 无法读取最终姿态: {e}")
        
        return success
        
    except tango.DevFailed as e:
        print(f"\n✗ 移动命令失败:")
        for err in e.args:
            print(f"  - {err.reason}: {err.desc}")
        return False
    except Exception as e:
        print(f"\n✗ 错误: {e}")
        return False


def test_reciprocating_motion(device_name="sys/six_dof/1", target_pose=None, cycles=9, wait_time=3.0):
    """测试往复运动
    
    Args:
        device_name: 设备名称
        target_pose: 目标姿态 [x, y, z, rx, ry, rz]
        cycles: 往复次数
        wait_time: 每次到达后的等待时间（秒）
    """
    print(f"\n{'='*60}")
    print(f"六自由度设备往复运动测试")
    print(f"设备名称: {device_name}")
    print(f"往复次数: {cycles}")
    print(f"等待时间: {wait_time}秒")
    print(f"{'='*60}\n")
    
    # 零位姿态
    zero_pose = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
    
    # 默认目标姿态
    if target_pose is None:
        target_pose = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
    
    try:
        # 1. 连接设备
        print(f"[1/4] 正在连接设备: {device_name}...")
        device = tango.DeviceProxy(device_name)
        device.ping()
        print("✓ 设备连接成功")
        
        # 2. 检查设备状态
        print(f"\n[2/4] 检查设备状态...")
        initial_state = device.state()
        print(f"✓ 当前状态: {initial_state}")
        
        # 3. 读取当前姿态
        print(f"\n[3/4] 读取当前姿态...")
        try:
            current_pose = device.read_attribute("sixFreedomPose").value
            print(f"✓ 当前姿态 [x, y, z, rx, ry, rz]: {[f'{p:.2f}' for p in current_pose]}")
        except Exception as e:
            print(f"⚠ 无法读取姿态: {e}")
            current_pose = None
        
        # 4. 设置运动参数
        print(f"\n[4/4] 设置运动参数...")
        try:
            # 为6个轴分别设置参数
            for axis in range(6):
                move_params = [
                    float(axis),      # 轴号 (0-5)
                    3000,             # 起始速度 (unit/s)
                    10000,            # 最大速度 (unit/s)
                    0.1,              # 加速时间 (s)
                    0.1,              # 减速时间 (s)
                    100               # 停止速度 (unit/s)
                ]
                device.command_inout("moveAxisSet", move_params)
            print("✓ 运动参数设置成功")
        except Exception as e:
            print(f"⚠ 运动参数设置失败: {e}")
        
        # 显示运动计划
        print(f"\n{'='*60}")
        print("运动计划:")
        print(f"  零位: {[f'{p:.2f}' for p in zero_pose]}")
        print(f"  目标: {[f'{p:.2f}' for p in target_pose]}")
        print(f"  往复次数: {cycles}")
        print(f"  每次等待: {wait_time}秒")
        print(f"{'='*60}\n")
        
        # 用户确认
        response = input("是否开始往复运动? (y/N): ")
        if response.lower() != 'y':
            print("已取消操作")
            return
        
        # 开始往复运动
        print(f"\n{'='*60}")
        print("开始往复运动")
        print(f"{'='*60}")
        
        overall_start = time.time()
        
        for cycle in range(cycles):
            print(f"\n{'='*60}")
            print(f"第 {cycle + 1}/{cycles} 次往复")
            print(f"{'='*60}")
            
            # 移动到目标位置
            print(f"\n[{cycle + 1}.1] 移动到目标位置...")
            if not move_to_pose(device, target_pose, "目标位置"):
                print("✗ 移动失败，停止往复运动")
                break
            
            print(f"\n等待 {wait_time} 秒...")
            time.sleep(wait_time)
            
            # 移动回零位
            print(f"\n[{cycle + 1}.2] 返回零位...")
            if not move_to_pose(device, zero_pose, "零位"):
                print("✗ 移动失败，停止往复运动")
                break
            
            print(f"\n等待 {wait_time} 秒...")
            time.sleep(wait_time)
            
            print(f"\n✓ 第 {cycle + 1} 次往复完成")
        
        overall_time = time.time() - overall_start
        
        # 最终状态
        print(f"\n{'='*60}")
        print("往复运动完成")
        print(f"{'='*60}")
        print(f"总耗时: {overall_time:.1f} 秒")
        
        print(f"\n最终状态检查:")
        final_state = device.state()
        print(f"  设备状态: {final_state}")
        
        # 读取最终姿态
        try:
            final_pose = device.read_attribute("sixFreedomPose").value
            print(f"  最终姿态: {[f'{p:.2f}' for p in final_pose]}")
        except Exception as e:
            print(f"  ⚠ 无法读取最终姿态: {e}")
        
        print(f"\n{'='*60}")
        print("测试完成!")
        print(f"{'='*60}\n")
        
    except tango.DevFailed as e:
        print(f"\n✗ Tango 错误:")
        for err in e.args:
            print(f"  - {err.reason}: {err.desc}")
        sys.exit(1)
    except Exception as e:
        print(f"\n✗ 错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


def main():
    """主函数"""
    default_device = "sys/six_dof/1"
    
    print(f"\n使用方法: python3 {sys.argv[0]} [设备名称] [x] [y] [z] [rx] [ry] [rz] [往复次数] [等待时间]")
    print(f"示例 1: python3 {sys.argv[0]}")
    print(f"示例 2: python3 {sys.argv[0]} sys/six_dof/1 10.0 10.0 10.0 0.0 0.0 0.0")
    print(f"示例 3: python3 {sys.argv[0]} sys/six_dof/1 10.0 10.0 10.0 0.0 0.0 0.0 10 5.0")
    print()
    
    # 解析命令行参数
    device_name = default_device
    target_pose = None
    cycles = 5
    wait_time = 3.0
    
    if len(sys.argv) >= 2:
        device_name = sys.argv[1]
    
    if len(sys.argv) >= 8:
        try:
            target_pose = [float(sys.argv[i]) for i in range(2, 8)]
        except ValueError:
            print("✗ 错误: 姿态参数必须是数字")
            sys.exit(1)
    
    if len(sys.argv) >= 9:
        try:
            cycles = int(sys.argv[8])
            if cycles <= 0:
                print("✗ 错误: 往复次数必须大于0")
                sys.exit(1)
        except ValueError:
            print("✗ 错误: 往复次数必须是整数")
            sys.exit(1)
    
    if len(sys.argv) >= 10:
        try:
            wait_time = float(sys.argv[9])
            if wait_time < 0:
                print("✗ 错误: 等待时间不能为负数")
                sys.exit(1)
        except ValueError:
            print("✗ 错误: 等待时间必须是数字")
            sys.exit(1)
    
    test_reciprocating_motion(device_name, target_pose, cycles, wait_time)


if __name__ == "__main__":
    main()
