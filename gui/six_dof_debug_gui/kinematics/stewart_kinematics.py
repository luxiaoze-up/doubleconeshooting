"""
Stewart平台逆运动学计算模块（Python版本）
Stewart Platform Inverse Kinematics Module (Python Implementation)
"""

import math
import numpy as np
from typing import List, Tuple, Optional


class Pose:
    """六自由度位姿结构体"""
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0,
                 rx: float = 0.0, ry: float = 0.0, rz: float = 0.0):
        """
        初始化位姿
        
        Args:
            x, y, z: 平移量（mm）
            rx, ry, rz: 旋转量（度）
        """
        self.x = x
        self.y = y
        self.z = z
        self.rx = rx  # Roll (度)
        self.ry = ry  # Pitch (度)
        self.rz = rz  # Yaw (度)


class StewartPlatformKinematics:
    """Stewart平台逆运动学求解器"""
    
    def __init__(self, config: dict):
        """
        初始化运动学求解器
        
        Args:
            config: 几何配置字典，包含：
                - r1: 上平台半径
                - r2: 下平台半径
                - hh: 上下平台铰点面间距离
                - a1: 上平台第一点与X轴夹角（度）
                - a2: 下平台第一点与X轴夹角（度）
                - h: 动坐标点与上平台下表面的垂直距离
                - h3: 铰点到上平台下表面垂直高度
                - ll: 标称连杆长度（可选）
        """
        self.r1 = float(config.get("r1", 110.0))
        self.r2 = float(config.get("r2", 193.0))
        self.hh = float(config.get("hh", 408.0))
        self.a1 = float(config.get("a1", 40.0))
        self.a2 = float(config.get("a2", 14.0))
        self.h = float(config.get("h", 575.5))
        self.h3 = float(config.get("h3", 57.0))
        self.ll = float(config.get("ll", 421.4857))
        
        # 计算参数
        self.H_target_upd = self.h  # 靶点到上平台距离
        self.H_upd_up = self.h3     # 上平台到上铰点面距离
        self.H_up_down = self.hh    # 上铰点面到下铰点面距离
        
        # 初始化铰点坐标
        self.base_points = np.zeros((6, 3))      # 下平台铰点
        self.platform_points = np.zeros((6, 3))  # 上平台铰点
        self._initialize_geometry()
        
        # 计算标称连杆长度
        self.nominal_leg_length = self._calculate_nominal_leg_length()
    
    def _initialize_geometry(self):
        """初始化几何参数（计算铰点坐标）"""
        deg_to_rad = math.pi / 180.0
        R_down = self.r2
        R_up = self.r1
        a_down = self.a2
        a_up = self.a1
        
        # 下平台铰点角度
        for i in range(6):
            if i % 2 == 0:
                angle = math.pi / 3 * i + a_down * deg_to_rad
            else:
                angle = math.pi / 3 * (i + 1) - a_down * deg_to_rad
            self.base_points[i][0] = R_down * math.cos(angle)
            self.base_points[i][1] = R_down * math.sin(angle)
            self.base_points[i][2] = 0.0
        
        # 上平台铰点角度
        for i in range(6):
            if i % 2 == 0:
                angle = math.pi / 3 * i + a_up * deg_to_rad
            else:
                angle = math.pi / 3 * (i + 1) - a_up * deg_to_rad
            self.platform_points[i][0] = R_up * math.cos(angle)
            self.platform_points[i][1] = R_up * math.sin(angle)
            self.platform_points[i][2] = 0.0
    
    def _calculate_nominal_leg_length(self) -> float:
        """计算标称连杆长度"""
        if self.ll > 0:
            return self.ll
        
        # 参考BGsystem: L = sqrt(H_up_down^2 + d1^2 + d2^2)
        d1 = self.platform_points[0][0] - self.base_points[0][0]
        d2 = self.platform_points[0][1] - self.base_points[0][1]
        H = self.H_up_down
        return math.sqrt(H * H + d1 * d1 + d2 * d2)
    
    def _get_rotation_matrix(self, rx: float, ry: float, rz: float) -> np.ndarray:
        """
        计算旋转矩阵 (Rx * Ry * Rz)
        
        Args:
            rx, ry, rz: 旋转角度（度）
        
        Returns:
            3x3旋转矩阵
        """
        deg_to_rad = math.pi / 180.0
        ca = math.cos(rx * deg_to_rad)  # cos(roll)
        sa = math.sin(rx * deg_to_rad)  # sin(roll)
        cb = math.cos(ry * deg_to_rad)  # cos(pitch)
        sb = math.sin(ry * deg_to_rad)  # sin(pitch)
        cc = math.cos(rz * deg_to_rad)  # cos(yaw)
        sc = math.sin(rz * deg_to_rad)  # sin(yaw)
        
        # 标准 Rx * Ry * Rz 旋转矩阵
        R = np.array([
            [cc * cb, -cb * sc, sb],
            [cc * sb * sa + ca * sc, cc * ca - sc * sb * sa, -sa * cb],
            [sa * sc - ca * sb * cc, ca * sb * sc + sa * cc, cb * ca]
        ])
        
        return R
    
    def calculate_inverse_kinematics(self, pose: Pose) -> Optional[List[float]]:
        """
        计算逆运动学（给定位姿下的腿长）
        
        Args:
            pose: 目标位姿（铰点面位姿，不是靶点）
        
        Returns:
            6个腿的长度列表（mm），如果超出范围返回None
        """
        R = self._get_rotation_matrix(pose.rx, pose.ry, pose.rz)
        
        # 平移向量 T
        T = np.array([pose.x, pose.y, pose.z + self.H_up_down])
        
        leg_lengths = []
        
        for i in range(6):
            # 计算变换后的平台铰点: q_i = T + R * p_i
            p = self.platform_points[i]
            q = T + R @ p
            
            # 计算向量 l_i = q_i - b_i
            b = self.base_points[i]
            l_vec = q - b
            
            # 计算长度
            length = np.linalg.norm(l_vec)
            leg_lengths.append(length)
        
        return leg_lengths
    
    def convert_target_pose_to_platform_pose(self, target_pose: Pose) -> Pose:
        """
        将靶点pose转换为上平台铰点面pose
        
        Args:
            target_pose: 靶点位姿
        
        Returns:
            上平台铰点面位姿
        """
        platform_pose = Pose()
        # 姿态不变
        platform_pose.rx = target_pose.rx
        platform_pose.ry = target_pose.ry
        platform_pose.rz = target_pose.rz
        # Z向偏移
        platform_pose.x = target_pose.x
        platform_pose.y = target_pose.y
        platform_pose.z = target_pose.z + self.H_target_upd + self.H_upd_up
        
        return platform_pose
    
    def calculate_z_axis_displacement(self, pose: Pose) -> Optional[List[float]]:
        """
        计算Z轴投影位移（BGsystem风格）
        适用于直线推杆只能在Z方向移动的情况
        
        Args:
            pose: 上平台铰点面的位姿（不是靶点！）
        
        Returns:
            6个推杆的Z向位移列表（mm），如果超出范围返回None
        """
        R = self._get_rotation_matrix(pose.rx, pose.ry, pose.rz)
        
        # 平移向量 T = [pose.x, pose.y, pose.z + H_up_down]
        T = np.array([pose.x, pose.y, pose.z + self.H_up_down])
        
        L = self.nominal_leg_length
        L_squared = L * L
        
        z_displacements = []
        
        for i in range(6):
            # 计算平台铰点在世界坐标系中的位置
            p = self.platform_points[i]
            q = T + R @ p
            
            # 计算水平面内的距离分量
            d1 = q[0] - self.base_points[i][0]
            d2 = q[1] - self.base_points[i][1]
            d_horizontal_squared = d1 * d1 + d2 * d2
            
            # Z投影公式：z_proj = z - sqrt(L² - d_horizontal²)
            # 其中 z 是 q[2]（平台铰点的Z坐标）
            z_proj = q[2] - math.sqrt(L_squared - d_horizontal_squared)
            
            z_displacements.append(z_proj)
        
        return z_displacements
    
    def calculate_z_axis_displacement_from_target(self, target_pose: Pose) -> Optional[List[float]]:
        """
        直接从靶点pose计算Z轴投影位移（BGsystem Cal_s 等价接口）
        
        Args:
            target_pose: 靶点的位姿
        
        Returns:
            6个推杆的Z向位移列表（mm），如果超出范围返回None
        """
        # 先转换为铰点面pose
        platform_pose = self.convert_target_pose_to_platform_pose(target_pose)
        # 再计算Z轴投影位移
        return self.calculate_z_axis_displacement(platform_pose)
    
    def calculate_displacement(self, target_pose: Pose, current_leg_lengths: List[float]) -> Optional[List[float]]:
        """
        计算增量位移（相对于当前位置）
        
        Args:
            target_pose: 目标位姿
            current_leg_lengths: 当前腿长列表（6个值）
        
        Returns:
            增量位移列表（mm），如果超出范围返回None
        """
        target_lengths = self.calculate_inverse_kinematics(target_pose)
        if target_lengths is None:
            return None
        
        delta_lengths = [target_lengths[i] - current_leg_lengths[i] for i in range(6)]
        return delta_lengths
