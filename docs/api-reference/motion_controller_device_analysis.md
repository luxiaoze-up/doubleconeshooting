# motion_controller_device.cpp 调用情况分析

文件: /app/src/device_services/motion_controller_device.cpp
行数: 1537
日期: 2025-12-30

## 一、 头文件依赖

项目头文件:
- device_services/motion_controller_device.h
- common/system_config.h
- drivers/LTSMC.h

标准库:
- iostream, cstdio, stdexcept, sstream, chrono, ctime, iomanip

## 二、 LTSMC驱动函数 (共22个)

1. smc_board_init
2. smc_board_close
3. smc_check_done
4. smc_clear_stop_reason
5. smc_emg_stop
6. smc_get_position_unit
7. smc_get_ain
8. smc_home_move
9. smc_pmove_unit
10. smc_stop
11. smc_set_profile_unit
12. smc_set_equiv
13. smc_set_da_output
14. smc_set_encoder_unit
15. smc_write_sevon_pin
16. smc_read_sevon_pin
17. smc_read_org_pin
18. smc_read_elp_pin
19. smc_read_eln_pin
20. smc_read_inport
21. smc_read_outbit
22. smc_write_outbit

## 三、 Tango框架调用

- set_state(): 25 times
- set_status(): 23 times
- throw_exception: 20 times

## 四、 类方法 (共约70个)

Lifecycle: init_device, delete_device, always_executed_hook
Connection: connect, disconnect, check_connection, try_reconnect
Lock: devLock, devUnlock, devLockVerify, devLockQuery
Motion: reset, moveZero, moveRelative, moveAbsolute, stopMove
IO: readIO, writeIO, readAD, writeAD, readOrg, readEL, readPos
Parameters: setMoveParameter, setStructParameter, setAnalog

## 五、 关键调用流程

Init: init_device() -> smc_board_init() -> set_state()
Move: moveXxx() -> smc_write_sevon_pin() -> smc_pmove_unit()
Health: always_executed_hook() -> check_connection_health()

## 六、 状态转换

INIT -> ON/FAULT
ON -> STANDBY
STANDBY -> MOVING
MOVING -> STANDBY
STANDBY -> DISABLE
* -> FAULT
* -> OFF

## 七、 错误处理

异常类型: HardwareError, DriverError, InvalidArgs, DISABLED, FAULT
重连机制: max 3 attempts, 2000ms interval

## 八 统计信息

- 行数: 1537
- 方法: ~70
- SMC函数: 22
- Tango命令: ~30
- Tango属性: ~15
- 状态: 7
- 锁保护: 16

生成日期: 2025-12-30


## 九、 回零流程（smc_home_move）

- 源位置：`moveZero()` 定义在 [src/device_services/motion_controller_device.cpp#L557](src/device_services/motion_controller_device.cpp#L557)，调用在 [src/device_services/motion_controller_device.cpp#L602-L604](src/device_services/motion_controller_device.cpp#L602-L604)。
- 前置条件：设备不处于 `FAULT`/`OFF`/`DISABLE`，目标轴在允许移动列表内，必要的驱动与编码器已初始化。
- 自动使能：若 `smc_read_sevon_pin` 发现伺服未使能，则尝试 `smc_write_sevon_pin` 打使能。
- 清除停止原因：按需执行 `smc_emg_stop`、`smc_clear_stop_reason`，避免残留停止原因。
- 核心调用：`short ret = smc_home_move(card_id_, axis_id);` 非零返回走 `check_error(ret, "smc_home_move")`。
- 状态与跟踪：置为 `MOVING` 并记录到 `moving_axes_`；后续通过 `smc_check_done` 检测完成并清理。
- 完成与错误处理：完成后恢复 `ON/STANDBY` 并更新位置；错误进入 `FAULT`。

```cpp
void MotionControllerDevice::moveZero(Tango::DevShort axis_id) {
    // 前置检查与自动使能（smc_read_sevon_pin / smc_write_sevon_pin）...
    // 清除停止原因：smc_emg_stop(...); smc_clear_stop_reason(...);

    DEBUG_STREAM << "[SMC] smc_home_move(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    short ret = smc_home_move(card_id_, axis_id);
    DEBUG_STREAM << "[SMC] smc_home_move() returned: " << ret << std::endl;

    if (ret != 0) {
        check_error(ret, "smc_home_move");
        return;
    }

    set_state(Tango::MOVING);
    moving_axes_.insert(axis_id);
    // 后续轮询 smc_check_done(...) 判断到位并收尾
}
```

- 要点小结：检查状态 → 自动使能 → 清停原因 → `smc_home_move` → `MOVING` → 轮询完成；错误走 `check_error` → `FAULT`。
