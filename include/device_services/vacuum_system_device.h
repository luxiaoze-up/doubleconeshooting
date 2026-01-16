/**
 * @file vacuum_system_device.h
 * @brief 真空系统 Tango 设备服务 - 头文件
 * 
 * 设备名: sys/vacuum/2
 * 通讯协议: OPC UA
 * 
 * 功能:
 * 1. 自动模式: 一键抽真空、一键停机、腔室放气
 * 2. 手动模式: 各泵阀独立控制
 * 3. 状态监测: 阀门到位检测、报警事件推送
 * 4. 报警管理: 40种异常类型检测与事件通知
 */

#ifndef VACUUM_SYSTEM_DEVICE_H
#define VACUUM_SYSTEM_DEVICE_H

#include <tango.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <map>
#include <atomic>
#include <thread>
#include <queue>

#include "common/plc_communication.h"

namespace VacuumSystem {

// ============================================================================
// 枚举类型定义
// ============================================================================

/**
 * @brief 操作模式
 */
enum class OperationMode {
    AUTO = 0,       // 自动模式
    MANUAL = 1      // 手动模式
};

/**
 * @brief 系统状态
 */
enum class SystemState {
    IDLE = 0,           // 空闲
    PUMPING = 1,        // 抽真空中
    STOPPING = 2,       // 停机中
    VENTING = 3,        // 放气中
    FAULT = 4,          // 故障
    EMERGENCY_STOP = 5  // 急停
};

/**
 * @brief 阀门动作状态
 */
enum class ValveActionState {
    IDLE = 0,           // 空闲
    OPENING = 1,        // 正在打开
    CLOSING = 2,        // 正在关闭
    OPEN_TIMEOUT = 3,   // 开到位超时
    CLOSE_TIMEOUT = 4   // 关到位超时
};

/**
 * @brief 报警信息结构
 */
struct AlarmInfo {
    int alarm_code;
    std::string alarm_type;
    std::string description;
    std::string device_name;
    std::chrono::system_clock::time_point timestamp;
    bool acknowledged;
    
    AlarmInfo() : alarm_code(0), acknowledged(false) {}
    AlarmInfo(int code, const std::string& type, const std::string& desc, const std::string& dev)
        : alarm_code(code), alarm_type(type), description(desc), device_name(dev),
          timestamp(std::chrono::system_clock::now()), acknowledged(false) {}
};

/**
 * @brief 阀门动作跟踪器
 */
struct ValveActionTracker {
    int valve_index;
    bool target_open;  // true=开, false=关
    std::chrono::steady_clock::time_point start_time;
    ValveActionState state;
    
    ValveActionTracker() : valve_index(0), target_open(false), state(ValveActionState::IDLE) {}
};

// ============================================================================
// VacuumSystemDevice 类
// ============================================================================

class VacuumSystemDevice : public Tango::Device_4Impl {
public:
    VacuumSystemDevice(Tango::DeviceClass* device_class, std::string& device_name);
    VacuumSystemDevice(Tango::DeviceClass* device_class, const char* device_name);
    virtual ~VacuumSystemDevice();
    
    // ----- 设备生命周期 -----
    void init_device();
    void delete_device();
    void always_executed_hook();
    void read_attr_hardware(std::vector<long>& attr_list);
    void read_attr(Tango::Attribute& attr);
    void write_attr(Tango::WAttribute& attr);
    
    // ========================================================================
    // Tango 命令 (Commands)
    // ========================================================================
    
    // ----- 系统命令 -----
    void Init();
    void Reset();
    void SelfCheck();
    
    // ----- 模式切换 -----
    void SwitchToAuto();
    void SwitchToManual();
    
    // ----- 自动模式操作 -----
    void OneKeyVacuumStart();           // 一键抽真空
    void OneKeyVacuumStop();            // 一键停机
    void ChamberVent();                 // 腔室放气
    void FaultReset();                  // 故障复位
    void EmergencyStop();               // 紧急停止
    
    // ----- 泵控制 (手动模式) -----
    void SetScrewPumpPower(Tango::DevBoolean state);
    void SetScrewPumpStartStop(Tango::DevBoolean state);
    void SetRootsPumpPower(Tango::DevBoolean state);
    void SetMolecularPumpPower(const Tango::DevVarShortArray* argin);   // [index, state]
    void SetMolecularPumpStartStop(const Tango::DevVarShortArray* argin);
    
    // ----- 阀门控制 (手动模式) -----
    void SetGateValve(const Tango::DevVarShortArray* argin);            // [index, operation]
    void SetElectromagneticValve(const Tango::DevVarShortArray* argin); // [index, state]
    void SetVentValve(const Tango::DevVarShortArray* argin);            // [index, state]
    void SetWaterValve(const Tango::DevVarShortArray* argin);           // [index, state]
    void SetAirMainValve(Tango::DevBoolean state);
    
    // ----- 报警管理 -----
    void AcknowledgeAlarm(Tango::DevLong alarm_code);
    void AcknowledgeAllAlarms();
    void ClearAlarmHistory();
    
    // ----- 查询命令 -----
    Tango::DevString GetOperationConditions(Tango::DevString device_name);  // 获取操作先决条件
    Tango::DevString GetActiveAlarms();                                      // 获取当前报警列表
    Tango::DevString GetSystemStatus();                                      // 获取系统状态JSON
    
    // ========================================================================
    // Tango 属性 (Attributes)
    // ========================================================================
    
    // ----- 系统状态属性 -----
    void read_operationMode(Tango::Attribute& attr);
    void read_systemState(Tango::Attribute& attr);
    void read_simulatorMode(Tango::Attribute& attr);
    
    // ----- 泵状态属性 -----
    void read_screwPumpPower(Tango::Attribute& attr);
    void read_rootsPumpPower(Tango::Attribute& attr);
    void read_molecularPump1Power(Tango::Attribute& attr);
    void read_molecularPump2Power(Tango::Attribute& attr);
    void read_molecularPump3Power(Tango::Attribute& attr);
    void read_molecularPump1Speed(Tango::Attribute& attr);
    void read_molecularPump2Speed(Tango::Attribute& attr);
    void read_molecularPump3Speed(Tango::Attribute& attr);
    
    // ----- 分子泵启用配置属性 -----
    void read_molecularPump1Enabled(Tango::Attribute& attr);
    void read_molecularPump2Enabled(Tango::Attribute& attr);
    void read_molecularPump3Enabled(Tango::Attribute& attr);
    void write_molecularPump1Enabled(Tango::WAttribute& attr);
    void write_molecularPump2Enabled(Tango::WAttribute& attr);
    void write_molecularPump3Enabled(Tango::WAttribute& attr);
    
    // ----- 阀门状态属性 (开到位/关到位) -----
    void read_gateValve1Open(Tango::Attribute& attr);
    void read_gateValve1Close(Tango::Attribute& attr);
    void read_gateValve2Open(Tango::Attribute& attr);
    void read_gateValve2Close(Tango::Attribute& attr);
    void read_gateValve3Open(Tango::Attribute& attr);
    void read_gateValve3Close(Tango::Attribute& attr);
    void read_gateValve4Open(Tango::Attribute& attr);
    void read_gateValve4Close(Tango::Attribute& attr);
    void read_gateValve5Open(Tango::Attribute& attr);
    void read_gateValve5Close(Tango::Attribute& attr);
    
    void read_electromagneticValve1Open(Tango::Attribute& attr);
    void read_electromagneticValve1Close(Tango::Attribute& attr);
    void read_electromagneticValve2Open(Tango::Attribute& attr);
    void read_electromagneticValve2Close(Tango::Attribute& attr);
    void read_electromagneticValve3Open(Tango::Attribute& attr);
    void read_electromagneticValve3Close(Tango::Attribute& attr);
    void read_electromagneticValve4Open(Tango::Attribute& attr);
    void read_electromagneticValve4Close(Tango::Attribute& attr);
    
    void read_ventValve1Open(Tango::Attribute& attr);
    void read_ventValve1Close(Tango::Attribute& attr);
    void read_ventValve2Open(Tango::Attribute& attr);
    void read_ventValve2Close(Tango::Attribute& attr);
    
    // ----- 阀门动作状态属性 (用于闪烁效果) -----
    void read_gateValve1ActionState(Tango::Attribute& attr);
    void read_gateValve2ActionState(Tango::Attribute& attr);
    void read_gateValve3ActionState(Tango::Attribute& attr);
    void read_gateValve4ActionState(Tango::Attribute& attr);
    void read_gateValve5ActionState(Tango::Attribute& attr);
    
    // ----- 传感器属性 -----
    void read_vacuumGauge1(Tango::Attribute& attr);   // 前级电阻规
    void read_vacuumGauge2(Tango::Attribute& attr);   // 主真空计1
    void read_vacuumGauge3(Tango::Attribute& attr);   // 主真空计2
    void read_airPressure(Tango::Attribute& attr);    // 气源压力
    
    // ----- 新增属性 -----
    void read_screwPumpFrequency(Tango::Attribute& attr);   // 螺杆泵频率
    void read_rootsPumpFrequency(Tango::Attribute& attr);   // 罗茨泵频率
    void read_autoSequenceStep(Tango::Attribute& attr);     // 自动流程步骤
    void read_plcConnected(Tango::Attribute& attr);         // PLC 连接状态
    void read_phaseSequenceOk(Tango::Attribute& attr);      // 相序保护状态
    void read_motionSystemOnline(Tango::Attribute& attr);   // 运动控制系统在线
    void read_gateValve5Permit(Tango::Attribute& attr);     // 闸板阀5动作许可
    void read_waterValve1State(Tango::Attribute& attr);     // 水电磁阀1状态
    void read_waterValve2State(Tango::Attribute& attr);     // 水电磁阀2状态
    void read_waterValve3State(Tango::Attribute& attr);     // 水电磁阀3状态
    void read_waterValve4State(Tango::Attribute& attr);     // 水电磁阀4状态
    void read_waterValve5State(Tango::Attribute& attr);     // 水电磁阀5状态
    void read_waterValve6State(Tango::Attribute& attr);     // 水电磁阀6状态
    void read_airMainValveState(Tango::Attribute& attr);    // 气主电磁阀状态
    
    // ----- 报警相关属性 -----
    void read_activeAlarmCount(Tango::Attribute& attr);
    void read_hasUnacknowledgedAlarm(Tango::Attribute& attr);
    void read_latestAlarmJson(Tango::Attribute& attr);

private:
    // ========================================================================
    // 内部成员变量
    // ========================================================================
    
    // ----- PLC 通信 -----
    std::unique_ptr<Common::PLC::IPLCCommunication> plc_comm_;
    std::string plc_ip_;
    int plc_port_;
    bool sim_mode_;
    std::mutex plc_mutex_;
    std::atomic<bool> plc_connecting_{false};  // PLC 连接进行中标志
    std::chrono::steady_clock::time_point last_connect_attempt_;  // 上次连接尝试时间
    std::atomic<bool> plc_was_connected_{false};  // 上次连接状态（用于检测连接断开）
    static constexpr int PLC_RECONNECT_INTERVAL_SEC = 5;  // 重连间隔（秒）
    
    // ----- 状态管理 -----
    OperationMode operation_mode_;
    SystemState system_state_;
    std::mutex state_mutex_;
    
    // Tango 读取专用影子变量 (确保指针在函数返回后依然有效)
    Tango::DevShort attr_operationMode_read;
    Tango::DevShort attr_systemState_read;
    Tango::DevLong attr_autoSequenceStep_read;
    Tango::DevLong attr_molecularPump1Speed_read;
    Tango::DevLong attr_molecularPump2Speed_read;
    Tango::DevLong attr_molecularPump3Speed_read;
    Tango::DevLong attr_gateValve1ActionState_read;
    Tango::DevLong attr_gateValve2ActionState_read;
    Tango::DevLong attr_gateValve3ActionState_read;
    Tango::DevLong attr_gateValve4ActionState_read;
    Tango::DevLong attr_gateValve5ActionState_read;
    Tango::DevBoolean attr_molecularPump1Enabled_read;
    Tango::DevBoolean attr_molecularPump2Enabled_read;
    Tango::DevBoolean attr_molecularPump3Enabled_read;
    Tango::DevLong attr_screwPumpFrequency_read;
    Tango::DevLong attr_rootsPumpFrequency_read;
    Tango::DevLong attr_activeAlarmCount_read;
    
    // ----- 泵状态 -----
    bool screw_pump_power_;
    bool roots_pump_power_;
    bool molecular_pump1_power_;
    bool molecular_pump2_power_;
    bool molecular_pump3_power_;
    int molecular_pump1_speed_;
    int molecular_pump2_speed_;
    int molecular_pump3_speed_;
    
    // ----- 分子泵启用配置 -----
    bool molecular_pump1_enabled_;
    bool molecular_pump2_enabled_;
    bool molecular_pump3_enabled_;
    
    // ----- 阀门到位状态 -----
    // 闸板阀
    bool gate_valve1_open_, gate_valve1_close_;
    bool gate_valve2_open_, gate_valve2_close_;
    bool gate_valve3_open_, gate_valve3_close_;
    bool gate_valve4_open_, gate_valve4_close_;
    bool gate_valve5_open_, gate_valve5_close_;
    // 电磁阀
    bool electromagnetic_valve1_open_, electromagnetic_valve1_close_;
    bool electromagnetic_valve2_open_, electromagnetic_valve2_close_;
    bool electromagnetic_valve3_open_, electromagnetic_valve3_close_;
    bool electromagnetic_valve4_open_, electromagnetic_valve4_close_;
    // 放气阀
    bool vent_valve1_open_, vent_valve1_close_;
    bool vent_valve2_open_, vent_valve2_close_;
    
    // ----- 阀门动作跟踪 -----
    std::map<std::string, ValveActionTracker> valve_trackers_;
    std::mutex tracker_mutex_;
    static constexpr int VALVE_TIMEOUT_MS = 5000;  // 5秒超时
    
    // ----- 传感器读数 -----
    double vacuum_gauge1_;  // Pa
    double vacuum_gauge2_;  // Pa
    double vacuum_gauge3_;  // Pa
    double air_pressure_;   // MPa
    int screw_pump_frequency_;  // Hz
    int roots_pump_frequency_;  // Hz - 罗茨泵频率
    
    // ----- 系统联锁信号 -----
    bool phase_sequence_ok_;         // 相序保护
    bool motion_system_online_;      // 运动控制系统在线
    bool gate_valve5_permit_;        // 闸板阀5动作许可
    bool motion_req_open_gv5_;       // 运动系统请求开闸板阀5
    bool motion_req_close_gv5_;      // 运动系统请求关闸板阀5
    
    // ----- 水电磁阀状态 -----
    bool water_valve1_state_;
    bool water_valve2_state_;
    bool water_valve3_state_;
    bool water_valve4_state_;
    bool water_valve5_state_;
    bool water_valve6_state_;
    bool air_main_valve_state_;
    
    // ----- 报警管理 -----
    std::vector<AlarmInfo> active_alarms_;
    std::vector<AlarmInfo> alarm_history_;
    std::mutex alarm_mutex_;
    std::string alarm_log_path_;
    
    // ----- 后台轮询线程 -----
    std::thread poll_thread_;
    std::atomic<bool> poll_running_;
    int poll_interval_ms_;
    
    // ----- 自动模式状态机 -----
    int auto_sequence_step_;
    std::chrono::steady_clock::time_point auto_step_start_time_;
    bool vacuum_sequence_is_low_vacuum_;  // 记录当前抽真空流程类型：true=低真空流程(100-114)，false=非真空流程(1-10)
    
    // ========================================================================
    // 内部方法
    // ========================================================================
    
    // ----- PLC 通信 -----
    bool connectPLC();
    bool connectPLC_locked();  // 内部方法：假设调用者已持有 plc_mutex_
    void disconnectPLC();
    bool readPLCBool(const Common::PLC::PLCAddress& addr, bool& value);
    bool readPLCWord(const Common::PLC::PLCAddress& addr, uint16_t& value);
    bool writePLCBool(const Common::PLC::PLCAddress& addr, bool value);
    bool writePLCWord(const Common::PLC::PLCAddress& addr, uint16_t value);
    
    // ----- 状态更新 -----
    void synchronizeStateFromPLC();     // 从 PLC 同步系统状态
    void pollPLCStatus();               // PLC 状态轮询
    void updatePumpStatus();            // 更新泵状态
    void updateValveStatus();           // 更新阀门状态
    void updateWaterValveStatus();      // 更新水电磁阀和气主阀状态
    void updateSensorReadings();        // 更新传感器读数
    void checkValveTimeouts();          // 检查阀门超时
    void checkAlarmConditions();        // 检查报警条件
    
    // ----- 模拟模式 (sim_mode_=true) -----
    void runSimulation();               // 运行模拟逻辑（替代PLC读取）
    void simulateVacuumPhysics();       // 模拟真空度变化
    void simulatePumpBehavior();        // 模拟泵频率/转速变化
    void simulateValveActions();        // 模拟阀门动作延时
    
    // 自动流程使用的控制辅助方法（自动处理模拟模式）
    void ctrlScrewPump(bool power);
    void ctrlRootsPump(bool power);
    void ctrlMolecularPump(int index, bool power);
    void ctrlElectromagneticValve(int index, bool state);
    void ctrlGateValve(int index, bool open);
    void ctrlVentValve(int index, bool state);
    
    // ----- 报警管理 -----
    void raiseAlarm(int code, const std::string& type, 
                    const std::string& desc, const std::string& device);
    void clearAlarm(int code);
    void saveAlarmToFile(const AlarmInfo& alarm);
    void pushAlarmEvent(const AlarmInfo& alarm);
    
    // ----- 条件检查 -----
    bool checkAutoModePrerequisites();
    bool checkManualOperationAllowed(const std::string& device, const std::string& operation);
    std::string getPrerequisiteStatus(const std::string& device, const std::string& operation);
    
    // ----- 阀门动作跟踪 -----
    void startValveAction(const std::string& valve_id, bool target_open);
    void updateValveAction(const std::string& valve_id, bool current_open, bool current_close);
    ValveActionState getValveActionState(const std::string& valve_id);
    
    // ----- 自动流程状态机 -----
    void processAutoVacuumSequence();
    void processAutoStopSequence();
    void processVentSequence();
    
    // ----- 辅助方法 -----
    void logEvent(const std::string& event);
    std::string getCurrentTimestamp();
};

// ============================================================================
// VacuumSystemDeviceClass 类
// ============================================================================

class VacuumSystemDeviceClass : public Tango::DeviceClass {
public:
    static VacuumSystemDeviceClass* instance();
    static VacuumSystemDeviceClass* init(const char* class_name);
    
    void attribute_factory(std::vector<Tango::Attr*>& att_list);
    void command_factory();
    void device_factory(const Tango::DevVarStringArray* devlist_ptr);
    
    ~VacuumSystemDeviceClass() { _instance = nullptr; }
    
protected:
    VacuumSystemDeviceClass(const std::string& class_name);
    
private:
    static VacuumSystemDeviceClass* _instance;
};

} // namespace VacuumSystem

#endif // VACUUM_SYSTEM_DEVICE_H

