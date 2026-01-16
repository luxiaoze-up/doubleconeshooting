"""
运动控制器通信模块（SMC）
Motion Controller Communication Module (SMC)
封装LTSMC库API，直接通过网络访问运动控制器
"""

import ctypes
import os
import sys
from pathlib import Path
from typing import Optional, Tuple


class SMCController:
    """SMC运动控制器（直接调用LTSMC库）"""
    
    def __init__(self):
        self.smc = None
        self.is_windows = os.name == 'nt'
        self.card_id = 0
        self.connected = False
        self.dll_path = None
        # 延迟加载：不在初始化时加载，而是在连接时加载
        # self._load_library()
    
    def _find_lib_directory(self):
        """查找lib目录（支持打包和开发环境）"""
        # 检查是否在PyInstaller打包环境中
        if getattr(sys, 'frozen', False):
            # 打包环境：从exe所在目录查找
            # sys.executable 指向exe文件路径
            exe_dir = Path(sys.executable).parent
            # 尝试多个可能的路径
            possible_paths = [
                exe_dir / "lib",  # exe同目录下的lib
                exe_dir.parent / "lib",  # 上一级目录的lib
            ]
        else:
            # 开发环境：从项目根目录查找
            workspace_root = Path(__file__).parent.parent.parent.parent
            possible_paths = [
                workspace_root / "lib",
            ]
        
        # 查找存在的lib目录
        for lib_dir in possible_paths:
            dll_path = lib_dir / "LTSMC.dll"
            if dll_path.exists():
                return dll_path
        
        # 如果都没找到，返回第一个可能的路径（用于错误提示）
        return possible_paths[0] / "LTSMC.dll"
    
    def _load_library(self):
        """加载SMC库"""
        import platform
        
        # 使用新的查找方法
        dll_path = self._find_lib_directory()
        self.dll_path = dll_path
        
        if self.is_windows:
            
            if not dll_path.exists():
                # 友好的错误信息
                if getattr(sys, 'frozen', False):
                    exe_dir = Path(sys.executable).parent
                    search_paths = [
                        exe_dir / 'lib' / 'LTSMC.dll',
                        exe_dir.parent / 'lib' / 'LTSMC.dll'
                    ]
                else:
                    workspace_root = Path(__file__).parent.parent.parent.parent
                    search_paths = [workspace_root / 'lib' / 'LTSMC.dll']
                
                error_msg = (
                    "未找到运动控制器驱动文件\n\n"
                    "程序需要 LTSMC.dll 文件才能与运动控制器通信。\n\n"
                    "请确保该文件位于以下位置之一：\n"
                )
                for path in search_paths:
                    error_msg += f"  • {path}\n"
                error_msg += (
                    "\n解决方案：\n"
                    "1. 检查 lib 文件夹是否存在\n"
                    "2. 确认 LTSMC.dll 文件是否在 lib 文件夹中\n"
                    "3. 如果文件缺失，请从安装包中复制该文件"
                )
                raise FileNotFoundError(error_msg)
            
            # 检查文件是否可读
            if not os.access(dll_path, os.R_OK):
                raise PermissionError(
                    "无法访问驱动文件\n\n"
                    f"程序无法读取文件：{dll_path}\n\n"
                    "可能的原因：\n"
                    "• 文件被其他程序占用\n"
                    "• 文件权限不足\n"
                    "• 文件损坏\n\n"
                    "解决方案：\n"
                    "1. 关闭可能占用该文件的其他程序\n"
                    "2. 检查文件权限设置\n"
                    "3. 尝试以管理员身份运行程序"
                )
            
            try:
                # 尝试加载DLL
                self.smc = ctypes.WinDLL(str(dll_path))
            except OSError as e:
                error_code = e.winerror if hasattr(e, 'winerror') else None
                if error_code == 193:
                    # WinError 193: %1 不是有效的Win32应用程序
                    python_arch = platform.architecture()[0]
                    raise OSError(
                        "无法加载运动控制器驱动\n\n"
                        "驱动文件与当前系统不兼容。\n\n"
                        "可能的原因：\n"
                        "• 驱动文件版本不匹配（32位/64位）\n"
                        "• 缺少必要的系统运行库\n"
                        "• 驱动文件损坏\n\n"
                        "解决方案：\n"
                        "1. 确认驱动文件版本与程序版本匹配\n"
                        "2. 安装 Microsoft Visual C++ Redistributable 运行库\n"
                        "   （可从微软官网下载）\n"
                        "3. 重新安装程序或从备份恢复驱动文件\n\n"
                        f"当前系统架构：{python_arch}"
                    ) from e
                else:
                    raise OSError(
                        "无法加载运动控制器驱动\n\n"
                        f"程序在加载驱动文件时遇到错误。\n\n"
                        "可能的原因：\n"
                        "• 驱动文件损坏\n"
                        "• 系统缺少必要的运行库\n"
                        "• 文件被占用或权限不足\n\n"
                        "建议操作：\n"
                        "1. 重新安装程序\n"
                        "2. 安装 Microsoft Visual C++ Redistributable\n"
                        "3. 检查系统日志获取更多信息\n\n"
                        f"错误代码：{error_code}"
                    ) from e
        else:
            # Linux环境：查找so文件
            if getattr(sys, 'frozen', False):
                exe_dir = Path(sys.executable).parent
                so_path = exe_dir / "lib" / "libLTSMC.so"
            else:
                workspace_root = Path(__file__).parent.parent.parent.parent
                so_path = workspace_root / "lib" / "libLTSMC.so"
            
            self.dll_path = so_path
            
            if not so_path.exists():
                raise FileNotFoundError(
                    f"SMC库未找到: {so_path}\n"
                    f"请确保libLTSMC.so文件存在于lib目录中。"
                )
            
            try:
                self.smc = ctypes.CDLL(str(so_path))
            except OSError as e:
                raise OSError(
                    f"无法加载SMC库: {so_path}\n"
                    f"错误: {e}\n"
                    f"可能的原因: 库文件损坏或依赖缺失"
                ) from e
        
        # 定义函数签名
        self._define_function_signatures()
    
    def _define_function_signatures(self):
        """定义LTSMC库函数签名"""
        # 安全地定义函数签名，如果函数不存在则跳过
        def safe_define(func_name, argtypes, restype, optional=False):
            """
            安全定义函数签名
            
            Args:
                func_name: 函数名
                argtypes: 参数类型列表
                restype: 返回类型
                optional: 是否为可选函数（如果不存在不打印警告）
            """
            if hasattr(self.smc, func_name):
                try:
                    func = getattr(self.smc, func_name)
                    func.argtypes = argtypes
                    func.restype = restype
                except Exception as e:
                    # 函数签名定义失败，静默处理（不影响功能）
                    pass
            else:
                # 可选函数不打印警告，因为已有降级处理
                # 非可选函数也不打印警告，避免干扰用户
                pass
        
        # 连接管理
        safe_define('smc_set_connect_timeout', [ctypes.c_uint32], ctypes.c_int16)
        safe_define('smc_board_init', [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_char_p, ctypes.c_uint32], ctypes.c_int16)
        safe_define('smc_board_close', [ctypes.c_uint16], ctypes.c_int16)
        
        # IO控制
        safe_define('smc_write_outbit', [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16], ctypes.c_int16)
        safe_define('smc_read_outbit', [ctypes.c_uint16, ctypes.c_uint16], ctypes.c_int16)
        
        # 位置读取
        safe_define('smc_get_position_unit', [ctypes.c_uint16, ctypes.c_uint16, ctypes.POINTER(ctypes.c_double)], ctypes.c_int16)
        
        # 运动控制
        safe_define('smc_pmove_unit', [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_double, ctypes.c_uint16], ctypes.c_int16)
        
        # 速度参数设置
        safe_define('smc_set_profile_unit', [
            ctypes.c_uint16, ctypes.c_uint16, 
            ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double
        ], ctypes.c_int16)
        
        # 等效脉冲设置
        safe_define('smc_set_equiv', [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_double], ctypes.c_int16)
        
        # 运动状态检查
        safe_define('smc_check_done', [ctypes.c_uint16, ctypes.c_uint16], ctypes.c_int16)
        
        # 停止运动（stop_mode: 0=减速停止, 1=立即停止）
        safe_define('smc_stop', [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16], ctypes.c_int16)
        
        # 紧急停止
        safe_define('smc_emg_stop', [ctypes.c_uint16], ctypes.c_int16)
        
        # 复位
        safe_define('smc_clear_stop_reason', [ctypes.c_uint16, ctypes.c_uint16], ctypes.c_int16)
    
    def connect(self, controller_ip: str, card_id: int = 0, timeout_ms: int = 3000) -> bool:
        """
        连接控制器
        
        Args:
            controller_ip: 控制器IP地址
            card_id: 卡ID（默认0）
            timeout_ms: 连接超时时间（毫秒）
        
        Returns:
            连接是否成功
        """
        try:
            # 如果库未加载，先加载
            if self.smc is None:
                self._load_library()
            
            # 如果已连接，先断开
            if self.connected:
                self.disconnect()
            
            # 设置连接超时
            self.smc.smc_set_connect_timeout(timeout_ms)
            
            # 初始化连接（type=2表示TCP网络连接）
            ip_bytes = controller_ip.encode('utf-8')
            ret = self.smc.smc_board_init(card_id, 2, ip_bytes, 0)
            
            if ret != 0:
                self.connected = False
                return False
            
            self.card_id = card_id
            self.connected = True
            return True
        except Exception as e:
            error_msg = str(e)
            # 如果是DLL加载错误，抛出更详细的异常
            if "WinError 193" in error_msg or "无法加载SMC库" in error_msg:
                raise RuntimeError(error_msg) from e
            # 其他连接异常静默处理
            self.connected = False
            return False
    
    def disconnect(self):
        """断开连接"""
        try:
            if self.connected:
                self.smc.smc_board_close(self.card_id)
            self.connected = False
        except Exception:
            # 断开连接异常静默处理
            self.connected = False
    
    def is_connected(self) -> bool:
        """检查是否已连接"""
        return self.connected
    
    def move_relative(self, axis: int, distance: float) -> bool:
        """
        相对运动
        
        Args:
            axis: 轴号（0-5）
            distance: 运动距离（单位：根据等效脉冲设置）
        
        Returns:
            命令是否成功
        """
        if not self.connected:
            return False
        
        try:
            ret = self.smc.smc_pmove_unit(self.card_id, axis, distance, 0)  # 0=相对模式
            return ret == 0
        except Exception:
            return False
    
    def move_absolute(self, axis: int, position: float) -> bool:
        """
        绝对运动
        
        Args:
            axis: 轴号（0-5）
            position: 目标位置（单位：根据等效脉冲设置）
        
        Returns:
            命令是否成功
        """
        if not self.connected:
            return False
        
        try:
            ret = self.smc.smc_pmove_unit(self.card_id, axis, position, 1)  # 1=绝对模式
            return ret == 0
        except Exception:
            return False
    
    def get_position(self, axis: int) -> Optional[float]:
        """
        读取当前位置
        
        Args:
            axis: 轴号（0-5）
        
        Returns:
            当前位置，失败返回None
        """
        if not self.connected:
            return None
        
        try:
            pos = ctypes.c_double(0.0)
            ret = self.smc.smc_get_position_unit(self.card_id, axis, ctypes.byref(pos))
            if ret == 0:
                return pos.value
            return None
        except Exception:
            return None
    
    def set_speed_profile(self, axis: int, start_vel: float, max_vel: float, 
                         acc_time: float, dec_time: float, stop_vel: float) -> bool:
        """
        设置速度参数
        
        Args:
            axis: 轴号（0-5）
            start_vel: 启动速度
            max_vel: 最大速度
            acc_time: 加速度时间
            dec_time: 减速度时间
            stop_vel: 停止速度
        
        Returns:
            设置是否成功
        """
        if not self.connected:
            return False
        
        try:
            ret = self.smc.smc_set_profile_unit(
                self.card_id, axis, start_vel, max_vel, acc_time, dec_time, stop_vel
            )
            return ret == 0
        except Exception:
            return False
    
    def set_equiv(self, axis: int, equiv: float) -> bool:
        """
        设置等效脉冲
        
        Args:
            axis: 轴号（0-5）
            equiv: 等效脉冲值（计算公式：equiv = (step_angle * gear_ratio) / (360.0 * subdivision)）
        
        Returns:
            设置是否成功
        """
        if not self.connected:
            return False
        
        try:
            ret = self.smc.smc_set_equiv(self.card_id, axis, equiv)
            return ret == 0
        except Exception:
            return False
    
    def check_done(self, axis: int) -> Optional[bool]:
        """
        检查运动是否完成
        
        Args:
            axis: 轴号（0-5）
        
        Returns:
            True=已完成，False=运动中，None=检查失败
        """
        if not self.connected:
            return None
        
        try:
            ret = self.smc.smc_check_done(self.card_id, axis)
            # smc_check_done返回值: 0=运动中, 1=已停止
            if ret == 0:
                return False  # 运动中
            elif ret == 1:
                return True   # 已完成
            else:
                return None   # 错误
        except Exception:
            return None
    
    def stop_move(self, axis: int, stop_mode: int = 0) -> bool:
        """
        停止指定轴运动
        
        Args:
            axis: 轴号（0-5）
            stop_mode: 停止模式（0=减速停止，1=立即停止，默认0）
        
        Returns:
            命令是否成功
        """
        if not self.connected:
            return False
        
        try:
            # 使用标准的smc_stop函数
            if hasattr(self.smc, 'smc_stop'):
                ret = self.smc.smc_stop(self.card_id, axis, stop_mode)
                return ret == 0
            else:
                # 如果smc_stop不存在（不应该发生），降级到紧急停止
                return self.emergency_stop()
        except Exception:
            # 如果调用失败，尝试使用紧急停止
            try:
                return self.emergency_stop()
            except:
                return False
    
    def emergency_stop(self) -> bool:
        """
        紧急停止所有轴
        
        Returns:
            命令是否成功
        """
        if not self.connected:
            return False
        
        try:
            ret = self.smc.smc_emg_stop(self.card_id)
            return ret == 0
        except Exception:
            return False
    
    def write_io(self, bitno: int, logical_value: int) -> bool:
        """
        写IO输出（抱闸控制）
        
        Args:
            bitno: 端口号（0-11），六自由度抱闸使用OUT3
            logical_value: 逻辑值（1=开启/释放，0=关闭/启用）
        
        Returns:
            命令是否成功
        
        注意：OUT端口是低电平有效（active low）
        - 逻辑值 1 (开启) -> 硬件值 0 (LOW)
        - 逻辑值 0 (关闭) -> 硬件值 1 (HIGH)
        """
        if not self.connected:
            return False
        
        try:
            # 低电平有效：逻辑值转换为硬件值
            normalized_value = 1 if logical_value != 0 else 0
            hardware_value = 1 - normalized_value
            
            ret = self.smc.smc_write_outbit(self.card_id, bitno, hardware_value)
            return ret == 0
        except Exception:
            return False
    
    def read_io(self, bitno: int) -> Optional[int]:
        """
        读取IO输出状态
        
        Args:
            bitno: 端口号（0-11）
        
        Returns:
            硬件值（0=LOW, 1=HIGH），失败返回None
        """
        if not self.connected:
            return None
        
        try:
            value = self.smc.smc_read_outbit(self.card_id, bitno)
            return value
        except Exception:
            return None
