"""
真空系统 GUI 模块

设备名: sys/vacuum/2
"""

from .config import *
from .styles import *
from .widgets import *
from .tango_worker import VacuumTangoWorker, MockTangoWorker
from .alarm_manager import AlarmManager, AlarmIntegration
from .main_page import VacuumSystemMainPage
from .digital_twin_page import DigitalTwinPage
from .record_pages import AlarmRecordPage, OperationHistoryPage, TrendCurvePage
from .main_window import VacuumSystemMainWindow, main

__version__ = "1.0.0"
__all__ = [
    "VacuumSystemMainWindow",
    "VacuumSystemMainPage",
    "DigitalTwinPage",
    "AlarmRecordPage",
    "OperationHistoryPage",
    "TrendCurvePage",
    "VacuumTangoWorker",
    "MockTangoWorker",
    "AlarmManager",
    "main"
]

