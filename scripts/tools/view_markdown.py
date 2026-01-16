#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Markdown文件查看器 - 提供友好的Web界面查看Markdown文件
"""

import os
import sys
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import markdown
from pathlib import Path

class MarkdownViewerHandler(BaseHTTPRequestHandler):
    """Markdown查看器HTTP处理器"""
    
    def do_GET(self):
        """处理GET请求"""
        parsed_path = urlparse(self.path)
        query_params = parse_qs(parsed_path.query)
        
        if parsed_path.path == '/':
            # 显示文件列表
            self.send_response(200)
            self.send_header('Content-type', 'text/html; charset=utf-8')
            self.end_headers()
            self.wfile.write(self.get_file_list_html().encode('utf-8'))
        elif parsed_path.path == '/view':
            # 查看指定文件
            file_path = query_params.get('file', [None])[0]
            if file_path:
                self.send_response(200)
                self.send_header('Content-type', 'text/html; charset=utf-8')
                self.end_headers()
                self.wfile.write(self.get_markdown_html(file_path).encode('utf-8'))
            else:
                self.send_error(400, "Missing file parameter")
        else:
            self.send_error(404, "Not Found")
    
    def get_file_list_html(self):
        """生成文件列表HTML"""
        docs_dir = Path(self.server.docs_dir)
        md_files = sorted(docs_dir.rglob('*.md'))
        
        html = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Markdown文件查看器</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            overflow: hidden;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }}
        .header h1 {{
            font-size: 28px;
            margin-bottom: 10px;
        }}
        .header p {{
            opacity: 0.9;
        }}
        .file-list {{
            padding: 30px;
        }}
        .file-item {{
            background: #f8f9fa;
            border: 1px solid #e9ecef;
            border-radius: 8px;
            padding: 20px;
            margin-bottom: 15px;
            transition: all 0.3s;
            cursor: pointer;
        }}
        .file-item:hover {{
            background: #e9ecef;
            transform: translateX(5px);
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }}
        .file-item h3 {{
            color: #667eea;
            margin-bottom: 8px;
            font-size: 18px;
        }}
        .file-item .path {{
            color: #6c757d;
            font-size: 14px;
            font-family: 'Courier New', monospace;
        }}
        .empty {{
            text-align: center;
            padding: 60px 20px;
            color: #6c757d;
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📄 Markdown文件查看器</h1>
            <p>选择要查看的文件</p>
        </div>
        <div class="file-list">
"""
        
        if md_files:
            for md_file in md_files:
                rel_path = md_file.relative_to(docs_dir)
                html += f"""
            <div class="file-item" onclick="window.location.href='/view?file={rel_path.as_posix()}'">
                <h3>{md_file.name}</h3>
                <div class="path">{rel_path.as_posix()}</div>
            </div>
"""
        else:
            html += """
            <div class="empty">
                <h2>📭 没有找到Markdown文件</h2>
                <p>请确保docs目录下有.md文件</p>
            </div>
"""
        
        html += """
        </div>
    </div>
</body>
</html>
"""
        return html
    
    def get_markdown_html(self, file_path):
        """生成Markdown渲染HTML"""
        docs_dir = Path(self.server.docs_dir)
        full_path = docs_dir / file_path
        
        if not full_path.exists() or not full_path.is_file():
            return f"<html><body><h1>文件不存在: {file_path}</h1></body></html>"
        
        try:
            with open(full_path, 'r', encoding='utf-8') as f:
                md_content = f.read()
            
            # 转换为HTML
            html_content = markdown.markdown(
                md_content,
                extensions=['tables', 'fenced_code', 'codehilite', 'toc']
            )
            
            # 后处理：确保表格第一行有正确的类名
            import re
            # 为所有表格的第一行添加表头样式
            html_content = re.sub(
                r'<table>',
                r'<table class="markdown-table">',
                html_content
            )
            
            return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{Path(file_path).name} - Markdown查看器</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/github-markdown-css/5.2.0/github-markdown.min.css">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css">
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', sans-serif;
            background: #f5f5f5;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }}
        .header-content {{
            max-width: 1200px;
            margin: 0 auto;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }}
        .header h1 {{
            font-size: 24px;
            margin-bottom: 5px;
        }}
        .header .path {{
            opacity: 0.9;
            font-size: 14px;
            font-family: 'Courier New', monospace;
        }}
        .back-btn {{
            background: rgba(255,255,255,0.2);
            color: white;
            border: 1px solid rgba(255,255,255,0.3);
            padding: 8px 16px;
            border-radius: 5px;
            text-decoration: none;
            transition: all 0.3s;
        }}
        .back-btn:hover {{
            background: rgba(255,255,255,0.3);
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            padding: 30px 20px;
        }}
        .markdown-body {{
            background: white;
            padding: 40px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }}
        .markdown-body table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
            font-size: 14px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
            border-radius: 6px;
            overflow: hidden;
            background: white;
        }}
        /* 所有表格行和单元格的基础样式 */
        .markdown-body table tr {{
            background: #ffffff !important;
        }}
        .markdown-body table th,
        .markdown-body table td {{
            border: 1px solid #c8d6e5 !important;
            padding: 12px 16px !important;
            text-align: left !important;
            color: #2c3e50 !important;
            background: #ffffff !important;
        }}
        /* 表头样式 - 浅蓝色背景，白色文字 */
        .markdown-body table thead th,
        .markdown-body table tr:first-child th,
        .markdown-body table tr:first-child td {{
            background: linear-gradient(135deg, #4a90e2 0%, #5ba3f5 100%) !important;
            color: #ffffff !important;
            font-weight: 600 !important;
            text-shadow: 0 1px 2px rgba(0,0,0,0.15);
        }}
        /* 数据行样式 */
        .markdown-body table tbody tr {{
            background: #ffffff !important;
            transition: background 0.2s ease;
        }}
        .markdown-body table tbody tr:nth-child(even),
        .markdown-body table tr:nth-child(even) {{
            background: #e8f4fd !important;
        }}
        .markdown-body table tbody tr:nth-child(even) td,
        .markdown-body table tr:nth-child(even) td {{
            background: #e8f4fd !important;
        }}
        .markdown-body table tbody tr:hover,
        .markdown-body table tr:hover {{
            background: #d0e8f7 !important;
        }}
        .markdown-body table tbody tr:hover td,
        .markdown-body table tr:hover td {{
            background: #d0e8f7 !important;
        }}
        /* 确保所有数据单元格都有正确的文字颜色 */
        .markdown-body table tbody td,
        .markdown-body table td {{
            color: #2c3e50 !important;
        }}
        /* 处理空单元格 */
        .markdown-body table td:empty {{
            background: inherit !important;
        }}
        /* 强制覆盖所有可能的表格样式 */
        .markdown-body table * {{
            box-sizing: border-box;
        }}
        /* 确保所有文字都可见 */
        .markdown-body table th,
        .markdown-body table td {{
            min-height: 40px;
        }}
        /* 处理可能的黑色背景单元格 */
        .markdown-body table td[style*="background"],
        .markdown-body table th[style*="background"] {{
            background: inherit !important;
        }}
        .markdown-body table td[style*="color"],
        .markdown-body table th[style*="color"] {{
            color: inherit !important;
        }}
        .markdown-body h1 {{
            border-bottom: 2px solid #eaecef;
            padding-bottom: 10px;
            margin-bottom: 20px;
        }}
        .markdown-body h2 {{
            border-bottom: 1px solid #eaecef;
            padding-bottom: 8px;
            margin-top: 30px;
            margin-bottom: 15px;
        }}
        code {{
            background: #f6f8fa;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
        }}
        pre {{
            background: #f6f8fa;
            padding: 16px;
            border-radius: 6px;
            overflow-x: auto;
        }}
        pre code {{
            background: none;
            padding: 0;
        }}
    </style>
</head>
<body>
    <div class="header">
        <div class="header-content">
            <div>
                <h1>📄 {Path(file_path).name}</h1>
                <div class="path">{file_path}</div>
            </div>
            <a href="/" class="back-btn">← 返回文件列表</a>
        </div>
    </div>
    <div class="container">
        <article class="markdown-body">
            {html_content}
        </article>
    </div>
    <script>
        // 高亮代码
        hljs.highlightAll();
    </script>
</body>
</html>
"""
        except Exception as e:
            return f"<html><body><h1>错误</h1><p>{str(e)}</p></body></html>"
    
    def log_message(self, format, *args):
        """禁用默认日志输出"""
        pass


def start_server(docs_dir, port=8000):
    """启动Markdown查看器服务器"""
    server_address = ('', port)
    httpd = HTTPServer(server_address, MarkdownViewerHandler)
    httpd.docs_dir = docs_dir
    
    url = f'http://localhost:{port}'
    print(f"\n{'='*60}")
    print(f"📄 Markdown文件查看器已启动")
    print(f"{'='*60}")
    print(f"🌐 访问地址: {url}")
    print(f"📁 文档目录: {docs_dir}")
    print(f"{'='*60}")
    print(f"\n按 Ctrl+C 停止服务器\n")
    
    # 自动打开浏览器
    try:
        webbrowser.open(url)
    except:
        pass
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\n服务器已停止")
        httpd.shutdown()


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='Markdown文件查看器')
    parser.add_argument('--port', type=int, default=8000, help='服务器端口 (默认: 8000)')
    parser.add_argument('--dir', type=str, help='文档目录路径 (默认: docs/接口定义)')
    
    args = parser.parse_args()
    
    # 确定文档目录（从 scripts/tools/ 向上两级到项目根目录）
    script_dir = Path(__file__).parent
    workspace_root = script_dir.parent.parent
    
    if args.dir:
        docs_dir = Path(args.dir).resolve()
    else:
        docs_dir = workspace_root / "docs" / "接口定义"
    
    if not docs_dir.exists():
        print(f"错误: 目录不存在: {docs_dir}")
        sys.exit(1)
    
    start_server(str(docs_dir), args.port)

