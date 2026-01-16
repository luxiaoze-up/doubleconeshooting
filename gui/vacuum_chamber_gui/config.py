"""
真空腔体系统控制 GUI - 配置文件
Configuration for Vacuum Chamber System Control GUI
"""

# =============================================================================
# 设备名称配置 (必须与 Tango 数据库注册一致)
# =============================================================================

DEVICES = {
    # 大行程运动（靶定位相关）
    "large_stroke": "sys/large_stroke/1",
    
    # 六自由度调整（靶定位相关）
    "six_dof": "sys/six_dof/1",
    
    # 运动控制器
    "motion_controller_1": "sys/motion/1",
    "motion_controller_2": "sys/motion/2",
    "motion_controller_3": "sys/motion/3",
    
    # 编码器
    "encoder": "sys/encoder/1",
    
    # 辅助支撑设备（5个实例，根据测试数据文档）
    "auxiliary_1": "sys/auxiliary/1",  # M14, AXIS-0, 编码器通道4
    "auxiliary_2": "sys/auxiliary/2",  # M15, AXIS-1, 编码器通道5
    "auxiliary_3": "sys/auxiliary/3",  # M16, AXIS-2, 编码器通道6
    "auxiliary_4": "sys/auxiliary/4",  # M17, AXIS-3, 编码器通道7
    "auxiliary_5": "sys/auxiliary/5",  # M18, AXIS-4, 编码器通道8
    
    # 反射光成像
    "reflection": "sys/reflection/1",
    
    # 真空系统
    "vacuum": "sys/vacuum/1",
    
    # 联锁服务
    "interlock": "sys/interlock/1",
}

# =============================================================================
# 导航结构配置
# =============================================================================

NAVIGATION = [
    {
        "id": "target_positioning",
        "name": "靶定位",
        "icon": "🎯",
        "description": "大行程运动与六自由度精密调整",
    },
    {
        "id": "reflection_imaging",
        "name": "反射光成像",
        "icon": "📷",
        "description": "CCD相机图像采集与显示",
    },
    {
        "id": "auxiliary_support",
        "name": "辅助支撑",
        "icon": "🔧",
        "description": "五组辅助支撑控制",
    },
    {
        "id": "vacuum_control",
        "name": "真空抽气控制",
        "icon": "🌀",
        "description": "真空系统抽气与压力控制",
    },
]

# =============================================================================
# UI设置
# =============================================================================

UI_SETTINGS = {
    "window_title": "打靶控制系统",
    "window_size": (1600, 1000),
    "sidebar_width": 180,
    "status_panel_width": 420,  # 默认右侧状态面板宽度（可被各页面覆盖）
    # 各页面可单独设置右侧状态面板宽度
    # key 建议与 pages/*.py 中的 page_key 保持一致
    "status_panel_widths": {
        # 电机状态表尽量横向完整显示：默认给更宽，仍可拖动分割条微调
        "auxiliary_support": 460,
        "reflection_imaging": 460,
        "target_positioning": 540,
    },
    "refresh_interval_ms": 500,  # 状态刷新间隔
    "enable_high_dpi": True,
}

# =============================================================================
# 靶定位参数配置
# =============================================================================

TARGET_POSITIONING_CONFIG = {
    # 大行程轴配置
    "large_stroke_axes": [
        {"id": "X", "name": "X轴", "unit": "mm", "range": (-500, 500), "precision": 0.01},
        {"id": "Y", "name": "Y轴", "unit": "mm", "range": (-500, 500), "precision": 0.01},
        {"id": "Z", "name": "Z轴", "unit": "mm", "range": (-200, 200), "precision": 0.01},
    ],
    # 六自由度配置
    "six_dof_axes": [
        {"id": "X", "name": "X", "unit": "mm", "range": (-10, 10), "precision": 0.001},
        {"id": "Y", "name": "Y", "unit": "mm", "range": (-10, 10), "precision": 0.001},
        {"id": "Z", "name": "Z", "unit": "mm", "range": (-5, 5), "precision": 0.001},
        {"id": "RX", "name": "Xθ", "unit": "°", "range": (-5, 5), "precision": 0.001},
        {"id": "RY", "name": "Yθ", "unit": "°", "range": (-5, 5), "precision": 0.001},
        {"id": "RZ", "name": "Zθ", "unit": "°", "range": (-5, 5), "precision": 0.001},
    ],
    # 状态面板电机配置
    # 六自由度电机：每个电机独立控制（有输入框和控制按钮）
    # 大行程电机：只显示状态，不提供控制（readonly=True）
    "status_motors": [
        {"id": "motor_1", "name": "电机1", "device": "six_dof", "axis": 0},
        {"id": "motor_2", "name": "电机2", "device": "six_dof", "axis": 1},
        {"id": "motor_3", "name": "电机3", "device": "six_dof", "axis": 2},
        {"id": "motor_4", "name": "电机4", "device": "six_dof", "axis": 3},
        {"id": "motor_5", "name": "电机5", "device": "six_dof", "axis": 4},
        {"id": "motor_6", "name": "电机6", "device": "six_dof", "axis": 5},
        {"id": "motor_large", "name": "行程电机", "device": "large_stroke", "axis": 0, "readonly": True},
    ]
}

# =============================================================================
# 辅助支撑配置
# =============================================================================

AUXILIARY_SUPPORT_CONFIG = {
    "groups": [
        {"id": "1", "name": "辅助支撑设备1 (M14)", "device": DEVICES["auxiliary_1"]},
        {"id": "2", "name": "辅助支撑设备2 (M15)", "device": DEVICES["auxiliary_2"]},
        {"id": "3", "name": "辅助支撑设备3 (M16)", "device": DEVICES["auxiliary_3"]},
        {"id": "4", "name": "辅助支撑设备4 (M17)", "device": DEVICES["auxiliary_4"]},
        {"id": "5", "name": "辅助支撑设备5 (M18)", "device": DEVICES["auxiliary_5"]},
    ],
    "common_operations": [
        {"id": "hold", "name": "夹持", "command": "setHoldPos"},
        {"id": "release", "name": "释放", "command": "release"},
        {"id": "move_up", "name": "上移", "command": "moveUp"},
        {"id": "move_down", "name": "下移", "command": "moveDown"},
    ],
    # 状态面板电机配置 (辅助支撑)
    "status_motors": [
        {"id": "auxiliary_1", "name": "辅助支撑1 (M14)", "device": "auxiliary_1", "axis": 0},
        {"id": "auxiliary_2", "name": "辅助支撑2 (M15)", "device": "auxiliary_2", "axis": 0},
        {"id": "auxiliary_3", "name": "辅助支撑3 (M16)", "device": "auxiliary_3", "axis": 0},
        {"id": "auxiliary_4", "name": "辅助支撑4 (M17)", "device": "auxiliary_4", "axis": 0},
        {"id": "auxiliary_5", "name": "辅助支撑5 (M18)", "device": "auxiliary_5", "axis": 0},
    ],
}

# =============================================================================
# 真空系统配置
# =============================================================================

VACUUM_CONTROL_CONFIG = {
    # 1. 真空规配置
    "gauges": [
        {"id": "gauge_fore", "name": "前级电阻规", "attr": "vacuumGauge1", "unit": "Pa"},
        {"id": "gauge_main1", "name": "主真空计1", "attr": "vacuumGauge2", "unit": "Pa"},
        {"id": "gauge_main2", "name": "主真空计2", "attr": "vacuumGauge3", "unit": "Pa"},
    ],
    
    # 2. 系统控制按钮
    "system_controls": [
        {"id": "one_key_start", "name": "一键抽真空", "command": {"name": "oneKeyVacuumStart", "args": []}, "role": "primary"},
        {"id": "one_key_stop", "name": "一键停机", "command": {"name": "oneKeyVacuumStop", "args": []}, "role": "stop"},
        {"id": "vent_start", "name": "放气启动", "command": {"name": "ventStart", "args": []}, "role": "warning"},
        {"id": "fault_reset", "name": "故障复位", "command": {"name": "reset", "args": []}, "role": "secondary"},
        {"id": "auto_mode", "name": "自动模式", "command": {"name": "switchMode", "args": [0]}, "role": "primary"},
        {"id": "manual_mode", "name": "手动模式", "command": {"name": "switchMode", "args": [1]}, "role": "secondary"},
    ],
    
    # 3. 前级泵阀组
    "foreline_group": {
        "name": "前级泵阀组",
        "devices": [
            {
                "id": "roots_pump", "name": "罗茨泵", "type": "pump", 
                "attr_state": "rootsPumpPower", 
                # "attr_freq": "rootsPumpSpeed", # Server端暂无此属性
                "cmd_start": {"name": "setRootsPumpPower", "args": [True]}, 
                "cmd_stop": {"name": "setRootsPumpPower", "args": [False]}
            },
            {
                "id": "screw_pump", "name": "螺杆泵", "type": "pump_with_water", 
                "attr_state": "screwPumpPower", 
                "attr_water": "screwPumpWaterFault", 
                "attr_freq": "screwPumpSpeed", 
                "cmd_start": {"name": "setScrewPumpPower", "args": [True]}, 
                "cmd_stop": {"name": "setScrewPumpPower", "args": [False]}
            },
            {
                "id": "valve_tail", "name": "尾气电磁阀", "type": "valve", 
                "attr_state": "electromagneticValve4Open",
                "cmd_open": {"name": "setElectromagneticValve", "args": [4, 1]}, 
                "cmd_close": {"name": "setElectromagneticValve", "args": [4, 0]}
            },
            {
                "id": "valve_rough", "name": "粗抽闸板阀", "type": "valve", 
                "attr_state": "gateValve4Open", 
                "cmd_open": {"name": "setGateValve", "args": [4, 1]}, 
                "cmd_close": {"name": "setGateValve", "args": [4, 0]}
            },
        ]
    },
    
    # 4. 分子泵阀组 (3组)
    "molecular_groups": [
        {
            "id": "mol_group_1", "name": "分子泵阀组一",
            "devices": [
                {
                    "id": "valve_gate_1", "name": "闸板阀1", "type": "valve", 
                    "attr_state": "gateValve1Open",
                    "cmd_open": {"name": "setGateValve", "args": [1, 1]},
                    "cmd_close": {"name": "setGateValve", "args": [1, 0]}
                },
                {
                    "id": "mol_pump_1", "name": "分子泵1", "type": "pump_with_water", 
                    "attr_state": "molecularPump1Power", 
                    "attr_water": "molecularPump1WaterFault", 
                    "attr_freq": "molecularPump1Speed",
                    "cmd_start": {"name": "setMolecularPumpStartStop", "args": [1, 1]},
                    "cmd_stop": {"name": "setMolecularPumpStartStop", "args": [1, 0]}
                },
                {
                    "id": "valve_mag_1", "name": "电磁阀1", "type": "valve", 
                    "attr_state": "electromagneticValve1Open",
                    "cmd_open": {"name": "setElectromagneticValve", "args": [1, 1]},
                    "cmd_close": {"name": "setElectromagneticValve", "args": [1, 0]}
                },
            ]
        },
        {
            "id": "mol_group_2", "name": "分子泵阀组二",
            "devices": [
                {
                    "id": "valve_gate_2", "name": "闸板阀2", "type": "valve", 
                    "attr_state": "gateValve2Open",
                    "cmd_open": {"name": "setGateValve", "args": [2, 1]},
                    "cmd_close": {"name": "setGateValve", "args": [2, 0]}
                },
                {
                    "id": "mol_pump_2", "name": "分子泵2", "type": "pump_with_water", 
                    "attr_state": "molecularPump2Power", 
                    "attr_water": "molecularPump2WaterFault", 
                    "attr_freq": "molecularPump2Speed",
                    "cmd_start": {"name": "setMolecularPumpStartStop", "args": [2, 1]},
                    "cmd_stop": {"name": "setMolecularPumpStartStop", "args": [2, 0]}
                },
                {
                    "id": "valve_mag_2", "name": "电磁阀2", "type": "valve", 
                    "attr_state": "electromagneticValve2Open",
                    "cmd_open": {"name": "setElectromagneticValve", "args": [2, 1]},
                    "cmd_close": {"name": "setElectromagneticValve", "args": [2, 0]}
                },
            ]
        },
        {
            "id": "mol_group_3", "name": "分子泵阀组三",
            "devices": [
                {
                    "id": "valve_gate_3", "name": "闸板阀3", "type": "valve", 
                    "attr_state": "gateValve3Open",
                    "cmd_open": {"name": "setGateValve", "args": [3, 1]},
                    "cmd_close": {"name": "setGateValve", "args": [3, 0]}
                },
                {
                    "id": "mol_pump_3", "name": "分子泵3", "type": "pump_with_water", 
                    "attr_state": "molecularPump3Power", 
                    "attr_water": "molecularPump3WaterFault", 
                    "attr_freq": "molecularPump3Speed",
                    "cmd_start": {"name": "setMolecularPumpStartStop", "args": [3, 1]},
                    "cmd_stop": {"name": "setMolecularPumpStartStop", "args": [3, 0]}
                },
                {
                    "id": "valve_mag_3", "name": "电磁阀3", "type": "valve", 
                    "attr_state": "electromagneticValve3Open",
                    "cmd_open": {"name": "setElectromagneticValve", "args": [3, 1]},
                    "cmd_close": {"name": "setElectromagneticValve", "args": [3, 0]}
                },
            ]
        },
    ],
    
    # 5. 主阀及放气阀模块
    "main_vent_group": {
        "name": "主阀及放气阀",
        "devices": [
            {
                "id": "valve_main", "name": "主闸板阀", "type": "valve", 
                "attr_state": "gateValve5Open",
                "cmd_open": {"name": "setGateValve", "args": [5, 1]},
                "cmd_close": {"name": "setGateValve", "args": [5, 0]}
            },
            {
                "id": "valve_vent_1", "name": "放气阀1", "type": "valve", 
                "attr_state": "ventValve1Open",
                "cmd_open": {"name": "setVentValve", "args": [1, 1]},
                "cmd_close": {"name": "setVentValve", "args": [1, 0]}
            },
            {
                "id": "valve_vent_2", "name": "放气阀2", "type": "valve", 
                "attr_state": "ventValve2Open",
                "cmd_open": {"name": "setVentValve", "args": [2, 1]},
                "cmd_close": {"name": "setVentValve", "args": [2, 0]}
            },
        ],
        "display_only": [
            {"name": "主真空计1手动阀", "status": "OPEN"}, # 模拟状态
            {"name": "主真空计2手动阀", "status": "OPEN"},
        ],
        "system_status": [
            {"name": "允许抽真空", "attr": "allow_vacuum"},
            {"name": "允许放气", "attr": "allow_vent"},
        ]
    },
    
    # 状态面板配置 (真空)
    "status_items": [
        {"category": "真空规读数", "items": ["前级电阻规", "主真空计1", "主真空计2"]},
        {"category": "前级泵阀", "items": ["罗茨泵", "螺杆泵", "尾气电磁阀", "粗抽闸板阀"]},
        {"category": "分子泵阀组1", "items": ["分子泵1", "闸板阀1", "电磁阀1"]},
        {"category": "分子泵阀组2", "items": ["分子泵2", "闸板阀2", "电磁阀2"]},
        {"category": "分子泵阀组3", "items": ["分子泵3", "闸板阀3", "电磁阀3"]},
        {"category": "主阀/放气", "items": ["主闸板阀", "放气阀1", "放气阀2"]},
    ]
}

# =============================================================================
# 反射光成像配置
# =============================================================================

REFLECTION_IMAGING_CONFIG = {
    "cameras": [
        {"id": "upper_1x", "name": "上CCD-1倍", "position": "upper", "magnification": "1x"},
        {"id": "upper_10x", "name": "上CCD-10倍", "position": "upper", "magnification": "10x"},
        {"id": "lower_1x", "name": "下CCD-1倍", "position": "lower", "magnification": "1x"},
        {"id": "lower_10x", "name": "下CCD-10倍", "position": "lower", "magnification": "10x"},
    ],
    "api_base_url": "http://localhost:8080/api",
    "stream_interval_ms": 100,
    # 状态面板电机配置 (反射光成像)
    "status_motors": [
        {"id": "upper_x", "name": "上平台X", "device": "reflection_imaging", "axis": 0},
        {"id": "upper_y", "name": "上平台Y", "device": "reflection_imaging", "axis": 1},
        {"id": "upper_z", "name": "上平台Z", "device": "reflection_imaging", "axis": 2},
        {"id": "lower_x", "name": "下平台X", "device": "reflection_imaging", "axis": 3},
        {"id": "lower_y", "name": "下平台Y", "device": "reflection_imaging", "axis": 4},
        {"id": "lower_z", "name": "下平台Z", "device": "reflection_imaging", "axis": 5},
    ],
    # CCD参数配置
    "ccd_params": [
        {"id": "exposure", "name": "曝光时间", "unit": "ms", "default": 100, "type": "float", "min": 100, "max": 10000},
        {"id": "gain", "name": "增益", "unit": "dB", "default": 0, "type": "float", "min": 0, "max": 20},
        {"id": "brightness", "name": "亮度", "unit": "", "default": 50, "type": "int", "min": 0, "max": 100},
        {"id": "contrast", "name": "对比度", "unit": "", "default": 50, "type": "int", "min": 0, "max": 100},
        {"id": "trigger_mode", "name": "触发模式", "unit": "", "default": "Software", "type": "enum", "options": ["Software", "Hardware", "Continuous"]},
        {"id": "resolution", "name": "分辨率", "unit": "", "default": "1920x1080", "type": "enum", "options": ["1920x1080", "1280x720", "640x480"]},
    ],
    "default_save_path": "D:/Images",
}

# =============================================================================
# 状态显示配置
# =============================================================================

STATUS_DISPLAY_CONFIG = {
    "position_display": {
        "show_large_stroke": True,
        "show_six_dof": True,
        "show_encoder": True,
    },
    "vacuum_display": {
        "show_pressure": True,
        "show_pump_status": True,
        "show_valve_status": True,
    },
    "system_display": {
        "show_interlock": True,
        "show_connection": True,
        "show_errors": True,
    },
}

# =============================================================================
# 命令标签映射
# =============================================================================

COMMAND_LABELS = {
    # 通用
    "init": "初始化",
    "reset": "复位",
    "stop": "停止",
    "selfCheck": "自检",
    
    # 运动相关
    "moveAbsolute": "绝对运动",
    "moveRelative": "相对运动",
    "moveZero": "回零",
    "movePoseAbsolute": "位姿绝对运动",
    "movePoseRelative": "位姿相对运动",
    
    # 真空相关
    "startPump": "启动抽气",
    "stopPump": "停止抽气",
    "openValve": "开阀",
    "closeValve": "关阀",
    "startMolecularPump": "启动分子泵",
    "stopMolecularPump": "停止分子泵",
    
    # 辅助支撑
    "setHoldPos": "设置夹持位置",
    "release": "释放",
    "readForce": "读取力值",
}

# =============================================================================
# 隐藏的命令（不在界面显示）
# =============================================================================

HIDDEN_COMMANDS = {
    "State", "Status", "Init",
    "devLock", "devUnlock", "devLockVerify", "devLockQuery", "devUserConfig",
}
