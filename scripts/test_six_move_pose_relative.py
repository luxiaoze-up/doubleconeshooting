#!/usr/bin/env python3
"""
六自由度设备 movePoseRelative 命令测试脚本
该脚本用于测试六自由度设备的绝对姿态移动功能
"""

import sys
import time
import tango

def test_move_pose_absolute(device_name="sys/six_dof/1", target_pose=None):
    """测试 movePoseRelative 命令
    
    Args:
        device_name: 设备名称
        target_pose: 目标姿态 [x, y, z, rx, ry, rz]，如果为None则使用默认值
    """
    print(f"\n{'='*60}")
    print(f"测试六自由度设备 movePoseRelative 命令")
    print(f"设备名称: {device_name}")
    print(f"{'='*60}\n")
    
    # 默认目标姿态（可根据实际需求修改）
    if target_pose is None:
        target_pose = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]  # [x, y, z, rx, ry, rz]
    
    try:
        # 1. 连接设备
        print(f"[1/9] 正在连接设备: {device_name}...")
        device = tango.DeviceProxy(device_name)
        device.ping()
        print("✓ 设备连接成功")
        
        # 2. 检查设备状态
        print(f"\n[2/9] 检查设备状态...")
        initial_state = device.state()
        print(f"✓ 当前状态: {initial_state}")
        
        # 3. 初始化六自由度设备 (已注释：设备服务器启动时已自动初始化，无需手动init)
        # print(f"\n[3/9] 初始化六自由度设备...")
        # try:
        #     device.command_inout("init")
        #     print("✓ 六自由度设备初始化成功")
        #     time.sleep(1)
        # except Exception as e:
        #     print(f"⚠ 初始化失败: {e}")
        
        # 4. 初始化运动控制器 (已注释：six_dof的init_device已处理连接)
        print(f"\n[3/9] 检查运动控制器...")
        try:
            motion_controller_name = device.get_property("motionControllerName")["motionControllerName"][0]
            print(f"  运动控制器: {motion_controller_name}")
            
            motion_controller = tango.DeviceProxy(motion_controller_name)
            motion_controller.set_logging_level(5)
            motion_controller.ping()
            print(f"  ✓ 运动控制器连接成功")
            
            # 不需要手动调用connect，six_dof设备已在初始化时处理
            # try:
            #     motion_controller.command_inout("connect")
            #     print(f"  ✓ 运动控制器连接命令执行成功 (smc_board_init 已调用)")
            # except Exception as e:
            #     print(f"  ⚠ 连接命令失败: {e}")
            
            mc_state = motion_controller.state()
            print(f"  运动控制器状态: {mc_state}")
            
        except Exception as e:
            print(f"  ⚠ 运动控制器初始化失败: {e}")
        
        # 5. 读取编码器位置
        print(f"\n[4/9] 读取当前编码器位置...")
        try:
            encoder_values = device.command_inout("readEncoder")
            print(f"✓ 当前位置 (6轴): {[f'{val:.2f}' for val in encoder_values]}")
        except Exception as e:
            print(f"⚠ 无法读取编码器: {e}")
        
        # 6. 读取当前姿态
        print(f"\n[5/9] 读取当前姿态...")
        try:
            current_pose = device.read_attribute("sixFreedomPose").value
            print(f"✓ 当前姿态 [x, y, z, rx, ry, rz]: {[f'{p:.2f}' for p in current_pose]}")
        except Exception as e:
            print(f"⚠ 无法读取姿态: {e}")
            current_pose = None
        
        # 7. 显示目标姿态
        print(f"\n[6/9] 设置目标姿态...")
        print(f"目标姿态 [x, y, z, rx, ry, rz]: {[f'{p:.2f}' for p in target_pose]}")
        
        if current_pose is not None:
            print(f"姿态差值:")
            for i, (label) in enumerate(['X', 'Y', 'Z', 'RX', 'RY', 'RZ']):
                diff = target_pose[i] - current_pose[i]
                print(f"  {label}: {diff:+.2f}")
        
        # # 用户确认
        # print(f"\n⚠ 即将执行绝对姿态移动")
        # response = input("是否继续? (y/N): ")
        # if response.lower() != 'y':
        #     print("已取消操作")
        #     return
        
        # 7. 设置运动参数
        print(f"\n[7/9] 设置运动参数...")
        try:
            # 参数格式: [axis, startSpeed, maxSpeed, accTime, decTime, stopSpeed]
            # 速度单位为 unit/s (pulse/s)
            
            # 为6个轴分别设置参数
            for axis in range(6):
                move_params = [
                    float(axis),                          # 轴号 (0-5)
                    3000,                                 # 起始速度 (unit/s)
                    10000,                                # 最大速度 (unit/s)
                    0.1,                                  # 加速时间 (s)
                    0.1,                                  # 减速时间 (s)
                    100                                   # 停止速度 (unit/s)
                ]
                device.command_inout("moveAxisSet", move_params)
            print("✓ 运动参数设置成功 (5-50 mm/s)")
        except Exception as e:
            print(f"⚠ 运动参数设置失败: {e}")
        
        # 8. 执行 movePoseRelative
        print(f"\n[8/9] 执行 movePoseRelative 命令...")
        
        # 将 Python list 转换为 Tango DeviceData（更安全的方式）
        pose_data = tango.DeviceData()
        pose_data.insert(tango.DevVarDoubleArray, target_pose)
        
        start_time = time.time()
        device.command_inout("movePoseRelative", pose_data)
        print("✓ movePoseRelative 命令已发送")
        
        # 9. 监控运动状态
        print(f"\n[9/9] 监控运动状态...")
        timeout = 120  # 增加超时时间，因为姿态移动可能需要更长时间
        poll_interval = 0.5
        
        last_pose = None
        while time.time() - start_time < timeout:
            try:
                current_state = device.state()
                elapsed = time.time() - start_time
                
                # 读取当前姿态
                try:
                    pose = device.read_attribute("sixFreedomPose").value
                    pose_str = f"[{pose[0]:.2f}, {pose[1]:.2f}, {pose[2]:.2f}, {pose[3]:.2f}, {pose[4]:.2f}, {pose[5]:.2f}]"
                    last_pose = pose
                except:
                    pose_str = "N/A"
                
                print(f"\r  时间: {elapsed:.1f}s | 状态: {current_state} | 姿态: {pose_str}", end="", flush=True)
                
                if current_state not in [tango.DevState.MOVING, tango.DevState.RUNNING]:
                    print()
                    break
                
                time.sleep(poll_interval)
            except KeyboardInterrupt:
                print("\n⚠ 用户中断")
                break
            except Exception as e:
                print(f"\n⚠ 状态查询异常: {e}")
                break
        else:
            print(f"\n⚠ 超时")
        
        # 最终状态
        print(f"\n最终状态检查:")
        final_state = device.state()
        print(f"  设备状态: {final_state}")
        
        # 读取最终姿态
        print(f"\n最终姿态检查:")
        try:
            final_pose = device.read_attribute("sixFreedomPose").value
            print(f"  当前姿态: {[f'{p:.2f}' for p in final_pose]}")
            print(f"  目标姿态: {[f'{p:.2f}' for p in target_pose]}")
            
            print(f"\n姿态误差:")
            for i, (label) in enumerate(['X', 'Y', 'Z', 'RX', 'RY', 'RZ']):
                error = final_pose[i] - target_pose[i]
                status = "✓" if abs(error) < 0.1 else "⚠"
                print(f"  {status} {label}: {error:+.4f}")
        except Exception as e:
            print(f"  ⚠ 无法读取最终姿态: {e}")
        
        # 读取编码器最终位置
        print(f"\n最终编码器位置:")
        try:
            final_encoder = device.command_inout("readEncoder")
            print(f"  {[f'{val:.2f}' for val in final_encoder]}")
        except Exception as e:
            print(f"  ⚠ 无法读取编码器: {e}")
        
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
    
    print(f"\n使用方法: python3 {sys.argv[0]} [设备名称] [x] [y] [z] [rx] [ry] [rz]")
    print(f"示例 1: python3 {sys.argv[0]} sys/six_dof/1")
    print(f"示例 2: python3 {sys.argv[0]} sys/six_dof/1 10.0 10.0 10.0 0.0 0.0 0.0")
    print()
    
    # 解析命令行参数
    if len(sys.argv) == 1:
        # 使用默认设备和默认姿态
        device_name = default_device
        target_pose = None
    elif len(sys.argv) == 2:
        # 指定设备，使用默认姿态
        device_name = sys.argv[1]
        target_pose = None
    elif len(sys.argv) == 8:
        # 指定设备和完整姿态
        device_name = sys.argv[1]
        try:
            target_pose = [float(sys.argv[i]) for i in range(2, 8)]
        except ValueError:
            print("✗ 错误: 姿态参数必须是数字")
            sys.exit(1)
    else:
        print("✗ 错误: 参数数量不正确")
        print("请提供 0 个参数（使用默认值）、1 个参数（设备名称）或 7 个参数（设备名称 + 6个姿态值）")
        sys.exit(1)
    
    test_move_pose_absolute(device_name, target_pose)


if __name__ == "__main__":
    main()
