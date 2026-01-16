# 双锥射击控制系统架构设计文档 (SAD)

**版本**: 2.0  
**日期**: 2025-12-09  
**更新**: 完成全部设备服务层与系统服务层重构

## 1. 系统概述

本系统采用 **分层架构** 设计，基于 **Tango Controls** 分布式控制框架。系统将硬件设备的物理细节抽象为软件服务，通过网络进行通信，实现了用户界面与底层硬件的解耦。

## 2. 逻辑架构视图

系统自上而下分为三层：

### 2.1 集成控制层 (GUI Layer)
*   **技术栈**: C++, Qt 5/6
*   **核心组件**: `MainController` (1465+ 行)
*   **职责**:
    *   提供人机交互界面 (Sidebar, StackedWidget)。
    *   通过 `TangoDeviceWrapper` 或 `MockDevice` 与下层通信。
    *   实现业务流程逻辑（如一键流程）。
    *   **模块划分**:
        *   `TargetPositioningPage`: 负责六自由度与大行程。
        *   `AuxiliarySupportPage`: 负责辅助支撑系统。
        *   `BacklightPage`: 负责背光系统。
        *   `VacuumPage`: 负责真空系统控制。
        *   `InterlockPage`: 负责联锁状态显示。

### 2.2 系统服务层 (System Service Layer)
*   **技术栈**: C++, Tango Controls
*   **核心组件**: `InterlockService` (20个命令, 9个属性)
*   **职责**:
    *   作为独立的 Tango Device Server 运行。
    *   **协调者**: 监控多个底层设备的状态。
    *   **安全卫士**: 执行跨设备的联锁逻辑（例如：位置冲突检测）。
    *   **急停中心**: 接收急停指令并广播至所有相关设备。
*   **标准接口** (继承自 `StandardSystemDevice`):
    *   `devLock/devUnlock/devLockVerify/devLockQuery` - 设备锁定管理
    *   `devUserConfig` - 用户配置
    *   `selfCheck/selfCheckResult` - 自检功能
    *   `init/reset` - 初始化与复位
    *   `positionUnit` - 位置单位
    *   `groupAttributeJson` - 属性分组JSON

### 2.3 设备服务层 (Device Service Layer)
*   **技术栈**: C++, Tango Controls, 硬件 SDK (LTSMC)
*   **职责**: 直接与硬件控制器通信，将硬件能力暴露为 Tango Attributes (状态) 和 Commands (动作)。
*   **核心设备类**:

| 设备类 | 命令数 | 属性数 | 控制对象 |
| :--- | :---: | :---: | :--- |
| `MotionController` | 30+ | 15+ | 步进电机运动控制 |
| `EncoderDevice` | 16 | 7 | 绝对式编码器 |
| `SixDofDevice` | 25 | 15 | 六自由度平台 (6轴) |
| `LargeStrokeDevice` | 24 | 12 | 大行程平移台 (单轴) |
| `VacuumDevice` | 15 | 10 | 真空系统 (泵/阀/规) |
| `MultiAxisDevice` | 21 | 12 | 辅助支撑/背光系统 |

## 3. 标准设备接口规范

所有设备服务实现以下标准Tango接口：

### 3.1 通用命令
```
devLock(int timeout)      - 设备锁定
devUnlock()               - 设备解锁
devLockVerify(string pwd) - 锁定验证
devLockQuery() -> string  - 查询锁定状态
devUserConfig(string json)- 用户配置
selfCheck() -> bool       - 执行自检
init()                    - 初始化
reset()                   - 复位
```

### 3.2 通用属性
```
selfCheckResult (string)    - 自检结果JSON
positionUnit (string)       - 位置单位 (mm/deg/rad)
groupAttributeJson (string) - 属性分组信息JSON
```

## 4. 物理部署视图

系统进程分布如下：

| 进程名称 | 实例名 (Instance) | 描述 |
| :--- | :--- | :--- |
| `main_controller` | - | Qt 用户界面客户端 |
| `motion_controller_server` | `motion` | 运动控制器服务器 |
| `encoder_server` | `encoder` | 编码器设备服务器 |
| `large_stroke_server` | `large_stroke` | 大行程设备服务器 |
| `multi_axis_server` | `auxiliary` | 辅助支撑设备服务器 |
| `multi_axis_server` | `backlight` | 背光系统设备服务器 |
| `six_dof_server` | `six_dof` | 六自由度设备服务器 |
| `vacuum_server` | `vacuum` | 真空控制服务器 |
| `interlock_server` | `interlock` | 联锁服务服务器 |

*注: 所有服务器可运行在同一台工控机上，也可分布在网络中的不同节点。*

## 5. 数据流视图

1.  **指令流**: 用户点击 GUI -> `MainController` 调用 Tango Proxy -> Tango 网络 -> Device Server -> 硬件 SDK -> 运动控制器 -> 电机。
2.  **反馈流**: 编码器/传感器 -> 采集卡 -> 硬件 SDK -> Device Server 更新 Attribute -> Tango Event/Polling -> GUI 定时器刷新显示。

## 6. 关键设计决策

### 6.1 模拟/实物切换 (MOCK_HARDWARE)
*   **设计**: 编译时通过 `MOCK_HARDWARE` 宏切换。
*   **实现**: 
    ```cpp
    #ifdef MOCK_HARDWARE
        // 纯内存模拟，无需真实硬件
    #else
        // 连接真实硬件SDK
    #endif
    ```
*   **优势**: 开发测试阶段无需连接硬件。

### 6.2 通用多轴设备 (`MultiAxisDevice`)
*   **背景**: 辅助支撑和背光系统本质上都是多轴独立控制，逻辑高度相似。
*   **决策**: 不为每个子系统编写单独的 C++ 类，而是实现一个可配置的 `MultiAxisDevice`。
*   **配置**: 在 Tango 数据库中定义 `axis_ids` 属性（如 `[0,1,2,3,4]` 或 `[5,6,7,8,9,10]`），服务器启动时读取该属性初始化对应数量的轴。

### 6.3 联锁解耦
*   **背景**: 联锁逻辑可能随机械结构调整而变化。
*   **决策**: 联锁逻辑不硬编码在电机驱动中，而是集中在 `InterlockService`。
*   **优势**: 修改联锁规则只需更新 `InterlockService`，无需重启底层电机服务。

### 6.4 StandardSystemDevice基类
*   **设计**: 提供标准接口的默认实现
*   **继承关系**: 
    ```
    Tango::TANGO_BASE_CLASS
           ↓
    StandardSystemDevice (common/)
           ↓
    InterlockService, etc.
    ```

## 7. 目录结构说明

```
DoubleConeShooting/
├── src/
│   ├── integrated_control/  # GUI 源码 (main_controller.cpp/h)
│   ├── system_services/     # 联锁服务源码 (interlock_service.cpp/h)
│   ├── device_services/     # 设备服务源码
│   │   ├── motion_controller_device.cpp/h  # 运动控制器
│   │   ├── encoder_device.cpp/h            # 编码器
│   │   ├── six_dof_device.cpp/h            # 六自由度
│   │   ├── large_stroke_device.cpp/h       # 大行程
│   │   ├── vacuum_device.cpp/h             # 真空系统
│   │   └── multi_axis_device.cpp/h         # 多轴设备
│   ├── common/              # 接口定义与辅助类
│   │   ├── device_interface.cpp/h          # Mock设备接口
│   │   ├── standard_system_device.cpp/h    # 标准设备基类
│   │   ├── tango_device_wrapper.cpp/h      # Tango包装器
│   │   └── kinematics.cpp/h                # 运动学计算
│   └── test/                # 单元测试
│       └── test_devices.cpp                # 28个测试用例
├── include/                 # 头文件
├── config/                  # 配置文件
├── scripts/                 # 启动脚本与数据库注册脚本
├── build-linux/             # Linux构建输出 (9个可执行文件)
└── docs/                    # 设计文档
```

## 8. 构建与测试

### 8.1 编译
```bash
cd DoubleConeShooting
./wsl_build.sh
```

### 8.2 运行测试
```bash
cd build-linux
./test_devices   # 28个测试用例
```

### 8.3 生成的可执行文件
- `encoder_server`
- `interlock_server`
- `large_stroke_server`
- `main_controller`
- `motion_controller_server`
- `multi_axis_server`
- `six_dof_server`
- `vacuum_server`
- `test_devices`
