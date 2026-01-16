#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
过滤 PLC Tags Excel 文件，删除备用及无意义的 Tag
"""

import pandas as pd
import os
import sys

def is_backup_or_meaningless_tag(name, comment=None):
    """
    判断是否为备用或无意义的 Tag
    """
    if pd.isna(name) or name == '':
        return True
    
    name_str = str(name).strip()
    
    # 判断规则：
    # 1. 名称包含"备用"、"预留"、"预留"、"spare"、"backup"、"reserve"
    # 2. 名称是通用标签如 Tag_1, Tag_2 等（无意义的命名）
    # 3. 名称是临时变量如"上升沿暂存"、"暂存"等
    # 4. 名称是系统内部变量如"System_Byte"、"Clock_Byte"等（这些通常不需要在接口中暴露）
    
    backup_keywords = ['备用', '预留', 'spare', 'backup', 'reserve', '预留']
    temp_keywords = ['暂存', 'temp', '临时']
    system_keywords = ['System_', 'Clock_', 'FirstScan', 'DiagStatus', 'AlwaysTRUE', 'AlwaysFALSE', '常闭']
    generic_tag_patterns = ['Tag_', 'tag_', 'TAG_']
    
    # 检查是否包含备用关键词
    for keyword in backup_keywords:
        if keyword in name_str:
            return True
    
    # 检查是否为通用标签（Tag_数字格式）
    for pattern in generic_tag_patterns:
        if name_str.startswith(pattern):
            # 检查是否为 Tag_数字 格式
            remaining = name_str[len(pattern):]
            if remaining.isdigit() or remaining == '':
                return True
    
    # 检查是否为临时变量
    for keyword in temp_keywords:
        if keyword in name_str:
            return True
    
    # 检查是否为系统内部变量（但保留一些可能有用的）
    for keyword in system_keywords:
        if keyword in name_str:
            return True
    
    return False

def parse_sheet2_data(df_raw):
    """
    解析 Sheet 2 的特殊格式数据
    格式: "名称 类型 地址"
    """
    result_rows = []
    for idx, row in df_raw.iterrows():
        value = str(row.iloc[0]).strip()
        # 跳过标题行和空行
        if value == '' or value == 'Static' or value.startswith('Static'):
            continue
        
        # 解析格式: "名称 类型 地址"
        parts = value.split()
        if len(parts) >= 2:
            name = parts[0]
            data_type = parts[1] if len(parts) > 1 else ''
            address = ' '.join(parts[2:]) if len(parts) > 2 else ''
            result_rows.append({
                'Name': name,
                'Data Type': data_type,
                'Logical Address': address,
                'Path': 'Static',
                'Comment': '',
                'Hmi Visible': True,
                'Hmi Accessible': True,
                'Hmi Writeable': True,
                'Typeobject ID': '',
                'Version ID': ''
            })
    
    return pd.DataFrame(result_rows)

def filter_plc_tags(input_file, output_file):
    """
    过滤 PLC Tags Excel 文件
    """
    print(f"正在读取文件: {input_file}")
    
    # 读取所有工作表
    xls = pd.ExcelFile(input_file)
    print(f"找到 {len(xls.sheet_names)} 个工作表: {xls.sheet_names}")
    
    filtered_data = {}
    total_removed = 0
    total_kept = 0
    
    for sheet_name in xls.sheet_names:
        print(f"\n处理工作表: {sheet_name}")
        df_raw = pd.read_excel(xls, sheet_name=sheet_name)
        print(f"  原始行数: {len(df_raw)}")
        
        # 检查是否为 Sheet 2 格式（只有一列且第一行是 "Static"）
        if len(df_raw.columns) == 1 and 'Name' not in df_raw.columns:
            # Sheet 2 格式，需要解析
            df = parse_sheet2_data(df_raw)
            print(f"  解析后行数: {len(df)}")
        else:
            # Sheet 1 格式，直接使用
            df = df_raw.copy()
        
        # 过滤数据
        if 'Name' in df.columns:
            mask = df['Name'].apply(lambda x: not is_backup_or_meaningless_tag(x))
            filtered_df = df[mask].copy()
        else:
            # 如果没有 Name 列，保留所有数据
            print(f"  警告: 未找到 'Name' 列，跳过过滤")
            filtered_df = df.copy()
        
        removed_count = len(df) - len(filtered_df)
        kept_count = len(filtered_df)
        
        print(f"  保留行数: {kept_count}")
        print(f"  删除行数: {removed_count}")
        
        # 显示被删除的 Tag 名称
        if removed_count > 0 and 'Name' in df.columns:
            removed_tags = df[~mask]['Name'].tolist()
            print(f"  删除的 Tag:")
            for tag in removed_tags:
                print(f"    - {tag}")
        
        filtered_data[sheet_name] = filtered_df
        total_removed += removed_count
        total_kept += kept_count
    
    # 保存到新文件
    print(f"\n正在保存到: {output_file}")
    with pd.ExcelWriter(output_file, engine='openpyxl') as writer:
        for sheet_name, df in filtered_data.items():
            df.to_excel(writer, sheet_name=sheet_name, index=False)
    
    print(f"\n处理完成!")
    print(f"总计保留: {total_kept} 个 Tag")
    print(f"总计删除: {total_removed} 个 Tag")
    print(f"输出文件: {output_file}")

if __name__ == '__main__':
    # 从 scripts/tools/ 向上两级到项目根目录
    workspace_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    input_file = os.path.join(workspace_root, 'docs', '接口定义', '真空集成1200PLCTags.xlsx')
    output_file = os.path.join(workspace_root, 'docs', '接口定义', '真空集成1200PLCTags_已过滤.xlsx')
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    
    if not os.path.exists(input_file):
        print(f"错误: 输入文件不存在: {input_file}")
        sys.exit(1)
    
    filter_plc_tags(input_file, output_file)
