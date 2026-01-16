#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
项目核心代码、脚本和配置文件备份工具

功能：
    - 备份核心源代码（src/, include/）
    - 备份脚本文件（scripts/）
    - 备份配置文件（config/）
    - 备份构建文件（CMakeLists.txt等）
    - 备份GUI代码（gui/）
    - 备份文档（docs/）
    - 备份根目录重要文件（README.md等）

使用方法:
    python scripts/backup_project.py [--output-dir OUTPUT_DIR]
    
示例:
    python scripts/backup_project.py
    python scripts/backup_project.py --output-dir D:/backups
"""

import os
import sys
import zipfile
import shutil
from pathlib import Path
from datetime import datetime
from typing import List, Tuple

# 项目根目录
PROJECT_ROOT = Path(__file__).parent.parent

# 需要备份的目录和文件
BACKUP_PATTERNS = {
    # 核心源代码
    'src/': 'src',
    'include/': 'include',
    
    # 脚本和工具
    'scripts/': 'scripts',
    
    # 配置文件
    'config/': 'config',
    
    # 构建文件
    'CMakeLists.txt': 'CMakeLists.txt',
    'CMakeLists_*.cmake': '.',  # 备份到根目录
    
    # GUI代码
    'gui/': 'gui',
    
    # 文档
    'docs/': 'docs',
    
    # 根目录重要文件
    'README.md': 'README.md',
    'requirements.txt': 'requirements.txt',
    'pytest.ini': 'pytest.ini',
    
    # 测试文件
    'tests/': 'tests',
}

# 需要排除的文件和目录模式
EXCLUDE_PATTERNS = [
    '__pycache__',
    '*.pyc',
    '*.pyo',
    '*.pyd',
    '.git',
    '.gitignore',
    '*.log',
    '*.tmp',
    '*.swp',
    '*.bak',
    'build/',
    'build-linux/',
    'htmlcov/',
    '*.jar',
    '*.dll',
    '*.so',
    '*.a',
    '*.exe',
    '*.o',
    '*.d',
    '.vscode/',
    '.idea/',
    '*.user',
    '*.suo',
    '*.sdf',
    '*.opensdf',
    '*.db',
    '*.sln',
    '*.vcxproj',
    '*.vcxproj.filters',
    '*.vcxproj.user',
    '*.cmake',
    'CMakeFiles/',
    '*.make',
    'Makefile',
    'compile_commands.json',
    'CMakeCache.txt',
    'cmake_install.cmake',
    '*.rsp',
    '*.autogen/',
    '*.md~',
    '~$*',
]


def should_exclude(file_path: Path, root: Path) -> bool:
    """检查文件是否应该被排除"""
    relative_path = file_path.relative_to(root)
    file_name = file_path.name
    
    # 特殊处理：CMakeLists_*.cmake文件不应该被排除
    if file_name.startswith('CMakeLists_') and file_name.endswith('.cmake'):
        return False
    
    # 检查排除模式
    for pattern in EXCLUDE_PATTERNS:
        # 目录模式
        if pattern.endswith('/'):
            pattern_name = pattern.rstrip('/')
            if pattern_name in str(relative_path).split(os.sep):
                return True
        # 文件扩展名模式
        elif pattern.startswith('*.'):
            ext = pattern[1:]
            # 对于*.cmake，排除除了CMakeLists_*.cmake之外的所有.cmake文件
            if ext == '.cmake' and not (file_name.startswith('CMakeLists_') and file_name.endswith('.cmake')):
                if file_path.suffix == ext:
                    return True
            elif file_path.suffix == ext:
                return True
        # 精确匹配
        elif pattern in str(relative_path) or pattern in file_name:
            return True
    
    return False


def collect_files(root: Path, patterns: dict) -> List[Tuple[Path, str]]:
    """收集需要备份的文件"""
    files_to_backup = []
    
    for pattern, target_dir in patterns.items():
        # 处理通配符模式
        if '*' in pattern:
            if pattern.startswith('CMakeLists_'):
                # 查找所有匹配的CMakeLists文件
                for cmake_file in root.glob(pattern):
                    if cmake_file.is_file() and not should_exclude(cmake_file, root):
                        # 如果target_dir是'.'，使用文件名；否则使用target_dir/文件名
                        if target_dir == '.':
                            target_path = cmake_file.name
                        else:
                            target_path = f"{target_dir}/{cmake_file.name}".replace('\\', '/')
                        files_to_backup.append((cmake_file, target_path))
        # 处理目录
        elif pattern.endswith('/'):
            dir_name = pattern.rstrip('/')
            source_dir = root / dir_name
            if source_dir.exists() and source_dir.is_dir():
                for file_path in source_dir.rglob('*'):
                    if file_path.is_file() and not should_exclude(file_path, root):
                        # 保持目录结构
                        rel_path = file_path.relative_to(source_dir)
                        target_path = f"{target_dir}/{rel_path}".replace('\\', '/')
                        files_to_backup.append((file_path, target_path))
        # 处理单个文件
        else:
            source_file = root / pattern
            if source_file.exists() and source_file.is_file():
                if not should_exclude(source_file, root):
                    files_to_backup.append((source_file, target_dir))
    
    return files_to_backup


def create_backup(output_dir: Path = None) -> Path:
    """创建备份压缩包"""
    # 确定输出目录
    if output_dir is None:
        output_dir = PROJECT_ROOT / 'backups'
    else:
        output_dir = Path(output_dir)
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # 生成带时间戳的文件名
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    zip_filename = f'DoubleConeShooting_backup_{timestamp}.zip'
    zip_path = output_dir / zip_filename
    
    print(f"开始备份项目...")
    print(f"项目根目录: {PROJECT_ROOT}")
    print(f"输出文件: {zip_path}")
    print()
    
    # 收集文件
    print("正在收集文件...")
    files_to_backup = collect_files(PROJECT_ROOT, BACKUP_PATTERNS)
    print(f"找到 {len(files_to_backup)} 个文件需要备份")
    print()
    
    # 创建压缩包
    print("正在创建压缩包...")
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for i, (source_file, target_path) in enumerate(files_to_backup, 1):
            try:
                zipf.write(source_file, target_path)
                if i % 100 == 0:
                    print(f"  已处理 {i}/{len(files_to_backup)} 个文件...")
            except Exception as e:
                print(f"  警告: 无法备份 {source_file}: {e}")
    
    # 获取文件大小
    file_size = zip_path.stat().st_size
    size_mb = file_size / (1024 * 1024)
    
    print()
    print("=" * 60)
    print("备份完成！")
    print(f"备份文件: {zip_path}")
    print(f"文件大小: {size_mb:.2f} MB ({file_size:,} 字节)")
    print(f"包含文件数: {len(files_to_backup)}")
    print("=" * 60)
    
    return zip_path


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='备份项目核心代码、脚本和配置文件',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        '--output-dir',
        type=str,
        default=None,
        help='备份文件输出目录（默认：项目根目录下的backups文件夹）'
    )
    
    args = parser.parse_args()
    
    try:
        create_backup(args.output_dir)
    except KeyboardInterrupt:
        print("\n\n备份已取消")
        sys.exit(1)
    except Exception as e:
        print(f"\n错误: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()

