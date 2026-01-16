"""
配置管理模块
Configuration Management Module
"""

import json
import os
from pathlib import Path
from typing import Dict, Any, Optional


class Config:
    """配置管理类"""
    
    DEFAULT_CONFIG = {
        "motion_controller": {
            "ip": "192.168.1.13",
            "card_id": 0
        },
        "encoder_collector": {
            "ip": "192.168.1.199",
            "port": 5000,
            "channels": [0, 1, 2, 3, 4, 5],
            "encoder_resolution": 0.001  # mm per count
        },
        "pulse_calculation": {
            "lead": 2.0,  # 导程(mm)
            "subdivision": 4000,  # 细分数
            "gear_ratio": 29.0,
            "gear_ratio_denominator": 54.0,
            "reduction_ratio": 1.0,
            "reduction_ratio_denominator": 8.0
        },
        "kinematics": {
            "r1": 110,  # 上平台半径
            "r2": 193,  # 下平台半径
            "hh": 408,  # 上下平台铰点面间距离
            "a1": 40,   # 上平台第一点与X轴夹角
            "a2": 14,   # 下平台第一点与X轴夹角
            "h": 575.5,  # 动坐标点与上平台下表面的垂直距离
            "h3": 57,   # 铰点到上平台下表面垂直高度
            "ll": 421.4857  # 标称连杆长度
        },
        "speed_defaults": {
            "start_speed": 100,
            "max_speed": 1000,
            "acc_time": 0.1,
            "dec_time": 0.1,
            "stop_speed": 50
        },
        "brake_control": {
            "port": 3,  # OUT3端口
            "active_low": True  # 低电平有效
        }
    }
    
    def __init__(self, config_file: Optional[str] = None):
        """
        初始化配置
        
        Args:
            config_file: 配置文件路径，如果为None则使用默认路径
        """
        if config_file is None:
            # 默认配置文件路径：gui/six_dof_debug_gui/config.json
            # 支持打包环境
            import sys
            if getattr(sys, 'frozen', False):
                # 打包环境：从exe所在目录
                exe_dir = Path(sys.executable).parent
                config_file = str(exe_dir / "config.json")
            else:
                # 开发环境：从模块所在目录
                base_dir = Path(__file__).parent
                config_file = str(base_dir / "config.json")
        
        self.config_file = config_file
        self.config = self._load_config()
    
    def _load_config(self) -> Dict[str, Any]:
        """加载配置文件"""
        config_path = Path(self.config_file)
        
        # 如果配置文件不存在，创建默认配置
        if not config_path.exists():
            self._save_config(self.DEFAULT_CONFIG)
            return self.DEFAULT_CONFIG.copy()
        
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
            
            # 合并默认配置，确保所有键都存在
            merged_config = self.DEFAULT_CONFIG.copy()
            merged_config.update(config)
            
            # 深度合并嵌套字典
            for key, value in config.items():
                if isinstance(value, dict) and key in merged_config:
                    merged_config[key].update(value)
            
            return merged_config
        except Exception as e:
            print(f"加载配置文件失败: {e}，使用默认配置")
            return self.DEFAULT_CONFIG.copy()
    
    def _save_config(self, config: Dict[str, Any]):
        """保存配置文件"""
        try:
            config_path = Path(self.config_file)
            config_path.parent.mkdir(parents=True, exist_ok=True)
            
            with open(config_path, 'w', encoding='utf-8') as f:
                json.dump(config, f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f"保存配置文件失败: {e}")
    
    def save(self):
        """保存当前配置到文件"""
        self._save_config(self.config)
    
    def get(self, key: str, default: Any = None) -> Any:
        """获取配置值（支持点号分隔的嵌套键）"""
        keys = key.split('.')
        value = self.config
        
        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                return default
        
        return value
    
    def set(self, key: str, value: Any):
        """设置配置值（支持点号分隔的嵌套键）"""
        keys = key.split('.')
        config = self.config
        
        # 导航到目标字典
        for k in keys[:-1]:
            if k not in config:
                config[k] = {}
            config = config[k]
        
        # 设置值
        config[keys[-1]] = value
    
    def get_motion_controller_config(self) -> Dict[str, Any]:
        """获取运动控制器配置"""
        return self.config.get("motion_controller", {})
    
    def get_encoder_collector_config(self) -> Dict[str, Any]:
        """获取编码器采集器配置"""
        return self.config.get("encoder_collector", {})
    
    def get_pulse_calculation_config(self) -> Dict[str, Any]:
        """获取脉冲数计算配置"""
        return self.config.get("pulse_calculation", {})
    
    def get_kinematics_config(self) -> Dict[str, Any]:
        """获取运动学配置"""
        return self.config.get("kinematics", {})
    
    def get_speed_defaults(self) -> Dict[str, Any]:
        """获取速度默认值"""
        return self.config.get("speed_defaults", {})
    
    def get_brake_control_config(self) -> Dict[str, Any]:
        """获取抱闸控制配置"""
        return self.config.get("brake_control", {})
