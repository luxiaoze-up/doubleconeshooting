# 真空系统GUI - 三种运行模式说明

## 概述

真空系统GUI现在支持三种运行模式：

1. **Tango模式** - 通过Tango设备服务连接真实PLC（原有模式）
2. **Mock模式** - 模拟模式，无需任何硬件连接
3. **Direct模式** - 直接通过OPC UA连接真实PLC，不依赖Tango（新增模式）

## 模式对比

| 特性 | Tango模式 | Mock模式 | Direct模式 |
|------|----------|---------|-----------|
| 需要Tango环境 | ✓ | ✗ | ✗ |
| 需要PLC | ✓ | ✗ | ✓ |
| 需要python-opcua | ✗ | ✗ | ✓ |
| 适用场景 | 生产环境 | 开发测试 | 独立部署 |
| 报警功能 | 完整 | 模拟 | 完整 |
| 自动序列 | 完整 | 模拟 | 完整 |

## 安装依赖

### Tango模式依赖
```bash
pip install pytango>=9.3.0
```

### Direct模式依赖
```bash
pip install opcua>=0.98.13
```

### 通用依赖
```bash
pip install PyQt5>=5.15.0 pyqtgraph>=0.12.0 numpy>=1.20.0
```

或者使用requirements.txt：
```bash
pip install -r requirements.txt
```

## 运行方式

### 1. Tango模式（默认）
```bash
python run_gui.py
```

**前提条件：**
- Tango设备服务器 `sys/vacuum/2` 已启动
- 网络可访问Tango数据库
- PLC通过Tango设备服务连接

**适用场景：**
- 完整的Tango系统环境
- 需要与其他Tango设备集成
- 生产环境部署

### 2. Mock模式
```bash
python run_gui_mock.py
# 或
python run_gui.py --mock
```

**前提条件：**
- 无需任何硬件
- 无需Tango环境

**适用场景：**
- 开发阶段功能测试
- UI调试
- 演示展示
- 无硬件环境的使用

### 3. Direct模式（新增）
```bash
python run_gui_direct.py
# 或
python run_gui.py --direct
```

**前提条件：**
- 已安装 `python-opcua` 库
- PLC在线且OPC UA服务已启动
- 网络可访问PLC（默认：192.168.1.100:4840）

**适用场景：**
- 独立部署，无需Tango环境
- 临时测试和调试
- 单机应用
- Tango服务暂时不可用时的备用方案

## 配置

### PLC连接配置

编辑 `config.py` 中的 `PLC_CONNECTION` 配置：

```python
PLC_CONNECTION = {
    "ip": "192.168.1.100",    # PLC IP地址
    "port": 4840,              # OPC UA端口
    "timeout_ms": 5000         # 超时时间
}
```

### PLC节点映射

Direct模式使用的PLC变量映射定义在 `plc_nodes_mapping.py` 中。

节点ID格式：
```
ns=4;s=|var|CODESYS Control Win V3 x64.Application.GVL.gVacuumSystem.<变量名>
```

如需修改映射，请编辑该文件中的节点定义。

## 功能对比

### 完整功能（三种模式均支持）

- ✓ 系统状态监控
- ✓ 泵控制（螺杆泵、罗茨泵、分子泵）
- ✓ 阀门控制（闸板阀、电磁阀、放气阀）
- ✓ 真空度监控
- ✓ 操作模式切换（手动/自动）
- ✓ 一键抽真空
- ✓ 一键停机
- ✓ 腔室放气
- ✓ 报警管理
- ✓ 操作历史记录
- ✓ 趋势曲线

### Mock模式特殊说明

Mock模式会模拟以下行为：
- 真空度变化（物理模拟）
- 泵启动延时
- 阀门动作延时
- 自动序列流程
- 随机故障（可配置关闭）

### Direct模式实现细节

Direct模式通过以下组件实现：

1. **plc_opcua_client.py** - OPC UA客户端通信封装
2. **plc_nodes_mapping.py** - PLC节点ID映射定义
3. **DirectPLCWorker** - 直接PLC通信Worker类

所有Tango模式的功能都在Direct模式中完整实现。

## 故障排查

### Tango模式连接失败
- 检查Tango设备服务是否启动
- 检查网络连接
- 验证设备名 `sys/vacuum/2` 是否正确

### Direct模式连接失败
- 检查PLC是否在线：`ping 192.168.1.100`
- 验证OPC UA服务已启动
- 检查防火墙是否允许4840端口
- 确认已安装python-opcua库：`pip list | grep opcua`

### Mock模式问题
- Mock模式不需要任何外部依赖，如有问题请检查Python环境和PyQt5安装

## 性能说明

- **轮询间隔：** 100ms（所有模式）
- **Tango模式延迟：** ~50-100ms（取决于网络）
- **Direct模式延迟：** ~20-50ms（本地网络）
- **Mock模式延迟：** <10ms（无网络通信）

## 开发建议

1. **日常开发：** 使用Mock模式进行UI和逻辑开发
2. **功能测试：** 使用Direct模式进行快速PLC功能测试
3. **集成测试：** 使用Tango模式进行完整系统集成测试
4. **生产部署：** 使用Tango模式（推荐）或Direct模式（备用）

## 更新日志

### 2026-01-21
- ✨ 新增Direct模式，支持直接通过OPC UA连接PLC
- ✨ 添加plc_opcua_client.py OPC UA通信模块
- ✨ 添加plc_nodes_mapping.py节点映射配置
- ✨ 添加run_gui_direct.py启动脚本
- ✨ 窗口标题显示当前运行模式
- 📝 更新requirements.txt添加opcua依赖

## 技术支持

如有问题或建议，请联系开发团队。
