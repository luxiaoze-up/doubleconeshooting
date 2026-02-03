#!/usr/bin/env python3
"""
反射光成像设备各轴相对运动测试脚本
测试上平台和下平台的 X、Y、Z 轴相对移动功能
"""

import sys
import time
import math
import tango
from typing import List, Tuple


class ReflectionMotionTester:
    """反射光成像设备运动测试类"""
    
    def __init__(self, device_name: str = "sys/reflection/1"):
        self.device_name = device_name
        self.device = None
        self.motion_controller = None
        
    def connect(self) -> bool:
        """连接设备"""
        try:
            print(f"\n{'='*70}")
            print(f"反射光成像设备各轴相对运动测试")
            print(f"设备名称: {self.device_name}")
            print(f"{'='*70}\n")
            
            print("[1/3] 正在连接设备...")
            self.device = tango.DeviceProxy(self.device_name)
            self.device.ping()
            print("✓ 设备连接成功")
            
            # 获取状态
            state = self.device.state()
            print(f"✓ 当前状态: {state}")
            
            # 获取运动控制器
            print("\n[2/3] 连接运动控制器...")
            motion_controller_name = self.device.get_property("motionControllerName")["motionControllerName"][0]
            print(f"  运动控制器: {motion_controller_name}")
            
            self.motion_controller = tango.DeviceProxy(motion_controller_name)
            self.motion_controller.ping()
            mc_state = self.motion_controller.state()
            print(f"  ✓ 运动控制器连接成功, 状态: {mc_state}")
            
            return True
            
        except Exception as e:
            print(f"✗ 连接失败: {e}")
            return False
    
    def read_encoder_positions(self) -> Tuple[List[float], List[float]]:
        """读取上下平台编码器位置"""
        try:
            upper = self.device.command_inout("upperPlatformReadEncoder")
            lower = self.device.command_inout("lowerPlatformReadEncoder")
            return list(upper), list(lower)
        except Exception as e:
            print(f"⚠ 读取编码器失败: {e}")
            return None, None
    
    def display_current_positions(self):
        """显示当前位置"""
        print("\n[3/3] 读取当前编码器位置...")
        upper, lower = self.read_encoder_positions()
        
        if upper and lower:
            print(f"  上平台 [X, Y, Z]: [{upper[0]:.2f}, {upper[1]:.2f}, {upper[2]:.2f}] mm")
            print(f"  下平台 [X, Y, Z]: [{lower[0]:.2f}, {lower[1]:.2f}, {lower[2]:.2f}] mm")
            return upper, lower
        return None, None
    
    def wait_for_motion_complete(self, timeout: int = 30):
        """等待运动完成"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                state = self.device.state()
                if state == tango.DevState.ON or state == tango.DevState.STANDBY:
                    return True
                time.sleep(0.1)
            except:
                time.sleep(0.1)
        return False
    
    def test_upper_platform_single_axis(self, axis_name: str, distance: float):
        """测试上平台单轴相对移动
        
        Args:
            axis_name: 轴名称 ('X', 'Y', 'Z')
            distance: 相对距离 (mm)
        """
        print(f"\n{'='*70}")
        print(f"测试上平台 {axis_name} 轴相对移动: {distance:+.2f} mm")
        print(f"{'='*70}")
        
        # 读取初始位置
        upper_before, _ = self.read_encoder_positions()
        if not upper_before:
            print("✗ 无法读取初始位置")
            return False
        
        axis_idx = {'X': 0, 'Y': 1, 'Z': 2}[axis_name]
        print(f"初始位置: {upper_before[axis_idx]:.2f} mm")
        
        # 执行相对移动
        try:
            command_name = f"upper{axis_name}MoveRelative"
            print(f"执行命令: {command_name}({distance})")
            self.device.command_inout(command_name, distance)
            
            # 等待运动完成
            print("等待运动完成...", end='', flush=True)
            if self.wait_for_motion_complete():
                print(" ✓")
                time.sleep(0.5)  # 额外等待确保稳定
                
                # 读取最终位置
                upper_after, _ = self.read_encoder_positions()
                if upper_after:
                    actual_move = upper_after[axis_idx] - upper_before[axis_idx]
                    error = abs(actual_move - distance)
                    
                    print(f"最终位置: {upper_after[axis_idx]:.2f} mm")
                    print(f"实际移动: {actual_move:+.2f} mm")
                    print(f"误差: {error:.2f} mm")
                    
                    # 编码器圈数验证
                    print(f"\n编码器圈数差值验证:")
                    # X/Y轴: 3.175 mm/圈, Z轴: 4 mm/圈
                    mm_per_circle = 3.175 if axis_name in ['X', 'Y'] else 4.0
                    encoder_diff = actual_move / mm_per_circle
                    expected_circles = distance / mm_per_circle
                    
                    print(f"  转换公式: 圈数 × {mm_per_circle} mm")
                    print(f"  实际移动: {actual_move:+.4f} mm")
                    print(f"  换算圈数: {encoder_diff:+.6f} 圈")
                    print(f"  目标圈数: {expected_circles:+.6f} 圈")
                    print(f"  圈数差异: {abs(encoder_diff - expected_circles):.6f} 圈")
                    
                    if error < 0.1:  # 误差小于0.1mm认为成功
                        print("✓ 测试通过")
                        return True
                    else:
                        print(f"⚠ 误差较大: {error:.2f} mm")
                        return False
            else:
                print(" ✗ 超时")
                return False
                
        except Exception as e:
            print(f"✗ 移动失败: {e}")
            return False
    
    def test_lower_platform_single_axis(self, axis_name: str, distance: float):
        """测试下平台单轴相对移动
        
        Args:
            axis_name: 轴名称 ('X', 'Y', 'Z')
            distance: 相对距离 (mm)
        """
        print(f"\n{'='*70}")
        print(f"测试下平台 {axis_name} 轴相对移动: {distance:+.2f} mm")
        print(f"{'='*70}")
        
        # 读取初始位置
        _, lower_before = self.read_encoder_positions()
        if not lower_before:
            print("✗ 无法读取初始位置")
            return False
        
        axis_idx = {'X': 0, 'Y': 1, 'Z': 2}[axis_name]
        print(f"初始位置: {lower_before[axis_idx]:.2f} mm")
        
        # 执行相对移动
        try:
            command_name = f"lower{axis_name}MoveRelative"
            print(f"执行命令: {command_name}({distance})")
            self.device.command_inout(command_name, distance)
            
            # 等待运动完成
            print("等待运动完成...", end='', flush=True)
            if self.wait_for_motion_complete():
                print(" ✓")
                time.sleep(0.5)  # 额外等待确保稳定
                
                # 读取最终位置
                _, lower_after = self.read_encoder_positions()
                if lower_after:
                    actual_move = lower_after[axis_idx] - lower_before[axis_idx]
                    error = abs(actual_move - distance)
                    
                    print(f"最终位置: {lower_after[axis_idx]:.2f} mm")
                    print(f"实际移动: {actual_move:+.2f} mm")
                    print(f"误差: {error:.2f} mm")
                    
                    # 编码器圈数验证
                    print(f"\n编码器圈数差值验证:")
                    # X/Y轴: 3.175 mm/圈, Z轴: 4 mm/圈
                    mm_per_circle = 3.175 if axis_name in ['X', 'Y'] else 4.0
                    encoder_diff = actual_move / mm_per_circle
                    expected_circles = distance / mm_per_circle
                    
                    print(f"  转换公式: 圈数 × {mm_per_circle} mm")
                    print(f"  实际移动: {actual_move:+.4f} mm")
                    print(f"  换算圈数: {encoder_diff:+.6f} 圈")
                    print(f"  目标圈数: {expected_circles:+.6f} 圈")
                    print(f"  圈数差异: {abs(encoder_diff - expected_circles):.6f} 圈")
                    
                    if error < 0.1:  # 误差小于0.1mm认为成功
                        print("✓ 测试通过")
                        return True
                    else:
                        print(f"⚠ 误差较大: {error:.2f} mm")
                        return False
            else:
                print(" ✗ 超时")
                return False
                
        except Exception as e:
            print(f"✗ 移动失败: {e}")
            return False
    
    def test_upper_platform_multi_axis(self, distances: List[float]):
        """测试上平台三轴同时相对移动
        
        Args:
            distances: [X, Y, Z] 相对距离 (mm)
        """
        print(f"\n{'='*70}")
        print(f"测试上平台三轴同时相对移动")
        print(f"X: {distances[0]:+.2f} mm, Y: {distances[1]:+.2f} mm, Z: {distances[2]:+.2f} mm")
        print(f"{'='*70}")
        
        # 读取初始位置
        upper_before, _ = self.read_encoder_positions()
        if not upper_before:
            print("✗ 无法读取初始位置")
            return False
        
        print(f"初始位置 [X, Y, Z]: [{upper_before[0]:.2f}, {upper_before[1]:.2f}, {upper_before[2]:.2f}] mm")
        
        # 执行相对移动
        try:
            print(f"执行命令: upperPlatformMoveRelative({distances})")
            self.device.command_inout("upperPlatformMoveRelative", distances)
            
            # 等待运动完成
            print("等待运动完成...", end='', flush=True)
            if self.wait_for_motion_complete():
                print(" ✓")
                time.sleep(0.5)
                
                # 读取最终位置
                upper_after, _ = self.read_encoder_positions()
                if upper_after:
                    actual_moves = [upper_after[i] - upper_before[i] for i in range(3)]
                    errors = [abs(actual_moves[i] - distances[i]) for i in range(3)]
                    
                    print(f"最终位置 [X, Y, Z]: [{upper_after[0]:.2f}, {upper_after[1]:.2f}, {upper_after[2]:.2f}] mm")
                    print(f"实际移动 [X, Y, Z]: [{actual_moves[0]:+.2f}, {actual_moves[1]:+.2f}, {actual_moves[2]:+.2f}] mm")
                    print(f"误差 [X, Y, Z]: [{errors[0]:.2f}, {errors[1]:.2f}, {errors[2]:.2f}] mm")
                    
                    # 编码器圈数验证
                    print(f"\n编码器圈数差值验证:")
                    axis_names = ['X', 'Y', 'Z']
                    for i, axis_name in enumerate(axis_names):
                        mm_per_circle = 3.175 if axis_name in ['X', 'Y'] else 4.0
                        encoder_diff = actual_moves[i] / mm_per_circle
                        expected_circles = distances[i] / mm_per_circle
                        print(f"  {axis_name}轴: 实际 {encoder_diff:+.4f} 圈, 目标 {expected_circles:+.4f} 圈, 差异 {abs(encoder_diff - expected_circles):.4f} 圈")
                    
                    if all(e < 0.1 for e in errors):
                        print("✓ 测试通过")
                        return True
                    else:
                        print(f"⚠ 部分轴误差较大")
                        return False
            else:
                print(" ✗ 超时")
                return False
                
        except Exception as e:
            print(f"✗ 移动失败: {e}")
            return False
    
    def test_lower_platform_multi_axis(self, distances: List[float]):
        """测试下平台三轴同时相对移动
        
        Args:
            distances: [X, Y, Z] 相对距离 (mm)
        """
        print(f"\n{'='*70}")
        print(f"测试下平台三轴同时相对移动")
        print(f"X: {distances[0]:+.2f} mm, Y: {distances[1]:+.2f} mm, Z: {distances[2]:+.2f} mm")
        print(f"{'='*70}")
        
        # 读取初始位置
        _, lower_before = self.read_encoder_positions()
        if not lower_before:
            print("✗ 无法读取初始位置")
            return False
        
        print(f"初始位置 [X, Y, Z]: [{lower_before[0]:.2f}, {lower_before[1]:.2f}, {lower_before[2]:.2f}] mm")
        
        # 执行相对移动
        try:
            print(f"执行命令: lowerPlatformMoveRelative({distances})")
            self.device.command_inout("lowerPlatformMoveRelative", distances)
            
            # 等待运动完成
            print("等待运动完成...", end='', flush=True)
            if self.wait_for_motion_complete():
                print(" ✓")
                time.sleep(0.5)
                
                # 读取最终位置
                _, lower_after = self.read_encoder_positions()
                if lower_after:
                    actual_moves = [lower_after[i] - lower_before[i] for i in range(3)]
                    errors = [abs(actual_moves[i] - distances[i]) for i in range(3)]
                    
                    print(f"最终位置 [X, Y, Z]: [{lower_after[0]:.2f}, {lower_after[1]:.2f}, {lower_after[2]:.2f}] mm")
                    print(f"实际移动 [X, Y, Z]: [{actual_moves[0]:+.2f}, {actual_moves[1]:+.2f}, {actual_moves[2]:+.2f}] mm")
                    print(f"误差 [X, Y, Z]: [{errors[0]:.2f}, {errors[1]:.2f}, {errors[2]:.2f}] mm")
                    
                    # 编码器圈数验证
                    print(f"\n编码器圈数差值验证:")
                    axis_names = ['X', 'Y', 'Z']
                    for i, axis_name in enumerate(axis_names):
                        mm_per_circle = 3.175 if axis_name in ['X', 'Y'] else 4.0
                        encoder_diff = actual_moves[i] / mm_per_circle
                        expected_circles = distances[i] / mm_per_circle
                        print(f"  {axis_name}轴: 实际 {encoder_diff:+.4f} 圈, 目标 {expected_circles:+.4f} 圈, 差异 {abs(encoder_diff - expected_circles):.4f} 圈")
                    
                    if all(e < 0.1 for e in errors):
                        print("✓ 测试通过")
                        return True
                    else:
                        print(f"⚠ 部分轴误差较大")
                        return False
            else:
                print(" ✗ 超时")
                return False
                
        except Exception as e:
            print(f"✗ 移动失败: {e}")
            return False


def run_comprehensive_test(device_name: str = "sys/reflection/1"):
    """运行完整测试"""
    tester = ReflectionMotionTester(device_name)
    
    # 连接设备
    if not tester.connect():
        print("\n✗ 设备连接失败，测试终止")
        return
    
    # 显示初始位置
    upper_init, lower_init = tester.display_current_positions()
    if not upper_init or not lower_init:
        print("\n✗ 无法读取初始位置，测试终止")
        return
    
    # 用户确认
    print(f"\n{'='*70}")
    print("即将开始测试，请确认:")
    print("1. 设备已正常初始化")
    print("2. 运动范围内无障碍物")
    print("3. 测试距离为小幅度移动 (±10mm)")
    print(f"{'='*70}")
    
    response = input("\n是否继续测试? (y/n): ")
    if response.lower() != 'y':
        print("测试取消")
        return
    
    # 测试结果统计
    results = []
    
    # ===== 上平台单轴测试 =====
    print(f"\n{'#'*70}")
    print("# 上平台单轴相对运动测试")
    print(f"{'#'*70}")
    
    # X轴正向
    results.append(("上平台 X+", tester.test_upper_platform_single_axis('X', 10.0)))
    time.sleep(1)
    
    # X轴负向 (回到原位)
    results.append(("上平台 X-", tester.test_upper_platform_single_axis('X', -10.0)))
    time.sleep(1)
    
    # Y轴正向
    results.append(("上平台 Y+", tester.test_upper_platform_single_axis('Y', 10.0)))
    time.sleep(1)
    
    # Y轴负向 (回到原位)
    results.append(("上平台 Y-", tester.test_upper_platform_single_axis('Y', -10.0)))
    time.sleep(1)
    
    # Z轴正向
    results.append(("上平台 Z+", tester.test_upper_platform_single_axis('Z', 5.0)))
    time.sleep(1)
    
    # Z轴负向 (回到原位)
    results.append(("上平台 Z-", tester.test_upper_platform_single_axis('Z', -5.0)))
    time.sleep(1)
    
    # ===== 下平台单轴测试 =====
    print(f"\n{'#'*70}")
    print("# 下平台单轴相对运动测试")
    print(f"{'#'*70}")
    
    # X轴正向
    results.append(("下平台 X+", tester.test_lower_platform_single_axis('X', 10.0)))
    time.sleep(1)
    
    # X轴负向 (回到原位)
    results.append(("下平台 X-", tester.test_lower_platform_single_axis('X', -10.0)))
    time.sleep(1)
    
    # Y轴正向
    results.append(("下平台 Y+", tester.test_lower_platform_single_axis('Y', 10.0)))
    time.sleep(1)
    
    # Y轴负向 (回到原位)
    results.append(("下平台 Y-", tester.test_lower_platform_single_axis('Y', -10.0)))
    time.sleep(1)
    
    # Z轴正向
    results.append(("下平台 Z+", tester.test_lower_platform_single_axis('Z', 5.0)))
    time.sleep(1)
    
    # Z轴负向 (回到原位)
    results.append(("下平台 Z-", tester.test_lower_platform_single_axis('Z', -5.0)))
    time.sleep(1)
    
    # ===== 多轴同时运动测试 =====
    print(f"\n{'#'*70}")
    print("# 多轴同时相对运动测试")
    print(f"{'#'*70}")
    
    # 上平台三轴同时运动
    results.append(("上平台三轴+", tester.test_upper_platform_multi_axis([10.0, 10.0, 5.0])))
    time.sleep(1)
    
    # 上平台三轴回到原位
    results.append(("上平台三轴-", tester.test_upper_platform_multi_axis([-10.0, -10.0, -5.0])))
    time.sleep(1)
    
    # 下平台三轴同时运动
    results.append(("下平台三轴+", tester.test_lower_platform_multi_axis([10.0, 10.0, 5.0])))
    time.sleep(1)
    
    # 下平台三轴回到原位
    results.append(("下平台三轴-", tester.test_lower_platform_multi_axis([-10.0, -10.0, -5.0])))
    
    # ===== 测试总结 =====
    print(f"\n{'='*70}")
    print("测试总结")
    print(f"{'='*70}")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    print(f"\n总测试数: {total}")
    print(f"通过: {passed}")
    print(f"失败: {total - passed}")
    print(f"成功率: {passed/total*100:.1f}%\n")
    
    print("详细结果:")
    for name, result in results:
        status = "✓ 通过" if result else "✗ 失败"
        print(f"  {name:20s} {status}")
    
    # 验证最终位置
    print(f"\n{'='*70}")
    print("最终位置验证")
    print(f"{'='*70}")
    upper_final, lower_final = tester.read_encoder_positions()
    
    if upper_final and lower_final:
        upper_drift = [abs(upper_final[i] - upper_init[i]) for i in range(3)]
        lower_drift = [abs(lower_final[i] - lower_init[i]) for i in range(3)]
        
        print(f"上平台初始: [{upper_init[0]:.2f}, {upper_init[1]:.2f}, {upper_init[2]:.2f}] mm")
        print(f"上平台最终: [{upper_final[0]:.2f}, {upper_final[1]:.2f}, {upper_final[2]:.2f}] mm")
        print(f"上平台漂移: [{upper_drift[0]:.2f}, {upper_drift[1]:.2f}, {upper_drift[2]:.2f}] mm")
        
        print(f"\n下平台初始: [{lower_init[0]:.2f}, {lower_init[1]:.2f}, {lower_init[2]:.2f}] mm")
        print(f"下平台最终: [{lower_final[0]:.2f}, {lower_final[1]:.2f}, {lower_final[2]:.2f}] mm")
        print(f"下平台漂移: [{lower_drift[0]:.2f}, {lower_drift[1]:.2f}, {lower_drift[2]:.2f}] mm")
        
        max_drift = max(max(upper_drift), max(lower_drift))
        if max_drift < 0.5:
            print(f"\n✓ 位置漂移在可接受范围内 (最大漂移: {max_drift:.2f} mm)")
        else:
            print(f"\n⚠ 位置漂移较大 (最大漂移: {max_drift:.2f} mm)")
    
    print(f"\n{'='*70}")
    print("测试完成")
    print(f"{'='*70}\n")


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description='反射光成像设备各轴相对运动测试')
    parser.add_argument('--device', '-d', default='sys/reflection/1',
                       help='设备名称 (默认: sys/reflection/1)')
    parser.add_argument('--platform', '-p', choices=['upper', 'lower', 'all'],
                       default='all', help='测试平台 (默认: all)')
    parser.add_argument('--axis', '-a', choices=['X', 'Y', 'Z', 'all'],
                       default='all', help='测试轴 (默认: all)')
    parser.add_argument('--distance', '-m', type=float, default=10.0,
                       help='测试距离/mm (默认: 10.0)')
    
    args = parser.parse_args()
    
    # 如果是完整测试
    if args.platform == 'all' and args.axis == 'all':
        run_comprehensive_test(args.device)
    else:
        # 单独测试
        tester = ReflectionMotionTester(args.device)
        if not tester.connect():
            print("设备连接失败")
            return
        
        tester.display_current_positions()
        
        print(f"\n即将测试 {args.platform} 平台 {args.axis} 轴移动 {args.distance:+.2f} mm")
        response = input("是否继续? (y/n): ")
        if response.lower() != 'y':
            return
        
        if args.platform == 'upper':
            if args.axis == 'all':
                tester.test_upper_platform_multi_axis([args.distance, args.distance, args.distance/2])
            else:
                tester.test_upper_platform_single_axis(args.axis, args.distance)
        else:
            if args.axis == 'all':
                tester.test_lower_platform_multi_axis([args.distance, args.distance, args.distance/2])
            else:
                tester.test_lower_platform_single_axis(args.axis, args.distance)


if __name__ == "__main__":
    main()
