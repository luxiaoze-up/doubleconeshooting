"""
PLC OPC UA 客户端通信模块

直接与PLC通过OPC UA协议进行通信，不依赖Tango
"""

import time
from typing import Optional, Dict, Any
from utils.logger import get_logger

logger = get_logger(__name__)

try:
    from opcua import Client, ua
    OPCUA_AVAILABLE = True
except ImportError:
    OPCUA_AVAILABLE = False
    logger.warning("python-opcua 未安装，无法使用 Direct PLC 模式")


class PLCOPCUAClient:
    """PLC OPC UA 客户端"""
    
    def __init__(self, ip: str = "192.168.1.100", port: int = 4840):
        """
        初始化 OPC UA 客户端
        
        Args:
            ip: PLC IP地址
            port: OPC UA端口，默认4840
        """
        if not OPCUA_AVAILABLE:
            raise ImportError("python-opcua 未安装，请安装: pip install opcua")
            
        self.ip = ip
        self.port = port
        self.url = f"opc.tcp://{ip}:{port}"
        self.client: Optional[Client] = None
        self.connected = False
        
        # 节点ID缓存
        self._node_cache: Dict[str, Any] = {}
        
    def connect(self) -> bool:
        """
        连接到PLC
        
        Returns:
            True表示连接成功，False表示失败
        """
        try:
            if self.client is None:
                self.client = Client(self.url)
                # 不设置安全策略，使用默认的无安全模式
                
            self.client.connect()
            
            # 验证连接
            root = self.client.get_root_node()
            _ = root.get_browse_name()
            
            self.connected = True
            logger.info(f"成功连接到 PLC: {self.url}")
            return True
            
        except Exception as e:
            logger.error(f"连接 PLC 失败 ({self.url}): {e}")
            self.connected = False
            self.client = None  # 重置客户端以便下次重试
            return False
            
    def disconnect(self):
        """断开连接"""
        try:
            if self.client:
                self.client.disconnect()
                self.connected = False
                logger.info(f"已断开 PLC 连接: {self.url}")
        except Exception as e:
            logger.warning(f"断开 PLC 连接时出错: {e}")
            
    def is_connected(self) -> bool:
        """检查连接状态"""
        return self.connected
        
    def _get_node(self, node_id: str):
        """
        获取节点对象（带缓存）
        
        Args:
            node_id: 节点ID，如 "ns=4;s=|var|CODESYS Control Win V3 x64.Application.GVL.gVacuumSystem.bScrewPumpPower"
            
        Returns:
            节点对象
        """
        if node_id not in self._node_cache:
            self._node_cache[node_id] = self.client.get_node(node_id)
        return self._node_cache[node_id]
        
    def read_bool(self, node_id: str) -> Optional[bool]:
        """
        读取布尔值
        
        Args:
            node_id: 节点ID
            
        Returns:
            布尔值，失败返回None
        """
        try:
            node = self._get_node(node_id)
            value = node.get_value()
            return bool(value)
        except Exception as e:
            logger.error(f"读取布尔值失败 ({node_id}): {e}")
            return None
            
    def read_int(self, node_id: str) -> Optional[int]:
        """
        读取整数值
        
        Args:
            node_id: 节点ID
            
        Returns:
            整数值，失败返回None
        """
        try:
            node = self._get_node(node_id)
            value = node.get_value()
            return int(value)
        except Exception as e:
            logger.error(f"读取整数值失败 ({node_id}): {e}")
            return None
            
    def read_real(self, node_id: str) -> Optional[float]:
        """
        读取浮点数值
        
        Args:
            node_id: 节点ID
            
        Returns:
            浮点数值，失败返回None
        """
        try:
            node = self._get_node(node_id)
            value = node.get_value()
            return float(value)
        except Exception as e:
            logger.error(f"读取浮点数值失败 ({node_id}): {e}")
            return None
            
    def write_bool(self, node_id: str, value: bool) -> bool:
        """
        写入布尔值
        
        Args:
            node_id: 节点ID
            value: 要写入的布尔值
            
        Returns:
            True表示写入成功，False表示失败
        """
        try:
            node = self._get_node(node_id)
            node.set_value(ua.Variant(value, ua.VariantType.Boolean))
            return True
        except Exception as e:
            logger.error(f"写入布尔值失败 ({node_id}): {e}")
            return False
            
    def write_int(self, node_id: str, value: int) -> bool:
        """
        写入整数值
        
        Args:
            node_id: 节点ID
            value: 要写入的整数值
            
        Returns:
            True表示写入成功，False表示失败
        """
        try:
            node = self._get_node(node_id)
            node.set_value(ua.Variant(value, ua.VariantType.Int16))
            return True
        except Exception as e:
            logger.error(f"写入整数值失败 ({node_id}): {e}")
            return False
            
    def write_real(self, node_id: str, value: float) -> bool:
        """
        写入浮点数值
        
        Args:
            node_id: 节点ID
            value: 要写入的浮点数值
            
        Returns:
            True表示写入成功，False表示失败
        """
        try:
            node = self._get_node(node_id)
            node.set_value(ua.Variant(value, ua.VariantType.Float))
            return True
        except Exception as e:
            logger.error(f"写入浮点数值失败 ({node_id}): {e}")
            return False
