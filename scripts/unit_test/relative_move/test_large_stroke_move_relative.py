#!/usr/bin/env python3
"""
大行程平台 MoveRelative 命令测试脚本
该脚本用于测试大行程平台的 X 轴相对移动功能
"""

import sys
import time
import math
import tango

def test_x_move_relative(device_name="sys/large_stroke/1", distance=None):
    """测试 MoveRelative 命令
    
    Args:
        device_name: 设备名称
        distance: 相对移动距离 (mm)，如果为None则使用默认值
    """
    print(f"\n{'='*60}")
    print(f"测试大行程平台 MoveRelative 命令")
    print(f"设备名称: {device_name}")
    print(f"{'='*60}\n")
    
    # 默认移动距离（可根据实际需求修改）
    if distance is None:
        distance = 50.0  # 默认 50mm
    
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
        
        # 如果设备处于 FAULT 状态，尝试复位
        if initial_state == tango.DevState.FAULT:
            print(f"  ⚠ 设备处于 FAULT 状态，尝试复位...")
            try:
                # 读取报警信息
                alarm_state = device.read_attribute("alarmState").value
                if alarm_state:
                    print(f"  报警信息: {alarm_state}")
                
                # 执行复位
                device.command_inout("reset")
                time.sleep(1)
                
                # 检查复位后的状态
                new_state = device.state()
                print(f"  复位后状态: {new_state}")
                
                if new_state == tango.DevState.FAULT:
                    print(f"  ✗ 复位失败，设备仍处于 FAULT 状态")
                    return
                else:
                    print(f"  ✓ 复位成功")
                    initial_state = new_state
            except Exception as e:
                print(f"  ✗ 复位失败: {e}")
                return
        
        # 3. 检查运动控制器
        print(f"\n[3/9] 检查运动控制器...")
        try:
            motion_controller_name = device.get_property("motionControllerName")["motionControllerName"][0]
            print(f"  运动控制器: {motion_controller_name}")
            
            motion_controller = tango.DeviceProxy(motion_controller_name)
            motion_controller.ping()
            mc_state = motion_controller.state()
            print(f"  ✓ 运动控制器连接成功, 状态: {mc_state}")
            
        except Exception as e:
            print(f"  ⚠ 运动控制器连接失败: {e}")
        
        # 4. 读取初始编码器位置
        print(f"\n[4/9] 读取当前编码器位置...")
        try:
            initial_encoder = device.command_inout("readEncoder")
            print(f"✓ 初始位置 (X轴): {initial_encoder:.2f} mm")
        except Exception as e:
            print(f"⚠ 无法读取编码器: {e}")
            initial_encoder = None
        
        # 5. 显示目标移动距离
        print(f"\n[5/9] 设置移动参数...")
        print(f"相对移动距离: {distance:+.2f} mm")
        
        if initial_encoder is not None:
            # 计算预期的编码器圈数变化
            expected_encoder_diff = distance / (4.5 * math.pi)
            target_encoder = initial_encoder + expected_encoder_diff
            print(f"预期编码器圈数变化: {expected_encoder_diff:+.6f} 圈")
            print(f"预期最终编码器圈数: {target_encoder:.6f} 圈")
        
        # 用户确认
        print(f"\n⚠ 即将执行相对移动")
        response = input("是否继续? (y/N): ")
        if response.lower() != 'y':
            print("已取消操作")
            return
        
        # 6. 释放刹车
        print(f"\n[6/9] 释放刹车...")
        try:
            device.command_inout("releaseBrake")
            print("✓ 刹车已释放")
            time.sleep(0.5)  # 等待刹车释放完成
        except Exception as e:
            print(f"⚠ 刹车释放失败（可能未配置刹车）: {e}")
        
        # 7. 设置运动参数
        print(f"\n[7/9] 设置运动参数...")
        try:
            # 参数格式: [axis, startSpeed, maxSpeed, accTime, decTime, stopSpeed]
            move_params = [
                6.0,        # 轴号 (X轴 = 6)
                3000.0,     # 起始速度 (pulse/s)
                10000.0,    # 最大速度 (pulse/s)
                0.2,        # 加速时间 (s)
                0.2,        # 减速时间 (s)
                1000.0      # 停止速度 (pulse/s)
            ]
            device.command_inout("moveAxisSet", move_params)
            print("✓ 运动参数设置成功")
        except Exception as e:
            print(f"⚠ 运动参数设置失败: {e}")
        
        # 8. 执行 MoveRelative
        print(f"\n[8/9] 执行 MoveRelative 命令...")
        
        start_time = time.time()
        device.command_inout("MoveRelative", distance)
        print("✓ MoveRelative 命令已发送")
        
        # 9. 监控运动状态
        print(f"\n[9/9] 监控运动状态...")
        timeout = 120  # 超时时间 (秒)
        poll_interval = 0.1  # 100ms以触发read_attr_hardware状态检查
        
        motion_controller = tango.DeviceProxy("sys/motion/3")
        while time.time() - start_time < timeout:
            try:
                current_state = device.state()
                motion_state = motion_controller.state()  # 触发hook

                elapsed = time.time() - start_time
                
                # 读取当前编码器圈数
                try:
                    current_encoder = device.command_inout("readEncoder")
                    position_str = f"{current_encoder:.4f} 圈"
                    
                    # 计算已移动距离
                    if initial_encoder is not None:
                        encoder_diff = current_encoder - initial_encoder
                        moved_mm = encoder_diff * 4.5 * math.pi
                        position_str += f" (已移动: {moved_mm:+.2f} mm)"
                except:
                    position_str = "N/A"
                
                print(f"\r  时间: {elapsed:.1f}s | 状态: {current_state} | 编码器: {position_str}", end="", flush=True)
                
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
        
        # 如果是 FAULT 状态，读取报警信息
        if final_state == tango.DevState.FAULT:
            print(f"\n⚠ 设备处于 FAULT 状态，读取错误信息:")
            try:
                alarm_state = device.read_attribute("alarmState").value
                if alarm_state:
                    print(f"  报警信息: {alarm_state}")
                else:
                    print(f"  报警信息: (无)")
            except Exception as e:
                print(f"  无法读取报警信息: {e}")
            
            try:
                status = device.status()
                print(f"  设备状态详情: {status}")
            except Exception as e:
                print(f"  无法读取设备状态: {e}")
        
        # 读取最终编码器圈数并计算
        print(f"\n编码器圈数差值验证:")
        try:
            final_encoder = device.command_inout("readEncoder")
            print(f"  初始编码器圈数: {initial_encoder:.6f} 圈")
            print(f"  最终编码器圈数: {final_encoder:.6f} 圈")
            
            if initial_encoder is not None:
                encoder_diff = final_encoder - initial_encoder
                # 编码器差值转换公式: 圈数 × 4.5 × π = mm
                encoder_moved_mm = encoder_diff * 4.5 * math.pi
                
                print(f"  编码器圈数差值: {encoder_diff:+.6f} 圈")
                print(f"  转换公式: 圈数 × 4.5 × π")
                print(f"  编码器换算距离: {encoder_moved_mm:+.4f} mm")
                print(f"  目标移动距离: {distance:+.2f} mm")
                
                # 计算误差
                error = encoder_moved_mm - distance
                print(f"\n移动结果:")
                print(f"  实际移动: {encoder_moved_mm:+.4f} mm")
                print(f"  目标移动: {distance:+.2f} mm")
                print(f"  误差: {error:+.4f} mm")
                
                if abs(error) < 0.5:
                    print(f"  ✓ 误差在可接受范围内 (< 0.5mm)")
                else:
                    print(f"  ⚠ 误差较大 (≥ 0.5mm)")
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
    default_device = "sys/large_stroke/1"
    
    print(f"\n使用方法: python3 {sys.argv[0]} [设备名称] [距离]")
    print(f"示例 1: python3 {sys.argv[0]} (使用默认设备和默认距离 50mm)")
    print(f"示例 2: python3 {sys.argv[0]} sys/large_stroke/1 (指定设备，使用默认距离)")
    print(f"示例 3: python3 {sys.argv[0]} sys/large_stroke/1 100.0 (移动 100mm)")
    print(f"示例 4: python3 {sys.argv[0]} sys/large_stroke/1 -50.0 (反向移动 50mm)")
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
    
    test_x_move_relative(device_name, distance)


if __name__ == "__main__":
    main()
