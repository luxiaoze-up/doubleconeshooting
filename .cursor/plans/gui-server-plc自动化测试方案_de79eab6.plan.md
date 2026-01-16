---
name: GUI-Server-PLC自动化测试方案
overview: 为vacuum system创建完整的自动化测试框架，覆盖GUI->Server->PLC的端到端通信测试，使用pytest框架和Mock模式，确保所有通信链路正常工作。
todos:
  - id: setup_test_framework
    content: 创建测试框架基础：tests目录结构、conftest.py、pytest配置
    status: completed
  - id: enhance_mock_plc
    content: 增强MockPLCCommunication：添加场景预设、响应模拟、状态机模拟
    status: completed
  - id: create_mock_tango
    content: 创建MockTangoDeviceProxy用于GUI测试，实现Tango属性和命令接口
    status: completed
  - id: unit_tests_plc
    content: 实现PLC通信单元测试：连接、读写、批量操作、地址映射
    status: completed
    dependencies:
      - enhance_mock_plc
  - id: unit_tests_worker
    content: 实现TangoWorker单元测试：命令队列、状态轮询、报警检测
    status: completed
    dependencies:
      - create_mock_tango
  - id: integration_gui_server
    content: 实现GUI到Server集成测试：命令发送、状态更新、报警事件
    status: completed
    dependencies:
      - create_mock_tango
  - id: integration_server_plc
    content: 实现Server到PLC集成测试：状态读取、控制写入、同步机制
    status: completed
    dependencies:
      - enhance_mock_plc
  - id: e2e_operations
    content: 实现端到端测试：抽真空、停机、放气、阀门控制完整流程
    status: completed
    dependencies:
      - integration_gui_server
      - integration_server_plc
  - id: test_reports
    content: 配置测试报告：pytest-html、pytest-cov、日志输出
    status: completed
    dependencies:
      - unit_tests_plc
      - unit_tests_worker
  - id: ci_cd_setup
    content: 创建测试运行脚本和CI/CD配置（可选）
    status: completed
    dependencies:
      - test_reports
---

# GUI-Server-PLC自动化测试方案

## 1. 测试架构设计

### 1.1 测试层次

```javascript
┌─────────────────────────────────────────────────┐
│  端到端测试 (E2E Tests)                          │
│  GUI操作 → Tango命令 → Device Server → PLC读写   │
└─────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────┐
│  集成测试 (Integration Tests)                   │
│  - GUI ↔ Tango Device Server                    │
│  - Device Server ↔ PLC Communication            │
└─────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────┐
│  单元测试 (Unit Tests)                          │
│  - TangoWorker命令处理                         │
│  - PLC通信读写                                  │
│  - 状态同步逻辑                                 │
└─────────────────────────────────────────────────┘
```



### 1.2 Mock组件

- **MockPLCCommunication**: 模拟PLC硬件响应
- **MockTangoWorker**: 模拟Tango设备（已存在）
- **MockTangoDeviceProxy**: 模拟Tango DeviceProxy用于测试GUI

## 2. 测试文件结构

```javascript
tests/
├── __init__.py
├── conftest.py                    # pytest配置和fixtures
├── unit/
│   ├── test_plc_communication.py  # PLC通信单元测试
│   ├── test_tango_worker.py      # TangoWorker单元测试
│   └── test_state_sync.py        # 状态同步逻辑测试
├── integration/
│   ├── test_gui_to_server.py     # GUI到Server通信测试
│   ├── test_server_to_plc.py     # Server到PLC通信测试
│   └── test_command_flow.py       # 命令流程测试
└── e2e/
    ├── test_vacuum_operations.py  # 真空操作端到端测试
    ├── test_valve_control.py      # 阀门控制端到端测试
    └── test_alarm_handling.py     # 报警处理端到端测试
```



## 3. 核心测试组件实现

### 3.1 conftest.py - 测试配置和Fixtures

- **mock_plc_fixture**: 创建MockPLCCommunication实例
- **mock_tango_device_fixture**: 创建模拟Tango设备
- **gui_app_fixture**: 创建PyQt5 QApplication（用于GUI测试）
- **vacuum_worker_fixture**: 创建VacuumTangoWorker或MockTangoWorker

### 3.2 MockPLCCommunication增强

在现有MockPLCCommunication基础上添加：

- **场景预设**: 预定义测试场景（如"抽真空流程"、"故障状态"）
- **响应模拟**: 模拟PLC对写入操作的响应
- **状态机模拟**: 模拟阀门动作延时、泵启动过程等

### 3.3 MockTangoDeviceProxy

创建模拟Tango DeviceProxy，用于测试GUI：

- 实现常用Tango属性读取接口
- 实现常用Tango命令接口
- 支持状态变化模拟

## 4. 测试用例设计

### 4.1 单元测试

**test_plc_communication.py**

- 测试PLC连接/断开
- 测试BOOL/WORD/INT/REAL/DWORD读写
- 测试批量读取
- 测试地址映射正确性

**test_tango_worker.py**

- 测试命令队列处理
- 测试状态轮询
- 测试报警检测
- 测试连接重试机制

### 4.2 集成测试

**test_gui_to_server.py**

- 测试GUI发送命令到Server
- 测试GUI接收Server状态更新
- 测试GUI接收报警事件
- 测试连接状态变化处理

**test_server_to_plc.py**

- 测试Server读取PLC状态
- 测试Server写入PLC控制信号
- 测试PLC状态同步到Server属性
- 测试PLC连接断开处理

**test_command_flow.py**

- 测试一键抽真空命令流程
- 测试一键停机命令流程
- 测试腔室放气命令流程
- 测试手动模式操作流程

### 4.3 端到端测试

**test_vacuum_operations.py**

- 完整抽真空流程：GUI点击 → Server处理 → PLC响应 → 状态反馈
- 完整停机流程
- 模式切换流程

**test_valve_control.py**

- 闸板阀开关操作端到端
- 电磁阀开关操作端到端
- 放气阀操作端到端
- 阀门超时检测

**test_alarm_handling.py**

- 报警触发和通知流程
- 报警确认流程
- 报警历史记录

## 5. 测试数据管理

### 5.1 测试场景配置

创建`tests/fixtures/test_scenarios.json`：

```json
{
  "normal_operation": {
    "plc_states": {...},
    "expected_responses": {...}
  },
  "fault_condition": {...},
  "pumping_sequence": {...}
}
```



### 5.2 测试数据生成器

- 生成随机但合理的PLC状态数据
- 生成测试用的报警数据
- 生成模拟传感器读数序列

## 6. 测试报告和日志

### 6.1 pytest配置

- 使用pytest-html生成HTML报告
- 使用pytest-cov生成覆盖率报告
- 配置日志输出到文件

### 6.2 测试日志

- 记录每个测试步骤的执行时间
- 记录GUI-Server-PLC通信消息
- 记录状态变化时间线

## 7. CI/CD集成

### 7.1 测试脚本

创建`scripts/run_tests.sh`和`scripts/run_tests.bat`：

- 运行所有测试
- 生成测试报告
- 检查覆盖率阈值

### 7.2 GitHub Actions / GitLab CI配置

- 自动运行测试套件
- 发布测试报告
- 在测试失败时通知

## 8. 实施步骤

1. **创建测试框架基础** (conftest.py, 目录结构)
2. **增强MockPLCCommunication** (场景预设, 响应模拟)
3. **创建MockTangoDeviceProxy** (用于GUI测试)
4. **实现单元测试** (PLC通信, TangoWorker)
5. **实现集成测试** (GUI-Server, Server-PLC)
6. **实现端到端测试** (完整操作流程)
7. **配置测试报告和CI/CD**

## 9. 关键文件

- `tests/conftest.py` - pytest配置和fixtures
- `tests/unit/test_plc_communication.py` - PLC通信测试
- `tests/integration/test_gui_to_server.py` - GUI-Server集成测试
- `tests/e2e/test_vacuum_operations.py` - 端到端测试
- `tests/fixtures/test_scenarios.json` - 测试场景配置