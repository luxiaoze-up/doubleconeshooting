#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
真空系统PLC节点检查工具

根据vacuum_system_plc_mapping.h中定义的节点，检查这些节点是否存在以及正确的NodeID格式
"""

import sys
import json
import argparse
from typing import Dict, List, Tuple

try:
    from opcua import Client, ua
    OPCUA_AVAILABLE = True
except ImportError:
    print("错误: python-opcua 未安装")
    print("请安装: pip install opcua")
    sys.exit(1)


# 根据vacuum_system_plc_mapping.h定义的所有节点地址
VACUUM_SYSTEM_NODES = {
    # ========== 输入信号 (Bool) ==========
    "ScrewPumpPowerFeedback": "%I0.0",
    "RootsPumpPowerFeedback": "%I0.1",
    "MolecularPump1PowerFeedback": "%I0.2",
    "MolecularPump2PowerFeedback": "%I0.3",
    "MolecularPump3PowerFeedback": "%I0.4",
    "PhaseSequenceProtection": "%I0.5",
    "ElectromagneticValve1OpenFeedback": "%I0.6",
    "ElectromagneticValve1CloseFeedback": "%I0.7",
    "ElectromagneticValve2OpenFeedback": "%I1.0",
    "ElectromagneticValve2CloseFeedback": "%I1.1",
    "ElectromagneticValve3OpenFeedback": "%I1.2",
    "ElectromagneticValve3CloseFeedback": "%I1.3",
    "ElectromagneticValve4OpenFeedback": "%I1.4",
    "ElectromagneticValve4CloseFeedback": "%I1.5",
    "VentValve1OpenFeedback": "%I8.0",
    "VentValve1CloseFeedback": "%I8.1",
    "VentValve2OpenFeedback": "%I8.2",
    "VentValve2CloseFeedback": "%I8.3",
    "GateValve1OpenFeedback": "%I8.4",
    "GateValve1CloseFeedback": "%I8.5",
    "GateValve2OpenFeedback": "%I8.6",
    "GateValve2CloseFeedback": "%I8.7",
    "GateValve3OpenFeedback": "%I9.0",
    "GateValve3CloseFeedback": "%I9.1",
    "GateValve4OpenFeedback": "%I9.2",
    "GateValve4CloseFeedback": "%I9.3",
    "GateValve5OpenFeedback": "%I9.4",
    "GateValve5CloseFeedback": "%I9.5",
    "MotionControlSystemOnline": "%I9.6",
    "GateValve5ActionPermit": "%I9.7",
    "MotionControlRequestOpenGateValve5": "%I12.0",
    "MotionControlRequestCloseGateValve5": "%I12.1",
    
    # ========== 模拟量输入 (Word) ==========
    "ResistanceGaugeVoltage": "%IW130",
    "AirPressureSensorCurrent": "%IW132",
    "MolecularPump1Speed": "%IW24",
    "MolecularPump2Speed": "%IW36",
    "MolecularPump3Speed": "%IW48",
    
    # ========== 输出信号 (Bool) ==========
    "ScrewPumpStartStop": "%Q0.0",
    "ScrewPumpPowerOutput": "%Q0.1",
    "RootsPumpPowerOutput": "%Q0.2",
    "MolecularPump1PowerOutput": "%Q0.3",
    "MolecularPump2PowerOutput": "%Q0.4",
    "MolecularPump3PowerOutput": "%Q0.5",
    "ElectromagneticValve1Output": "%Q0.6",
    "ElectromagneticValve2Output": "%Q0.7",
    "ElectromagneticValve3Output": "%Q1.0",
    "ElectromagneticValve4Output": "%Q8.0",
    "VentValve1Output": "%Q8.1",
    "VentValve2Output": "%Q8.2",
    "GateValve1OpenOutput": "%Q8.3",
    "GateValve1CloseOutput": "%Q8.4",
    "GateValve2OpenOutput": "%Q8.5",
    "GateValve2CloseOutput": "%Q8.6",
    "GateValve3OpenOutput": "%Q8.7",
    "GateValve3CloseOutput": "%Q9.0",
    "GateValve4OpenOutput": "%Q9.1",
    "GateValve4CloseOutput": "%Q9.2",
    "GateValve5OpenOutput": "%Q9.3",
    "GateValve5CloseOutput": "%Q9.4",
    "WaterValve1Output": "%Q12.0",
    "WaterValve2Output": "%Q12.1",
    "WaterValve3Output": "%Q12.2",
    "WaterValve4Output": "%Q12.3",
    "WaterValve5Output": "%Q12.4",
    "WaterValve6Output": "%Q12.5",
    "AirMainValveOutput": "%Q12.6",
    "ScrewPumpFaultReset": "%Q12.7",
    "MolecularPump1StartStop": "%Q13.0",
    "MolecularPump2StartStop": "%Q13.1",
    "MolecularPump3StartStop": "%Q13.2",
    "MolecularPump1Enabled": "%Q13.3",
    "MolecularPump2Enabled": "%Q13.4",
    "MolecularPump3Enabled": "%Q13.5",
    
    # ========== 模拟量输出 (Word) ==========
    "MolecularPump1AddressTransfer": "%QW22",
    "MolecularPump2AddressTransfer": "%QW34",
    "MolecularPump3AddressTransfer": "%QW46",
}


class VacuumNodeChecker:
    """真空系统节点检查器"""
    
    def __init__(self, ip: str = "192.168.1.100", port: int = 4840):
        self.url = f"opc.tcp://{ip}:{port}"
        self.client = None
        self.results = []
        
    def connect(self) -> bool:
        """连接到PLC"""
        try:
            print(f"正在连接到 {self.url}...")
            self.client = Client(self.url)
            self.client.connect()
            print(f"✓ 成功连接到 {self.url}\n")
            return True
        except Exception as e:
            print(f"✗ 连接失败: {e}")
            return False
            
    def disconnect(self):
        """断开连接"""
        if self.client:
            try:
                self.client.disconnect()
                print("\n已断开连接")
            except:
                pass
    
    def try_node_id_formats(self, name: str, plc_address: str) -> Tuple[bool, str, str, any]:
        """
        尝试多种NodeID格式
        
        Returns:
            (success, node_id, data_type, value)
        """
        # 可能的NodeID格式
        formats = [
            f"ns=3;s={plc_address}",  # C++代码中使用的格式
            f"ns=4;s=|var|CODESYS Control Win V3 x64.Application.GVL.gVacuumSystem.{name}",  # CODESYS格式
            f"ns=4;s={plc_address}",  # namespace 4
            f"ns=2;s={plc_address}",  # namespace 2
        ]
        
        for node_id_str in formats:
            try:
                node = self.client.get_node(node_id_str)
                
                # 尝试读取值
                data_value = node.get_data_value()
                value = data_value.Value.Value
                variant_type = data_value.Value.VariantType
                data_type = str(variant_type).split('.')[-1]
                
                return (True, node_id_str, data_type, value)
            except:
                continue
                
        return (False, None, None, None)
    
    def check_all_nodes(self):
        """检查所有节点"""
        print("="*80)
        print("开始检查真空系统节点...")
        print(f"共需检查 {len(VACUUM_SYSTEM_NODES)} 个节点")
        print("="*80 + "\n")
        
        success_count = 0
        failed_nodes = []
        
        for name, plc_address in VACUUM_SYSTEM_NODES.items():
            success, node_id, data_type, value = self.try_node_id_formats(name, plc_address)
            
            if success:
                success_count += 1
                status = "✓"
                print(f"{status} {name:45s} {plc_address:10s} → {node_id}")
                print(f"   类型: {data_type:15s} 值: {value}")
                
                self.results.append({
                    "name": name,
                    "plc_address": plc_address,
                    "node_id": node_id,
                    "data_type": data_type,
                    "value": str(value),
                    "success": True
                })
            else:
                status = "✗"
                print(f"{status} {name:45s} {plc_address:10s} → 未找到")
                failed_nodes.append((name, plc_address))
                
                self.results.append({
                    "name": name,
                    "plc_address": plc_address,
                    "node_id": None,
                    "data_type": None,
                    "value": None,
                    "success": False
                })
        
        # 输出总结
        print("\n" + "="*80)
        print(f"检查完成:")
        print(f"  成功: {success_count}/{len(VACUUM_SYSTEM_NODES)}")
        print(f"  失败: {len(failed_nodes)}/{len(VACUUM_SYSTEM_NODES)}")
        print("="*80)
        
        if failed_nodes:
            print("\n未找到的节点:")
            for name, addr in failed_nodes:
                print(f"  - {name} ({addr})")
    
    def export_mapping_code(self, filename: str):
        """导出Python映射代码"""
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                f.write('#!/usr/bin/env python\n')
                f.write('# -*- coding: utf-8 -*-\n')
                f.write('"""\n')
                f.write('真空系统PLC节点映射 - 自动生成\n')
                f.write('"""\n\n')
                
                # 写入namespace前缀
                # 根据第一个成功的节点确定namespace
                namespace = None
                for result in self.results:
                    if result['success'] and result['node_id']:
                        # 提取namespace (例如 "ns=3;s=..." -> "ns=3;s=")
                        node_id = result['node_id']
                        if ';s=' in node_id:
                            namespace = node_id.split(';s=')[0] + ';s='
                            break
                
                if namespace:
                    f.write(f'# Namespace前缀\n')
                    f.write(f'NAMESPACE_PREFIX = "{namespace}"\n\n')
                
                f.write('# ========== 输入信号 (Bool) ==========\n')
                for result in self.results:
                    if result['plc_address'].startswith('%I') and '.' in result['plc_address']:
                        if result['success']:
                            f.write(f"{result['name']} = \"{result['node_id']}\"\n")
                        else:
                            f.write(f"# {result['name']} = None  # 未找到 ({result['plc_address']})\n")
                
                f.write('\n# ========== 模拟量输入 (Word) ==========\n')
                for result in self.results:
                    if result['plc_address'].startswith('%IW'):
                        if result['success']:
                            f.write(f"{result['name']} = \"{result['node_id']}\"\n")
                        else:
                            f.write(f"# {result['name']} = None  # 未找到 ({result['plc_address']})\n")
                
                f.write('\n# ========== 输出信号 (Bool) ==========\n')
                for result in self.results:
                    if result['plc_address'].startswith('%Q') and '.' in result['plc_address']:
                        if result['success']:
                            f.write(f"{result['name']} = \"{result['node_id']}\"\n")
                        else:
                            f.write(f"# {result['name']} = None  # 未找到 ({result['plc_address']})\n")
                
                f.write('\n# ========== 模拟量输出 (Word) ==========\n')
                for result in self.results:
                    if result['plc_address'].startswith('%QW'):
                        if result['success']:
                            f.write(f"{result['name']} = \"{result['node_id']}\"\n")
                        else:
                            f.write(f"# {result['name']} = None  # 未找到 ({result['plc_address']})\n")
            
            print(f"\n✓ Python映射代码已导出到: {filename}")
        except Exception as e:
            print(f"✗ 导出失败: {e}")
    
    def export_to_json(self, filename: str):
        """导出JSON格式结果"""
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                json.dump(self.results, f, ensure_ascii=False, indent=2)
            print(f"✓ JSON结果已导出到: {filename}")
        except Exception as e:
            print(f"✗ 导出失败: {e}")


def main():
    parser = argparse.ArgumentParser(
        description="真空系统PLC节点检查工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 检查所有节点
  python check_vacuum_nodes.py
  
  # 指定PLC地址
  python check_vacuum_nodes.py --ip 192.168.1.100 --port 4840
  
  # 导出映射代码
  python check_vacuum_nodes.py --export-mapping plc_nodes_mapping.py
  
  # 导出JSON结果
  python check_vacuum_nodes.py --export-json vacuum_nodes.json
        """
    )
    
    parser.add_argument('--ip', default='192.168.1.100',
                        help='PLC IP地址 (默认: 192.168.1.100)')
    parser.add_argument('--port', type=int, default=4840,
                        help='OPC UA端口 (默认: 4840)')
    parser.add_argument('--export-mapping', default=None,
                        help='导出Python映射代码到指定文件')
    parser.add_argument('--export-json', default=None,
                        help='导出JSON结果到指定文件')
    
    args = parser.parse_args()
    
    # 创建检查器
    checker = VacuumNodeChecker(args.ip, args.port)
    
    # 连接到PLC
    if not checker.connect():
        return 1
    
    try:
        # 检查所有节点
        checker.check_all_nodes()
        
        # 导出结果
        if args.export_mapping:
            checker.export_mapping_code(args.export_mapping)
        if args.export_json:
            checker.export_to_json(args.export_json)
            
    finally:
        checker.disconnect()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
