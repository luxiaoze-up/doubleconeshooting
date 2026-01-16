# 通用IO控制接口调用位置汇总

## 概述

本文档汇总了server中所有调用通用IO控制接口（`readIO`和`writeIO`）的位置。

**重要说明**：OUT端口是**低电平有效（active low）**，`motion_controller_device.cpp`中的`writeIO`和`readIO`函数已经自动处理了逻辑值与硬件值的转换：
- 逻辑值 1 (开启) ↔ 硬件值 0 (LOW)
- 逻辑值 0 (关闭) ↔ 硬件值 1 (HIGH)

因此，所有调用`writeIO`的地方传入的**逻辑值**（1=开启，0=关闭）会被自动转换为正确的硬件值。

---

## 1. 辅助支撑设备 (AuxiliarySupportDevice)

**文件**: `src/device_services/auxiliary_support_device.cpp`

### 1.1 驱动器上电
- **函数**: `enable_driver_power()`
- **位置**: 第1477-1478行
- **调用**: `power_ctrl->command_inout("writeIO", data)`
- **参数**: `[driver_power_port_, 1.0]` (逻辑值1=开启)
- **用途**: 启用辅助支撑驱动器电源（OUT6）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(driver_power_port_);
params[1] = 1.0;  // 逻辑值1=开启（硬件值0=LOW，低电平有效）
Tango::DeviceData data;
data << params;
INFO_STREAM << "[PowerControl] Calling writeIO with port=" << driver_power_port_ << ", value=1" << std::endl;
power_ctrl->command_inout("writeIO", data);
```

### 1.2 驱动器断电
- **函数**: `disable_driver_power()`
- **位置**: 第1524行
- **调用**: `power_ctrl->command_inout("writeIO", data)`
- **参数**: `[driver_power_port_, 0.0]` (逻辑值0=关闭)
- **用途**: 关闭辅助支撑驱动器电源（OUT6）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(driver_power_port_);
params[1] = 0.0;  // 逻辑值0=关闭（硬件值1=HIGH，低电平有效）
Tango::DeviceData data;
data << params;
power_ctrl->command_inout("writeIO", data);
```

---

## 2. 反射光成像设备 (ReflectionImagingDevice)

**文件**: `src/device_services/reflection_imaging_device.cpp`

### 2.1 驱动器上电
- **函数**: `enable_driver_power()`
- **位置**: 第4071-4073行
- **调用**: `power_ctrl.command_inout("writeIO", data)`
- **参数**: `[driver_power_port_, 1.0]` (逻辑值1=开启)
- **用途**: 启用反射光成像驱动器电源（OUT2）
- **控制器**: 使用配置的专用控制器（`driver_power_controller_`）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(driver_power_port_);
params[1] = 1.0;  // 逻辑值1=开启（硬件值0=LOW，低电平有效）
Tango::DeviceData data;
data << params;
INFO_STREAM << "[PowerControl] Calling writeIO on " << driver_power_controller_ 
           << " with port=" << driver_power_port_ << ", value=1" << endl;
power_ctrl.command_inout("writeIO", data);
```

### 2.2 驱动器断电
- **函数**: `disable_driver_power()`
- **位置**: 第4108行
- **调用**: `power_ctrl.command_inout("writeIO", data)`
- **参数**: `[driver_power_port_, 0.0]` (逻辑值0=关闭)
- **用途**: 关闭反射光成像驱动器电源（OUT2）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(driver_power_port_);
params[1] = 0.0;  // 逻辑值0=关闭（硬件值1=HIGH，低电平有效）
Tango::DeviceData data;
data << params;
power_ctrl.command_inout("writeIO", data);
```

---

## 3. 六自由度设备 (SixDofDevice)

**文件**: `src/device_services/six_dof_device.cpp`

### 3.1 驱动器上电
- **函数**: `enable_driver_power()`
- **位置**: 第1493-1494行
- **调用**: `power_ctrl->command_inout("writeIO", data)`
- **参数**: `[driver_power_port_, 1.0]` (逻辑值1=开启)
- **用途**: 启用六自由度驱动器电源（OUT0）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(driver_power_port_);
params[1] = 1.0;  // 逻辑值1=开启（硬件值0=LOW，低电平有效）
Tango::DeviceData data;
data << params;
INFO_STREAM << "[PowerControl] Calling writeIO with port=" << driver_power_port_ << ", value=1" << endl;
power_ctrl->command_inout("writeIO", data);
```

### 3.2 驱动器断电
- **函数**: `disable_driver_power()`
- **位置**: 第1540行
- **调用**: `power_ctrl->command_inout("writeIO", data)`
- **参数**: `[driver_power_port_, 0.0]` (逻辑值0=关闭)
- **用途**: 关闭六自由度驱动器电源（OUT0）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(driver_power_port_);
params[1] = 0.0;  // 逻辑值0=关闭（硬件值1=HIGH，低电平有效）
Tango::DeviceData data;
data << params;
power_ctrl->command_inout("writeIO", data);
```

### 3.3 释放刹车
- **函数**: `release_brake()`
- **位置**: 第1594-1595行
- **调用**: `brake_ctrl->command_inout("writeIO", data)`
- **参数**: `[brake_power_port_, 1.0]` (逻辑值1=释放)
- **用途**: 释放六自由度刹车（OUT3）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(brake_power_port_);
params[1] = 1.0;  // 逻辑值1=释放（硬件值0=LOW，低电平有效）
Tango::DeviceData data;
data << params;
INFO_STREAM << "[PowerControl] Calling writeIO for brake with port=" << brake_power_port_ << ", value=1" << endl;
brake_ctrl->command_inout("writeIO", data);
```

### 3.4 启用刹车
- **函数**: `engage_brake()`
- **位置**: 第1641行
- **调用**: `brake_ctrl->command_inout("writeIO", data)`
- **参数**: `[brake_power_port_, 0.0]` (逻辑值0=启用)
- **用途**: 启用六自由度刹车（OUT3）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(brake_power_port_);
params[1] = 0.0;  // 逻辑值0=启用（硬件值1=HIGH，低电平有效）
Tango::DeviceData data;
data << params;
brake_ctrl->command_inout("writeIO", data);
```

---

## 4. 大行程设备 (LargeStrokeDevice)

**文件**: `src/device_services/large_stroke_device.cpp`

### 4.1 驱动器上电
- **函数**: `enable_driver_power()`
- **位置**: 第1463-1464行
- **调用**: `power_ctrl->command_inout("writeIO", data)`
- **参数**: `[driver_power_port_, 1.0]` (逻辑值1=开启)
- **用途**: 启用大行程驱动器电源（OUT0）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(driver_power_port_);
params[1] = 1.0;  // 逻辑值1=开启（硬件值0=LOW，低电平有效）
Tango::DeviceData data;
data << params;
INFO_STREAM << "[PowerControl] Calling writeIO with port=" << driver_power_port_ << ", value=1" << std::endl;
power_ctrl->command_inout("writeIO", data);
```

### 4.2 驱动器断电
- **函数**: `disable_driver_power()`
- **位置**: 第1510行
- **调用**: `power_ctrl->command_inout("writeIO", data)`
- **参数**: `[driver_power_port_, 0.0]` (逻辑值0=关闭)
- **用途**: 关闭大行程驱动器电源（OUT0）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(driver_power_port_);
params[1] = 0.0;  // 逻辑值0=关闭（硬件值1=HIGH，低电平有效）
Tango::DeviceData data;
data << params;
power_ctrl->command_inout("writeIO", data);
```

### 4.3 释放刹车
- **函数**: `release_brake()`
- **位置**: 第1564-1565行
- **调用**: `brake_ctrl->command_inout("writeIO", data)`
- **参数**: `[brake_power_port_, 1.0]` (逻辑值1=释放)
- **用途**: 释放大行程刹车（OUT4）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(brake_power_port_);
params[1] = 1.0;  // 逻辑值1=释放（硬件值0=LOW，低电平有效）
Tango::DeviceData data;
data << params;
INFO_STREAM << "[PowerControl] Calling writeIO for brake with port=" << brake_power_port_ << ", value=1" << std::endl;
brake_ctrl->command_inout("writeIO", data);
```

### 4.4 启用刹车
- **函数**: `engage_brake()`
- **位置**: 第1611行
- **调用**: `brake_ctrl->command_inout("writeIO", data)`
- **参数**: `[brake_power_port_, 0.0]` (逻辑值0=启用)
- **用途**: 启用大行程刹车（OUT4）

```cpp
Tango::DevVarDoubleArray params;
params.length(2);
params[0] = static_cast<double>(brake_power_port_);
params[1] = 0.0;  // 逻辑值0=启用（硬件值1=HIGH，低电平有效）
Tango::DeviceData data;
data << params;
brake_ctrl->command_inout("writeIO", data);
```

---

## 5. 测试脚本

### 5.1 诊断脚本
- **文件**: `scripts/diagnose_power_control.py`
- **位置**: 第139行、第146行、第151行
- **用途**: 测试`readIO`和`writeIO`命令

```python
# 读取当前状态
current = mc.command_inout("readIO", port)

# 写入值
params = [float(port), 1.0]  # 逻辑值1=开启
mc.command_inout("writeIO", params)

# 验证写入
after = mc.command_inout("readIO", port)
```

---

## 端口映射总结

根据配置文件，各设备的IO端口映射如下：

| 设备 | 端口 | 功能 | 控制器 |
|------|------|------|--------|
| 六自由度 | OUT0 | 驱动器上电 | sys/motion/3 |
| 六自由度 | OUT3 | 刹车供电 | sys/motion/3 |
| 大行程 | OUT0 | 驱动器上电 | sys/motion/3 |
| 大行程 | OUT4 | 刹车供电 | sys/motion/3 |
| 反射光成像 | OUT2 | 驱动器上电 | sys/motion/3 |
| 辅助支撑 | OUT6 | 驱动器上电 | sys/motion/3 |

---

## 注意事项

1. **所有调用都使用逻辑值**：传入`writeIO`的值是逻辑值（1=开启，0=关闭），`motion_controller_device.cpp`会自动转换为硬件值。

2. **注释需要更新**：代码中的注释如"HIGH = 上电"和"LOW = 断电"是误导性的，因为实际硬件是低电平有效。建议更新注释为"逻辑值1=开启"和"逻辑值0=关闭"。

3. **一致性**：所有设备服务都使用相同的模式调用`writeIO`，确保了代码的一致性。

4. **错误处理**：所有调用都包含在try-catch块中，会捕获`Tango::DevFailed`异常并记录错误日志。

---

## 修改建议

由于OUT端口是低电平有效，建议更新代码注释，将：
- `// HIGH = 上电` → `// 逻辑值1=开启（硬件值0=LOW，低电平有效）`
- `// LOW = 断电` → `// 逻辑值0=关闭（硬件值1=HIGH，低电平有效）`

这样可以避免混淆，明确说明传入的是逻辑值，硬件转换由底层函数自动处理。

