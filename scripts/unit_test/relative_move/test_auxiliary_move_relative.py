#!/usr/bin/env python3
"""
辅助支撑设备 MoveRelative 命令测试脚本
该脚本用于测试辅助支撑设备的相对移动功能
支持5个辅助支撑设备：sys/auxiliary/1 到 sys/auxiliary/5
"""

import sys
import time
import math
import os

# 设置环境变量
os.environ['TANGO_HOST'] = '192.168.1.177:10000'

# 获取项目根目录
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(project_root, 'gui'))

import tango

def test_auxiliary_move_relative(device_name="sys/auxiliary/1", distance=None):
    """测试 MoveRelative 命令
    
    Args:
        device_name: 设备名称 (sys/auxiliary/1 到 sys/auxiliary/5)
        distance: 相对移动距离 (mm)，如果为None则使用默认值
    """
    print(f"\n{'='*60}")
    print(f"测试辅助支撑设备 MoveRelative 命令")
    print(f"设备名称: {device_name}")
    print(f"{'='*60}\n")
    
    # 默认移动距离（可根据实际需求修改）
    if distance is None:
        distance = 10.0  # 默认 10mm（辅助支撑行程较小，使用较小的测试距离）
    
    try:
        # 1. 连接设备
        print(f"[1/8] 正在连接设备: {device_name}...")
        device = tango.DeviceProxy(device_name)
        device.ping()
        print("✓ 设备连接成功")
        
        # 2. 检查设备状态
        print(f"\n[2/8] 检查设备状态...")
        initial_state = device.state()
        print(f"✓ 当前状态: {initial_state}")
        
        # 3. 检查运动控制器
        print(f"\n[3/8] 检查运动控制器...")
        try:
            motion_controller_name = device.get_property("motionControllerName")["motionControllerName"][0]
            axis_id = device.get_property("axisId")["axisId"][0]
            print(f"  运动控制器: {motion_controller_name}")
            print(f"  轴号: {axis_id}")
            
            motion_controller = tango.DeviceProxy(motion_controller_name)
            motion_controller.ping()
            mc_state = motion_controller.state()
            print(f"  ✓ 运动控制器连接成功, 状态: {mc_state}")
            
        except Exception as e:
            print(f"  ⚠ 运动控制器连接失败: {e}")
        
        # 4. 读取初始编码器位置和原始圈数
        print(f"\n[4/8] 读取当前编码器位置...")
        try:
            initial_position = device.read_attribute("encoderPosition").value
            print(f"✓ 初始位置: {initial_position:.3f} mm")
        except Exception as e:
            print(f"⚠ 无法读取编码器位置: {e}")
            initial_position = None
        
        # 读取编码器原始圈数（用于后续验证）
        try:
            initial_encoder = device.command_inout("readEncoder")
            print(f"✓ 初始编码器圈数: {initial_encoder:.6f} 圈")
        except Exception as e:
            print(f"⚠ 无法读取编码器圈数: {e}")
            initial_encoder = None
        
        # 5. 显示目标移动距离
        print(f"\n[5/8] 设置移动参数...")
        print(f"相对移动距离: {distance:+.3f} mm")
        
        if initial_position is not None:
            target_position = initial_position + distance
            print(f"预期最终位置: {target_position:.3f} mm")
        
        # 用户确认
        print(f"\n⚠ 即将执行相对移动")
        response = input("是否继续? (y/N): ")
        if response.lower() != 'y':
            print("已取消操作")
            return
        
        # 6. 设置运动参数（如果设备支持）
        print(f"\n[6/8] 设置运动参数...")
        try:
            # 辅助支撑设备的运动参数
            # 参数格式: [axis, startSpeed, maxSpeed, accTime, decTime, stopSpeed]
            move_params = [
                int(axis_id),   # 轴号
                1000.0,         # 起始速度 (pulse/s)
                5000.0,         # 最大速度 (pulse/s)
                0.2,            # 加速时间 (s)
                0.2,            # 减速时间 (s)
                500.0           # 停止速度 (pulse/s)
            ]
            device.command_inout("moveAxisSet", move_params)
            print("✓ 运动参数设置成功")
        except Exception as e:
            print(f"⚠ 运动参数设置失败（可能设备不支持）: {e}")
        
        # 7. 执行 MoveRelative
        print(f"\n[7/8] 执行 MoveRelative 命令...")
        
        start_time = time.time()
        device.command_inout("MoveRelative", distance)
        print("✓ MoveRelative 命令已发送")
        
        # 8. 监控运动状态
        print(f"\n[8/8] 监控运动状态...")
        timeout = 60  # 超时时间 (秒)
        poll_interval = 0.5
        
        last_position = initial_position
        while time.time() - start_time < timeout:
            try:
                current_state = device.state()
                elapsed = time.time() - start_time
                
                # 读取当前位置
                try:
                    position = device.read_attribute("encoderPosition").value
                    position_str = f"{position:.3f} mm"
                    last_position = position
                    
                    # 计算已移动距离
                    if initial_position is not None:
                        moved = position - initial_position
                        position_str += f" (已移动: {moved:+.3f} mm)"
                except:
                    position_str = "N/A"
                
                print(f"\r  时间: {elapsed:.1f}s | 状态: {current_state} | 位置: {position_str}", end="", flush=True)
                
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
        
        # 读取最终位置
        print(f"\n最终位置检查:")
        try:
            final_position = device.read_attribute("encoderPosition").value
            print(f"  初始位置: {initial_position:.3f} mm")
            print(f"  最终位置: {final_position:.3f} mm")
            print(f"  目标移动: {distance:+.3f} mm")
            
            if initial_position is not None:
                actual_moved = final_position - initial_position
                error = actual_moved - distance
                
                print(f"\n移动结果:")
                print(f"  实际移动: {actual_moved:+.3f} mm")
                print(f"  误差: {error:+.4f} mm")
                
                if abs(error) < 0.1:
                    print(f"  ✓ 误差在可接受范围内 (< 0.1mm)")
                else:
                    print(f"  ⚠ 误差较大 (≥ 0.1mm)")
        except Exception as e:
            print(f"  ⚠ 无法读取最终位置: {e}")
        
        # 编码器圈数差值验证
        print(f"\n编码器圈数差值验证:")
        try:
            final_encoder = device.command_inout("readEncoder")
            print(f"  初始编码器圈数: {initial_encoder:.6f} 圈")
            print(f"  最终编码器圈数: {final_encoder:.6f} 圈")
            
            if initial_encoder is not None:
                encoder_diff = final_encoder - initial_encoder
                # 编码器差值转换公式: 圈数 × 3/3.6 = mm
                encoder_moved_mm = encoder_diff * (3.0 / 3.6)
                
                print(f"  编码器圈数差值: {encoder_diff:+.6f} 圈")
                print(f"  转换公式: 圈数 × 3/3.6 (= {3.0/3.6:.6f} mm/圈)")
                print(f"  编码器换算距离: {encoder_moved_mm:+.4f} mm")
                print(f"  目标移动距离: {distance:+.3f} mm")
                
                # 计算误差
                encoder_error = encoder_moved_mm - distance
                print(f"\n编码器验证结果:")
                print(f"  编码器换算: {encoder_moved_mm:+.4f} mm")
                print(f"  目标移动: {distance:+.3f} mm")
                print(f"  偏差: {encoder_error:+.4f} mm")
                
                if abs(encoder_error) < 0.1:
                    print(f"  ✓ 编码器数据验证通过 (< 0.1mm)")
                else:
                    print(f"  ⚠ 编码器数据偏差较大 (≥ 0.1mm)")
        except Exception as e:
            print(f"  ⚠ 无法读取编码器圈数: {e}")
        
        # 读取力传感器数据（如果有）
        try:
            force = device.read_attribute("forceValue").value
            print(f"\n力传感器读数: {force:.2f} N")
        except:
            pass
        
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
    default_device = "sys/auxiliary/1"
    
    print(f"\n使用方法: python3 {sys.argv[0]} [设备名称] [距离]")
    print(f"")
    print(f"可用设备:")
    print(f"  sys/auxiliary/1 - 辅助支撑设备1 (AXIS-0)")
    print(f"  sys/auxiliary/2 - 辅助支撑设备2 (AXIS-1)")
    print(f"  sys/auxiliary/3 - 辅助支撑设备3 (AXIS-2)")
    print(f"  sys/auxiliary/4 - 辅助支撑设备4 (AXIS-3)")
    print(f"  sys/auxiliary/5 - 辅助支撑设备5 (AXIS-4)")
    print(f"")
    print(f"示例:")
    print(f"  python3 {sys.argv[0]}                    # 使用默认设备和默认距离 10mm")
    print(f"  python3 {sys.argv[0]} sys/auxiliary/1    # 指定设备1，使用默认距离")
    print(f"  python3 {sys.argv[0]} sys/auxiliary/2 5.0  # 设备2移动 5mm")
    print(f"  python3 {sys.argv[0]} sys/auxiliary/3 -3.0 # 设备3反向移动 3mm")
    print()
    
    # 解析命令行参数
    if len(sys.argv) == 1:
        # 使用默认设备和默认距离
        device_name = default_device
        distance = None
    elif len(sys.argv) == 2:
        # 指定设备，使用默认距离
        device_name = sys.argv[1]
        distance = None
    elif len(sys.argv) == 3:
        # 指定设备和距离
        device_name = sys.argv[1]
        try:
            distance = float(sys.argv[2])
        except ValueError:
            print("✗ 错误: 距离参数必须是数字")
            sys.exit(1)
    else:
        print("✗ 错误: 参数数量不正确")
        print("请提供 0-2 个参数")
        sys.exit(1)
    
    test_auxiliary_move_relative(device_name, distance)


if __name__ == "__main__":
    main()
