# PLC 工具脚本

## check_vacuum_nodes.py - 真空系统节点检查器 (推荐⭐)

**简化版工具**，专门用于检查vacuum_system_plc_mapping.h中定义的节点（共80+个），不遍历整个PLC节点树，速度快。

### 功能

- ✓ 检查所有真空系统映射节点是否存在
- ✓ 自动尝试多种NodeID格式（ns=3/4, CODESYS格式等）
- ✓ 显示每个节点的实际值和数据类型
- ✓ 导出Python映射代码（可直接替换plc_nodes_mapping.py）
- ✓ 导出JSON结果

### 使用方法

```bash
# 检查所有节点
python tools/check_vacuum_nodes.py

# 指定PLC地址
python tools/check_vacuum_nodes.py --ip 192.168.1.100 --port 4840

# 检查并导出Python映射代码
python tools/check_vacuum_nodes.py --export-mapping plc_nodes_mapping_new.py

# 导出JSON结果
python tools/check_vacuum_nodes.py --export-json vacuum_nodes.json
```

### 输出示例

```
✓ ScrewPumpPowerFeedback                     %I0.0      → ns=3;s=%I0.0
   类型: Boolean         值: False
✓ RootsPumpPowerFeedback                     %I0.1      → ns=3;s=%I0.1
   类型: Boolean         值: False
✗ MolecularPump1PowerFeedback                %I0.2      → 未找到
...
检查完成:
  成功: 75/80
  失败: 5/80
```

---

## browse_plc_nodes.py - PLC节点浏览器 (完整版)

**完整版工具**，用于浏览PLC的整个OPC UA节点树（可能有数千个节点），适合探索未知的PLC结构。

### 安装依赖

```bash
pip install opcua
```

### 使用方法

#### 1. 浏览整个节点树

```bash
python tools/browse_plc_nodes.py
```

#### 2. 指定PLC地址

```bash
python tools/browse_plc_nodes.py --ip 192.168.1.100 --port 4840
```

#### 3. 从指定路径开始浏览

通常真空系统的变量在特定路径下，可以直接浏览该路径：

```bash
# 浏览 CODESYS 应用程序节点
python tools/browse_plc_nodes.py --path "Objects/DeviceSet/CODESYS Control Win V3 x64/Application"

# 或者浏览全局变量列表
python tools/browse_plc_nodes.py --path "Objects/Application/GVL"
```

#### 4. 限制浏览深度

如果节点树太大，可以限制深度：

```bash
python tools/browse_plc_nodes.py --max-level 3
```

#### 5. 搜索特定节点

浏览完成后搜索包含关键字的节点：

```bash
python tools/browse_plc_nodes.py --search "Vacuum"
python tools/browse_plc_nodes.py --search "Pump"
python tools/browse_plc_nodes.py --search "Valve"
```

#### 6. 导出节点信息

将节点信息导出到文件以便后续分析：

```bash
# 导出为JSON格式
python tools/browse_plc_nodes.py --export-json plc_nodes.json

# 导出为文本格式
python tools/browse_plc_nodes.py --export-text plc_nodes.txt

# 同时导出两种格式
python tools/browse_plc_nodes.py --export-json plc_nodes.json --export-text plc_nodes.txt
```

#### 7. 组合使用

```bash
# 浏览特定路径，搜索关键字，并导出结果
python tools/browse_plc_nodes.py \
    --path "Objects/Application" \
    --max-level 5 \
    --search "gVacuumSystem" \
    --export-json vacuum_nodes.json
```

### 输出示例

```
正在连接到 opc.tcp://192.168.1.100:4840...
✓ 成功连接到 opc.tcp://192.168.1.100:4840

================================================================================
开始浏览节点树...
================================================================================

├─ [Object] Root
│  NodeID: i=84
  ├─ [Object] Objects
  │  NodeID: i=85
    ├─ [Object] Server
    │  NodeID: i=2253
    ├─ [Object] DeviceSet
    │  NodeID: ns=4;i=1001
      ├─ [Object] CODESYS Control Win V3 x64
      │  NodeID: ns=4;i=1002
        ├─ [Object] Application
        │  NodeID: ns=4;i=1003
          ├─ [Object] GVL
          │  NodeID: ns=4;i=1004
            ├─ [Object] gVacuumSystem
            │  NodeID: ns=4;i=1005
              ├─ [Variable] bScrewPumpPower (Boolean) = False
              │  NodeID: ns=4;s=|var|CODESYS Control Win V3 x64.Application.GVL.gVacuumSystem.bScrewPumpPower
              ├─ [Variable] rVacuumGauge1 (Float) = 101325.0
              │  NodeID: ns=4;s=|var|CODESYS Control Win V3 x64.Application.GVL.gVacuumSystem.rVacuumGauge1
              ...

================================================================================
浏览完成，共找到 256 个节点
================================================================================
```

### 更新节点映射

找到正确的节点ID后，更新 `plc_nodes_mapping.py` 文件中的映射：

```python
# 例如，如果找到真空规1的节点ID是：
# ns=4;s=|var|CODESYS Control Win V3.Application.GVL.Vacuum.Gauge1
# 则更新为：
VACUUM_GAUGE1 = get_node_id("Gauge1")  # 或完整路径

# 或者修改 NAMESPACE_PREFIX 为实际路径
NAMESPACE_PREFIX = "ns=4;s=|var|CODESYS Control Win V3.Application.GVL.Vacuum"
```

### 常见PLC路径

不同的PLC可能使用不同的节点结构，常见路径包括：

- `Objects/DeviceSet/<PLC名称>/Application/GVL`
- `Objects/Application/GlobalVariables`
- `Objects/ServerInterfaces/OPC_UA`
- `Objects/DataAccess`

建议先从 `Objects` 开始浏览，找到应用程序节点所在位置。

### 故障排除

1. **连接失败**
   - 确认PLC的IP地址和端口正确
   - 确认PLC的OPC UA服务已启用
   - 检查网络连接和防火墙设置

2. **找不到节点**
   - 使用 `--max-level 15` 增加浏览深度
   - 尝试不同的起始路径
   - 使用搜索功能查找特定变量名

3. **节点ID格式不同**
   - 不同PLC厂商的节点ID格式可能不同
   - Siemens S7: `ns=3;s=DB1.DBX0.0`
   - CODESYS: `ns=4;s=|var|Application.GVL.Variable`
   - Beckhoff TwinCAT: `ns=4;s=MAIN.Variable`
