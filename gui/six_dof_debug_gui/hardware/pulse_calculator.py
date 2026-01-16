"""
脉冲数计算模块
Pulse Calculation Module
将物理量（mm）转换为脉冲数
"""

from typing import Dict, Any


class PulseCalculator:
    """脉冲数计算器"""
    
    def __init__(self, config: Dict[str, Any]):
        """
        初始化脉冲数计算器
        
        Args:
            config: 脉冲计算配置字典，包含：
                - lead: 导程(mm)
                - subdivision: 细分数
                - gear_ratio: 齿轮比分子
                - gear_ratio_denominator: 齿轮比分母
                - reduction_ratio: 减速比分子
                - reduction_ratio_denominator: 减速比分母
        """
        self.lead = float(config.get("lead", 2.0))
        self.subdivision = float(config.get("subdivision", 4000))
        self.gear_ratio = float(config.get("gear_ratio", 29.0)) / float(config.get("gear_ratio_denominator", 54.0))
        self.reduction_ratio = float(config.get("reduction_ratio", 1.0)) / float(config.get("reduction_ratio_denominator", 8.0))
        
        # 计算每毫米对应的脉冲数
        # 公式：pulses_per_mm = (subdivision * gear_ratio * reduction_ratio) / lead
        self.pulses_per_mm = (self.subdivision * self.gear_ratio * self.reduction_ratio) / self.lead
    
    def mm_to_pulses(self, mm: float) -> int:
        """
        将毫米转换为脉冲数
        
        Args:
            mm: 位移量（毫米）
        
        Returns:
            脉冲数（整数）
        
        公式：
            脉冲数 = (mm / 导程) * 细分数 * 齿轮比 * 减速比
            或：脉冲数 = mm * pulses_per_mm
        """
        pulses = mm * self.pulses_per_mm
        return int(round(pulses))
    
    def pulses_to_mm(self, pulses: int) -> float:
        """
        将脉冲数转换为毫米
        
        Args:
            pulses: 脉冲数
        
        Returns:
            位移量（毫米）
        
        公式：
            mm = (pulses * 导程) / (细分数 * 齿轮比 * 减速比)
            或：mm = pulses / pulses_per_mm
        """
        if self.pulses_per_mm == 0:
            return 0.0
        return pulses / self.pulses_per_mm
    
    def get_pulses_per_mm(self) -> float:
        """
        获取每毫米对应的脉冲数
        
        Returns:
            每毫米脉冲数
        """
        return self.pulses_per_mm
    
    def get_parameters(self) -> Dict[str, float]:
        """
        获取计算参数
        
        Returns:
            参数字典
        """
        return {
            "lead": self.lead,
            "subdivision": self.subdivision,
            "gear_ratio": self.gear_ratio,
            "reduction_ratio": self.reduction_ratio,
            "pulses_per_mm": self.pulses_per_mm
        }
