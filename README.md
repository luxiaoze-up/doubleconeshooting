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
- `build/`：Ubuntu 24.04 上的 CMake 构建输出目录

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

## 4. Ubuntu 24.04 环境与运行

项目仅支持 **Ubuntu 24.04 x86-64**。建议在项目根目录使用 Python 虚拟环境，系统服务通过 systemd 管理。

### 4.1 基础依赖

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  libomniorb4-dev libzmq3-dev qtbase5-dev \
  python3 python3-venv python3-pip

python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip
python -m pip install -r requirements.txt
python -m pip install -r gui/requirements.txt
python -m pip install -r gui/vacuum_system_gui/requirements.txt
```

Tango Controls、open62541、Snap7 和硬件厂商 SDK 按现场版本安装。运动控制使用仓库中的 Ubuntu x86-64 库 `lib/libLTSMC.so`。

### 4.2 编译

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Release 构建使用 `-DCMAKE_BUILD_TYPE=Release`。如果修改了编译器或系统依赖，删除并重新生成 `build/`，不要复用其他机器生成的 CMake 缓存。

### 4.3 Tango 与设备服务

```bash
export TANGO_HOST=127.0.0.1:10000
sudo systemctl status mariadb
tango_admin --ping-database

python3 scripts/register_devices.py --config config/devices_config.json --force
python3 scripts/start_servers.py
```

`start_servers.py` 已包含 `VacuumSystem`。仅在不运行通用启动器、需要单独维护真空系统时使用：

```bash
scripts/start_vacuum_system.sh
# 后台运行
scripts/start_vacuum_system.sh --background
```

日志写入 `logs/*.log`。真实设备联调前必须核对 `config/devices_config.json`、控制器网段、轴映射、限位、刹车和电源 IO。

### 4.4 GUI 与图像流 API

```bash
# 主 GUI
python3 gui/vacuum_chamber_gui/main.py

# 真空系统 GUI
python3 gui/vacuum_system_gui/run_gui.py
python3 gui/vacuum_system_gui/run_gui.py --mock

# 六自由度独立调试 GUI
python3 -m gui.six_dof_debug_gui.main

# 图像流 API
scripts/start_image_api.sh
```

C++/Qt 客户端是可选目标：`./build/main_controller --sim`。

## 5. 测试与验证

```bash
# 配置和编译检查
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

# Python 单元测试
scripts/run_tests.sh --unit -v

# 带 HTML 与覆盖率报告
scripts/run_tests.sh --html --cov
```

`scripts/unit_test/` 中部分用例会连接 Tango 或真实硬件；运行前先阅读测试文件，确认目标 IP、设备名和运动范围。详细说明见 `docs/编译和测试指南.md` 与 `docs/端到端测试底层实现原理详解.md`。

## 6. 待开发事项（基于仓库内 TODO/文档）

> 这里聚合“代码内 TODO + 文档中待确认/待实现项”，方便排期。
### 最近更新（2026-02-03）

**✅ 六自由度服务、大行程服务、反射光成像服务、辅助支撑服务刹车控制完善**

- 位置：`six_dof_device.cpp`、`large_stroke_device.cpp`、`reflection_imaging_device.cpp`、`auxiliary_support_device.cpp`

**✅ 真空系统plc的opcua通讯实现**

- 位置：`vacuum_device.cpp`、`opcua_plc_interface.cpp`

**✅ 腔体灯光电源控制实现**
- 位置：大行程设备（large_stroke_device）
- 新增功能：
  - 添加 `cavityLightPort` 和 `cavityLightController` 配置项（OUT7端口控制）
  - 实现 `enableCavityLight()` / `disableCavityLight()` 命令
  - 服务启动时自动启用腔体灯光（在驱动器上电和刹车释放后）
  - 更新 `queryPowerStatus()` 命令输出，包含腔体灯光状态
- 配置：`config/devices_config.json` 中 large_stroke 设备已添加 `cavityLightPort: 7` 和 `cavityLightController: sys/motion/3`

**✅ 编码器接口梳理和读取统一实现**

- 位置：`encoder_device.cpp`、`encoder_interface.cpp`

**✅ 大行程服务回零（负限位）实现**

- 位置：`large_stroke_device.cpp`

**✅ 运动完成后自动启用刹车（安全增强）**
- 影响设备：large_stroke、six_dof、reflection_imaging、auxiliary_support
- 改进内容：
  - **运动前**：自动释放刹车，允许运动（已有功能）
  - **运动后**：自动启用刹车，提供安全保护（新增）
  - **故障时**：自动启用刹车（已有功能）
- 安全优势：
  - 防止运动停止后的位置漂移
  - 断电时提供机械锁定保护
  - 减少人工干预，提升安全性
- 修改位置：
  - [large_stroke_device.cpp](src/device_services/large_stroke_device.cpp) - 运动完成状态转换时自动启用刹车
  - [six_dof_device.cpp](src/device_services/six_dof_device.cpp) - MOVING→ON状态转换时自动启用刹车
  - [reflection_imaging_device.cpp](src/device_services/reflection_imaging_device.cpp) - Z轴运动完成后自动启用刹车
  - [auxiliary_support_device.cpp](src/device_services/auxiliary_support_device.cpp) - 添加刹车控制预留注释

**✅ 相机上电控制实现**
- 位置：反射光成像设备（reflection_imaging_device）
- 新增功能：
  - 实现 `enableCameraPower()` / `disableCameraPower()` 命令
  - 更新 `queryPowerStatus()` 命令输出，包含相机电源状态
### P0 / 需要尽快落实

1) **真空系统启动方式固化**
- 当前 `scripts/start_servers.py` 已包含 `VacuumSystem`；`scripts/start_vacuum_system.sh` 供单独维护时使用，两者不要同时启动同一实例。
- 建议补充：在现场 SOP/快速操作卡中明确采用通用启动器还是独立真空脚本。

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
