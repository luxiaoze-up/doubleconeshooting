#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
PLC OPC UA 节点浏览工具

用于浏览PLC的OPC UA节点树，找到正确的节点ID
"""

import sys
import json
import argparse
from typing import List, Dict, Any

try:
    from opcua import Client, ua
    OPCUA_AVAILABLE = True
except ImportError:
    print("错误: python-opcua 未安装")
    print("请安装: pip install opcua")
    sys.exit(1)


class PLCNodeBrowser:
    """PLC节点浏览器"""
    
    def __init__(self, ip: str = "192.168.1.100", port: int = 4840):
        self.url = f"opc.tcp://{ip}:{port}"
        self.client = None
        self.nodes_data = []
        
    def connect(self) -> bool:
        """连接到PLC"""
        try:
            print(f"正在连接到 {self.url}...")
            self.client = Client(self.url)
            self.client.connect()
            
            # 验证连接
            root = self.client.get_root_node()
            _ = root.get_browse_name()
            
            print(f"✓ 成功连接到 {self.url}")
            return True
        except Exception as e:
            print(f"✗ 连接失败: {e}")
            return False
            
    def disconnect(self):
        """断开连接"""
        if self.client:
            try:
                self.client.disconnect()
                print("已断开连接")
            except:
                pass
                
    def browse_node(self, node, level: int = 0, max_level: int = 10, parent_path: str = ""):
        """
        递归浏览节点
        
        Args:
            node: 要浏览的节点
            level: 当前层级
            max_level: 最大递归层级
            parent_path: 父节点路径
        """
        if level > max_level:
            return
            
        try:
            # 获取节点信息
            node_id = node.nodeid.to_string()
            browse_name = node.get_browse_name().Name
            display_name = node.get_display_name().Text
            node_class = node.get_node_class()
            
            # 构建路径
            current_path = f"{parent_path}/{browse_name}" if parent_path else browse_name
            
            # 尝试获取数据类型
            data_type = "Unknown"
            value = None
            try:
                if node_class == ua.NodeClass.Variable:
                    data_value = node.get_data_value()
                    value = data_value.Value.Value
                    variant_type = data_value.Value.VariantType
                    data_type = str(variant_type).split('.')[-1]
            except:
                pass
            
            # 记录节点信息
            node_info = {
                "level": level,
                "path": current_path,
                "node_id": node_id,
                "browse_name": browse_name,
                "display_name": display_name,
                "node_class": str(node_class).split('.')[-1],
                "data_type": data_type,
                "value": str(value) if value is not None else None
            }
            self.nodes_data.append(node_info)
            
            # 打印节点信息
            indent = "  " * level
            class_str = str(node_class).split('.')[-1]
            
            if node_class == ua.NodeClass.Variable and value is not None:
                print(f"{indent}├─ [{class_str}] {display_name} ({data_type}) = {value}")
                print(f"{indent}│  NodeID: {node_id}")
            else:
                print(f"{indent}├─ [{class_str}] {display_name}")
                print(f"{indent}│  NodeID: {node_id}")
            
            # 递归浏览子节点
            try:
                children = node.get_children()
                for child in children:
                    self.browse_node(child, level + 1, max_level, current_path)
            except:
                pass
                
        except Exception as e:
            indent = "  " * level
            print(f"{indent}├─ [ERROR] 无法读取节点: {e}")
            
    def browse_from_root(self, max_level: int = 10):
        """从根节点开始浏览"""
        print("\n" + "="*80)
        print("开始浏览节点树...")
        print("="*80 + "\n")
        
        root = self.client.get_root_node()
        self.browse_node(root, level=0, max_level=max_level)
        
        print("\n" + "="*80)
        print(f"浏览完成，共找到 {len(self.nodes_data)} 个节点")
        print("="*80)
        
    def browse_from_path(self, path: str, max_level: int = 5):
        """从指定路径开始浏览"""
        print(f"\n从路径 '{path}' 开始浏览...")
        print("="*80 + "\n")
        
        try:
            # 尝试按路径查找节点
            root = self.client.get_root_node()
            node = root
            
            # 解析路径（例如：Objects/Application/GVL）
            parts = [p for p in path.split('/') if p]
            for part in parts:
                children = node.get_children()
                found = False
                for child in children:
                    if child.get_browse_name().Name == part:
                        node = child
                        found = True
                        break
                if not found:
                    print(f"✗ 找不到路径: {part}")
                    return
                    
            print(f"✓ 找到节点: {node.get_display_name().Text}")
            self.browse_node(node, level=0, max_level=max_level)
            
        except Exception as e:
            print(f"✗ 浏览失败: {e}")
            
    def search_nodes(self, keyword: str, case_sensitive: bool = False):
        """搜索节点"""
        print(f"\n搜索包含 '{keyword}' 的节点...")
        print("="*80 + "\n")
        
        if not case_sensitive:
            keyword = keyword.lower()
            
        matches = []
        for node_info in self.nodes_data:
            search_text = f"{node_info['display_name']} {node_info['browse_name']} {node_info['path']}"
            if not case_sensitive:
                search_text = search_text.lower()
                
            if keyword in search_text:
                matches.append(node_info)
                
        if matches:
            print(f"找到 {len(matches)} 个匹配的节点:\n")
            for i, node in enumerate(matches, 1):
                print(f"{i}. {node['display_name']} ({node['node_class']})")
                print(f"   路径: {node['path']}")
                print(f"   NodeID: {node['node_id']}")
                if node['value']:
                    print(f"   值: {node['value']} ({node['data_type']})")
                print()
        else:
            print("没有找到匹配的节点")
            
    def export_to_json(self, filename: str):
        """导出节点数据到JSON文件"""
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                json.dump(self.nodes_data, f, ensure_ascii=False, indent=2)
            print(f"✓ 节点数据已导出到: {filename}")
        except Exception as e:
            print(f"✗ 导出失败: {e}")
            
    def export_to_text(self, filename: str):
        """导出节点数据到文本文件"""
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                f.write("PLC OPC UA 节点列表\n")
                f.write("="*80 + "\n\n")
                
                for node in self.nodes_data:
                    indent = "  " * node['level']
                    f.write(f"{indent}{node['display_name']} ({node['node_class']})\n")
                    f.write(f"{indent}  NodeID: {node['node_id']}\n")
                    f.write(f"{indent}  Path: {node['path']}\n")
                    if node['value']:
                        f.write(f"{indent}  Value: {node['value']} ({node['data_type']})\n")
                    f.write("\n")
                    
            print(f"✓ 节点数据已导出到: {filename}")
        except Exception as e:
            print(f"✗ 导出失败: {e}")


def main():
    parser = argparse.ArgumentParser(
        description="PLC OPC UA 节点浏览工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 浏览整个节点树（默认）
  python browse_plc_nodes.py
  
  # 指定PLC地址
  python browse_plc_nodes.py --ip 192.168.1.100 --port 4840
  
  # 从指定路径开始浏览
  python browse_plc_nodes.py --path "Objects/Application/GVL"
  
  # 限制浏览深度
  python browse_plc_nodes.py --max-level 3
  
  # 搜索特定节点
  python browse_plc_nodes.py --search "Vacuum"
  
  # 导出节点信息
  python browse_plc_nodes.py --export-json nodes.json
  python browse_plc_nodes.py --export-text nodes.txt
        """
    )
    
    parser.add_argument('--ip', default='192.168.1.100',
                        help='PLC IP地址 (默认: 192.168.1.100)')
    parser.add_argument('--port', type=int, default=4840,
                        help='OPC UA端口 (默认: 4840)')
    parser.add_argument('--path', default=None,
                        help='从指定路径开始浏览 (例如: Objects/Application)')
    parser.add_argument('--max-level', type=int, default=10,
                        help='最大浏览深度 (默认: 10)')
    parser.add_argument('--search', default=None,
                        help='搜索包含指定关键字的节点')
    parser.add_argument('--case-sensitive', action='store_true',
                        help='搜索时区分大小写')
    parser.add_argument('--export-json', default=None,
                        help='导出节点信息到JSON文件')
    parser.add_argument('--export-text', default=None,
                        help='导出节点信息到文本文件')
    
    args = parser.parse_args()
    
    # 创建浏览器
    browser = PLCNodeBrowser(args.ip, args.port)
    
    # 连接到PLC
    if not browser.connect():
        return 1
        
    try:
        # 浏览节点
        if args.path:
            browser.browse_from_path(args.path, args.max_level)
        else:
            browser.browse_from_root(args.max_level)
            
        # 搜索节点
        if args.search:
            browser.search_nodes(args.search, args.case_sensitive)
            
        # 导出数据
        if args.export_json:
            browser.export_to_json(args.export_json)
        if args.export_text:
            browser.export_to_text(args.export_text)
            
    finally:
        browser.disconnect()
        
    return 0


if __name__ == "__main__":
    sys.exit(main())
