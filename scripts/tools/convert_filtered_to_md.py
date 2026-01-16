#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
将过滤后的 PLC Tags Excel 文件转换为 Markdown 格式
"""

import pandas as pd
import os
import sys

def excel_to_markdown(input_file, output_file):
    """
    将 Excel 文件转换为 Markdown 格式
    """
    print(f"正在读取文件: {input_file}")
    
    xls = pd.ExcelFile(input_file)
    print(f"找到 {len(xls.sheet_names)} 个工作表: {xls.sheet_names}")
    
    md_content = f"# 真空集成1200 PLC Tags (已过滤)\n\n"
    md_content += f"**来源文件**: `{os.path.basename(input_file)}`\n\n"
    md_content += f"**说明**: 本文件已删除所有备用及无意义的 Tag\n\n"
    
    for sheet_name in xls.sheet_names:
        md_content += f"## {sheet_name}\n\n"
        df = pd.read_excel(xls, sheet_name=sheet_name)
        
        md_content += f"**总计**: {len(df)} 个 Tag\n\n"
        
        # 转换为 Markdown 表格
        # 只显示关键列
        display_cols = ['Name', 'Data Type', 'Logical Address', 'Comment']
        available_cols = [col for col in display_cols if col in df.columns]
        df_display = df[available_cols].fillna('')
        
        md_content += df_display.to_markdown(index=False) + "\n\n"
    
    # 保存 Markdown 文件
    print(f"正在保存到: {output_file}")
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(md_content)
    
    print(f"转换完成!")

if __name__ == '__main__':
    # 从 scripts/tools/ 向上两级到项目根目录
    workspace_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    input_file = os.path.join(workspace_root, 'docs', '接口定义', '真空集成1200PLCTags_已过滤.xlsx')
    output_file = os.path.join(workspace_root, 'docs', '接口定义', '真空集成1200PLCTags_已过滤.md')
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    
    if not os.path.exists(input_file):
        print(f"错误: 输入文件不存在: {input_file}")
        sys.exit(1)
    
    excel_to_markdown(input_file, output_file)
