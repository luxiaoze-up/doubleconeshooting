"""
编码器采集器通信模块
Encoder Collector Communication Module
通过TCP Socket直接访问编码器采集器
"""

import socket
import threading
import time
from typing import Dict, Optional, Callable
from collections import deque


class EncoderCollector:
    """编码器采集器客户端"""
    
    # 帧格式常量
    FRAME_HEAD = 0x7E
    FRAME_TAIL = 0x7F
    FRAME_LEN = 7
    
    def __init__(self, ip: str, port: int, channels: list, encoder_resolution: float = 0.001):
        """
        初始化编码器采集器客户端
        
        Args:
            ip: 编码器采集器IP地址
            port: 端口号（默认5000）
            channels: 通道列表（六自由度使用[0,1,2,3,4,5]）
            encoder_resolution: 编码器分辨率（mm/count，默认0.001）
        """
        self.ip = ip
        self.port = port
        self.channels = channels
        self.encoder_resolution = encoder_resolution
        
        self.socket: Optional[socket.socket] = None
        self.connected = False
        self.running = False
        self.thread: Optional[threading.Thread] = None
        
        # 数据缓存：channel -> (raw_value, timestamp, position_mm)
        self.readings: Dict[int, tuple] = {}
        self.readings_lock = threading.Lock()
        
        # 接收缓冲区
        self.buffer = bytearray()
        self.buffer_lock = threading.Lock()
        
        # 回调函数（可选）
        self.data_callback: Optional[Callable[[int, int, float], None]] = None
    
    def set_data_callback(self, callback: Callable[[int, int, float], None]):
        """
        设置数据回调函数
        
        Args:
            callback: 回调函数，参数为(channel, raw_value, position_mm)
        """
        self.data_callback = callback
    
    def connect(self, timeout: float = 3.0) -> bool:
        """
        连接到编码器采集器
        
        Args:
            timeout: 连接超时时间（秒）
        
        Returns:
            连接是否成功
        """
        if self.connected:
            return True
        
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(timeout)
            self.socket.connect((self.ip, self.port))
            self.socket.settimeout(0.3)  # 接收超时300ms
            self.connected = True
            
            # 启动接收线程
            self.running = True
            self.thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.thread.start()
            
            return True
        except Exception as e:
            print(f"编码器采集器连接失败: {e}")
            self.connected = False
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            return False
    
    def disconnect(self):
        """断开连接"""
        self.running = False
        self.connected = False
        
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
        
        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=2.0)
    
    def is_connected(self) -> bool:
        """检查是否已连接"""
        return self.connected and self.socket is not None
    
    def _receive_loop(self):
        """接收数据循环（后台线程）"""
        while self.running and self.connected:
            try:
                if not self.socket:
                    break
                
                # 接收数据
                data = self.socket.recv(1024)
                if not data:
                    # 连接断开
                    self.connected = False
                    break
                
                # 添加到缓冲区
                with self.buffer_lock:
                    self.buffer.extend(data)
                
                # 解析帧
                self._parse_frames()
                
            except socket.timeout:
                # 超时是正常的，继续循环
                continue
            except Exception as e:
                print(f"编码器接收异常: {e}")
                self.connected = False
                break
    
    def _parse_frames(self):
        """从缓冲区解析完整帧"""
        with self.buffer_lock:
            while len(self.buffer) >= self.FRAME_LEN:
                try:
                    # 查找帧头
                    head_idx = self.buffer.find(self.FRAME_HEAD)
                    if head_idx == -1:
                        # 没有找到帧头，清空缓冲区
                        self.buffer.clear()
                        break
                    
                    # 检查是否有完整的帧
                    if len(self.buffer) - head_idx < self.FRAME_LEN:
                        # 数据不完整，保留帧头及之后的数据
                        if head_idx > 0:
                            del self.buffer[:head_idx]
                        break
                    
                    # 检查帧尾
                    if self.buffer[head_idx + self.FRAME_LEN - 1] != self.FRAME_TAIL:
                        # 帧尾不匹配，跳过这个字节继续查找
                        del self.buffer[:head_idx + 1]
                        continue
                    
                    # 解析帧
                    frame = self.buffer[head_idx:head_idx + self.FRAME_LEN]
                    channel = frame[1]
                    raw_value = (frame[2] << 24) | (frame[3] << 16) | (frame[4] << 8) | frame[5]
                    
                    # 移除已解析的帧
                    del self.buffer[:head_idx + self.FRAME_LEN]
                    
                    # 更新读数
                    if channel in self.channels:
                        position_mm = raw_value * self.encoder_resolution
                        timestamp = time.time()
                        
                        with self.readings_lock:
                            self.readings[channel] = (raw_value, timestamp, position_mm)
                        
                        # 调用回调函数
                        if self.data_callback:
                            try:
                                self.data_callback(channel, raw_value, position_mm)
                            except Exception as e:
                                print(f"数据回调异常: {e}")
                
                except Exception as e:
                    print(f"解析帧异常: {e}")
                    # 清空缓冲区，重新同步
                    self.buffer.clear()
                    break
    
    def get_reading(self, channel: int) -> Optional[tuple]:
        """
        获取指定通道的读数
        
        Args:
            channel: 通道号
        
        Returns:
            (raw_value, timestamp, position_mm) 或 None
        """
        with self.readings_lock:
            return self.readings.get(channel)
    
    def get_all_readings(self) -> Dict[int, tuple]:
        """
        获取所有通道的读数
        
        Returns:
            字典：channel -> (raw_value, timestamp, position_mm)
        """
        with self.readings_lock:
            return dict(self.readings)
    
    def get_position(self, channel: int) -> Optional[float]:
        """
        获取指定通道的位置（mm）
        
        Args:
            channel: 通道号
        
        Returns:
            位置值（mm）或None
        """
        reading = self.get_reading(channel)
        if reading:
            return reading[2]  # position_mm
        return None
