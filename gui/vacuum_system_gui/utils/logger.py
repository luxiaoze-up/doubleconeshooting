"""
日志配置模块
"""

import logging
import os
from pathlib import Path

# 全局日志器字典
_loggers = {}


def setup_logger(name: str, log_file: str = None, level: int = logging.INFO) -> logging.Logger:
    """
    设置日志器
    
    Args:
        name: 日志器名称
        log_file: 日志文件路径（可选）
        level: 日志级别
    
    Returns:
        配置好的日志器
    """
    # 如果已存在，直接返回
    if name in _loggers:
        return _loggers[name]
    
    logger = logging.getLogger(name)
    logger.setLevel(level)
    
    # 避免重复添加处理器
    if logger.handlers:
        _loggers[name] = logger
        return logger
    
    # 格式
    formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    
    # 控制台处理器（只显示 WARNING 及以上）
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.WARNING)
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)
    
    # 文件处理器（如果指定了日志文件）
    if log_file:
        # 确保日志目录存在
        log_path = Path(log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        
        file_handler = logging.FileHandler(log_file, encoding='utf-8')
        file_handler.setLevel(level)
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)
    
    _loggers[name] = logger
    return logger


def get_logger(name: str) -> logging.Logger:
    """
    获取日志器（如果不存在则创建默认配置）
    
    Args:
        name: 日志器名称
    
    Returns:
        日志器实例
    """
    if name in _loggers:
        return _loggers[name]
    
    # 默认配置
    return setup_logger(name, f'logs/{name}.log')

