"""
真空系统 GUI 配置文件

设备名: sys/vacuum/2
"""

# =============================================================================
# Tango 设备配置
# =============================================================================

VACUUM_SYSTEM_DEVICE = "sys/vacuum/2"

# =============================================================================
# 操作模式
# =============================================================================

class OperationMode:
    AUTO = 0
    MANUAL = 1
    
    @staticmethod
    def to_string(mode):
        return "自动模式" if mode == OperationMode.AUTO else "手动模式"

# =============================================================================
# 系统状态
# =============================================================================

class SystemState:
    IDLE = 0
    PUMPING = 1
    STOPPING = 2
    VENTING = 3
    FAULT = 4
    EMERGENCY_STOP = 5
    
    @staticmethod
    def to_string(state):
        names = {
            0: "空闲",
            1: "抽真空中",
            2: "停机中",
            3: "放气中",
            4: "故障",
            5: "急停"
        }
        return names.get(state, "未知")

# =============================================================================
# 阀门动作状态 (用于闪烁效果)
# =============================================================================

class ValveActionState:
    IDLE = 0
    OPENING = 1
    CLOSING = 2
    OPEN_TIMEOUT = 3
    CLOSE_TIMEOUT = 4
    
    @staticmethod
    def to_string(state):
        names = {
            0: "空闲",
            1: "正在打开",
            2: "正在关闭",
            3: "开到位超时",
            4: "关到位超时"
        }
        return names.get(state, "未知")

# =============================================================================
# 闪烁颜色配置
# =============================================================================

class BlinkColors:
    """闪烁状态颜色定义"""
    
    # 正在执行操作 - 蓝色闪烁
    OPERATING_ON = "#2196F3"   # 蓝色
    OPERATING_OFF = "#1565C0"  # 深蓝色
    
    # 超时/故障 - 红色闪烁
    TIMEOUT_ON = "#F44336"     # 红色
    TIMEOUT_OFF = "#B71C1C"    # 深红色
    
    # 正常状态
    NORMAL_OPEN = "#4CAF50"    # 绿色 (已开启)
    NORMAL_CLOSE = "#9E9E9E"   # 灰色 (已关闭)
    
    # 未到位 - 黄色
    NOT_IN_POSITION = "#FFC107"

# =============================================================================
# 设备信息
# =============================================================================

# 泵类设备
PUMP_DEVICES = {
    "ScrewPump": {
        "name": "螺杆泵",
        "power_attr": "screwPumpPower",
        "power_cmd": "SetScrewPumpPower",
        "start_stop_attr": None,  # 使用 SetScrewPumpStartStop
        "start_stop_cmd": "SetScrewPumpStartStop"
    },
    "RootsPump": {
        "name": "罗茨泵",
        "power_attr": "rootsPumpPower",
        "power_cmd": "SetRootsPumpPower",
        "start_stop_attr": None,
        "start_stop_cmd": None
    },
    "MolecularPump1": {
        "name": "分子泵1",
        "power_attr": "molecularPump1Power",
        "speed_attr": "molecularPump1Speed",
        "index": 1
    },
    "MolecularPump2": {
        "name": "分子泵2",
        "power_attr": "molecularPump2Power",
        "speed_attr": "molecularPump2Speed",
        "index": 2
    },
    "MolecularPump3": {
        "name": "分子泵3",
        "power_attr": "molecularPump3Power",
        "speed_attr": "molecularPump3Speed",
        "index": 3
    }
}

# 闸板阀
GATE_VALVES = {
    "GateValve1": {
        "name": "闸板阀1",
        "open_attr": "gateValve1Open",
        "close_attr": "gateValve1Close",
        "action_state_attr": "gateValve1ActionState",
        "index": 1
    },
    "GateValve2": {
        "name": "闸板阀2",
        "open_attr": "gateValve2Open",
        "close_attr": "gateValve2Close",
        "action_state_attr": "gateValve2ActionState",
        "index": 2
    },
    "GateValve3": {
        "name": "闸板阀3",
        "open_attr": "gateValve3Open",
        "close_attr": "gateValve3Close",
        "action_state_attr": "gateValve3ActionState",
        "index": 3
    },
    "GateValve4": {
        "name": "闸板阀4",
        "open_attr": "gateValve4Open",
        "close_attr": "gateValve4Close",
        "action_state_attr": "gateValve4ActionState",
        "index": 4
    },
    "GateValve5": {
        "name": "闸板阀5",
        "open_attr": "gateValve5Open",
        "close_attr": "gateValve5Close",
        "action_state_attr": "gateValve5ActionState",
        "index": 5
    }
}

# 电磁阀
ELECTROMAGNETIC_VALVES = {
    "ElectromagneticValve1": {
        "name": "电磁阀1",
        "open_attr": "electromagneticValve1Open",
        "close_attr": "electromagneticValve1Close",
        "index": 1
    },
    "ElectromagneticValve2": {
        "name": "电磁阀2",
        "open_attr": "electromagneticValve2Open",
        "close_attr": "electromagneticValve2Close",
        "index": 2
    },
    "ElectromagneticValve3": {
        "name": "电磁阀3",
        "open_attr": "electromagneticValve3Open",
        "close_attr": "electromagneticValve3Close",
        "index": 3
    },
    "ElectromagneticValve4": {
        "name": "电磁阀4",
        "open_attr": "electromagneticValve4Open",
        "close_attr": "electromagneticValve4Close",
        "index": 4
    }
}

# 放气阀
VENT_VALVES = {
    "VentValve1": {
        "name": "放气阀1",
        "open_attr": "ventValve1Open",
        "close_attr": "ventValve1Close",
        "index": 1
    },
    "VentValve2": {
        "name": "放气阀2",
        "open_attr": "ventValve2Open",
        "close_attr": "ventValve2Close",
        "index": 2
    }
}

# 传感器
SENSORS = {
    "VacuumGauge1": {
        "name": "前级真空计",
        "attr": "vacuumGauge1",
        "unit": "Pa"
    },
    "VacuumGauge2": {
        "name": "真空计1",
        "attr": "vacuumGauge2",
        "unit": "Pa"
    },
    "VacuumGauge3": {
        "name": "真空计2",
        "attr": "vacuumGauge3",
        "unit": "Pa"
    },
    "AirPressure": {
        "name": "气源压力",
        "attr": "airPressure",
        "unit": "MPa"
    }
}

# =============================================================================
# 水路配置
# 注意: Device Server 没有 waterFlow1-4 和 waterFlowOk 属性
# 水路状态应该从 waterValve1-4State (水电磁阀状态) 推导
# =============================================================================

WATER_CHANNELS = {
    "WaterChannel1": {
        "name": "冷却水路1",
        "valve_attr": "waterValve1State",  # 水电磁阀1状态
        "description": "螺杆泵冷却水"
    },
    "WaterChannel2": {
        "name": "冷却水路2",
        "valve_attr": "waterValve2State",  # 水电磁阀2状态
        "description": "分子泵1冷却水"
    },
    "WaterChannel3": {
        "name": "冷却水路3",
        "valve_attr": "waterValve3State",  # 水电磁阀3状态
        "description": "分子泵2冷却水"
    },
    "WaterChannel4": {
        "name": "冷却水路4",
        "valve_attr": "waterValve4State",  # 水电磁阀4状态
        "description": "分子泵3冷却水"
    }
}

# 水路总体状态 - 从水电磁阀状态推导
# 注意: Device Server 没有 waterFlowOk 属性
WATER_STATUS = {
    "name": "水路状态",
    "valve_attrs": ["waterValve1State", "waterValve2State", "waterValve3State", "waterValve4State"],
    "description": "水电磁阀1-4全部开启表示水路正常"
}

# 水电磁阀配置
WATER_VALVES = {
    "WaterValve1": {
        "name": "水电磁阀1",
        "attr": "waterValve1State",
        "description": "螺杆泵冷却水阀"
    },
    "WaterValve2": {
        "name": "水电磁阀2",
        "attr": "waterValve2State",
        "description": "罗茨泵冷却水阀"
    },
    "WaterValve3": {
        "name": "水电磁阀3",
        "attr": "waterValve3State",
        "description": "分子泵组冷却水阀"
    },
    "WaterValve4": {
        "name": "水电磁阀4",
        "attr": "waterValve4State",
        "description": "备用冷却水阀"
    },
    "WaterValve5": {
        "name": "水电磁阀5",
        "attr": "waterValve5State",
        "description": "扩展冷却水阀1"
    },
    "WaterValve6": {
        "name": "水电磁阀6",
        "attr": "waterValve6State",
        "description": "扩展冷却水阀2"
    }
}

# 气主电磁阀配置
AIR_MAIN_VALVE = {
    "name": "气主电磁阀",
    "attr": "airMainValveState",
    "description": "主气源控制阀"
}

# =============================================================================
# 气路配置
# 注意: Device Server 没有 airSupplyOk 属性
# 气路状态应该从 airPressure >= 0.4 MPa 推导
# =============================================================================

AIR_SUPPLY = {
    "name": "气路状态",
    "pressure_attr": "airPressure",
    "min_pressure": 0.4,  # MPa
    "unit": "MPa",
    "description": "气源压力 >= 0.4 MPa 表示气路正常"
}

# =============================================================================
# 系统联锁状态配置
# =============================================================================

SYSTEM_INTERLOCKS = {
    "phaseSequenceOk": {
        "name": "相序保护",
        "attr": "phaseSequenceOk",
        "description": "三相电源相序正常",
        "critical": True
    },
    "motionSystemOnline": {
        "name": "运动系统联机",
        "attr": "motionSystemOnline",
        "description": "运动控制系统在线状态",
        "critical": True
    },
    "gateValve5Permit": {
        "name": "GV5动作许可",
        "attr": "gateValve5Permit",
        "description": "闸板阀5动作许可信号",
        "critical": False
    }
}

# 螺杆泵状态扩展
SCREW_PUMP_EXTENDED = {
    "frequency_attr": "screwPumpFrequency",
    "target_frequency": 110,  # Hz
    "frequency_tolerance": 5  # Hz
}

# =============================================================================
# 操作先决条件配置
# =============================================================================

OPERATION_PREREQUISITES = {
    # =============================================================================
    # 自动模式（一键流程）
    # =============================================================================
    "Auto_OneKeyVacuumStart": [
        {"description": "4路水路正常（水流开关反馈有水流）", "check": "water_flow_ok"},
        {"description": "放气阀1/2均处于关闭状态", "check": "vent_valves_closed"},
        {"description": "闸板阀5处于关闭状态", "check": "gate_valve5_closed"},
        {"description": "真空计2/3读数正常", "check": "vacuum_gauges_ok"},
        {"description": "气源气压≥0.4兆帕", "check": "air_pressure_ok"},
        {"description": "无系统故障/报警（至少非故障态）", "check": "no_pump_fault"},
        {"description": "供电正常", "check": "power_ok"},
    ],
    "Auto_OneKeyVacuumStop": [
        # 停机流程基本条件：系统不能处于故障或急停状态
        # 注意：允许在空闲状态下执行（用于确保阀门关闭）
        {"description": "系统非故障/急停状态", "check": "system_not_in_fault_or_estop"},
    ],
    "Auto_ChamberVent": [
        {"description": "闸板阀1-5均处于关闭状态", "check": "all_gate_valves_closed"},
        {"description": "系统非抽真空状态", "check": "not_pumping"},
    ],
    "Auto_FaultReset": [
        {"description": "系统处于故障/急停状态或有活跃报警", "check": "system_needs_reset"},
    ],

    # =============================================================================
    # 泵控制 - 启动/停止先决条件
    # =============================================================================
    "ScrewPump_Start": [
        {"description": "4路水路正常（水流开关反馈有水流）", "check": "water_flow_ok"},
        {"description": "电磁阀4处于开启状态", "check": "electromagnetic_valve4_open"},
        {"description": "无泵体故障码（变频器无报错）", "check": "no_pump_fault"},
        {"description": "供电电源正常（接触器反馈吸合）", "check": "power_ok"}
    ],
    "ScrewPump_Stop": [
        {"description": "罗茨泵已完全关闭（0赫兹状态）", "check": "roots_pump_stopped"},
        {"description": "分子泵1-3均已关闭（转速<5Hz）", "check": "molecular_pumps_stopped"}
    ],
    "RootsPump_Start": [
        {"description": "螺杆泵已启动且运行频率≥110Hz", "check": "screw_pump_running_110hz"},
        {"description": "真空计3读数≤7000Pa", "check": "vacuum3_le_7000pa"},
        {"description": "电磁阀4处于开启状态", "check": "electromagnetic_valve4_open"},
        {"description": "无泵体故障码", "check": "no_pump_fault"},
        {"description": "供电正常", "check": "power_ok"}
    ],
    "RootsPump_Stop": [
        {"description": "分子泵1-3均已满转（518Hz稳定状态）", "check": "molecular_pumps_full_speed"},
    ],
    # 分子泵1-3启动需要对应的电磁阀和闸板阀分别开启
    "MolecularPump1_Start": [
        {"description": "螺杆泵已启动且运行正常", "check": "screw_pump_running"},
        {"description": "电磁阀1处于开启状态", "check": "electromagnetic_valve1_open"},
        {"description": "闸板阀1处于开启状态", "check": "gate_valve1_open"},
        {"description": "前级真空计读数≤45Pa", "check": "high_vacuum_ok"},
        {"description": "4路水路正常", "check": "water_flow_ok"},
        {"description": "无泵体故障码", "check": "no_pump_fault"},
        {"description": "供电正常", "check": "power_ok"}
    ],
    "MolecularPump2_Start": [
        {"description": "螺杆泵已启动且运行正常", "check": "screw_pump_running"},
        {"description": "电磁阀2处于开启状态", "check": "electromagnetic_valve2_open"},
        {"description": "闸板阀2处于开启状态", "check": "gate_valve2_open"},
        {"description": "前级真空计读数≤45Pa", "check": "high_vacuum_ok"},
        {"description": "4路水路正常", "check": "water_flow_ok"},
        {"description": "无泵体故障码", "check": "no_pump_fault"},
        {"description": "供电正常", "check": "power_ok"}
    ],
    "MolecularPump3_Start": [
        {"description": "螺杆泵已启动且运行正常", "check": "screw_pump_running"},
        {"description": "电磁阀3处于开启状态", "check": "electromagnetic_valve3_open"},
        {"description": "闸板阀3处于开启状态", "check": "gate_valve3_open"},
        {"description": "前级真空计读数≤45Pa", "check": "high_vacuum_ok"},
        {"description": "4路水路正常", "check": "water_flow_ok"},
        {"description": "无泵体故障码", "check": "no_pump_fault"},
        {"description": "供电正常", "check": "power_ok"}
    ],
    # 保留批量配置用于兼容性（要求所有对应设备都开启）
    "MolecularPump_Start": [
        {"description": "螺杆泵已启动且运行正常", "check": "screw_pump_running"},
        {"description": "电磁阀1处于开启状态", "check": "electromagnetic_valve1_open"},
        {"description": "电磁阀2处于开启状态", "check": "electromagnetic_valve2_open"},
        {"description": "电磁阀3处于开启状态", "check": "electromagnetic_valve3_open"},
        {"description": "闸板阀1处于开启状态", "check": "gate_valve1_open"},
        {"description": "闸板阀2处于开启状态", "check": "gate_valve2_open"},
        {"description": "闸板阀3处于开启状态", "check": "gate_valve3_open"},
        {"description": "前级真空计读数≤45Pa", "check": "high_vacuum_ok"},
        {"description": "4路水路正常", "check": "water_flow_ok"},
        {"description": "无泵体故障码", "check": "no_pump_fault"},
        {"description": "供电正常", "check": "power_ok"}
    ],
    "MolecularPump_Stop": [
        # 分子泵停止需要等待转速降低，无强制条件
    ],

    # =============================================================================
    # 闸板阀 - 开启/关闭先决条件
    # =============================================================================
    # 闸板阀1-3开启需要对应的电磁阀1-3分别开启，而非全部开启
    "GateValve1_Open": [
        {"description": "放气阀2处于关闭状态", "check": "vent_valve2_closed"},
        {"description": "闸板阀5处于关闭状态", "check": "gate_valve5_closed"},
        {"description": "腔室与前级管道压差<3000Pa", "check": "pressure_diff_ok"},
        {"description": "电磁阀1已开启", "check": "electromagnetic_valve1_open"},
        {"description": "气源气压≥0.4MPa", "check": "air_pressure_ok"}
    ],
    "GateValve2_Open": [
        {"description": "放气阀2处于关闭状态", "check": "vent_valve2_closed"},
        {"description": "闸板阀5处于关闭状态", "check": "gate_valve5_closed"},
        {"description": "腔室与前级管道压差<3000Pa", "check": "pressure_diff_ok"},
        {"description": "电磁阀2已开启", "check": "electromagnetic_valve2_open"},
        {"description": "气源气压≥0.4MPa", "check": "air_pressure_ok"}
    ],
    "GateValve3_Open": [
        {"description": "放气阀2处于关闭状态", "check": "vent_valve2_closed"},
        {"description": "闸板阀5处于关闭状态", "check": "gate_valve5_closed"},
        {"description": "腔室与前级管道压差<3000Pa", "check": "pressure_diff_ok"},
        {"description": "电磁阀3已开启", "check": "electromagnetic_valve3_open"},
        {"description": "气源气压≥0.4MPa", "check": "air_pressure_ok"}
    ],
    # 保留批量操作配置（用于兼容性，但实际应使用单独配置）
    "GateValve123_Open": [
        {"description": "放气阀2处于关闭状态", "check": "vent_valve2_closed"},
        {"description": "闸板阀5处于关闭状态", "check": "gate_valve5_closed"},
        {"description": "腔室与前级管道压差<3000Pa", "check": "pressure_diff_ok"},
        {"description": "电磁阀1已开启", "check": "electromagnetic_valve1_open"},
        {"description": "电磁阀2已开启", "check": "electromagnetic_valve2_open"},
        {"description": "电磁阀3已开启", "check": "electromagnetic_valve3_open"},
        {"description": "气源气压≥0.4MPa", "check": "air_pressure_ok"}
    ],
    # 闸板阀关闭需要"对应的"分子泵关闭，而非全部
    "GateValve1_Close": [
        {"description": "分子泵1已关闭", "check": "molecular_pump1_stopped"},
    ],
    "GateValve2_Close": [
        {"description": "分子泵2已关闭", "check": "molecular_pump2_stopped"},
    ],
    "GateValve3_Close": [
        {"description": "分子泵3已关闭", "check": "molecular_pump3_stopped"},
    ],
    "GateValve123_Close": [
        # 批量关闭时，需要全部对应分子泵都关闭
        {"description": "分子泵1-3均已关闭", "check": "molecular_pumps_stopped"},
    ],
    "GateValve4_Open": [
        {"description": "放气阀2处于关闭状态", "check": "vent_valve2_closed"},
        {"description": "闸板阀5处于关闭状态", "check": "gate_valve5_closed"},
        {"description": "腔室真空度<3000Pa", "check": "chamber_vacuum_ok"},
        {"description": "螺杆泵已启动且运行正常（达110Hz）", "check": "screw_pump_running_110hz"},
        {"description": "气源气压≥0.4MPa", "check": "air_pressure_ok"}
    ],
    "GateValve4_Close": [
        # 闸板阀4关闭通常无强制条件
    ],
    "GateValve5_Open": [
        {"description": "闸板阀两侧气压差<3000Pa", "check": "pressure_diff_ok"},
        {"description": "闸板阀1-4均处于关闭状态", "check": "gate_valves_1_4_closed"},
        {"description": "放气阀2处于关闭状态", "check": "vent_valve2_closed"},
        {"description": "外部大行程系统发出允许开启信号", "check": "motion_permit"},
        {"description": "气源气压≥0.4MPa", "check": "air_pressure_ok"}
    ],
    "GateValve5_Close": [
        {"description": "外部大行程系统发出允许关闭信号", "check": "motion_permit_close"},
    ],

    # =============================================================================
    # 电磁阀 - 开启/关闭先决条件
    # =============================================================================
    "ElectromagneticValve123_Open": [
        {"description": "放气阀1处于关闭状态", "check": "vent_valve1_closed"},
    ],
    # 电磁阀关闭需要"对应的"分子泵和闸板阀关闭，而非全部
    "ElectromagneticValve1_Close": [
        {"description": "分子泵1已关闭（0Hz状态）", "check": "molecular_pump1_stopped"},
        {"description": "闸板阀1处于关闭状态", "check": "gate_valve1_closed"}
    ],
    "ElectromagneticValve2_Close": [
        {"description": "分子泵2已关闭（0Hz状态）", "check": "molecular_pump2_stopped"},
        {"description": "闸板阀2处于关闭状态", "check": "gate_valve2_closed"}
    ],
    "ElectromagneticValve3_Close": [
        {"description": "分子泵3已关闭（0Hz状态）", "check": "molecular_pump3_stopped"},
        {"description": "闸板阀3处于关闭状态", "check": "gate_valve3_closed"}
    ],
    "ElectromagneticValve123_Close": [
        # 批量关闭时，需要全部对应设备都关闭
        {"description": "分子泵1-3均已关闭（0Hz状态）", "check": "molecular_pumps_stopped"},
        {"description": "闸板阀1-3均处于关闭状态", "check": "gate_valves_123_closed"}
    ],
    "ElectromagneticValve4_Open": [
        # 文档规定：无前置条件
    ],
    "ElectromagneticValve4_Close": [
        {"description": "螺杆泵已停止", "check": "screw_pump_stopped"},
        {"description": "罗茨泵已停止", "check": "roots_pump_stopped"},
        {"description": "分子泵1-3均已停止", "check": "molecular_pumps_stopped"}
    ],

    # =============================================================================
    # 放气阀 - 开启/关闭先决条件
    # =============================================================================
    "VentValve1_Open": [
        {"description": "闸板阀1-4均处于关闭状态", "check": "gate_valves_1_4_closed"},
    ],
    "VentValve1_Close": [
        {"description": "前级管道已放气至大气状态（真空计3≥80000Pa）", "check": "vacuum3_ge_80000pa"},
    ],
    "VentValve2_Open": [
        {"description": "闸板阀1-5均处于关闭状态", "check": "all_gate_valves_closed"},
    ],
    "VentValve2_Close": [
        {"description": "腔室已放气至大气状态（真空计1/2≥80000Pa）", "check": "chamber_at_atmosphere"},
    ]
}

# =============================================================================
# 自动流程分子泵配置
# =============================================================================

AUTO_MOLECULAR_PUMP_CONFIG = {
    "molecularPump1Enabled": True,  # 分子泵1是否参与自动流程
    "molecularPump2Enabled": True,  # 分子泵2是否参与自动流程
    "molecularPump3Enabled": True,  # 分子泵3是否参与自动流程
}

# =============================================================================
# 自动流程分支判据
# 注意: Device Server 的 molecularPumpXSpeed 属性返回的是 RPM 单位
# 518 Hz ≈ 31080 RPM (满转), 5 Hz ≈ 300 RPM (停止)
# =============================================================================

AUTO_BRANCH_THRESHOLDS = {
    "粗抽分支阈值Pa": 3000,  # ≥3000Pa 走粗抽分支（开闸板阀4）
    "精抽分支阈值Pa": 3000,  # <3000Pa 走精抽分支（直接开闸板阀1-3）
    "罗茨泵启动阈值Pa": 80000,  # 前级真空≤80000Pa 可启动罗茨泵
    "分子泵关阀转速RPM": 300,  # 分子泵转速<300RPM (约5Hz) 可关闭电磁阀
    "分子泵满转转速RPM": 31000,  # 分子泵满转 ≈518Hz ≈31080RPM
}

# =============================================================================
# 自动流程步骤描述
# =============================================================================

AUTO_SEQUENCE_STEPS = {
    # 抽真空中 (SystemState.PUMPING = 1)
    1: {
        0: "准备中",
        # 非真空流程 (步骤 1-10)
        1: "步骤1: 开启电磁阀4",
        2: "步骤2: 开启电磁阀1-3",
        3: "步骤3: 等待电磁阀1-3到位",
        4: "步骤4: 开启闸板阀1-3",
        5: "步骤5: 等待螺杆泵达110Hz",
        6: "步骤6: 等待真空度 < 7000Pa",
        7: "步骤7: 等待真空度 < 45Pa",
        8: "步骤8: 等待分子泵满转",
        9: "步骤9: 延时关闭罗茨泵",
        10: "抽真空流程完成",
        
        # 低真空流程 (步骤 100-114)
        100: "步骤100: 开启电磁阀4",
        101: "步骤101: 开放气阀1",
        102: "步骤102: 等待平衡至大气压",
        103: "步骤103: 启动螺杆泵",
        104: "步骤104: 等待螺杆泵达110Hz",
        105: "步骤105: 等待真空度 < 7000Pa",
        106: "步骤106: 等待真空度 < 3000Pa",
        107: "步骤107: 等待闸板阀4开启",
        108: "步骤108: 开启电磁阀1-3",
        109: "步骤109: 开启闸板阀1-3",
        110: "步骤110: 关闭闸板阀4",
        111: "步骤111: 等待真空度 < 45Pa",
        112: "步骤112: 等待分子泵满转",
        113: "步骤113: 延时关闭罗茨泵",
        114: "抽真空流程完成",
    },
    # 停机中 (SystemState.STOPPING = 2)
    2: {
        1: "步骤1: 停止分子泵1-3",
        2: "步骤2: 等待分子泵停止",
        3: "步骤3: 关闭闸板阀1-3",
        4: "步骤4: 关闭电磁阀1-3",
        5: "步骤5: 停止罗茨泵",
        6: "步骤6: 停止螺杆泵并关闭电磁阀4",
        0: "停机流程完成"
    },
    # 放气中 (SystemState.VENTING = 3)
    3: {
        1: "步骤1: 检查闸板阀并开启放气阀2",
        2: "步骤2: 等待压力达到大气压",
        0: "放气流程完成"
    }
}

# =============================================================================
# 报警类型
# =============================================================================

ALARM_TYPES = {
    # 阀开到位异常
    1: "闸板阀1开到位超时",
    2: "闸板阀2开到位超时",
    3: "闸板阀3开到位超时",
    4: "闸板阀4开到位超时",
    5: "闸板阀5开到位超时",
    6: "电磁阀1开到位超时",
    7: "电磁阀2开到位超时",
    8: "电磁阀3开到位超时",
    9: "电磁阀4开到位超时",
    10: "放气阀1开到位超时",
    11: "放气阀2开到位超时",
    
    # 阀关到位异常
    20: "闸板阀1关到位超时",
    21: "闸板阀2关到位超时",
    22: "闸板阀3关到位超时",
    23: "闸板阀4关到位超时",
    24: "闸板阀5关到位超时",
    25: "电磁阀1关到位超时",
    26: "电磁阀2关到位超时",
    27: "电磁阀3关到位超时",
    28: "电磁阀4关到位超时",
    29: "放气阀1关到位超时",
    30: "放气阀2关到位超时",
    
    # 泵故障
    40: "螺杆泵故障",
    41: "罗茨泵故障",
    42: "分子泵1故障",
    43: "分子泵2故障",
    44: "分子泵3故障",
    
    # 电源异常
    50: "电源1异常",
    51: "电源2异常",
    52: "电源3异常",
    53: "电源4异常",
    
    # 其他
    70: "气源压力不足",
    71: "真空计1读数异常",
    72: "真空计2读数异常",
    73: "真空计3读数异常",
    74: "主电源相序异常"
}

# =============================================================================
# 报警文件路径
# =============================================================================

ALARM_LOG_FILE = "logs/vacuum_system_alarms.json"
HISTORY_LOG_FILE = "logs/vacuum_system_history.json"

# =============================================================================
# UI 配置
# =============================================================================

UI_CONFIG = {
    "poll_interval_ms": 300,       # 轮询间隔
    "blink_interval_ms": 500,      # 闪烁间隔
    "valve_timeout_s": 5,          # 阀门超时时间
    "window_min_width": 1280,
    "window_min_height": 720
}

# =============================================================================
# 趋势曲线配置
# =============================================================================

TREND_CONFIG = {
    "max_points": 1000,            # 最大数据点数
    "update_interval_ms": 1000,    # 更新间隔
    "channels": [
        {"name": "真空计1", "attr": "vacuumGauge1", "color": "#F44336"},
        {"name": "真空计2", "attr": "vacuumGauge2", "color": "#2196F3"},
        {"name": "真空计3", "attr": "vacuumGauge3", "color": "#4CAF50"},
        {"name": "气源压力", "attr": "airPressure", "color": "#FF9800"}
    ]
}

