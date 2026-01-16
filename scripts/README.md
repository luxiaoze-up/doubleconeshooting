# Scripts 目录

本目录包含项目运行和维护所需的各种脚本。

## 正式脚本

### 系统启动脚本

- `start_servers.py` - 启动所有设备服务器
- `start_vacuum_system.sh` - 启动真空系统服务
- `start_image_api.sh` / `start_image_api.bat` - 启动图像流 API 服务

### 系统配置脚本

- `setup_omniORB.sh` - 配置 omniORB（解决 DNS 反向解析问题）
- `fix_wsl_hosts.sh` - WSL hosts 文件修复工具

### 设备管理脚本

- `register_devices.py` - 注册 Tango 设备到数据库

### 测试脚本

- `run_tests.sh` - 运行测试套件（Linux/WSL）
- `run_tests.bat` - 运行测试套件（Windows）
- `run_tests_with_reports.sh` - 运行测试并生成报告

### API 服务

- `image_stream_api.py` - 图像流 REST API 服务
- `requirements_api.txt` - API 服务的 Python 依赖

## 工具脚本

临时和开发工具脚本已移至 `tools/` 子目录，包括：
- 文档转换工具
- 数据处理工具
- 测试脚本
- 查看工具

详见 `tools/README.md`

