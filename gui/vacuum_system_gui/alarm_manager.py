"""
真空系统报警管理器

功能:
- 报警弹窗显示
- 报警确认/清除
- JSON 文件持久化
"""

import os
import json
from datetime import datetime
from typing import List, Dict, Optional, Callable
from PyQt5.QtCore import QObject, pyqtSignal, QTimer
from PyQt5.QtWidgets import QWidget

from config import ALARM_LOG_FILE, ALARM_TYPES
from widgets import AlarmPopup
from utils.logger import get_logger

logger = get_logger(__name__)


class AlarmRecord:
    """报警记录"""
    
    def __init__(self, alarm_code: int, alarm_type: str, description: str,
                 device_name: str, timestamp: str = None, acknowledged: bool = False):
        self.alarm_code = alarm_code
        self.alarm_type = alarm_type
        self.description = description
        self.device_name = device_name
        self.timestamp = timestamp or datetime.now().isoformat()
        self.acknowledged = acknowledged
        
    def to_dict(self) -> dict:
        """转换为字典"""
        return {
            "alarm_code": self.alarm_code,
            "alarm_type": self.alarm_type,
            "description": self.description,
            "device_name": self.device_name,
            "timestamp": self.timestamp,
            "acknowledged": self.acknowledged
        }
        
    @classmethod
    def from_dict(cls, data: dict) -> 'AlarmRecord':
        """从字典创建"""
        return cls(
            alarm_code=data.get("alarm_code", 0),
            alarm_type=data.get("alarm_type", ""),
            description=data.get("description", ""),
            device_name=data.get("device_name", ""),
            timestamp=data.get("timestamp"),
            acknowledged=data.get("acknowledged", False)
        )


class AlarmManager(QObject):
    """报警管理器"""
    
    # 信号
    alarm_added = pyqtSignal(dict)          # 新报警添加
    alarm_acknowledged = pyqtSignal(int)    # 报警已确认
    alarm_cleared = pyqtSignal(int)         # 报警已清除
    alarms_changed = pyqtSignal()           # 报警列表变化
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        self._active_alarms: List[AlarmRecord] = []
        self._history_alarms: List[AlarmRecord] = []
        self._log_file = ALARM_LOG_FILE
        
        # 弹窗队列
        self._popup_queue: List[dict] = []
        self._current_popup: Optional[AlarmPopup] = None
        self._popup_parent: Optional[QWidget] = None
        
        # 确保日志目录存在
        os.makedirs(os.path.dirname(self._log_file), exist_ok=True)
        
        # 加载历史记录
        self._load_history()
        
    def set_popup_parent(self, parent: QWidget):
        """设置弹窗父窗口"""
        self._popup_parent = parent
        
    def add_alarm(self, alarm_data: dict):
        """添加新报警"""
        alarm_code = alarm_data.get("alarm_code", 0)
        
        # 检查是否已存在
        for alarm in self._active_alarms:
            if alarm.alarm_code == alarm_code:
                return
                
        # 创建记录
        record = AlarmRecord(
            alarm_code=alarm_code,
            alarm_type=alarm_data.get("alarm_type", "UNKNOWN"),
            description=alarm_data.get("description", ALARM_TYPES.get(alarm_code, "未知报警")),
            device_name=alarm_data.get("device_name", "")
        )
        
        self._active_alarms.append(record)
        self._history_alarms.append(record)
        
        # 保存到文件
        self._save_to_file(record)
        
        # 发送信号
        self.alarm_added.emit(record.to_dict())
        self.alarms_changed.emit()
        
        # 显示弹窗
        self._show_popup(record.to_dict())
        
    def acknowledge_alarm(self, alarm_code: int):
        """确认报警"""
        for alarm in self._active_alarms:
            if alarm.alarm_code == alarm_code:
                alarm.acknowledged = True
                self.alarm_acknowledged.emit(alarm_code)
                self.alarms_changed.emit()
                break
                
    def acknowledge_all(self):
        """确认所有报警并关闭弹窗"""
        for alarm in self._active_alarms:
            alarm.acknowledged = True
        
        # 强制关闭当前弹窗
        if self._current_popup:
            self._current_popup.close()
            self._current_popup = None
            
        # 清空队列
        self._popup_queue.clear()
        
        self.alarms_changed.emit()
        
    def clear_alarm(self, alarm_code: int):
        """清除报警（从活跃列表移除）"""
        self._active_alarms = [a for a in self._active_alarms if a.alarm_code != alarm_code]
        self.alarm_cleared.emit(alarm_code)
        self.alarms_changed.emit()
        
    def clear_all_active(self):
        """清除所有活跃报警"""
        self._active_alarms.clear()
        self.alarms_changed.emit()
        
    def clear_history(self):
        """清除历史记录"""
        self._history_alarms.clear()
        
        # 清空文件
        try:
            with open(self._log_file, 'w', encoding='utf-8') as f:
                json.dump([], f)
        except Exception as e:
            logger.error(f"清除历史记录失败: {e}", exc_info=True)
            
    def get_active_alarms(self) -> List[AlarmRecord]:
        """获取活跃报警列表"""
        return self._active_alarms.copy()
        
    def get_unacknowledged_alarms(self) -> List[AlarmRecord]:
        """获取未确认报警列表"""
        return [a for a in self._active_alarms if not a.acknowledged]
        
    def get_history_alarms(self) -> List[AlarmRecord]:
        """获取历史报警列表"""
        return self._history_alarms.copy()
        
    def has_unacknowledged(self) -> bool:
        """是否有未确认报警"""
        return any(not a.acknowledged for a in self._active_alarms)
        
    def _save_to_file(self, record: AlarmRecord):
        """保存报警到文件"""
        try:
            # 读取现有数据
            existing = []
            if os.path.exists(self._log_file):
                with open(self._log_file, 'r', encoding='utf-8') as f:
                    try:
                        existing = json.load(f)
                    except json.JSONDecodeError:
                        existing = []
                        
            # 添加新记录
            existing.append(record.to_dict())
            
            # 写回文件
            with open(self._log_file, 'w', encoding='utf-8') as f:
                json.dump(existing, f, ensure_ascii=False, indent=2)
                
        except Exception as e:
            logger.error(f"保存报警记录失败: {e}", exc_info=True)
            
    def _load_history(self):
        """加载历史记录"""
        try:
            if os.path.exists(self._log_file):
                with open(self._log_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    self._history_alarms = [AlarmRecord.from_dict(d) for d in data]
        except Exception as e:
            logger.error(f"加载历史记录失败: {e}", exc_info=True)
            self._history_alarms = []
            
    def _show_popup(self, alarm_data: dict):
        """显示报警弹窗"""
        if not self._popup_parent:
            return
            
        # 加入队列
        self._popup_queue.append(alarm_data)
        
        # 如果没有正在显示的弹窗，显示下一个
        if not self._current_popup:
            self._show_next_popup()
            
    def _show_next_popup(self):
        """显示下一个弹窗"""
        if not self._popup_queue:
            self._current_popup = None
            return
            
        alarm_data = self._popup_queue.pop(0)
        
        popup = AlarmPopup(alarm_data, self._popup_parent)
        popup.set_acknowledge_callback(self._on_popup_acknowledged)
        popup.show_centered(self._popup_parent)
        
        self._current_popup = popup
        
    def _on_popup_acknowledged(self, alarm_code: int):
        """弹窗确认回调 - 确认并直接清理"""
        self.acknowledge_alarm(alarm_code)
        self.clear_alarm(alarm_code)  # 确认后立即从活跃列表移除，确保界面立刻有反应
        
        # 延迟显示下一个报警（如果有）
        QTimer.singleShot(300, self._show_next_popup)


class AlarmIntegration:
    """报警集成 - 连接 TangoWorker 和 AlarmManager"""
    
    def __init__(self, worker, alarm_manager: AlarmManager):
        self._worker = worker
        self._alarm_manager = alarm_manager
        
        # 连接信号
        self._worker.alarm_received.connect(self._on_alarm_received)
        self._worker.command_result.connect(self._on_command_result)
        
    def _on_alarm_received(self, alarm_data: dict):
        """收到报警"""
        self._alarm_manager.add_alarm(alarm_data)

    def _on_command_result(self, cmd_name: str, success: bool, message: str):
        """处理命令结果 - 联动报警清理"""
        if cmd_name == "FaultReset" and success:
            # 点击故障复位成功后，自动确认并清理所有当前报警弹窗
            self._alarm_manager.acknowledge_all()
            self._alarm_manager.clear_all_active()

