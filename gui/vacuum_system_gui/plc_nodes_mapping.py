"""
PLC OPC UA 节点 ID 映射配置

定义真空系统PLC中所有变量的OPC UA节点ID映射
节点ID格式: ns=4;s=|var|CODESYS Control Win V3 x64.Application.GVL.gVacuumSystem.变量名
"""

# OPC UA 命名空间和路径前缀
NAMESPACE_PREFIX = "ns=4;s=|var|CODESYS Control Win V3 x64.Application.GVL.gVacuumSystem"


def get_node_id(variable_name: str) -> str:
    """
    获取变量的完整节点ID
    
    Args:
        variable_name: 变量名称
        
    Returns:
        完整的OPC UA节点ID
    """
    return f"{NAMESPACE_PREFIX}.{variable_name}"


# =============================================================================
# 输入信号节点 (来自PLC的反馈信号)
# =============================================================================

# 泵运行状态反馈
SCREW_PUMP_POWER = get_node_id("bScrewPumpPower")
ROOTS_PUMP_POWER = get_node_id("bRootsPumpPower")
MOLECULAR_PUMP1_POWER = get_node_id("bMolecularPump1Power")
MOLECULAR_PUMP2_POWER = get_node_id("bMolecularPump2Power")
MOLECULAR_PUMP3_POWER = get_node_id("bMolecularPump3Power")

# 泵频率/转速反馈
SCREW_PUMP_FREQUENCY = get_node_id("iScrewPumpFrequency")
ROOTS_PUMP_FREQUENCY = get_node_id("iRootsPumpFrequency")
MOLECULAR_PUMP1_SPEED = get_node_id("iMolecularPump1Speed")
MOLECULAR_PUMP2_SPEED = get_node_id("iMolecularPump2Speed")
MOLECULAR_PUMP3_SPEED = get_node_id("iMolecularPump3Speed")

# 闸板阀状态反馈
GATE_VALVE1_OPEN = get_node_id("bGateValve1Open")
GATE_VALVE1_CLOSE = get_node_id("bGateValve1Close")
GATE_VALVE2_OPEN = get_node_id("bGateValve2Open")
GATE_VALVE2_CLOSE = get_node_id("bGateValve2Close")
GATE_VALVE3_OPEN = get_node_id("bGateValve3Open")
GATE_VALVE3_CLOSE = get_node_id("bGateValve3Close")
GATE_VALVE4_OPEN = get_node_id("bGateValve4Open")
GATE_VALVE4_CLOSE = get_node_id("bGateValve4Close")
GATE_VALVE5_OPEN = get_node_id("bGateValve5Open")
GATE_VALVE5_CLOSE = get_node_id("bGateValve5Close")

# 电磁阀状态反馈
EM_VALVE1_OPEN = get_node_id("bElectromagneticValve1Open")
EM_VALVE1_CLOSE = get_node_id("bElectromagneticValve1Close")
EM_VALVE2_OPEN = get_node_id("bElectromagneticValve2Open")
EM_VALVE2_CLOSE = get_node_id("bElectromagneticValve2Close")
EM_VALVE3_OPEN = get_node_id("bElectromagneticValve3Open")
EM_VALVE3_CLOSE = get_node_id("bElectromagneticValve3Close")
EM_VALVE4_OPEN = get_node_id("bElectromagneticValve4Open")
EM_VALVE4_CLOSE = get_node_id("bElectromagneticValve4Close")

# 放气阀状态反馈
VENT_VALVE1_OPEN = get_node_id("bVentValve1Open")
VENT_VALVE1_CLOSE = get_node_id("bVentValve1Close")
VENT_VALVE2_OPEN = get_node_id("bVentValve2Open")
VENT_VALVE2_CLOSE = get_node_id("bVentValve2Close")

# 真空规反馈
VACUUM_GAUGE1 = get_node_id("rVacuumGauge1")
VACUUM_GAUGE2 = get_node_id("rVacuumGauge2")
VACUUM_GAUGE3 = get_node_id("rVacuumGauge3")

# 压力传感器反馈
AIR_PRESSURE = get_node_id("rAirPressure")
WATER_PRESSURE = get_node_id("rWaterPressure")

# 水电磁阀状态反馈
WATER_VALVE1_STATE = get_node_id("bWaterValve1State")
WATER_VALVE2_STATE = get_node_id("bWaterValve2State")
WATER_VALVE3_STATE = get_node_id("bWaterValve3State")
WATER_VALVE4_STATE = get_node_id("bWaterValve4State")
WATER_VALVE5_STATE = get_node_id("bWaterValve5State")
WATER_VALVE6_STATE = get_node_id("bWaterValve6State")
AIR_MAIN_VALVE_STATE = get_node_id("bAirMainValveState")

# 系统联锁信号
PHASE_SEQUENCE_OK = get_node_id("bPhaseSequenceOk")
MOTION_SYSTEM_ONLINE = get_node_id("bMotionSystemOnline")
GATE_VALVE5_PERMIT = get_node_id("bGateValve5Permit")

# 操作模式反馈
AUTO_STATE = get_node_id("bAutoState")
MANUAL_STATE = get_node_id("bManualState")
LOCAL_STATE = get_node_id("bLocalState")
REMOTE_STATE = get_node_id("bRemoteState")

# 系统状态反馈
SYSTEM_STATE = get_node_id("iSystemState")  # 0=Idle, 1=Pumping, 2=Stopping, 3=Venting, 4=Fault, 5=EmergencyStop
AUTO_SEQUENCE_STEP = get_node_id("iAutoSequenceStep")

# =============================================================================
# 输出信号节点 (发送给PLC的控制信号)
# =============================================================================

# 泵电源控制
CMD_SCREW_PUMP_POWER = get_node_id("bCmdScrewPumpPower")
CMD_ROOTS_PUMP_POWER = get_node_id("bCmdRootsPumpPower")
CMD_MOLECULAR_PUMP1_POWER = get_node_id("bCmdMolecularPump1Power")
CMD_MOLECULAR_PUMP2_POWER = get_node_id("bCmdMolecularPump2Power")
CMD_MOLECULAR_PUMP3_POWER = get_node_id("bCmdMolecularPump3Power")

# 泵启停控制（上电后的启动/停止）
CMD_SCREW_PUMP_START_STOP = get_node_id("bCmdScrewPumpStartStop")
CMD_MOLECULAR_PUMP1_START_STOP = get_node_id("bCmdMolecularPump1StartStop")
CMD_MOLECULAR_PUMP2_START_STOP = get_node_id("bCmdMolecularPump2StartStop")
CMD_MOLECULAR_PUMP3_START_STOP = get_node_id("bCmdMolecularPump3StartStop")

# 闸板阀控制
CMD_GATE_VALVE1_OPEN = get_node_id("bCmdGateValve1Open")
CMD_GATE_VALVE1_CLOSE = get_node_id("bCmdGateValve1Close")
CMD_GATE_VALVE2_OPEN = get_node_id("bCmdGateValve2Open")
CMD_GATE_VALVE2_CLOSE = get_node_id("bCmdGateValve2Close")
CMD_GATE_VALVE3_OPEN = get_node_id("bCmdGateValve3Open")
CMD_GATE_VALVE3_CLOSE = get_node_id("bCmdGateValve3Close")
CMD_GATE_VALVE4_OPEN = get_node_id("bCmdGateValve4Open")
CMD_GATE_VALVE4_CLOSE = get_node_id("bCmdGateValve4Close")
CMD_GATE_VALVE5_OPEN = get_node_id("bCmdGateValve5Open")
CMD_GATE_VALVE5_CLOSE = get_node_id("bCmdGateValve5Close")

# 电磁阀控制
CMD_EM_VALVE1 = get_node_id("bCmdElectromagneticValve1")
CMD_EM_VALVE2 = get_node_id("bCmdElectromagneticValve2")
CMD_EM_VALVE3 = get_node_id("bCmdElectromagneticValve3")
CMD_EM_VALVE4 = get_node_id("bCmdElectromagneticValve4")

# 放气阀控制
CMD_VENT_VALVE1 = get_node_id("bCmdVentValve1")
CMD_VENT_VALVE2 = get_node_id("bCmdVentValve2")

# 水电磁阀控制
CMD_WATER_VALVE1 = get_node_id("bCmdWaterValve1")
CMD_WATER_VALVE2 = get_node_id("bCmdWaterValve2")
CMD_WATER_VALVE3 = get_node_id("bCmdWaterValve3")
CMD_WATER_VALVE4 = get_node_id("bCmdWaterValve4")
CMD_WATER_VALVE5 = get_node_id("bCmdWaterValve5")
CMD_WATER_VALVE6 = get_node_id("bCmdWaterValve6")
CMD_AIR_MAIN_VALVE = get_node_id("bCmdAirMainValve")

# 系统控制命令
CMD_SWITCH_TO_AUTO = get_node_id("bCmdSwitchToAuto")
CMD_SWITCH_TO_MANUAL = get_node_id("bCmdSwitchToManual")
CMD_ONE_KEY_VACUUM_START = get_node_id("bCmdOneKeyVacuumStart")
CMD_ONE_KEY_VACUUM_STOP = get_node_id("bCmdOneKeyVacuumStop")
CMD_CHAMBER_VENT = get_node_id("bCmdChamberVent")
CMD_FAULT_RESET = get_node_id("bCmdFaultReset")
CMD_EMERGENCY_STOP = get_node_id("bCmdEmergencyStop")

# 分子泵启用配置（持久化配置）
CFG_MOLECULAR_PUMP1_ENABLED = get_node_id("bCfgMolecularPump1Enabled")
CFG_MOLECULAR_PUMP2_ENABLED = get_node_id("bCfgMolecularPump2Enabled")
CFG_MOLECULAR_PUMP3_ENABLED = get_node_id("bCfgMolecularPump3Enabled")

# =============================================================================
# 报警相关节点
# =============================================================================

ACTIVE_ALARM_COUNT = get_node_id("iActiveAlarmCount")
HAS_UNACKNOWLEDGED_ALARM = get_node_id("bHasUnacknowledgedAlarm")
LATEST_ALARM_CODE = get_node_id("iLatestAlarmCode")
LATEST_ALARM_TYPE = get_node_id("iLatestAlarmType")
LATEST_ALARM_DESCRIPTION = get_node_id("sLatestAlarmDescription")

CMD_ACKNOWLEDGE_ALARM = get_node_id("bCmdAcknowledgeAlarm")
CMD_ALARM_CODE_TO_ACK = get_node_id("iCmdAlarmCodeToAck")
