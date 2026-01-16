# six_dof_device.cpp 调用情况梳理

> 文件路: /app/src/device_services/six_dof_device.cpp  
> 总行数: 2351 行  
> 生成日期: 2025年12月31日

---

## 一、头文件依

### 项目/系统头文件
- device_services/six_dof_device.h
- common/system_config.h

### 标准库
- cmath, iostream, algorithm, sstream, ctime, iomanip, fstream, unordered_map, memory, chrono, stdexcept

---

## 二Tango框架与代理调用

### 设备代理与通信
- Tango::DeviceProxy 创建: create_proxy_and_ping(name, timeout_ms)
- Ping 封装: ping_with_temp_timeout_or_throw(proxy, timeout_ms), ping_with_temp_timeout(...)
- 代理重建: rebuild_motion_controller_proxy(timeout_ms), rebuild_encoder_proxy(timeout_ms)
- 代理获取/重置: get_motion_controller_proxy(), get_encoder_proxy(), reset_motion_controller_proxy(), reset_encoder_proxy()

### 框架统计
- set_state(): 19 次
- set_status(): 13 次
- Tango::Except::throw_exception: 37 次
- 常用类型: DevBoolean/DevShort/DevDouble/DevString/DevVarDoubleArray/DevVarStringArray, Attr/Attribute/AttrWriteType, DeviceClass/DeviceImpl/DeviceProxy

---

## 三、类方法分组 (节)

### 生命周期与连接管理
- SixDofDevice(Tango::DeviceClass*, std::string&)
- ~SixDofDevice()
- init_device() / delete_device()
- start_connection_monitor() / stop_connection_monitor() / connection_monitor_loop()
- connect_proxies() / perform_post_motion_reconnect_restore()

### 锁定与用户配置
- devLock() / devUnlock(DevBoolean) / devLockVerify() / devLockQuery() / devUserConfig()
- check_state(const std::string& cmd_name) 基于 kStateMatrix 检查状态

### 六自由度运动命令
- 组'EOF''EOF': moveAxisSet(const DevVarDoubleArray*), structAxisSet(const DevVarDoubleArray*)
- 姿态运动: movePoseRelative(const DevVarDoubleArray*), movePoseAbsolute(const DevVarDoubleArray*)
- 复位与回零: reset(), sixMoveZero(), singleReset(DevShort)
- 单轴运动: singleMoveRelative(const DevVarDoubleArray*), singleMoveAbsolute(const DevVarDoubleArray*), send_move_command(int axis, double position, bool relative)
- 急停: stop()

### 编码器与状态读写
- 读取: readEncoder(), readOrg(DevShort), readEL(DevShort), readtAxis()
- 属性读写接口: read_attr, write_attr, 以及大量只读属性: 
  - read_driver_power_status, read_brake_status, read_self_check_result, read_position_unit, write_position_unit
  - read_group_attribute_json, read_axis_pos, read_dire_pos, read_open_brake_state, read_six_freedom_pose, read_six_logs
  - read_alarm_state, read_axis_parameter, read_lim_org_state, read_sdof_state, read_result_value

### 电源与抱闸控制
- 命令接口: enableDriverPower(), disableDriverPower(), releaseBrake(), engageBrake(), queryPowerStatus()
- 底层实现: enable_driver_power(), disable_driver_power(), release_brake(), engage_brake()

### 运动学与姿态更新
- validate_pose(const std::array<double, NUM_AXES>&)
- update_axis_positions() / update_pose_from_encoders()
- configure_kinematics()

### 设备类与工厂
- SixDofDeviceClass::_instance, SixDofDeviceClass::instance()
- SixDofDeviceClass::attribute_factory(...)
- SixDofDeviceClass::command_factory()
- SixDofDeviceClass::device_factory(...)
- Tango::DServer::class_factory()

---

## 四、关键调用流程

### 4.1 初始化与连接
init_device()
  → 读取配置(SystemConfig)
  → 创建/重建代理(create_proxy_and_ping, rebuild_*_proxy)
  → set_state(ON/FAULT), set_status(...)
  → 启动连接监控(start_connection_monitor)

### 4.2 六自由度运动
movePoseAbsolute(pose)
  → validate_pose(pose)
  → check_state("movePoseAbsolute")
  → 根据坐标系/单位转换与限幅(POS_LIMIT/ROT_LIMIT)
  → 下发各轴命令(send_move_command)
  → 更新位置(update_axis_positions / update_pose_from_encoders)
 完成后 set_state(STANDBY/ON)

### 4.3 单轴运动与回零/复位
singleMoveAbsolute([axis, position]) / singleMoveRelative([axis, delta])
  → 参数校验
  → check_state(cmd)
  → send_move_command(axis, position, relative)
  → 状态与结果更新

sixMoveZero()
  → 逐轴回零
  → 编码器与姿态刷新

reset()/singleReset(axis)
  → 清错/恢复连接(perform_post_motion_reconnect_restore)
  → 状态恢复

### 4.4 电源与抱闸
enableDriverPower()/disableDriverPower()
  → enable_driver_power()/disable_driver_power()
  → 读取与更新 driver_power_status

releaseBrake()/engageBrake()
  → release_brake()/engage_brake()
  → 更新 brake_status/open_brake_state

### 4.5 连接监控与自检
start_connection_monitor() → connection_monitor_loop()
  → 周期 ping/proxy 检查
  → 失败时重建代理(rebuild_*_proxy)

selfCheck()/specific_self_check()
  → 编码器读数校验(readEncoder)
  → 关键属性更新

---

## 五、状态与权限矩阵

- kStateMatrix 定义各命令在 UNKNOWN/OFF/ON/FAULT 下是否允许执行
- check_state(cmd_name) 按矩阵校验并抛出异常(throw_exception)防止非法状态下的操作

---

## 六、错误处理与日志

- 统一抛错: Tango::Except::throw_exception (37次)
- 事件日志: log_event(const std::string &event) 统一记录
- 连接失败与恢复: 通过代理重建与状态恢复逻辑实现

---

## 七'EOF'

- 行数: 2351
- 方法数量(节选清单中列出超60个)
- set_state(): 19 次
- set_status(): 13 次
- throw_exception(): 37 次
- 关键限幅常量: POS_LIMIT=15.0, ROT_LIMIT=3.0
- 主要依: Tango 框架(DeviceProxy, Attr/Attribute, DeviceClass), 标准库容器/时间/IO

---

#
motion_controller_device_analysis.md  motion_controller_device_analysis. 2025年12月31日
