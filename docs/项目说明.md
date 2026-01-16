# DoubleConeShooting（双锥射击控制系统）项目说明

> 本文面向首次接手/联调人员，给出**架构总览**、**最常用的运行方式**、以及**待开发事项**清单。

## 1. 项目概览

本项目是一个基于 **Tango Controls** 的分布式控制系统：
- **C++ Tango Device Server**：对运动控制、编码器、六自由度、大行程、辅助支撑、反射光成像、真空系统等硬件进行抽象与服务化。
- **Python Qt GUI（当前使用）**：
  - `gui/vacuum_chamber_gui`：真空腔体系统控制 GUI（包含靶定位/反射光成像/辅助支撑/真空抽气控制等页面）。
  - `gui/vacuum_system_gui`：真空系统独立 GUI。
- **C++ Qt GUI（可选）**：仓库仍包含 `main_controller`（C++/Qt）目标，可用于特定场景或历史兼容。
- **Python 脚本**：用于设备注册、启动编排、测试、工具链辅助（日志、配置检查等）。

## 2. 项目架构

### 2.1 分层架构（逻辑视图）

自上而下三层：
1. **集成控制层（GUI Layer）**：当前以 Python/Qt GUI 为主（`gui/vacuum_chamber_gui`、`gui/vacuum_system_gui`），通过 Tango Proxy 调用下层。
2. **系统服务层（System Service Layer）**：跨设备的联锁/协调服务（如 `interlock_server`）。
3. **设备服务层（Device Service Layer）**：直接对接硬件 SDK/协议，将能力暴露为 Tango Attributes/Commands。

数据流（简化）：
- 指令流：GUI → Tango → Device Server → 硬件 SDK/协议 → 设备
- 反馈流：传感器/编码器 → SDK → Device Server 属性更新 → Tango（事件/轮询）→ GUI

详细设计文档：
- `docs/系统需求及设计/System_Architecture_Design.md`

### 2.2 进程/服务划分（部署视图）

常见服务进程（以 CMake 与启动脚本为准）：
- `motion_controller_server/ctrl1|ctrl2|ctrl3`：三台网络运动控制器服务
- `encoder_server/main`：编码器采集器
- `six_dof_server/six_dof`：六自由度平台
- `large_stroke_server/large_stroke`：大行程
- `auxiliary_support_server/auxiliary`：辅助支撑
- `reflection_imaging_server/reflection`：反射光成像表征
- `vacuum_system_server/vacuum2`：真空系统（单独脚本可启动）
- `interlock_server/interlock`：联锁服务
- `main_controller`：Qt GUI 客户端

启动编排脚本：
- `scripts/start_servers.py`（会按顺序启动服务，并将输出写入 `logs/*.log`）
- `scripts/start_vacuum_system.sh`（只启动真空系统服务）

### 2.3 代码目录与职责

- `src/common/`：通用能力（系统配置、PLC 通信、标准设备基类、运动学等）
- `src/device_services/`：设备服务实现（各 Tango Device Server）
- `src/system_services/`：系统服务实现（如联锁）
- `src/integrated_control/`：Qt GUI（集成控制层）
- `src/drivers/`：第三方硬件/相机等驱动封装（可能包含待补全的 SDK 适配）
- `config/`：系统与设备配置（JSON）
- `scripts/`：启动、注册、测试、运维脚本
- `build-linux/`：WSL/Linux 构建输出目录（由 `wsl_build.sh` 生成）

## 3. 配置说明

### 3.1 全局配置

- `config/system_config.json`
  - `tango_host`：Tango 数据库地址（例：`127.0.0.1:10000`）
  - `sim_mode`：是否模拟模式（`true/false`）
  - `plc_ip`、`controller_ip`：部分脚本/默认连接参数

### 3.2 设备注册与属性配置

- `config/devices_config.json`
  - 该文件是**设备注册与属性配置的主来源**（运动控制器 IP、轴映射、编码器通道、IO 映射、电源/刹车端口等）。
  - `scripts/register_devices.py` 会读取它并注册到 Tango 数据库。

建议工作流：
1) 修改 `config/devices_config.json`（与硬件表/现场一致）
2) 运行注册脚本写入 Tango DB（`--force` 强制更新属性）
3) 启动服务与 GUI 联调

### 3.3 真空系统服务配置

- `config/vacuum_system_config.json`
  - `plc_connection.protocol`：如 `opcua`
  - `ip/port/timeout_ms`：PLC 连接信息
  - `poll_interval_ms`：轮询周期
  - `auto_sequence`、`safety_limits`：自动流程与安全阈值

## 4. 使用方法

### 4.1 推荐方式：WSL/Linux（主运行形态）

此项目仓库已提供 WSL 相关脚本（最省心）：

1) **WSL 环境准备**
```bash
./wsl_setup.sh
```

2) **编译（WSL/Linux 输出到 build-linux）**
```bash
./wsl_build.sh
# 或 Release：
./wsl_build.sh release
# 或清理重编：
./wsl_build.sh clean
```

3) **启动/部署（可选：自动检查/启动 Tango DB，并提示 MySQL 依赖）**
```bash
./wsl_deploy.sh
```

4) **（真实模式）注册设备到 Tango 数据库**
- 常用：按 `config/devices_config.json` 全量注册
```bash
python3 scripts/register_devices.py --config config/devices_config.json --force
```
- 或只注册某些设备（脚本支持 `--devices` 过滤，具体键名见脚本/配置）

5) **启动设备服务**
```bash
python3 scripts/start_servers.py
```
- 日志输出：`logs/*.log`

6) **启动真空系统（单独启动）**
```bash
scripts/start_vacuum_system.sh
# 或后台：
scripts/start_vacuum_system.sh --background
```

> 约定：真空系统**保持单独启动**，不并入 `scripts/start_servers.py` 的统一编排。

7) **启动 GUI（Qt）**
在 WSL/Linux 下，当前使用的 GUI 为 Python/Qt：
```bash
# 先安装 GUI 依赖（按需二选一或都装）
python3 -m pip install -r gui/requirements.txt
python3 -m pip install -r gui/vacuum_system_gui/requirements.txt

# 启动：真空腔体系统控制 GUI（当前主 GUI）
python3 gui/vacuum_chamber_gui/main.py

# 启动：真空系统独立 GUI
python3 gui/vacuum_system_gui/run_gui.py          # 正常模式（需要 Tango）
python3 gui/vacuum_system_gui/run_gui.py --mock   # 模拟模式（无需 Tango）
```

可选：如果你需要使用 C++/Qt 的 `main_controller`（非当前主路径），可在 `build-linux/` 下启动：
```bash
cd build-linux
./main_controller --sim
```

> 说明：CMake 中会根据环境是否找到 Tango/Qt 决定是否构建相应目标。

### 4.2 Windows（辅助：仅构建/跑 Python 测试/编辑代码）

Windows 下可使用 VS + CMake 构建（详见 `docs/编译和测试指南.md`）。
如果主要目的是跑测试或工具脚本：
```powershell
pip install -r requirements.txt
scripts\run_tests.bat
```

### 4.3 图像流 API（如需）

- 启动：`scripts/start_image_api.sh` 或 `scripts/start_image_api.bat`
- 代码入口：`scripts/image_stream_api.py`

## 5. 测试与验证

### 5.1 C++ 测试
WSL/Linux 构建后：
```bash
cd build-linux
./test_devices
```

### 5.2 Python 自动化测试
- Linux/WSL：
```bash
scripts/run_tests.sh --html --cov
```
- Windows：
```powershell
scripts\run_tests.bat
```

详细测试说明：
- `docs/编译和测试指南.md`
- `docs/端到端测试底层实现原理详解.md`

## 6. 待开发事项（基于仓库内 TODO/文档）

> 这里聚合“代码内 TODO + 文档中待确认/待实现项”，方便排期。

### P0 / 需要尽快落实
1) **相机上电控制纳入联调验证**
- 已确认：相机供电需要软件控制。
- 配置依据：`config/devices_config.json` 中反射光成像设备的 `cameraPowerPort` 与 `cameraPowerController`（以及对应的 IO 映射）。
- 建议补充：在联调/验收测试中加入“相机上电→相机初始化→抓图”闭环用例。

2) **真空系统保持单独启动的操作固化**
- 已确认：真空系统不并入 `scripts/start_servers.py`，继续使用 `scripts/start_vacuum_system.sh` 独立管理。
- 建议补充：在现场 SOP/快速操作卡中明确启动顺序（先数据库/设备服务，再真空服务，再 GUI）。

### P1 / 功能完善（代码里明确存在 TODO）
1) **海康相机驱动（MV-CU020-19GC）接入真实 SDK**
- 位置：`src/drivers/mv_cu020_19gc.cpp` 大量 TODO（初始化/采集/参数设置/抓图/编码等）。

2) **反射光成像：自动抓取线程与同步触发**
- 位置：`src/device_services/reflection_imaging_device.cpp`（自动抓取线程、硬件同步触发等 TODO）。

3) **真空设备自检逻辑补全**
- 位置：`src/device_services/vacuum_device.cpp`（PLC/传感器/泵/阀检查目前是占位 TODO）。

4) **运动控制器 PVT/PVTS 能力补全**
- 位置：`src/device_services/motion_controller_device.cpp`（JSON 解析并调用底层 SDK 的 TODO）。

5) **数据导出能力（日志/采集数据导出 Excel/CSV）**
- 位置：`src/device_services/motion_controller_device.cpp`（导出 TODO）。

### P2 / 工程化改进（建议项）
1) **统一“模拟/真实模式”的入口与说明**
- 当前有 `config/system_config.json` 的 `sim_mode`、GUI 的 `--sim`、以及 CMake 的 `MOCK_HARDWARE` 宏等多种切换方式。
- 建议形成一套“唯一推荐路径”，并在启动脚本/文档中固化。

2) **日志治理**
- 目前服务日志写入 `logs/*.log`，真空脚本已做了简单的日志切割策略；可考虑统一所有服务的滚动策略。

## 7. 参考与入口索引

- 架构设计：`docs/系统需求及设计/System_Architecture_Design.md`
- 编译与测试：`docs/编译和测试指南.md`
- 自动上电实现总结：`docs/实施完成总结.md`
- 真空系统资料：`docs/真空系统资料/`
- 启动脚本说明：`scripts/README.md`
