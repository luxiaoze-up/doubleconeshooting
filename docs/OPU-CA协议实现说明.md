# OPU-CA协议实现说明

## 概述

OPU-CA协议是基于TCP/IP的Siemens S7-1200 PLC通信协议实现。本文档说明当前的实现状态和使用方法。

## 实现状态

### 已完成功能

1. **TCP/IP连接管理**
   - 支持Windows和Linux平台
   - 自动初始化和清理Winsock（Windows）
   - 连接/断开连接管理
   - 连接状态检查

2. **数据读写框架**
   - BOOL类型读写
   - WORD类型读写
   - INT类型读写
   - REAL类型读写
   - DWORD类型读写

3. **协议请求构建**
   - 读取请求构建
   - 写入请求构建
   - 地址编码（支持%I, %Q, %M, %IW, %QW格式）

### 待完善功能

1. **协议细节**
   - 当前实现使用简化的协议格式，需要根据实际OPU-CA协议文档完善：
     - 请求/响应格式
     - 错误码处理
     - 数据包校验
     - 超时处理

2. **错误处理**
   - 网络错误重试机制
   - 连接断开自动重连
   - 详细的错误日志

3. **性能优化**
   - 批量读取优化
   - 连接池管理
   - 数据缓存

## 使用方法

### 基本连接

```cpp
#include "common/plc_communication.h"

using namespace Common::PLC;

// 创建通信对象
OPUCACommunication plc;

// 连接PLC
if (plc.connect("192.168.1.100", 102)) {
    // 连接成功
}

// 读取数据
PLCAddress addr(PLCAddressType::INPUT, 0, 0);  // %I0.0
bool value;
if (plc.readBool(addr, value)) {
    // 读取成功
}

// 断开连接
plc.disconnect();
```

### 在VacuumDevice中使用

```cpp
// 在init_device()中初始化
plc_comm_ = std::make_unique<OPUCACommunication>();

// 连接PLC
if (plc_comm_->connect(plc_ip_, plc_port_)) {
    log_event("PLC连接成功");
}

// 读取数据
bool pump_power;
PLCAddress addr = PLC::VacuumPLCMapping::ScrewPumpPower();
if (plc_comm_->readBool(addr, pump_power)) {
    screw_pump_power_ = pump_power;
}
```

## 协议格式说明

### 当前实现格式（简化版）

**读取请求格式:**
```
[功能码: 0x01][地址类型][字节偏移][位偏移][数据长度]
```

**写入请求格式:**
```
[功能码: 0x02][地址类型][字节偏移][位偏移][数据长度][数据...]
```

**响应格式:**
```
[状态码][数据...]
```

> **注意**: 以上格式为简化实现，实际OPU-CA协议格式需要根据协议文档进行调整。

## 编译配置

### CMakeLists.txt配置

已自动配置网络库链接：
- Windows: 链接 `ws2_32`
- Linux: 使用系统socket库

### 编译命令

```bash
# Linux/WSL
mkdir -p build && cd build
cmake ..
make vacuum_server

# Windows (Visual Studio)
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

## 测试

### 使用测试脚本

```bash
# 运行Python测试脚本
python scripts/test_plc_communication.py
```

### 手动测试

1. **启动Tango设备服务器**
   ```bash
   python scripts/start_servers.py
   ```

2. **使用Tango工具测试**
   ```bash
   # 连接设备
   tango_admin --add-server vacuum_server VacuumDevice sys/vacuum/1
   
   # 测试连接
   python -c "import tango; d=tango.DeviceProxy('sys/vacuum/1'); d.connectPLC()"
   ```

## 与真实PLC联调

### 准备工作

1. **确认PLC配置**
   - IP地址: 192.168.1.100（默认）
   - 端口: 102（默认）
   - 确保PLC已启用TCP/IP通信

2. **网络连接**
   - 确保开发机与PLC在同一网络
   - 测试网络连通性: `ping 192.168.1.100`

3. **协议配置**
   - 根据实际OPU-CA协议文档调整请求/响应格式
   - 确认数据字节序（大端/小端）
   - 确认地址映射关系

### 联调步骤

1. **使用模拟模式测试**
   ```python
   # 在devices_config.json中设置
   "simulator_mode": true
   ```

2. **切换到真实PLC**
   ```python
   "simulator_mode": false
   "plc_ip": "192.168.1.100"
   ```

3. **逐步测试**
   - 先测试连接
   - 再测试简单数据读取（如BOOL）
   - 最后测试复杂数据（WORD, REAL等）

4. **日志分析**
   - 查看设备服务器日志
   - 使用Wireshark抓包分析（如需要）

## 故障排查

### 常见问题

1. **连接失败**
   - 检查IP地址和端口
   - 检查防火墙设置
   - 检查PLC是否允许TCP连接

2. **数据读取失败**
   - 检查地址映射是否正确
   - 检查数据类型是否匹配
   - 查看协议响应格式

3. **编译错误**
   - Windows: 确保链接了ws2_32库
   - Linux: 确保安装了开发工具链

## 后续优化建议

1. **协议完善**
   - 根据实际协议文档完善请求/响应格式
   - 添加协议版本协商
   - 实现数据包校验和

2. **性能优化**
   - 实现批量读取
   - 添加数据缓存机制
   - 优化网络I/O

3. **可靠性增强**
   - 自动重连机制
   - 连接健康检查
   - 超时和重试策略

4. **调试支持**
   - 详细的日志记录
   - 协议数据包转储
   - 性能统计

## 参考文档

- Siemens S7-1200通信协议文档
- OPU-CA协议规范（如可用）
- Tango设备服务器开发文档
