# Tango 运行环境说明

## 核心进程

- **MariaDB** 保存 Tango 设备、服务实例和属性配置；
- **Tango Database** 向客户端和设备服务提供注册发现；
- **Starter** 负责设备服务的生命周期管理；
- **Device Server** 将运动控制、编码器、真空、成像等硬件能力暴露为 Tango 属性和命令；
- **Python/Qt GUI** 通过 `DeviceProxy` 调用设备服务，不直接承担设备注册。

## 地址与发现

所有进程通过 `TANGO_HOST=host:port` 找到 Tango Database。生产环境应固定数据库地址，并让 systemd unit、交互终端和 GUI 使用同一个值。

设备服务启动时使用“可执行文件 + 实例名”，例如：

```bash
./build/motion_controller_server ctrl1 -v4
```

实例名必须与数据库注册一致，否则服务虽然运行，客户端也无法解析到正确设备。

## 配置来源

- `config/devices_config.json`：设备注册、控制器、轴和 IO；
- `config/system_config.json`：系统模式和公共连接参数；
- `config/vacuum_system_config.json`：真空 PLC、轮询和安全阈值；
- `/etc/omniORB.cfg`：ORB 超时与名称解析行为。

修改配置后，先确认变更影响，再更新 Tango 数据库属性并重启受影响服务。

## systemd 职责

Ubuntu 24.04 使用 systemd 管理 MariaDB、Tango Database、Starter 和生产设备服务。每个 unit 应至少定义：

- 明确的 `WorkingDirectory`；
- `TANGO_HOST` 和必要的动态库路径；
- 非 root 运行用户；
- 合理的重启策略；
- 日志归集和轮转；
- 对数据库或网络就绪状态的依赖。

## 故障定位顺序

1. 检查 MariaDB 和 Tango Database；
2. 检查 `TANGO_HOST`；
3. 检查设备是否注册、实例名是否一致；
4. 检查服务日志和 Tango 状态；
5. 检查主机到控制器/PLC/编码器的网络；
6. 检查厂商共享库与动态链接依赖；
7. 最后检查 GUI。

该顺序能把数据库发现问题、服务问题和真实硬件问题分离开，避免在 GUI 层反复试错。
