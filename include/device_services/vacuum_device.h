#ifndef VACUUM_DEVICE_H
#define VACUUM_DEVICE_H

#include "common/standard_system_device.h"
#include "common/plc_communication.h"
#include "device_services/vacuum_plc_mapping.h"
#include <tango.h>
#include <string>
#include <mutex>
#include <chrono>
#include <atomic>
#include <array>
#include <memory>
#include <vector>

namespace Vacuum {

// 自检状态枚举 - 按通信表定义
enum class SelfCheckState : Tango::DevLong {
    IDLE = 0,      // 程序启动时初始状态
    CHECKING = 1,  // 自检进行时
    SUCCEED = 2,   // 自检通过
    WARNING = 3,   // 自检通过，有报警但可用
    FAILED = 4     // 自检不通过
};

// 运行模式枚举
enum class OperationMode : Tango::DevShort {
    AUTO = 0,      // 自动模式
    MANUAL = 1     // 手动模式
};

// 系统状态枚举 - 基于新设计
enum class SystemState : Tango::DevShort {
    IDLE = 0,      // 空闲
    PUMPING = 1,   // 抽真空中
    VENTING = 2,   // 放气中
    FAULT = 3      // 故障
};

// 常量定义
constexpr size_t VACUUM_GAUGE_COUNT = 6;      // 真空计数量：主腔体4个, 电离室2个
constexpr size_t PRESSURE_SENSOR_COUNT = 6;   // 压力传感器数量
constexpr size_t LOW_PUMP_TEMP_COUNT = 8;     // 低温泵温度数量
constexpr size_t LOW_PUMP_VACUUM_COUNT = 4;   // 低温泵真空度数量
constexpr size_t EXHAUST_VALVE_COUNT = 10;    // 排气阀数量
constexpr size_t ANGLE_VALVE_COUNT = 6;       // 角阀数量
constexpr size_t GATE_VALVE_COUNT = 9;        // 闸板阀数量
constexpr size_t PUMP_COUNT = 11;             // 泵数量
constexpr size_t PENDULUM_VALVE_COUNT = 4;    // 摆阀数量
constexpr size_t ROUGHING_VALVE_COUNT = 6;    // 粗抽阀数量
constexpr size_t PURGE_VALVE_COUNT = 4;       // 吹扫阀数量
constexpr size_t COMM_VALVE_COUNT = 4;        // 连通阀数量
constexpr size_t AV_VALVE_COUNT = 8;          // AV阀数量
constexpr size_t BV_VALVE_COUNT = 8;          // BV阀数量
constexpr size_t CONNECTING_VALVE_COUNT = 2;  // 连接阀数量
constexpr size_t COARSE_PUMPING_VALVE_COUNT = 6; // 粗抽阀数量

class VacuumDevice : public Common::StandardSystemDevice {
private:
    // Lock system
    bool is_locked_;
    std::string lock_user_;
    std::mutex lock_mutex_;
    
    // ===== 固有状态属性 (CTS通信表 Sheet: 固有状态) =====
    std::string bundle_no_;           // 束组编码
    std::string laser_no_;            // 子束编码
    std::string system_no_;           // 系统/分系统编码
    std::string sub_dev_list_;        // 关联设备名 (JSON数组)
    std::vector<std::string> model_list_;  // 支持型号列表
    std::string current_model_;       // 当前型号
    std::string connect_string_;      // 设备连接信息 (JSON)
    std::string error_dict_;          // 错误编码信息 (JSON)
    std::string device_position_;     // 设备安装位置描述
    std::string device_product_date_; // 设备出厂日期
    std::string device_install_date_; // 设备上线日期
    bool sim_mode_;                   // 模拟模式（从数据库 simulator_mode 属性读取）
    
    // Configuration
    std::string plc_ip_;
    int plc_port_;                         // PLC端口，默认102
    
    // PLC通信
    std::unique_ptr<Common::PLC::IPLCCommunication> plc_comm_;
    std::mutex plc_comm_mutex_;            // PLC通信互斥锁
    int plc_update_interval_ms_;           // PLC状态更新周期（毫秒）
    std::chrono::steady_clock::time_point last_plc_update_;
    int plc_reconnect_interval_ms_;        // PLC重连节流周期（毫秒）
    std::chrono::steady_clock::time_point last_connect_attempt_;
    std::atomic<bool> reconnect_in_progress_;
    
    // 自检状态 - 按通信表定义
    SelfCheckState self_check_state_;      // 自检状态枚举
    std::string self_check_status_;        // 自检结果描述
    
    // 运行模式
    OperationMode mode_;                   // 0-自动模式 1-手动模式
    
    // 系统状态 - 基于新设计
    SystemState system_state_;             // 系统状态：空闲、抽真空中、放气中、故障
    
    // ===== 基于新PLC Tags的状态 =====
    // 泵状态
    bool screw_pump_power_;                // 螺杆泵上电反馈
    bool roots_pump_power_;                // 罗茨泵上电反馈
    bool molecular_pump1_power_;           // 分子泵1上电反馈
    bool molecular_pump2_power_;           // 分子泵2上电反馈
    bool molecular_pump3_power_;           // 分子泵3上电反馈
    double screw_pump_speed_;              // 螺杆泵转速
    uint16_t molecular_pump1_speed_;       // 分子泵1转速
    uint16_t molecular_pump2_speed_;       // 分子泵2转速
    uint16_t molecular_pump3_speed_;       // 分子泵3转速
    
    // 传感器数据
    double vacuum_gauge1_;                 // 前级电阻规示数
    double vacuum_gauge2_;                 // 真空规1示数
    double vacuum_gauge3_;                 // 真空规2示数
    double water_pressure_;                // 水路压力传感器
    double air_pressure_;                 // 气路压力传感器
    
    // 参数设置
    int16_t molecular_pump_start_stop_select_;  // 分子泵启停选择 (0-7, 位0-2对应分子泵1-3)
    int16_t gauge_criterion_;              // 规管判据
    int16_t molecular_pump_criterion_;     // 分子泵判据
    
    // 状态反馈
    bool auto_state_;                      // 自动状态
    bool manual_state_;                    // 手动状态
    bool remote_state_;                    // 远程状态
    
    // 异常标志
    bool gate_valve1_fault_;               // 闸板阀1到位异常
    bool gate_valve2_fault_;               // 闸板阀2到位异常
    bool gate_valve3_fault_;               // 闸板阀3到位异常
    bool gate_valve4_fault_;               // 闸板阀4到位异常
    bool gate_valve5_fault_;               // 闸板阀5到位异常
    bool electromagnetic_valve1_fault_;    // 电磁阀1异常
    bool electromagnetic_valve2_fault_;    // 电磁阀2异常
    bool electromagnetic_valve3_fault_;    // 电磁阀3异常
    bool electromagnetic_valve4_fault_;    // 电磁阀4异常
    bool vent_valve1_fault_;               // 放气阀1异常
    bool vent_valve2_fault_;               // 放气阀2异常
    bool phase_sequence_fault_;            // 相序异常报警
    bool screw_pump_water_fault_;          // 螺杆泵缺水故障
    bool molecular_pump1_water_fault_;     // 分子泵1缺水故障
    bool molecular_pump2_water_fault_;     // 分子泵2缺水故障
    bool molecular_pump3_water_fault_;     // 分子泵3缺水故障
    
    // 阀门状态（开/关位置反馈）
    bool gate_valve1_open_;                // 闸板阀1开到位
    bool gate_valve1_close_;               // 闸板阀1关到位
    bool gate_valve2_open_;                // 闸板阀2开到位
    bool gate_valve2_close_;               // 闸板阀2关到位
    bool gate_valve3_open_;                // 闸板阀3开到位
    bool gate_valve3_close_;               // 闸板阀3关到位
    bool gate_valve4_open_;                // 闸板阀4开到位
    bool gate_valve4_close_;               // 闸板阀4关到位
    bool gate_valve5_open_;                // 闸板阀5开到位
    bool gate_valve5_close_;               // 闸板阀5关到位
    
    // 电磁阀状态（开/关位置反馈）
    bool electromagnetic_valve1_open_;     // 电磁阀1开到位
    bool electromagnetic_valve1_close_;    // 电磁阀1关到位
    bool electromagnetic_valve2_open_;     // 电磁阀2开到位
    bool electromagnetic_valve2_close_;    // 电磁阀2关到位
    bool electromagnetic_valve3_open_;     // 电磁阀3开到位
    bool electromagnetic_valve3_close_;    // 电磁阀3关到位
    bool electromagnetic_valve4_open_;     // 电磁阀4开到位
    bool electromagnetic_valve4_close_;    // 电磁阀4关到位
    
    // 放气阀状态（开/关位置反馈）
    bool vent_valve1_open_;                // 放气阀1开到位
    bool vent_valve1_close_;               // 放气阀1关到位
    bool vent_valve2_open_;                // 放气阀2开到位
    bool vent_valve2_close_;               // 放气阀2关到位
    
    // 属性缓存变量 (用于Tango属性读取)
    Tango::DevShort attr_operation_mode_;  // operationMode属性值
    Tango::DevShort attr_system_state_;    // systemState属性值
    Tango::DevBoolean attr_sim_mode_;      // simulatorMode属性值
    Tango::DevBoolean attr_auto_state_;    // autoState属性值
    Tango::DevBoolean attr_manual_state_;  // manualState属性值
    Tango::DevBoolean attr_remote_state_;  // remoteState属性值
    Tango::DevLong attr_self_check_state_read; // 自检状态影子变量
    Tango::DevString attr_bundle_no_read;
    Tango::DevString attr_laser_no_read;
    Tango::DevString attr_system_no_read;
    Tango::DevString attr_sub_dev_list_read;
    Tango::DevString attr_model_list_read;
    Tango::DevString attr_current_model_read;
    Tango::DevString attr_connect_string_read;
    Tango::DevString attr_error_dict_read;
    Tango::DevString attr_device_position_read;
    Tango::DevString attr_device_product_date_read;
    Tango::DevString attr_device_install_date_read;
    Tango::DevString attr_self_check_status_read;
    Tango::DevString attr_group_attribute_json_read;
    // 泵状态缓存
    Tango::DevBoolean attr_screw_pump_power_;
    Tango::DevBoolean attr_roots_pump_power_;
    Tango::DevBoolean attr_molecular_pump1_power_;
    Tango::DevBoolean attr_molecular_pump2_power_;
    Tango::DevBoolean attr_molecular_pump3_power_;
    // 异常状态缓存
    Tango::DevBoolean attr_gate_valve1_fault_;
    Tango::DevBoolean attr_gate_valve2_fault_;
    Tango::DevBoolean attr_gate_valve3_fault_;
    Tango::DevBoolean attr_gate_valve4_fault_;
    Tango::DevBoolean attr_gate_valve5_fault_;
    Tango::DevBoolean attr_electromagnetic_valve1_fault_;
    Tango::DevBoolean attr_electromagnetic_valve2_fault_;
    Tango::DevBoolean attr_electromagnetic_valve3_fault_;
    Tango::DevBoolean attr_electromagnetic_valve4_fault_;
    Tango::DevBoolean attr_vent_valve1_fault_;
    Tango::DevBoolean attr_vent_valve2_fault_;
    Tango::DevBoolean attr_phase_sequence_fault_;
    Tango::DevBoolean attr_screw_pump_water_fault_;
    Tango::DevBoolean attr_molecular_pump1_water_fault_;
    Tango::DevBoolean attr_molecular_pump2_water_fault_;
    Tango::DevBoolean attr_molecular_pump3_water_fault_;
    
    // 阀门状态缓存
    Tango::DevBoolean attr_gate_valve1_open_;
    Tango::DevBoolean attr_gate_valve1_close_;
    Tango::DevBoolean attr_gate_valve2_open_;
    Tango::DevBoolean attr_gate_valve2_close_;
    Tango::DevBoolean attr_gate_valve3_open_;
    Tango::DevBoolean attr_gate_valve3_close_;
    Tango::DevBoolean attr_gate_valve4_open_;
    Tango::DevBoolean attr_gate_valve4_close_;
    Tango::DevBoolean attr_gate_valve5_open_;
    Tango::DevBoolean attr_gate_valve5_close_;
    
    Tango::DevBoolean attr_electromagnetic_valve1_open_;
    Tango::DevBoolean attr_electromagnetic_valve1_close_;
    Tango::DevBoolean attr_electromagnetic_valve2_open_;
    Tango::DevBoolean attr_electromagnetic_valve2_close_;
    Tango::DevBoolean attr_electromagnetic_valve3_open_;
    Tango::DevBoolean attr_electromagnetic_valve3_close_;
    Tango::DevBoolean attr_electromagnetic_valve4_open_;
    Tango::DevBoolean attr_electromagnetic_valve4_close_;
    
    Tango::DevBoolean attr_vent_valve1_open_;
    Tango::DevBoolean attr_vent_valve1_close_;
    Tango::DevBoolean attr_vent_valve2_open_;
    Tango::DevBoolean attr_vent_valve2_close_;
    
    // Runtime state
    short result_value_;       // 0=success, 1=fail
    
public:
    VacuumDevice(Tango::DeviceClass *device_class, std::string &device_name);
    virtual ~VacuumDevice();
    
    virtual void init_device() override;
    virtual void delete_device() override;
    
    // ===== COMMANDS =====
    // Lock/Unlock
    void devLock(Tango::DevString user_info);
    void devUnlock(Tango::DevBoolean unlock_all);
    void devLockVerify();
    Tango::DevString devLockQuery();
    void devUserConfig(Tango::DevString config);
    
    // System
    void selfCheck();
    void init();
    void reset();
    
    // ===== 基于新PLC Tags的命令 =====
    // 模式切换
    void switchMode(Tango::DevShort mode);  // 0-自动, 1-手动
    void setRemoteControl(Tango::DevBoolean remote); // true-远程, false-本地
    
    // 一键操作
    void oneKeyVacuumStart();               // 一键抽真空启动
    void oneKeyVacuumStop();                // 一键抽真空停止
    void ventStart();                       // 放气启动
    void ventStop();                        // 放气停止
    
    // 泵控制
    void setScrewPumpPower(Tango::DevBoolean state);
    void setScrewPumpStartStop(Tango::DevBoolean state);
    void setRootsPumpPower(Tango::DevBoolean state);
    void setMolecularPumpPower(const Tango::DevVarShortArray *argin);  // [index, state]
    void setMolecularPumpStartStop(const Tango::DevVarShortArray *argin);  // [index, state]
    void setScrewPumpSpeed(Tango::DevDouble speed);
    void resetScrewPumpFault();
    
    // 阀门控制
    void setGateValve(const Tango::DevVarShortArray *argin);  // [index, open/close: 1-open, 0-close]
    void setElectromagneticValve(const Tango::DevVarShortArray *argin);  // [index, state]
    void setVentValve(const Tango::DevVarShortArray *argin);  // [index, state]
    void setWaterElectromagneticValve(const Tango::DevVarShortArray *argin);  // [index, state]
    void setAirMainElectromagneticValve(Tango::DevBoolean state);
    
    // 参数设置
    void setMolecularPumpStartStopSelect(Tango::DevShort select);  // 0-7, 位0-2对应分子泵1-3
    void setGaugeCriterion(Tango::DevShort criterion);
    void setMolecularPumpCriterion(Tango::DevShort criterion);
    
    // 系统命令
    void alarmReset();                      // 报警复位
    void emergencyStop();                   // 急停
    
    // PLC通信相关
    void connectPLC();                     // 连接PLC
    void disconnectPLC();                  // 断开PLC
    void updatePLCData();                  // 更新PLC数据（从PLC读取）
    void syncPLCData();                    // 同步PLC数据（写入PLC）
    
    
    // ===== ATTRIBUTES =====
    virtual void read_attr(Tango::Attribute &attr) override;
    virtual void write_attr(Tango::WAttribute &attr);
    
    // 固有状态属性 (CTS通信表 Sheet: 固有状态)
    void read_bundle_no(Tango::Attribute &attr);
    void read_laser_no(Tango::Attribute &attr);
    void log_command_result(const std::string &name, const std::string &args, bool ok, bool plc_connected);
    void read_system_no(Tango::Attribute &attr);
    void read_sub_dev_list(Tango::Attribute &attr);
    void read_model_list(Tango::Attribute &attr);
    void read_current_model(Tango::Attribute &attr);
    void read_connect_string(Tango::Attribute &attr);
    void read_error_dict(Tango::Attribute &attr);
    void read_device_position(Tango::Attribute &attr);
    void read_device_product_date(Tango::Attribute &attr);
    void read_device_install_date(Tango::Attribute &attr);
    void read_simulator_mode(Tango::Attribute &attr);
    
    // 自检状态属性
    void read_self_check_state(Tango::Attribute &attr);    // 自检状态枚举
    void read_self_check_status(Tango::Attribute &attr);   // 自检结果描述
    void read_mode(Tango::Attribute &attr);                // 运行模式
    
    // ===== 基于新PLC Tags的属性读取 =====
    // 运行状态属性
    void read_operation_mode(Tango::Attribute &attr);
    void read_system_state(Tango::Attribute &attr);
    void read_auto_state(Tango::Attribute &attr);
    void read_manual_state(Tango::Attribute &attr);
    void read_remote_state(Tango::Attribute &attr);
    
    // 传感器数据属性
    void read_vacuum_gauge1(Tango::Attribute &attr);
    void read_vacuum_gauge2(Tango::Attribute &attr);
    void read_vacuum_gauge3(Tango::Attribute &attr);
    void read_water_pressure(Tango::Attribute &attr);
    void read_air_pressure(Tango::Attribute &attr);
    
    // 泵状态属性
    void read_screw_pump_power(Tango::Attribute &attr);
    void read_roots_pump_power(Tango::Attribute &attr);
    void read_molecular_pump1_power(Tango::Attribute &attr);
    void read_molecular_pump2_power(Tango::Attribute &attr);
    void read_molecular_pump3_power(Tango::Attribute &attr);
    void read_screw_pump_speed(Tango::Attribute &attr);
    void read_molecular_pump1_speed(Tango::Attribute &attr);
    void read_molecular_pump2_speed(Tango::Attribute &attr);
    void read_molecular_pump3_speed(Tango::Attribute &attr);
    
    // 参数设置属性
    void read_molecular_pump_start_stop_select(Tango::Attribute &attr);
    void read_gauge_criterion(Tango::Attribute &attr);
    void read_molecular_pump_criterion(Tango::Attribute &attr);
    
    // 异常状态属性
    void read_gate_valve1_fault(Tango::Attribute &attr);
    void read_gate_valve2_fault(Tango::Attribute &attr);
    void read_gate_valve3_fault(Tango::Attribute &attr);
    void read_gate_valve4_fault(Tango::Attribute &attr);
    void read_gate_valve5_fault(Tango::Attribute &attr);
    void read_electromagnetic_valve1_fault(Tango::Attribute &attr);
    void read_electromagnetic_valve2_fault(Tango::Attribute &attr);
    void read_electromagnetic_valve3_fault(Tango::Attribute &attr);
    void read_electromagnetic_valve4_fault(Tango::Attribute &attr);
    void read_vent_valve1_fault(Tango::Attribute &attr);
    void read_vent_valve2_fault(Tango::Attribute &attr);
    void read_phase_sequence_fault(Tango::Attribute &attr);
    void read_screw_pump_water_fault(Tango::Attribute &attr);
    void read_molecular_pump1_water_fault(Tango::Attribute &attr);
    void read_molecular_pump2_water_fault(Tango::Attribute &attr);
    void read_molecular_pump3_water_fault(Tango::Attribute &attr);
    
    // 阀门状态属性
    void read_gate_valve1_open(Tango::Attribute &attr);
    void read_gate_valve1_close(Tango::Attribute &attr);
    void read_gate_valve2_open(Tango::Attribute &attr);
    void read_gate_valve2_close(Tango::Attribute &attr);
    void read_gate_valve3_open(Tango::Attribute &attr);
    void read_gate_valve3_close(Tango::Attribute &attr);
    void read_gate_valve4_open(Tango::Attribute &attr);
    void read_gate_valve4_close(Tango::Attribute &attr);
    void read_gate_valve5_open(Tango::Attribute &attr);
    void read_gate_valve5_close(Tango::Attribute &attr);
    
    void read_electromagnetic_valve1_open(Tango::Attribute &attr);
    void read_electromagnetic_valve1_close(Tango::Attribute &attr);
    void read_electromagnetic_valve2_open(Tango::Attribute &attr);
    void read_electromagnetic_valve2_close(Tango::Attribute &attr);
    void read_electromagnetic_valve3_open(Tango::Attribute &attr);
    void read_electromagnetic_valve3_close(Tango::Attribute &attr);
    void read_electromagnetic_valve4_open(Tango::Attribute &attr);
    void read_electromagnetic_valve4_close(Tango::Attribute &attr);
    
    void read_vent_valve1_open(Tango::Attribute &attr);
    void read_vent_valve1_close(Tango::Attribute &attr);
    void read_vent_valve2_open(Tango::Attribute &attr);
    void read_vent_valve2_close(Tango::Attribute &attr);
    
    // 标准属性
    void read_result_value(Tango::Attribute &attr);
    void read_group_attribute_json(Tango::Attribute &attr);
    
    // Hooks
    virtual void specific_self_check() override;
    virtual void always_executed_hook() override;
    virtual void read_attr_hardware(std::vector<long> &attr_list) override;
    
    // Public interface
    double getCurrentPressure() const { return vacuum_gauge1_; }
    bool isPumping() const { return system_state_ == SystemState::PUMPING; }
    double getTargetPressure() const { return vacuum_gauge1_; }  // 使用真空规1作为当前压力
    
private:
    void log_event(const std::string &event);
    void ensure_unlocked(const char *origin);
    void ensure_permission(size_t permission_index, const char *origin, bool check_mode = true);
    
    // PLC通信辅助方法
    bool readPLCBool(const Common::PLC::PLCAddress& address, bool& value);
    bool readPLCWord(const Common::PLC::PLCAddress& address, uint16_t& value);
    bool readPLCInt(const Common::PLC::PLCAddress& address, int16_t& value);
    bool readPLCReal(const Common::PLC::PLCAddress& address, float& value);
    bool writePLCBool(const Common::PLC::PLCAddress& address, bool value);
    bool writePLCWord(const Common::PLC::PLCAddress& address, uint16_t value);
    bool writePLCInt(const Common::PLC::PLCAddress& address, int16_t value);
    
    // 状态机更新
    void updateSystemState();
    void checkFaults();
};

class VacuumDeviceClass : public Tango::DeviceClass {
public:
    static VacuumDeviceClass *instance();
    
protected:
    VacuumDeviceClass(std::string &class_name);
    
public:
    virtual void attribute_factory(std::vector<Tango::Attr *> &att_list) override;
    virtual void command_factory() override;
    virtual void device_factory(const Tango::DevVarStringArray *devlist_ptr) override;
    
private:
    static VacuumDeviceClass *_instance;
};

} // namespace Vacuum

#endif // VACUUM_DEVICE_H
