"""
先决条件检查工具类

统一管理所有操作的先决条件检查逻辑，避免代码重复。
"""

from typing import Dict, Any, List, Tuple, Optional
from config import SystemState, OPERATION_PREREQUISITES, AUTO_BRANCH_THRESHOLDS


class PrerequisiteChecker:
    """先决条件检查器"""
    
    @staticmethod
    def check(check_name: str, status: Dict[str, Any]) -> bool:
        """
        检查单个先决条件
        
        Args:
            check_name: 检查项名称
            status: 系统状态字典
        
        Returns:
            是否满足条件
        """
        # =========================================================================
        # 水路/电源/通用检查
        # =========================================================================
        if check_name == "water_flow_ok":
            # 检查水电磁阀1-4是否开启（表示水路正常）
            # 水电磁阀对应冷却水回路
            for i in range(1, 5):
                if not status.get(f'waterValve{i}State', False):
                    return False
            return True
        elif check_name == "power_ok":
            # 相序保护正常即表示供电正常
            return status.get('phaseSequenceOk', True)
        elif check_name == "no_pump_fault":
            return status.get('systemState', 0) != SystemState.FAULT
        elif check_name == "air_pressure_ok":
            return status.get('airPressure', 0) >= 0.4
        elif check_name == "motion_permit":
            # 运动控制系统在线且闸板阀5动作许可
            return status.get('motionSystemOnline', False) and status.get('gateValve5Permit', False)
        
        # =========================================================================
        # 系统状态检查
        # =========================================================================
        elif check_name == "system_in_fault_or_estop":
            return status.get('systemState', 0) in [SystemState.FAULT, SystemState.EMERGENCY_STOP]
        elif check_name == "system_not_in_fault_or_estop":
            # 系统不能处于故障或急停状态（用于停机等操作）
            return status.get('systemState', 0) not in [SystemState.FAULT, SystemState.EMERGENCY_STOP]
        elif check_name == "system_needs_reset":
            # 系统处于故障/急停状态，或者有活跃报警
            state_ok = status.get('systemState', 0) in [SystemState.FAULT, SystemState.EMERGENCY_STOP]
            has_alarm = status.get('activeAlarmCount', 0) > 0 or status.get('hasUnacknowledgedAlarm', False)
            return state_ok or has_alarm
        elif check_name == "not_pumping":
            return status.get('systemState', 0) != SystemState.PUMPING
        
        # =========================================================================
        # 真空计读数检查
        # =========================================================================
        elif check_name == "vacuum_gauges_ok":
            # 简单判断真空计读数是否为合理数值（避免 None/NaN 等）
            for key in ["vacuumGauge2", "vacuumGauge3"]:
                v = status.get(key, None)
                if not isinstance(v, (int, float)):
                    return False
                if v < 0 or v > 200000:
                    return False
            return True
        elif check_name == "vacuum_level_ok":
            return status.get('vacuumGauge3', 101325) <= 7000
        elif check_name == "high_vacuum_ok":
            # 真空计1/2读数≤45Pa（取较小值判断）
            v1 = status.get('vacuumGauge1', 101325)
            v2 = status.get('vacuumGauge2', 101325)
            return min(v1, v2) <= 45
        elif check_name == "fore_vacuum_80000pa":
            # 前级真空≤80000Pa（罗茨泵启动条件）
            threshold = AUTO_BRANCH_THRESHOLDS.get("罗茨泵启动阈值Pa", 80000)
            return status.get('vacuumGauge1', 101325) <= threshold
        elif check_name == "chamber_vacuum_ok":
            # 腔室真空<3000Pa
            threshold = AUTO_BRANCH_THRESHOLDS.get("粗抽分支阈值Pa", 3000)
            return status.get('vacuumGauge2', 101325) < threshold
        elif check_name == "pressure_diff_ok":
            # 检查前级和主真空压差<3000Pa
            v1 = status.get('vacuumGauge1', 101325)
            v2 = status.get('vacuumGauge2', 101325)
            threshold = AUTO_BRANCH_THRESHOLDS.get("粗抽分支阈值Pa", 3000)
            return abs(v1 - v2) < threshold
        
        # =========================================================================
        # 泵状态检查
        # =========================================================================
        elif check_name == "screw_pump_running":
            return status.get('screwPumpPower', False)
        elif check_name == "screw_pump_running_110hz":
            # 螺杆泵运行且达110Hz
            return status.get('screwPumpPower', False) and status.get('screwPumpFrequency', 0) >= 110
        elif check_name == "screw_pump_stopped":
            return not status.get('screwPumpPower', False)
        elif check_name == "roots_pump_stopped":
            return not status.get('rootsPumpPower', False)
        elif check_name == "molecular_pumps_stopped":
            # 允许微量转速（约300RPM/5Hz以下视为停止）
            # 注意: Device Server 返回的是 RPM 单位
            for i in range(1, 4):
                if status.get(f'molecularPump{i}Speed', 0) > 300:
                    return False
            return True
        elif check_name == "molecular_pump1_stopped":
            return status.get('molecularPump1Speed', 0) <= 300
        elif check_name == "molecular_pump2_stopped":
            return status.get('molecularPump2Speed', 0) <= 300
        elif check_name == "molecular_pump3_stopped":
            return status.get('molecularPump3Speed', 0) <= 300
        elif check_name == "molecular_pump_speed_low":
            # 分子泵转速<300RPM（关闭电磁阀条件，约5Hz）
            # 注意: Device Server 返回的是 RPM 单位
            threshold = AUTO_BRANCH_THRESHOLDS.get("分子泵关阀转速RPM", 300)
            for i in range(1, 4):
                if status.get(f'molecularPump{i}Speed', 0) >= threshold:
                    return False
            return True
        elif check_name == "molecular_pumps_full_speed":
            # 分子泵1-3均已满转（518Hz稳定状态，约31080 RPM）
            # 要求：所有分子泵都已启动，且转速都达到满转阈值
            threshold = 31000  # 518Hz * 60 ≈ 31080 RPM
            for i in range(1, 4):
                power = status.get(f'molecularPump{i}Power', False)
                speed = status.get(f'molecularPump{i}Speed', 0)
                # 如果泵已启动，必须达到满转；如果未启动，则不满足条件
                if power:
                    if speed < threshold:
                        return False
                else:
                    # 泵未启动，不满足"均已满转"的条件
                    return False
            return True
        
        # =========================================================================
        # 电磁阀状态检查
        # =========================================================================
        elif check_name == "electromagnetic_valve1_open":
            return status.get('electromagneticValve1Open', False)
        elif check_name == "electromagnetic_valve2_open":
            return status.get('electromagneticValve2Open', False)
        elif check_name == "electromagnetic_valve3_open":
            return status.get('electromagneticValve3Open', False)
        elif check_name == "electromagnetic_valve4_open":
            return status.get('electromagneticValve4Open', False)
        elif check_name == "electromagnetic_valve_open":
            # 检查电磁阀1-3至少有一个开启（保留用于兼容性）
            return any([status.get(f'electromagneticValve{i}Open', False) for i in range(1, 4)])
        elif check_name == "electromagnetic_valves_123_all_open":
            # 检查电磁阀1-3全部开启
            for i in range(1, 4):
                if not status.get(f'electromagneticValve{i}Open', False):
                    return False
            return True
        
        # =========================================================================
        # 闸板阀状态检查
        # =========================================================================
        elif check_name == "gate_valve1_open":
            return status.get('gateValve1Open', False)
        elif check_name == "gate_valve2_open":
            return status.get('gateValve2Open', False)
        elif check_name == "gate_valve3_open":
            return status.get('gateValve3Open', False)
        elif check_name == "gate_valve_open":
            # 检查闸板阀1-3至少有一个开启（保留用于兼容性）
            return any([status.get(f'gateValve{i}Open', False) for i in range(1, 4)])
        elif check_name == "gate_valves_123_closed":
            for i in range(1, 4):
                if status.get(f'gateValve{i}Open', False):
                    return False
            return True
        elif check_name == "gate_valve1_closed":
            return status.get('gateValve1Open', False) == False
        elif check_name == "gate_valve2_closed":
            return status.get('gateValve2Open', False) == False
        elif check_name == "gate_valve3_closed":
            return status.get('gateValve3Open', False) == False
        elif check_name == "gate_valves_1_4_closed":
            for i in range(1, 5):
                if status.get(f'gateValve{i}Open', False):
                    return False
            return True
        elif check_name == "gate_valve5_closed":
            return not status.get('gateValve5Open', False)
        elif check_name == "all_gate_valves_closed":
            for i in range(1, 6):
                if status.get(f'gateValve{i}Open', False):
                    return False
            return True
        
        # =========================================================================
        # 放气阀状态检查
        # =========================================================================
        elif check_name == "vent_valves_closed":
            return (not status.get('ventValve1Open', False)) and (not status.get('ventValve2Open', False))
        elif check_name == "vent_valve1_closed":
            return not status.get('ventValve1Open', False)
        elif check_name == "vent_valve2_closed":
            return not status.get('ventValve2Open', False)
        
        # =========================================================================
        # 新增真空度检查
        # =========================================================================
        elif check_name == "vacuum3_le_7000pa":
            # 真空计3读数≤7000Pa（罗茨泵启动条件）
            return status.get('vacuumGauge3', 101325) <= 7000
        elif check_name == "vacuum3_ge_80000pa":
            # 真空计3读数≥80000Pa（放气阀1关闭条件，前级已大气）
            return status.get('vacuumGauge3', 0) >= 80000
        elif check_name == "chamber_at_atmosphere":
            # 腔室已放气至大气状态（真空计1/2≥80000Pa）
            v1 = status.get('vacuumGauge1', 0)
            v2 = status.get('vacuumGauge2', 0)
            return v1 >= 80000 or v2 >= 80000
        
        # =========================================================================
        # 外部系统许可检查
        # =========================================================================
        elif check_name == "motion_permit_close":
            # 外部大行程系统发出允许关闭信号
            # 注意: Device Server 只有 gateValve5Permit，没有区分开/关许可
            # 这里假设 gateValve5Permit 同时控制开和关许可
            return status.get('motionSystemOnline', False) and status.get('gateValve5Permit', True)
        
        else:
            return False
    
    @staticmethod
    def check_with_details(check_name: str, status: Dict[str, Any]) -> Tuple[bool, Optional[str], Optional[str]]:
        """
        检查单个先决条件，并返回当前值和期望值
        
        Args:
            check_name: 检查项名称
            status: 系统状态字典
        
        Returns:
            (是否满足, 当前值字符串, 期望值字符串)
        """
        is_met = PrerequisiteChecker.check(check_name, status)
        current_val = None
        expected_val = None
        
        # =========================================================================
        # 根据检查类型返回详细值
        # =========================================================================
        if check_name == "water_flow_ok":
            # 水路状态 - 从水电磁阀状态推导
            water_ok = all([status.get(f'waterValve{i}State', False) for i in range(1, 5)])
            current_val = "水阀开启" if water_ok else "水阀未开"
            expected_val = "水电磁阀1-4全部开启"
        elif check_name == "air_pressure_ok":
            pressure = status.get('airPressure', 0)
            current_val = f"{pressure:.2f} MPa"
            expected_val = "≥ 0.4 MPa"
        elif check_name == "vacuum_gauges_ok":
            v2 = status.get('vacuumGauge2', None)
            v3 = status.get('vacuumGauge3', None)
            v2_str = f"{v2:.0f}" if isinstance(v2, (int, float)) else "--"
            v3_str = f"{v3:.0f}" if isinstance(v3, (int, float)) else "--"
            current_val = f"真空计2: {v2_str} Pa, 真空计3: {v3_str} Pa"
            expected_val = "0~200000 Pa"
        elif check_name == "vacuum_level_ok":
            v = status.get('vacuumGauge3', 101325)
            current_val = f"{v:.1f} Pa"
            expected_val = "≤ 7000 Pa"
        elif check_name == "high_vacuum_ok":
            v = status.get('vacuumGauge1', 101325)
            current_val = f"{v:.1f} Pa"
            expected_val = "≤ 45 Pa"
        elif check_name == "fore_vacuum_80000pa":
            v = status.get('vacuumGauge1', 101325)
            threshold = AUTO_BRANCH_THRESHOLDS.get("罗茨泵启动阈值Pa", 80000)
            current_val = f"{v:.0f} Pa"
            expected_val = f"≤ {threshold} Pa"
        elif check_name == "chamber_vacuum_ok":
            v = status.get('vacuumGauge2', 101325)
            threshold = AUTO_BRANCH_THRESHOLDS.get("粗抽分支阈值Pa", 3000)
            current_val = f"{v:.0f} Pa"
            expected_val = f"< {threshold} Pa"
        elif check_name == "pressure_diff_ok":
            v1 = status.get('vacuumGauge1', 101325)
            v2 = status.get('vacuumGauge2', 101325)
            diff = abs(v1 - v2)
            threshold = AUTO_BRANCH_THRESHOLDS.get("粗抽分支阈值Pa", 3000)
            current_val = f"压差 {diff:.0f} Pa"
            expected_val = f"< {threshold} Pa"
        elif check_name == "screw_pump_running":
            running = status.get('screwPumpPower', False)
            current_val = "运行中" if running else "已停止"
            expected_val = "运行中"
        elif check_name == "screw_pump_running_110hz":
            running = status.get('screwPumpPower', False)
            freq = status.get('screwPumpFrequency', 0)
            current_val = f"{'运行' if running else '停止'}, {freq} Hz"
            expected_val = "运行且 ≥ 110 Hz"
        elif check_name == "screw_pump_stopped":
            running = status.get('screwPumpPower', False)
            current_val = "已停止" if not running else "运行中"
            expected_val = "已停止"
        elif check_name == "roots_pump_stopped":
            running = status.get('rootsPumpPower', False)
            current_val = "已停止" if not running else "运行中"
            expected_val = "已停止"
        elif check_name == "molecular_pumps_stopped":
            # 注意: Device Server 返回的是 RPM 单位
            speeds = [status.get(f'molecularPump{i}Speed', 0) for i in range(1, 4)]
            current_val = f"转速: {speeds[0]:.0f}, {speeds[1]:.0f}, {speeds[2]:.0f} RPM"
            expected_val = "全部 ≤ 300 RPM (约5Hz)"
        elif check_name == "molecular_pump1_stopped":
            speed = status.get('molecularPump1Speed', 0)
            current_val = f"分子泵1转速: {speed:.0f} RPM"
            expected_val = "≤ 300 RPM"
        elif check_name == "molecular_pump2_stopped":
            speed = status.get('molecularPump2Speed', 0)
            current_val = f"分子泵2转速: {speed:.0f} RPM"
            expected_val = "≤ 300 RPM"
        elif check_name == "molecular_pump3_stopped":
            speed = status.get('molecularPump3Speed', 0)
            current_val = f"分子泵3转速: {speed:.0f} RPM"
            expected_val = "≤ 300 RPM"
        elif check_name == "molecular_pump_speed_low":
            # 注意: Device Server 返回的是 RPM 单位
            speeds = [status.get(f'molecularPump{i}Speed', 0) for i in range(1, 4)]
            threshold = AUTO_BRANCH_THRESHOLDS.get("分子泵关阀转速RPM", 300)
            current_val = f"转速: {speeds[0]:.0f}, {speeds[1]:.0f}, {speeds[2]:.0f} RPM"
            expected_val = f"全部 < {threshold} RPM"
        elif check_name == "electromagnetic_valve1_open":
            is_open = status.get('electromagneticValve1Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "开启"
        elif check_name == "electromagnetic_valve2_open":
            is_open = status.get('electromagneticValve2Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "开启"
        elif check_name == "electromagnetic_valve3_open":
            is_open = status.get('electromagneticValve3Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "开启"
        elif check_name == "electromagnetic_valve4_open":
            is_open = status.get('electromagneticValve4Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "开启"
        elif check_name == "electromagnetic_valve_open":
            opens = [status.get(f'electromagneticValve{i}Open', False) for i in range(1, 4)]
            states = ['开' if o else '关' for o in opens]
            current_val = f"电磁阀1-3: {'/'.join(states)}"
            expected_val = "至少一个开启"
        elif check_name == "gate_valve1_open":
            is_open = status.get('gateValve1Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "开启"
        elif check_name == "gate_valve2_open":
            is_open = status.get('gateValve2Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "开启"
        elif check_name == "gate_valve3_open":
            is_open = status.get('gateValve3Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "开启"
        elif check_name == "gate_valve_open":
            opens = [status.get(f'gateValve{i}Open', False) for i in range(1, 4)]
            states = ['开' if o else '关' for o in opens]
            current_val = f"闸板阀1-3: {'/'.join(states)}"
            expected_val = "至少一个开启"
        elif check_name == "gate_valves_123_closed":
            opens = [status.get(f'gateValve{i}Open', False) for i in range(1, 4)]
            states = ['开' if o else '关' for o in opens]
            current_val = f"闸板阀1-3: {'/'.join(states)}"
            expected_val = "全部关闭"
        elif check_name == "gate_valve1_closed":
            is_open = status.get('gateValve1Open', False)
            current_val = f"闸板阀1: {'开启' if is_open else '关闭'}"
            expected_val = "关闭"
        elif check_name == "gate_valve2_closed":
            is_open = status.get('gateValve2Open', False)
            current_val = f"闸板阀2: {'开启' if is_open else '关闭'}"
            expected_val = "关闭"
        elif check_name == "gate_valve3_closed":
            is_open = status.get('gateValve3Open', False)
            current_val = f"闸板阀3: {'开启' if is_open else '关闭'}"
            expected_val = "关闭"
        elif check_name == "gate_valves_1_4_closed":
            opens = [status.get(f'gateValve{i}Open', False) for i in range(1, 5)]
            states = ['开' if o else '关' for o in opens]
            current_val = f"闸板阀1-4: {'/'.join(states)}"
            expected_val = "全部关闭"
        elif check_name == "gate_valve5_closed":
            is_open = status.get('gateValve5Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "关闭"
        elif check_name == "all_gate_valves_closed":
            opens = [status.get(f'gateValve{i}Open', False) for i in range(1, 6)]
            states = ['开' if o else '关' for o in opens]
            current_val = f"闸板阀1-5: {'/'.join(states)}"
            expected_val = "全部关闭"
        elif check_name == "vent_valves_closed":
            v1 = status.get('ventValve1Open', False)
            v2 = status.get('ventValve2Open', False)
            current_val = f"放气阀1: {'开' if v1 else '关'}, 放气阀2: {'开' if v2 else '关'}"
            expected_val = "全部关闭"
        elif check_name == "vent_valve1_closed":
            is_open = status.get('ventValve1Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "关闭"
        elif check_name == "vent_valve2_closed":
            is_open = status.get('ventValve2Open', False)
            current_val = "开启" if is_open else "关闭"
            expected_val = "关闭"
        elif check_name == "vacuum3_le_7000pa":
            v3 = status.get('vacuumGauge3', 101325)
            current_val = f"{v3:.0f} Pa"
            expected_val = "≤ 7000 Pa"
        elif check_name == "vacuum3_ge_80000pa":
            v3 = status.get('vacuumGauge3', 0)
            current_val = f"{v3:.0f} Pa"
            expected_val = "≥ 80000 Pa (大气)"
        elif check_name == "chamber_at_atmosphere":
            v1 = status.get('vacuumGauge1', 0)
            v2 = status.get('vacuumGauge2', 0)
            current_val = f"真空计1: {v1:.0f} Pa, 真空计2: {v2:.0f} Pa"
            expected_val = "任一 ≥ 80000 Pa"
        elif check_name == "molecular_pumps_full_speed":
            # 显示所有分子泵的启动状态和转速
            pump_states = []
            for i in range(1, 4):
                power = status.get(f'molecularPump{i}Power', False)
                speed = status.get(f'molecularPump{i}Speed', 0)
                state_str = f"泵{i}:{'运行' if power else '停止'},{speed:.0f}RPM"
                pump_states.append(state_str)
            current_val = f"当前: {' | '.join(pump_states)}"
            expected_val = "全部启动且转速≥ 31000 RPM (518Hz)"
        elif check_name == "motion_permit_close":
            online = status.get('motionSystemOnline', False)
            # 注意: Device Server 只有 gateValve5Permit，没有区分开/关许可
            permit = status.get('gateValve5Permit', True)
            current_val = f"在线: {'是' if online else '否'}, 许可: {'是' if permit else '否'}"
            expected_val = "在线且有动作许可"
        elif check_name == "not_pumping":
            state = status.get('systemState', 0)
            state_names = {0: "空闲", 1: "抽真空", 2: "停机中", 3: "放气中", 4: "故障", 5: "急停"}
            current_val = state_names.get(state, "未知")
            expected_val = "非抽真空状态"
        elif check_name == "system_in_fault_or_estop":
            state = status.get('systemState', 0)
            state_names = {0: "空闲", 1: "抽真空", 2: "停机中", 3: "放气中", 4: "故障", 5: "急停"}
            current_val = state_names.get(state, "未知")
            expected_val = "故障或急停状态"
        elif check_name == "system_needs_reset":
            state = status.get('systemState', 0)
            state_names = {0: "空闲", 1: "抽真空", 2: "停机中", 3: "放气中", 4: "故障", 5: "急停"}
            alarm_count = status.get('activeAlarmCount', 0)
            has_unack = status.get('hasUnacknowledgedAlarm', False)
            state_str = state_names.get(state, "未知")
            if alarm_count > 0:
                current_val = f"状态: {state_str}, 活跃报警: {alarm_count}个"
            elif has_unack:
                current_val = f"状态: {state_str}, 有未确认报警"
            else:
                current_val = f"状态: {state_str}, 无报警"
            expected_val = "故障/急停状态 或 有活跃报警"
        
        return is_met, current_val, expected_val
    
    @staticmethod
    def check_all(operation: str, status: Dict[str, Any]) -> Tuple[bool, List[str]]:
        """
        检查操作的所有先决条件
        
        Args:
            operation: 操作名称（如 "Auto_OneKeyVacuumStart"）
            status: 系统状态字典
        
        Returns:
            (是否全部满足, 未满足的条件描述列表)
        """
        prereqs = OPERATION_PREREQUISITES.get(operation, [])
        if not prereqs:
            return True, []
        
        failed = []
        for prereq in prereqs:
            check_name = prereq['check']
            description = prereq['description']
            if not PrerequisiteChecker.check(check_name, status):
                failed.append(description)
        
        return len(failed) == 0, failed
    
    @staticmethod
    def check_all_with_details(operation: str, status: Dict[str, Any]) -> List[Dict[str, Any]]:
        """
        检查操作的所有先决条件，并返回详细信息
        
        Args:
            operation: 操作名称
            status: 系统状态字典
        
        Returns:
            先决条件结果列表，每项包含 description, is_met, current_value, expected_value
        """
        prereqs = OPERATION_PREREQUISITES.get(operation, [])
        results = []
        
        for prereq in prereqs:
            check_name = prereq['check']
            description = prereq['description']
            is_met, current_val, expected_val = PrerequisiteChecker.check_with_details(check_name, status)
            results.append({
                'description': description,
                'is_met': is_met,
                'current_value': current_val,
                'expected_value': expected_val
            })
        
        return results

