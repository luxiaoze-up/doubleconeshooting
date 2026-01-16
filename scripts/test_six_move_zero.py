#!/usr/bin/env python3
"""
README.md REMOVE_AUXILIARY_DEVICES.md __pycache__ backup_project.py check_auxiliary_server.py check_device_status.py check_mode.py check_reflection_config.py cleanup_auxiliary_devices.py diagnose_power_control.py find_auxiliary_server_config.sh find_server_config.sh fix_auxiliary_server_devices.py fix_wsl_hosts.sh image_stream_api.py list_server_devices.py register_devices.py requirements_api.txt run_opcua_plc_simulator.ps1 run_tests.bat run_tests.sh run_tests_with_reports.sh setup_omniORB.sh start_image_api.bat start_image_api.sh start_servers.py start_servers_target_positioning.py start_vacuum_system.sh test_auto_power_on.py test_encoder_collector_read.py test_force_sensor_read.py test_io_output.py test_server_info.py test_smc_capabilities test_smc_capabilities.cpp test_smc_connection.cpp test_smc_connection.sh test_smc_io_detailed test_smc_io_detailed.cpp tools view_vacuum_logs.sh  update_auxiliary_server.py/app/scripts/test_six_move_zero.py sixMoveZero 命令
README.md REMOVE_AUXILIARY_DEVICES.md __pycache__ backup_project.py check_auxiliary_server.py check_device_status.py check_mode.py check_reflection_config.py cleanup_auxiliary_devices.py diagnose_power_control.py find_auxiliary_server_config.sh find_server_config.sh fix_auxiliary_server_devices.py fix_wsl_hosts.sh image_stream_api.py list_server_devices.py register_devices.py requirements_api.txt run_opcua_plc_simulator.ps1 run_tests.bat run_tests.sh run_tests_with_reports.sh setup_omniORB.sh start_image_api.bat start_image_api.sh start_servers.py start_servers_target_positioning.py start_vacuum_system.sh test_auto_power_on.py test_encoder_collector_read.py test_force_sensor_read.py test_io_output.py test_server_info.py test_smc_capabilities test_smc_capabilities.cpp test_smc_connection.cpp test_smc_connection.sh test_smc_io_detailed test_smc_io_detailed.cpp tools update_auxiliary_server.py view_vacuum_logs.sh 
"""

import sys
import time
import tango

def test_six_move_zero(device_name="sys/six_dof/1"):
    """测试 sixMoveZero 命令"""
    print(f"\n{'='*60}")
    print(f"测试六自由度设备 sixMoveZero 命令")
    print(f"设备名称: {device_name}")
    print(f"{'='*60}\n")
    
    try:
        # 1. 连接设备
        print(f"[1/8] 正在连接设备: {device_name}...")
        device = tango.DeviceProxy(device_name)
        device.ping()
        print("✓ 设备连接成功")
        
        # 2. 检查设备状态
        print(f"\n[2/8] 检/app/scripts/test_six_move_zero.py...")
        initial_state = device.state()
        print(f"✓ 当前状态: {initial_state}")
        
        # 3. 初始化六自由度设备 (已注释：设备服务器启动时已自动初始化，无需手动init)
        # print("f\n[3/8]/app/scripts/test_six_move_zero.py...")
        # try:
        #     device.command_inout("init")
        #     print("✓ 六自由度设备初始化成功")
        #     time.sleep(1)
        # except Exception as e:
        #     print(f"⚠ 初始化失败: {e}")
        
        # 4. 初始化运动控制器 (已注释：six_dof的init_device已处理连接)
        print(f"\n[3/<<...")
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
            #     print("<< (smc_board_init 已调用)")
            # except Exception as e:
            #     print(f"  ⚠ 连接命令失败: {e}")
            
            mc_state = motion_controller.state()
            print(f"  运动控制器状态: {mc_state}")
            
        except Exception as e:
            print(f"  ⚠ 运动控制器初始化失败: {e}")
        
        # 5. 读取编码器位置
        print(f"\n[4/8] 读取当前编码器位置...")
        try:
            encoder_values = device.command_inout("readEncoder")
            print(f"✓ 当前位置 (6轴): {[f'{val:.2f}' for val in encoder_values]}")
        except Exception as e:
            print(f"⚠ 无法读取编码器: {e}")
        
        # 6. 读取姿态
        print(f"\n[5/8] 读取当前姿态...")
        try:
            pose = device.read_attribute("sixFreedomPose").value
            print(f"✓ 当前姿态: {[f'{p:.2f}' for p in pose]}")
        except Exception as e:
            print(f"⚠ 无法读取姿: {e}")
        
        # 7. 执行 sixMoveZero
        print(f"\n[6/8] 执行 sixMoveZero 命令...")
        print("提示: 该命令会将所有 6 个轴回零")
        
        start_time = time.time()
        device.command_inout("sixMoveZero")
        print("✓ sixMoveZero 命令已发送")
        
        # 8. 监控运动状态
        print(f"\n[7/8] 监控运动状态...")
        timeout = 60
        poll_interval = 0.5
        
        while time.time() - start_time < timeout:
            try:
                current_state = device.state()
                elapsed = time.time() - start_time
                print(f"\r  时间: {elapsed:.1f}s | 状态: {current_state}", end="", flush=True)
                
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
        print(f"  设备状态: {device.state()}")
        
        print(f"\n原点状态检查:")
        for axis in range(6):
            try:
                is_at_org = device.command_inout("readOrg", axis)
                status = "✓ 在原点" if is_at_org else "✗ 不在原点"
                print(f"  轴 {axis}: {status}")
            except Exception as e:
                print(f"  轴 {axis}: ⚠ 无法读取")
        
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
    default_device = "sys/six_dof/1"
    device_name = sys.argv[1] if len(sys.argv) > 1 else default_device
    
    print(f"\n使用方法: python3 {sys.argv[0]} [设备名称]")
    print(f"示例: python3 {sys.argv[0]} sys/six_dof/1")
    
    test_six_move_zero(device_name)


if __name__ == "__main__":
    main()
