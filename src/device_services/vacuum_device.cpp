#include "device_services/vacuum_device.h"
#include "common/system_config.h"
#include "common/plc_communication.h"
#include "device_services/vacuum_plc_mapping.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <memory>
#include <chrono>
#include <thread>

namespace Vacuum {

VacuumDevice::VacuumDevice(Tango::DeviceClass *device_class, std::string &device_name)
    : Common::StandardSystemDevice(device_class, device_name),
      is_locked_(false),
      lock_user_(""),
      connect_string_(""),
      sim_mode_(true),
      plc_ip_(Common::SystemConfig::DEFAULT_PLC_IP),
      plc_port_(102),
      plc_comm_(nullptr),
      plc_update_interval_ms_(2000),
      last_plc_update_(std::chrono::steady_clock::now() - std::chrono::milliseconds(plc_update_interval_ms_)),
      plc_reconnect_interval_ms_(5000),
      last_connect_attempt_(std::chrono::steady_clock::now() - std::chrono::milliseconds(plc_reconnect_interval_ms_)),
      reconnect_in_progress_(false),
      self_check_state_(SelfCheckState::IDLE),
      self_check_status_("初始状态"),
      mode_(OperationMode::AUTO),
      system_state_(SystemState::IDLE),
      // 基于新PLC Tags的状态初始化
      screw_pump_power_(false),
      roots_pump_power_(false),
      molecular_pump1_power_(false),
      molecular_pump2_power_(false),
      molecular_pump3_power_(false),
      screw_pump_speed_(0.0),
      molecular_pump1_speed_(0),
      molecular_pump2_speed_(0),
      molecular_pump3_speed_(0),
      vacuum_gauge1_(0.0),
      vacuum_gauge2_(0.0),
      vacuum_gauge3_(0.0),
      water_pressure_(0.0),
      air_pressure_(0.0),
      molecular_pump_start_stop_select_(0),
      gauge_criterion_(0),
      molecular_pump_criterion_(0),
      auto_state_(false),
      manual_state_(false),
      remote_state_(true),
      // 异常标志初始化
      gate_valve1_fault_(false),
      gate_valve2_fault_(false),
      gate_valve3_fault_(false),
      gate_valve4_fault_(false),
      gate_valve5_fault_(false),
      electromagnetic_valve1_fault_(false),
      electromagnetic_valve2_fault_(false),
      electromagnetic_valve3_fault_(false),
      electromagnetic_valve4_fault_(false),
      vent_valve1_fault_(false),
      vent_valve2_fault_(false),
      phase_sequence_fault_(false),
      screw_pump_water_fault_(false),
      molecular_pump1_water_fault_(false),
      molecular_pump2_water_fault_(false),
      molecular_pump3_water_fault_(false),
      // 阀门状态初始化 - 闸板阀默认关闭
      gate_valve1_open_(false),
      gate_valve1_close_(true),
      gate_valve2_open_(false),
      gate_valve2_close_(true),
      gate_valve3_open_(false),
      gate_valve3_close_(true),
      gate_valve4_open_(false),
      gate_valve4_close_(true),
      gate_valve5_open_(false),
      gate_valve5_close_(true),
      // 电磁阀状态初始化 - 默认关闭
      electromagnetic_valve1_open_(false),
      electromagnetic_valve1_close_(true),
      electromagnetic_valve2_open_(false),
      electromagnetic_valve2_close_(true),
      electromagnetic_valve3_open_(false),
      electromagnetic_valve3_close_(true),
      electromagnetic_valve4_open_(false),
      electromagnetic_valve4_close_(true),
      // 放气阀状态初始化 - 默认关闭
      vent_valve1_open_(false),
      vent_valve1_close_(true),
      vent_valve2_open_(false),
      vent_valve2_close_(true),
      // 属性缓存变量初始化
      attr_operation_mode_(0),
      attr_system_state_(0),
      attr_sim_mode_(0),
      attr_auto_state_(0),
      attr_manual_state_(0),
      attr_remote_state_(0),
      attr_screw_pump_power_(0),
      attr_roots_pump_power_(0),
      attr_molecular_pump1_power_(0),
      attr_molecular_pump2_power_(0),
      attr_molecular_pump3_power_(0),
      attr_gate_valve1_fault_(0),
      attr_gate_valve2_fault_(0),
      attr_gate_valve3_fault_(0),
      attr_gate_valve4_fault_(0),
      attr_gate_valve5_fault_(0),
      attr_electromagnetic_valve1_fault_(0),
      attr_electromagnetic_valve2_fault_(0),
      attr_electromagnetic_valve3_fault_(0),
      attr_electromagnetic_valve4_fault_(0),
      attr_vent_valve1_fault_(0),
      attr_vent_valve2_fault_(0),
      attr_phase_sequence_fault_(0),
      attr_screw_pump_water_fault_(0),
      attr_molecular_pump1_water_fault_(0),
      attr_molecular_pump2_water_fault_(0),
      attr_molecular_pump3_water_fault_(0),
      attr_gate_valve1_open_(0),
      attr_gate_valve1_close_(1),  // 默认关闭
      attr_gate_valve2_open_(0),
      attr_gate_valve2_close_(1),
      attr_gate_valve3_open_(0),
      attr_gate_valve3_close_(1),
      attr_gate_valve4_open_(0),
      attr_gate_valve4_close_(1),
      attr_gate_valve5_open_(0),
      attr_gate_valve5_close_(1),
      // 电磁阀属性缓存
      attr_electromagnetic_valve1_open_(0),
      attr_electromagnetic_valve1_close_(1),
      attr_electromagnetic_valve2_open_(0),
      attr_electromagnetic_valve2_close_(1),
      attr_electromagnetic_valve3_open_(0),
      attr_electromagnetic_valve3_close_(1),
      attr_electromagnetic_valve4_open_(0),
      attr_electromagnetic_valve4_close_(1),
      // 放气阀属性缓存
      attr_vent_valve1_open_(0),
      attr_vent_valve1_close_(1),
      attr_vent_valve2_open_(0),
      attr_vent_valve2_close_(1),
      // Runtime state
      result_value_(0) {
        init_device();
}

VacuumDevice::~VacuumDevice() {
    disconnectPLC();
    delete_device();
}

void VacuumDevice::init_device() {
    Common::StandardSystemDevice::init_device();
    
    // Get device properties
    Tango::DbData db_data;
    db_data.push_back(Tango::DbDatum("plc_ip"));
    db_data.push_back(Tango::DbDatum("connect_string"));
    // 固有状态属性
    db_data.push_back(Tango::DbDatum("bundle_no"));
    db_data.push_back(Tango::DbDatum("laser_no"));
    db_data.push_back(Tango::DbDatum("system_no"));
    db_data.push_back(Tango::DbDatum("sub_dev_list"));
    db_data.push_back(Tango::DbDatum("model_list"));
    db_data.push_back(Tango::DbDatum("current_model"));
    db_data.push_back(Tango::DbDatum("error_dict"));
    db_data.push_back(Tango::DbDatum("device_position"));
    db_data.push_back(Tango::DbDatum("device_product_date"));
    db_data.push_back(Tango::DbDatum("device_install_date"));
    db_data.push_back(Tango::DbDatum("simulator_mode"));
    get_db_device()->get_property(db_data);
    
    if (!db_data[0].is_empty()) {
        db_data[0] >> plc_ip_;
    } else {
        // 使用默认IP
        plc_ip_ = "192.168.0.100";
    }
    if (!db_data[1].is_empty()) {
        db_data[1] >> connect_string_;
    }
    // 读取固有状态属性
    if (!db_data[2].is_empty()) {
        db_data[2] >> bundle_no_;
    }
    if (!db_data[3].is_empty()) {
        db_data[3] >> laser_no_;
    }
    if (!db_data[4].is_empty()) {
        db_data[4] >> system_no_;
    }
    if (!db_data[5].is_empty()) {
        db_data[5] >> sub_dev_list_;
    }
    if (!db_data[6].is_empty()) {
        db_data[6] >> model_list_;
    }
    if (!db_data[7].is_empty()) {
        db_data[7] >> current_model_;
    }
    if (!db_data[8].is_empty()) {
        db_data[8] >> error_dict_;
    }
    if (!db_data[9].is_empty()) {
        db_data[9] >> device_position_;
    }
    if (!db_data[10].is_empty()) {
        db_data[10] >> device_product_date_;
    }
    if (!db_data[11].is_empty()) {
        db_data[11] >> device_install_date_;
    }
    // 从系统配置读取模拟模式设置（启动时的初始默认值）
    // 注意：运行时可以通过 simSwitch 命令或GUI切换，但重启后会恢复配置文件的值（不持久化）
    // 如果数据库中有 simulator_mode 属性，优先使用（向后兼容），否则使用配置文件
    if (!db_data[12].is_empty()) {
        short sim_val = 0;
        db_data[12] >> sim_val;
        sim_mode_ = (sim_val != 0);
        INFO_STREAM << "[DEBUG] 从数据库属性读取 simulator_mode=" << sim_mode_ << std::endl;
    } else {
        sim_mode_ = Common::SystemConfig::SIM_MODE;
        if (sim_mode_) {
            INFO_STREAM << "========================================" << std::endl;
            INFO_STREAM << "  模拟模式已启用 (从配置文件加载, SIM_MODE=true)" << std::endl;
            INFO_STREAM << "  不连接真实硬件，使用内部模拟逻辑" << std::endl;
            INFO_STREAM << "========================================" << std::endl;
        } else {
            INFO_STREAM << "[DEBUG] 运行模式: 真实模式 (从配置文件加载, SIM_MODE=false)" << std::endl;
            INFO_STREAM << "[DEBUG] 提示: 可通过 simSwitch 命令或GUI动态切换模拟模式" << std::endl;
        }
    }
    
    // 初始化PLC通信（必须在读取 sim_mode 之后）
    INFO_STREAM << "Initializing PLC communication, sim_mode=" << sim_mode_ << std::endl;
    if (sim_mode_) {
        plc_comm_ = std::make_unique<Common::PLC::MockPLCCommunication>();
    } else {
        plc_comm_ = std::make_unique<Common::PLC::S7Communication>();
    }
    
    // 连接PLC
    if (!plc_ip_.empty()) {
        connectPLC();
    }
    
    log_event("真空设备已初始化。PLC IP: " + plc_ip_ + ", 仿真模式: " + (sim_mode_ ? "是" : "否"));
    INFO_STREAM << "VacuumDevice initialized. PLC IP: " << plc_ip_ << std::endl;
}

void VacuumDevice::delete_device() {
    Common::StandardSystemDevice::delete_device();
}

// ===== LOCK/UNLOCK COMMANDS =====
void VacuumDevice::devLock(Tango::DevString user_info) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (is_locked_ && lock_user_ != std::string(user_info)) {
        Tango::Except::throw_exception("DEVICE_LOCKED",
            "Device locked by " + lock_user_, "VacuumDevice::devLock");
    }
    is_locked_ = true;
    lock_user_ = std::string(user_info);
    log_event("设备已被锁定，用户: " + lock_user_);
    result_value_ = 0;
}

void VacuumDevice::devUnlock(Tango::DevBoolean unlock_all) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (unlock_all || is_locked_) {
        log_event("设备已解锁（原用户: " + lock_user_ + "）");
        is_locked_ = false;
        lock_user_ = "";
    }
    result_value_ = 0;
}

void VacuumDevice::devLockVerify() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    result_value_ = is_locked_ ? 1 : 0;
}

Tango::DevString VacuumDevice::devLockQuery() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string result = is_locked_ ? lock_user_ : "UNLOCKED";
    return CORBA::string_dup(result.c_str());
}

void VacuumDevice::devUserConfig(Tango::DevString config) {
    std::string cfg(config);
    log_event("用户配置已应用: " + cfg);
    result_value_ = 0;
}

// ===== SYSTEM COMMANDS =====
void VacuumDevice::selfCheck() {
    log_event("自检开始");
    self_check_state_ = SelfCheckState::CHECKING;
    self_check_status_ = "自检进行中...";

    if (sim_mode_) {
        self_check_state_ = SelfCheckState::SUCCEED;
        self_check_status_ = "自检通过 - 模拟模式";
        result_value_ = 0;
        log_event("自检完成: " + self_check_status_);
        return;
    }
    
    // 实际硬件检查
    bool plc_ok = true;      // TODO: 检查PLC连接
    bool sensor_ok = true;   // TODO: 检查压力传感器
    bool pump_ok = true;     // TODO: 检查泵状态
    bool valve_ok = true;    // TODO: 检查阀门状态
    
    if (plc_ok && sensor_ok && pump_ok && valve_ok) {
        self_check_state_ = SelfCheckState::SUCCEED;
        self_check_status_ = "自检通过";
    } else if (plc_ok && sensor_ok) {
        // 有报警但可用
        self_check_state_ = SelfCheckState::WARNING;
        std::string warnings;
        if (!pump_ok) warnings += "泵状态异常; ";
        if (!valve_ok) warnings += "阀门状态异常; ";
        self_check_status_ = "自检通过(有报警): " + warnings;
    } else {
        self_check_state_ = SelfCheckState::FAILED;
        std::string errors;
        if (!plc_ok) errors += "PLC连接失败; ";
        if (!sensor_ok) errors += "传感器异常; ";
        self_check_status_ = "自检不通过: " + errors;
    }
    
    result_value_ = (self_check_state_ == SelfCheckState::SUCCEED || 
                     self_check_state_ == SelfCheckState::WARNING) ? 0 : 1;
    log_event("自检完成: " + self_check_status_);
}


void VacuumDevice::init() {
    log_event("初始化开始");
    self_check_state_ = SelfCheckState::IDLE;
    self_check_status_ = "初始状态";
    mode_ = OperationMode::AUTO;
    system_state_ = SystemState::IDLE;
    set_state(Tango::ON);
    result_value_ = 0;
    log_event("初始化完成");
}

void VacuumDevice::reset() {
    log_event("复位开始");
    Common::StandardSystemDevice::reset();
    self_check_state_ = SelfCheckState::IDLE;
    self_check_status_ = "复位后初始状态";
    // 复位系统状态
    system_state_ = SystemState::IDLE;
    set_state(Tango::ON);
    result_value_ = 0;
    log_event("复位完成");
}

// ===== 基于新PLC Tags的命令实现 =====
// 模式切换
void VacuumDevice::switchMode(Tango::DevShort mode) {
    ensure_unlocked("VacuumDevice::switchMode");
    
    if (mode == 0) {
        mode_ = OperationMode::AUTO;
        writePLCBool(PLC::VacuumPLCMapping::ManualAutoButton(), false);  // false = 自动
        auto_state_ = true;
        manual_state_ = false;
        log_event("Switched to AUTO mode");
    } else {
        mode_ = OperationMode::MANUAL;
        writePLCBool(PLC::VacuumPLCMapping::ManualAutoButton(), true);   // true = 手动
        auto_state_ = false;
        manual_state_ = true;
        log_event("Switched to MANUAL mode");
    }
    result_value_ = 0;
}

// 本地/远程切换
void VacuumDevice::setRemoteControl(Tango::DevBoolean remote) {
    ensure_unlocked("VacuumDevice::setRemoteControl");

    const bool target_remote = remote == true;
    const bool plc_connected = plc_comm_ && plc_comm_->isConnected();
    bool ok = false;
    if (plc_connected) {
        ok = writePLCBool(PLC::VacuumPLCMapping::LocalRemoteButton(), target_remote);
    }

    if (ok) {
        remote_state_ = target_remote;
        log_event(std::string("Switched to ") + (target_remote ? "REMOTE" : "LOCAL") + " mode");
        result_value_ = 0;
    } else {
        result_value_ = 1;
    }
    log_command_result("setRemoteControl", target_remote ? "remote" : "local", ok, plc_connected);
}

// 一键操作
void VacuumDevice::oneKeyVacuumStart() {
    ensure_unlocked("VacuumDevice::oneKeyVacuumStart");
    ensure_permission(1, "VacuumDevice::oneKeyVacuumStart");
    
    if (mode_ != OperationMode::AUTO) {
        Tango::Except::throw_exception("INVALID_MODE",
            "One-key vacuum start only available in AUTO mode", 
            "VacuumDevice::oneKeyVacuumStart");
    }
    
    writePLCBool(PLC::VacuumPLCMapping::OneKeyVacuumStart(), true);
    log_event("One-key vacuum start triggered");
    result_value_ = 0;
}

void VacuumDevice::oneKeyVacuumStop() {
    ensure_unlocked("VacuumDevice::oneKeyVacuumStop");
    ensure_permission(1, "VacuumDevice::oneKeyVacuumStop", false);
    
    writePLCBool(PLC::VacuumPLCMapping::OneKeyVacuumStop(), true);
    log_event("One-key vacuum stop triggered");
    result_value_ = 0;
}

void VacuumDevice::ventStart() {
    ensure_unlocked("VacuumDevice::ventStart");
    ensure_permission(2, "VacuumDevice::ventStart");
    
    writePLCBool(PLC::VacuumPLCMapping::VentStart(), true);
    log_event("Vent start triggered");
    result_value_ = 0;
}

void VacuumDevice::ventStop() {
    ensure_unlocked("VacuumDevice::ventStop");
    ensure_permission(2, "VacuumDevice::ventStop", false);
    
    writePLCBool(PLC::VacuumPLCMapping::VentStop(), true);
    log_event("Vent stop triggered");
    result_value_ = 0;
}

// 泵控制
void VacuumDevice::setScrewPumpPower(Tango::DevBoolean state) {
    ensure_unlocked("VacuumDevice::setScrewPumpPower");
    auto addr = PLC::VacuumPLCMapping::ScrewPumpPowerOutput();
    INFO_STREAM << "===[CMD] setScrewPumpPower: state=" << (state ? "ON" : "OFF") 
                << " -> PLC地址: " << addr.address_string << std::endl;
    bool plc_connected = plc_comm_ && plc_comm_->isConnected();
    bool ok = writePLCBool(addr, state != 0);
    log_event("Screw pump power: " + std::string(state ? "ON" : "OFF"));
    result_value_ = ok ? 0 : 1;
    log_command_result("setScrewPumpPower", state ? "1" : "0", ok, plc_connected);
}

void VacuumDevice::setScrewPumpStartStop(Tango::DevBoolean state) {
    ensure_unlocked("VacuumDevice::setScrewPumpStartStop");
    auto addr = PLC::VacuumPLCMapping::ScrewPumpStartStop();
    INFO_STREAM << "===[CMD] setScrewPumpStartStop: state=" << (state ? "START" : "STOP") 
                << " -> PLC地址: " << addr.address_string << std::endl;
    bool plc_connected = plc_comm_ && plc_comm_->isConnected();
    bool ok = writePLCBool(addr, state != 0);
    log_event("Screw pump start/stop: " + std::string(state ? "START" : "STOP"));
    result_value_ = ok ? 0 : 1;
    log_command_result("setScrewPumpStartStop", state ? "1" : "0", ok, plc_connected);
}

void VacuumDevice::setRootsPumpPower(Tango::DevBoolean state) {
    ensure_unlocked("VacuumDevice::setRootsPumpPower");
    auto addr = PLC::VacuumPLCMapping::RootsPumpPowerOutput();
    INFO_STREAM << "===[CMD] setRootsPumpPower: state=" << (state ? "ON" : "OFF") 
                << " -> PLC地址: " << addr.address_string << std::endl;
    bool plc_connected = plc_comm_ && plc_comm_->isConnected();
    bool ok = writePLCBool(addr, state != 0);
    log_event("Roots pump power: " + std::string(state ? "ON" : "OFF"));
    result_value_ = ok ? 0 : 1;
    log_command_result("setRootsPumpPower", state ? "1" : "0", ok, plc_connected);
}

void VacuumDevice::setMolecularPumpPower(const Tango::DevVarShortArray *argin) {
    ensure_unlocked("VacuumDevice::setMolecularPumpPower");
    if (argin->length() < 2) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "需要两个参数: [索引, 状态]", "VacuumDevice::setMolecularPumpPower");
    }
    Tango::DevShort index = (*argin)[0];
    Tango::DevShort state = (*argin)[1];
    
    if (index < 1 || index > 3) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "分子泵索引超出范围(1-3)", "VacuumDevice::setMolecularPumpPower");
    }
    
    // 根据索引选择对应的PLC地址
    Common::PLC::PLCAddress addr = PLC::VacuumPLCMapping::MolecularPump1PowerOutput();
    switch (index) {
        case 1: addr = PLC::VacuumPLCMapping::MolecularPump1PowerOutput(); break;
        case 2: addr = PLC::VacuumPLCMapping::MolecularPump2PowerOutput(); break;
        case 3: addr = PLC::VacuumPLCMapping::MolecularPump3PowerOutput(); break;
        default: break;
    }
    
    INFO_STREAM << "===[CMD] setMolecularPumpPower: index=" << index << ", state=" << (state ? "ON" : "OFF") 
                << " -> PLC地址: " << addr.address_string << std::endl;
    bool plc_connected = plc_comm_ && plc_comm_->isConnected();
    bool ok = writePLCBool(addr, state != 0);
    log_event("分子泵" + std::to_string(index) + "电源: " + (state ? "开" : "关"));
    result_value_ = ok ? 0 : 1;
    log_command_result("setMolecularPumpPower", std::to_string(index) + "," + std::to_string(state), ok, plc_connected);
}

void VacuumDevice::setMolecularPumpStartStop(const Tango::DevVarShortArray *argin) {
    ensure_unlocked("VacuumDevice::setMolecularPumpStartStop");
    if (argin->length() < 2) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "需要两个参数: [索引, 状态]", "VacuumDevice::setMolecularPumpStartStop");
    }
    Tango::DevShort index = (*argin)[0];
    Tango::DevShort state = (*argin)[1];
    
    if (index < 1 || index > 3) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "分子泵索引超出范围(1-3)", "VacuumDevice::setMolecularPumpStartStop");
    }
    
    // 根据索引选择对应的PLC地址
    Common::PLC::PLCAddress addr = PLC::VacuumPLCMapping::MolecularPump1StartStop();
    switch (index) {
        case 1: addr = PLC::VacuumPLCMapping::MolecularPump1StartStop(); break;
        case 2: addr = PLC::VacuumPLCMapping::MolecularPump2StartStop(); break;
        case 3: addr = PLC::VacuumPLCMapping::MolecularPump3StartStop(); break;
        default: break;
    }
    
    INFO_STREAM << "===[CMD] setMolecularPumpStartStop: index=" << index << ", state=" << (state ? "START" : "STOP") 
                << " -> PLC地址: " << addr.address_string << std::endl;
    bool plc_connected = plc_comm_ && plc_comm_->isConnected();
    bool ok = writePLCBool(addr, state != 0);
    log_event("分子泵" + std::to_string(index) + "启停: " + (state ? "启动" : "停止"));
    result_value_ = ok ? 0 : 1;
    log_command_result("setMolecularPumpStartStop", std::to_string(index) + "," + std::to_string(state), ok, plc_connected);
}

void VacuumDevice::setScrewPumpSpeed(Tango::DevDouble speed) {
    ensure_unlocked("VacuumDevice::setScrewPumpSpeed");
    if (speed < 0.0 || speed > 100.0) {
        Tango::Except::throw_exception("API_InvalidValue",
            "螺杆泵转速超出范围(0-100%)", "VacuumDevice::setScrewPumpSpeed");
    }
    
    // 将百分比转换为PLC的Word值
    uint16_t plc_value = static_cast<uint16_t>(speed);
    auto addr = PLC::VacuumPLCMapping::ScrewPumpSpeedOutput();
    INFO_STREAM << "===[CMD] setScrewPumpSpeed: speed=" << speed << "% -> PLC value=" << plc_value 
                << " -> PLC地址: " << addr.address_string << std::endl;
    writePLCWord(addr, plc_value);
    log_event("螺杆泵转速设置: " + std::to_string(speed) + "%");
    result_value_ = 0;
}

void VacuumDevice::resetScrewPumpFault() {
    ensure_unlocked("VacuumDevice::resetScrewPumpFault");
    auto addr = PLC::VacuumPLCMapping::ScrewPumpFaultReset();
    INFO_STREAM << "===[CMD] resetScrewPumpFault -> PLC地址: " << addr.address_string << std::endl;
    writePLCBool(addr, true);
    log_event("螺杆泵故障复位");
    result_value_ = 0;
}

// 阀门控制
void VacuumDevice::setGateValve(const Tango::DevVarShortArray *argin) {
    ensure_unlocked("VacuumDevice::setGateValve");
    if (argin->length() < 2) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "需要两个参数: [索引, 操作]", "VacuumDevice::setGateValve");
    }
    Tango::DevShort index = (*argin)[0];
    Tango::DevShort operation = (*argin)[1];
    
    INFO_STREAM << "===[CMD] setGateValve: index=" << index << ", operation=" << (operation == 1 ? "OPEN" : "CLOSE") << std::endl;
    
    if (index < 1 || index > 5) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "闸板阀索引超出范围(1-5)", "VacuumDevice::setGateValve");
    }
    
    Common::PLC::PLCAddress addr_open = PLC::VacuumPLCMapping::GateValve1OpenOutput();
    Common::PLC::PLCAddress addr_close = PLC::VacuumPLCMapping::GateValve1CloseOutput();

    switch (index) {
        case 1: 
            addr_open = PLC::VacuumPLCMapping::GateValve1OpenOutput(); 
            addr_close = PLC::VacuumPLCMapping::GateValve1CloseOutput();
            break;
        case 2: 
            addr_open = PLC::VacuumPLCMapping::GateValve2OpenOutput(); 
            addr_close = PLC::VacuumPLCMapping::GateValve2CloseOutput();
            break;
        case 3: 
            addr_open = PLC::VacuumPLCMapping::GateValve3OpenOutput(); 
            addr_close = PLC::VacuumPLCMapping::GateValve3CloseOutput();
            break;
        case 4: 
            addr_open = PLC::VacuumPLCMapping::GateValve4OpenOutput(); 
            addr_close = PLC::VacuumPLCMapping::GateValve4CloseOutput();
            break;
        case 5: 
            addr_open = PLC::VacuumPLCMapping::GateValve5OpenOutput(); 
            addr_close = PLC::VacuumPLCMapping::GateValve5CloseOutput();
            break;
        default: break;
    }
    
    INFO_STREAM << "  -> 闸板阀" << index << " OpenAddr: " << addr_open.address_string
                << ", CloseAddr: " << addr_close.address_string << std::endl;

    bool plc_connected = plc_comm_ && plc_comm_->isConnected();

    bool ok = true;
    if (operation == 1) {
        // Open: Set Open=True, Close=False
        INFO_STREAM << "  -> 执行打开操作: Open=TRUE, Close=FALSE" << std::endl;
        ok = writePLCBool(addr_open, true) && writePLCBool(addr_close, false);
        // 模拟模式下更新内部状态
        if (sim_mode_) {
            switch (index) {
                case 1: gate_valve1_open_ = true; gate_valve1_close_ = false; break;
                case 2: gate_valve2_open_ = true; gate_valve2_close_ = false; break;
                case 3: gate_valve3_open_ = true; gate_valve3_close_ = false; break;
                case 4: gate_valve4_open_ = true; gate_valve4_close_ = false; break;
                case 5: gate_valve5_open_ = true; gate_valve5_close_ = false; break;
            }
            INFO_STREAM << "  -> [SIM_MODE] 内部状态已更新" << std::endl;
        }
        log_event("闸板阀" + std::to_string(index) + "开");
    } else {
        // Close: Set Close=True, Open=False
        INFO_STREAM << "  -> 执行关闭操作: Close=TRUE, Open=FALSE" << std::endl;
        ok = writePLCBool(addr_close, true) && writePLCBool(addr_open, false);
        // 模拟模式下更新内部状态
        if (sim_mode_) {
            switch (index) {
                case 1: gate_valve1_open_ = false; gate_valve1_close_ = true; break;
                case 2: gate_valve2_open_ = false; gate_valve2_close_ = true; break;
                case 3: gate_valve3_open_ = false; gate_valve3_close_ = true; break;
                case 4: gate_valve4_open_ = false; gate_valve4_close_ = true; break;
                case 5: gate_valve5_open_ = false; gate_valve5_close_ = true; break;
            }
            INFO_STREAM << "  -> [SIM_MODE] 内部状态已更新" << std::endl;
        }
        log_event("闸板阀" + std::to_string(index) + "关");
    }
    result_value_ = ok ? 0 : 1;
    log_command_result("setGateValve", std::to_string(index) + "," + std::to_string(operation), ok, plc_connected);
}

void VacuumDevice::setElectromagneticValve(const Tango::DevVarShortArray *argin) {
    ensure_unlocked("VacuumDevice::setElectromagneticValve");
    if (argin->length() < 2) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "需要两个参数: [索引, 状态]", "VacuumDevice::setElectromagneticValve");
    }
    Tango::DevShort index = (*argin)[0];
    Tango::DevShort state = (*argin)[1];
    
    if (index < 1 || index > 4) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "电磁阀索引超出范围(1-4)", "VacuumDevice::setElectromagneticValve");
    }
    
    // 根据索引选择对应的PLC地址
    Common::PLC::PLCAddress addr = PLC::VacuumPLCMapping::ElectromagneticValve1Output();
    switch (index) {
        case 1: addr = PLC::VacuumPLCMapping::ElectromagneticValve1Output(); break;
        case 2: addr = PLC::VacuumPLCMapping::ElectromagneticValve2Output(); break;
        case 3: addr = PLC::VacuumPLCMapping::ElectromagneticValve3Output(); break;
        case 4: addr = PLC::VacuumPLCMapping::ElectromagneticValve4Output(); break;
        default: break;
    }
    
    bool plc_connected = plc_comm_ && plc_comm_->isConnected();
    INFO_STREAM << "===[CMD] setElectromagneticValve: index=" << index << ", state=" << (state ? "ON" : "OFF")
                << " -> PLC地址: " << addr.address_string << std::endl;
    bool ok = writePLCBool(addr, state != 0);
    // 模拟模式下更新内部状态
    if (sim_mode_) {
        switch (index) {
            case 1: 
                electromagnetic_valve1_open_ = (state != 0); 
                electromagnetic_valve1_close_ = (state == 0); 
                break;
            case 2: 
                electromagnetic_valve2_open_ = (state != 0); 
                electromagnetic_valve2_close_ = (state == 0); 
                break;
            case 3: 
                electromagnetic_valve3_open_ = (state != 0); 
                electromagnetic_valve3_close_ = (state == 0); 
                break;
            case 4: 
                electromagnetic_valve4_open_ = (state != 0); 
                electromagnetic_valve4_close_ = (state == 0); 
                break;
        }
        INFO_STREAM << "  -> [SIM_MODE] 内部状态已更新" << std::endl;
    }
    log_event("电磁阀" + std::to_string(index) + ": " + (state ? "开" : "关"));
    result_value_ = ok ? 0 : 1;
    log_command_result("setElectromagneticValve", std::to_string(index) + "," + std::to_string(state), ok, plc_connected);
}

void VacuumDevice::setVentValve(const Tango::DevVarShortArray *argin) {
    ensure_unlocked("VacuumDevice::setVentValve");
    if (argin->length() < 2) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "需要两个参数: [索引, 状态]", "VacuumDevice::setVentValve");
    }
    Tango::DevShort index = (*argin)[0];
    Tango::DevShort state = (*argin)[1];
    
    if (index < 1 || index > 2) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "放气阀索引超出范围(1-2)", "VacuumDevice::setVentValve");
    }
    
    // 根据索引选择对应的PLC地址
    Common::PLC::PLCAddress addr = PLC::VacuumPLCMapping::VentValve1Output();
    switch (index) {
        case 1: addr = PLC::VacuumPLCMapping::VentValve1Output(); break;
        case 2: addr = PLC::VacuumPLCMapping::VentValve2Output(); break;
        default: break;
    }
    
    bool plc_connected = plc_comm_ && plc_comm_->isConnected();
    INFO_STREAM << "===[CMD] setVentValve: index=" << index << ", state=" << (state ? "ON" : "OFF")
                << " -> PLC地址: " << addr.address_string << std::endl;
    bool ok = writePLCBool(addr, state != 0);
    // 模拟模式下更新内部状态
    if (sim_mode_) {
        switch (index) {
            case 1: 
                vent_valve1_open_ = (state != 0); 
                vent_valve1_close_ = (state == 0); 
                break;
            case 2: 
                vent_valve2_open_ = (state != 0); 
                vent_valve2_close_ = (state == 0); 
                break;
        }
        INFO_STREAM << "  -> [SIM_MODE] 内部状态已更新" << std::endl;
    }
    log_event("放气阀" + std::to_string(index) + ": " + (state ? "开" : "关"));
    result_value_ = ok ? 0 : 1;
    log_command_result("setVentValve", std::to_string(index) + "," + std::to_string(state), ok, plc_connected);
}

void VacuumDevice::setWaterElectromagneticValve(const Tango::DevVarShortArray *argin) {
    ensure_unlocked("VacuumDevice::setWaterElectromagneticValve");
    if (argin->length() < 2) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "需要两个参数: [索引, 状态]", "VacuumDevice::setWaterElectromagneticValve");
    }
    Tango::DevShort index = (*argin)[0];
    Tango::DevShort state = (*argin)[1];
    
    if (index < 1 || index > 2) {
        Tango::Except::throw_exception("API_InvalidArgs",
            "水电磁阀索引超出范围(1-2)", "VacuumDevice::setWaterElectromagneticValve");
    }
    
    // 根据索引选择对应的PLC地址
    Common::PLC::PLCAddress addr = PLC::VacuumPLCMapping::WaterElectromagneticValve1Output();
    switch (index) {
        case 1: addr = PLC::VacuumPLCMapping::WaterElectromagneticValve1Output(); break;
        case 2: addr = PLC::VacuumPLCMapping::WaterElectromagneticValve2Output(); break;
        default: break;
    }
    
    writePLCBool(addr, state != 0);
    log_event("水电磁阀" + std::to_string(index) + ": " + (state ? "开" : "关"));
    result_value_ = 0;
}

void VacuumDevice::setAirMainElectromagneticValve(Tango::DevBoolean state) {
    ensure_unlocked("VacuumDevice::setAirMainElectromagneticValve");
    writePLCBool(PLC::VacuumPLCMapping::AirMainElectromagneticValveOutput(), state != 0);
    log_event("气路总电磁阀: " + std::string(state ? "开" : "关"));
    result_value_ = 0;
}

// 参数设置
void VacuumDevice::setMolecularPumpStartStopSelect(Tango::DevShort select) {
    ensure_unlocked("VacuumDevice::setMolecularPumpStartStopSelect");
    if (select < 0 || select > 7) {
        Tango::Except::throw_exception("API_InvalidValue",
            "分子泵启停选择值超出范围(0-7)", "VacuumDevice::setMolecularPumpStartStopSelect");
    }
    writePLCWord(PLC::VacuumPLCMapping::MolecularPumpStartStopSelect(), select);
    log_event("分子泵启停选择: " + std::to_string(select));
    result_value_ = 0;
}

void VacuumDevice::setGaugeCriterion(Tango::DevShort criterion) {
    ensure_unlocked("VacuumDevice::setGaugeCriterion");
    writePLCWord(PLC::VacuumPLCMapping::GaugeCriterion(), criterion);
    log_event("真空规判定: " + std::to_string(criterion));
    result_value_ = 0;
}

void VacuumDevice::setMolecularPumpCriterion(Tango::DevShort criterion) {
    ensure_unlocked("VacuumDevice::setMolecularPumpCriterion");
    writePLCWord(PLC::VacuumPLCMapping::MolecularPumpCriterion(), criterion);
    log_event("分子泵判定: " + std::to_string(criterion));
    result_value_ = 0;
}

// 系统命令
void VacuumDevice::alarmReset() {
    ensure_unlocked("VacuumDevice::alarmReset");
    writePLCBool(PLC::VacuumPLCMapping::AlarmReset(), true);
    log_event("Alarm reset triggered");
    result_value_ = 0;
}

void VacuumDevice::emergencyStop() {
    ensure_unlocked("VacuumDevice::emergencyStop");
    writePLCBool(PLC::VacuumPLCMapping::EmergencyStop(), true);
    log_event("Emergency stop triggered");
    result_value_ = 0;
}

// PLC通信命令已移到文件末尾

// ===== ATTRIBUTE READERS =====
void VacuumDevice::read_attr(Tango::Attribute &attr) {
    std::string attr_name = attr.get_name();
    
    // ===== 固有状态属性 =====
    if (attr_name == "bundleNo") {
        read_bundle_no(attr);
    } else if (attr_name == "laserNo") {
        read_laser_no(attr);
    } else if (attr_name == "systemNo") {
        read_system_no(attr);
    } else if (attr_name == "subDevList") {
        read_sub_dev_list(attr);
    } else if (attr_name == "modelList") {
        read_model_list(attr);
    } else if (attr_name == "currentModel") {
        read_current_model(attr);
    } else if (attr_name == "connectString") {
        read_connect_string(attr);
    } else if (attr_name == "errorDict") {
        read_error_dict(attr);
    } else if (attr_name == "devicePosition") {
        read_device_position(attr);
    } else if (attr_name == "deviceProductDate") {
        read_device_product_date(attr);
    } else if (attr_name == "deviceInstallDate") {
        read_device_install_date(attr);
    } else if (attr_name == "simulatorMode") {
        read_simulator_mode(attr);
    }
    // ===== 自检状态属性 =====
    else if (attr_name == "selfCheckState") {
        read_self_check_state(attr);
    } else if (attr_name == "selfCheckStatus") {
        read_self_check_status(attr);
    } else if (attr_name == "mode") {
        read_mode(attr);
    } else if (attr_name == "resultValue") {
        read_result_value(attr);
    } else if (attr_name == "groupAttributeJson") {
        read_group_attribute_json(attr);
    }
    // ===== 基于新PLC Tags的属性读取 =====
    else if (attr_name == "operationMode") {
        read_operation_mode(attr);
    } else if (attr_name == "systemState") {
        read_system_state(attr);
    } else if (attr_name == "autoState") {
        read_auto_state(attr);
    } else if (attr_name == "manualState") {
        read_manual_state(attr);
    } else if (attr_name == "remoteState") {
        read_remote_state(attr);
    }
    // 传感器数据属性
    else if (attr_name == "vacuumGauge1") {
        read_vacuum_gauge1(attr);
    } else if (attr_name == "vacuumGauge2") {
        read_vacuum_gauge2(attr);
    } else if (attr_name == "vacuumGauge3") {
        read_vacuum_gauge3(attr);
    } else if (attr_name == "waterPressure") {
        read_water_pressure(attr);
    } else if (attr_name == "airPressure") {
        read_air_pressure(attr);
    }
    // 泵状态属性
    else if (attr_name == "screwPumpPower") {
        read_screw_pump_power(attr);
    } else if (attr_name == "rootsPumpPower") {
        read_roots_pump_power(attr);
    } else if (attr_name == "molecularPump1Power") {
        read_molecular_pump1_power(attr);
    } else if (attr_name == "molecularPump2Power") {
        read_molecular_pump2_power(attr);
    } else if (attr_name == "molecularPump3Power") {
        read_molecular_pump3_power(attr);
    } else if (attr_name == "screwPumpSpeed") {
        read_screw_pump_speed(attr);
    } else if (attr_name == "molecularPump1Speed") {
        read_molecular_pump1_speed(attr);
    } else if (attr_name == "molecularPump2Speed") {
        read_molecular_pump2_speed(attr);
    } else if (attr_name == "molecularPump3Speed") {
        read_molecular_pump3_speed(attr);
    }
    // 参数设置属性
    else if (attr_name == "molecularPumpStartStopSelect") {
        read_molecular_pump_start_stop_select(attr);
    } else if (attr_name == "gaugeCriterion") {
        read_gauge_criterion(attr);
    } else if (attr_name == "molecularPumpCriterion") {
        read_molecular_pump_criterion(attr);
    }
    // 异常状态属性
    else if (attr_name == "gateValve1Fault") {
        read_gate_valve1_fault(attr);
    } else if (attr_name == "gateValve2Fault") {
        read_gate_valve2_fault(attr);
    } else if (attr_name == "gateValve3Fault") {
        read_gate_valve3_fault(attr);
    } else if (attr_name == "gateValve4Fault") {
        read_gate_valve4_fault(attr);
    } else if (attr_name == "gateValve5Fault") {
        read_gate_valve5_fault(attr);
    } else if (attr_name == "electromagneticValve1Fault") {
        read_electromagnetic_valve1_fault(attr);
    } else if (attr_name == "electromagneticValve2Fault") {
        read_electromagnetic_valve2_fault(attr);
    } else if (attr_name == "electromagneticValve3Fault") {
        read_electromagnetic_valve3_fault(attr);
    } else if (attr_name == "electromagneticValve4Fault") {
        read_electromagnetic_valve4_fault(attr);
    } else if (attr_name == "ventValve1Fault") {
        read_vent_valve1_fault(attr);
    } else if (attr_name == "ventValve2Fault") {
        read_vent_valve2_fault(attr);
    } else if (attr_name == "phaseSequenceFault") {
        read_phase_sequence_fault(attr);
    } else if (attr_name == "screwPumpWaterFault") {
        read_screw_pump_water_fault(attr);
    } else if (attr_name == "molecularPump1WaterFault") {
        read_molecular_pump1_water_fault(attr);
    } else if (attr_name == "molecularPump2WaterFault") {
        read_molecular_pump2_water_fault(attr);
    } else if (attr_name == "molecularPump3WaterFault") {
        read_molecular_pump3_water_fault(attr);
    }
    // 阀门状态属性
    else if (attr_name == "gateValve1Open") {
        read_gate_valve1_open(attr);
    } else if (attr_name == "gateValve1Close") {
        read_gate_valve1_close(attr);
    } else if (attr_name == "gateValve2Open") {
        read_gate_valve2_open(attr);
    } else if (attr_name == "gateValve2Close") {
        read_gate_valve2_close(attr);
    } else if (attr_name == "gateValve3Open") {
        read_gate_valve3_open(attr);
    } else if (attr_name == "gateValve3Close") {
        read_gate_valve3_close(attr);
    } else if (attr_name == "gateValve4Open") {
        read_gate_valve4_open(attr);
    } else if (attr_name == "gateValve4Close") {
        read_gate_valve4_close(attr);
    } else if (attr_name == "gateValve5Open") {
        read_gate_valve5_open(attr);
    } else if (attr_name == "gateValve5Close") {
        read_gate_valve5_close(attr);
    } else if (attr_name == "electromagneticValve1Open") {
        read_electromagnetic_valve1_open(attr);
    } else if (attr_name == "electromagneticValve1Close") {
        read_electromagnetic_valve1_close(attr);
    } else if (attr_name == "electromagneticValve2Open") {
        read_electromagnetic_valve2_open(attr);
    } else if (attr_name == "electromagneticValve2Close") {
        read_electromagnetic_valve2_close(attr);
    } else if (attr_name == "electromagneticValve3Open") {
        read_electromagnetic_valve3_open(attr);
    } else if (attr_name == "electromagneticValve3Close") {
        read_electromagnetic_valve3_close(attr);
    } else if (attr_name == "electromagneticValve4Open") {
        read_electromagnetic_valve4_open(attr);
    } else if (attr_name == "electromagneticValve4Close") {
        read_electromagnetic_valve4_close(attr);
    } else if (attr_name == "ventValve1Open") {
        read_vent_valve1_open(attr);
    } else if (attr_name == "ventValve1Close") {
        read_vent_valve1_close(attr);
    } else if (attr_name == "ventValve2Open") {
        read_vent_valve2_open(attr);
    } else if (attr_name == "ventValve2Close") {
        read_vent_valve2_close(attr);
    }
}

// ===== 可写属性写入函数 =====
void VacuumDevice::write_attr(Tango::WAttribute &attr) {
    std::string attr_name = attr.get_name();
    
    if (attr_name == "molecularPumpStartStopSelect") {
        Tango::DevShort value;
        attr.get_write_value(value);
        molecular_pump_start_stop_select_ = value;
        // 写入 PLC
        if (!sim_mode_ && plc_comm_ && plc_comm_->isConnected()) {
            writePLCInt(PLC::VacuumPLCMapping::MolecularPumpStartStopSelect(), value);
        }
    } else if (attr_name == "gaugeCriterion") {
        Tango::DevShort value;
        attr.get_write_value(value);
        gauge_criterion_ = value;
        if (!sim_mode_ && plc_comm_ && plc_comm_->isConnected()) {
            writePLCInt(PLC::VacuumPLCMapping::GaugeCriterion(), value);
        }
    } else if (attr_name == "molecularPumpCriterion") {
        Tango::DevShort value;
        attr.get_write_value(value);
        molecular_pump_criterion_ = value;
        if (!sim_mode_ && plc_comm_ && plc_comm_->isConnected()) {
            writePLCInt(PLC::VacuumPLCMapping::MolecularPumpCriterion(), value);
        }
    }
}

// ===== 固有状态属性读取函数 =====
void VacuumDevice::read_bundle_no(Tango::Attribute &attr) {
    attr_bundle_no_read = Tango::string_dup(bundle_no_.c_str());
    attr.set_value(&attr_bundle_no_read);
}

void VacuumDevice::read_laser_no(Tango::Attribute &attr) {
    attr_laser_no_read = Tango::string_dup(laser_no_.c_str());
    attr.set_value(&attr_laser_no_read);
}

void VacuumDevice::read_system_no(Tango::Attribute &attr) {
    attr_system_no_read = Tango::string_dup(system_no_.c_str());
    attr.set_value(&attr_system_no_read);
}

void VacuumDevice::read_sub_dev_list(Tango::Attribute &attr) {
    attr_sub_dev_list_read = Tango::string_dup(sub_dev_list_.c_str());
    attr.set_value(&attr_sub_dev_list_read);
}

void VacuumDevice::read_model_list(Tango::Attribute &attr) {
    static std::vector<Tango::DevString> str_ptrs;
    // 清理之前的指针
    for (auto ptr : str_ptrs) {
        if (ptr) Tango::string_free(ptr);
    }
    str_ptrs.clear();
    str_ptrs.reserve(model_list_.size());
    for (const auto& s : model_list_) {
        str_ptrs.push_back(Tango::string_dup(s.c_str()));
    }
    if (!str_ptrs.empty()) {
        attr.set_value(str_ptrs.data(), str_ptrs.size());
    } else {
        static Tango::DevString empty_arr[1] = { const_cast<char*>("") };
        attr.set_value(empty_arr, 0);
    }
}

void VacuumDevice::read_current_model(Tango::Attribute &attr) {
    attr_current_model_read = Tango::string_dup(current_model_.c_str());
    attr.set_value(&attr_current_model_read);
}

void VacuumDevice::read_connect_string(Tango::Attribute &attr) {
    attr_connect_string_read = Tango::string_dup(connect_string_.c_str());
    attr.set_value(&attr_connect_string_read);
}

void VacuumDevice::read_error_dict(Tango::Attribute &attr) {
    attr_error_dict_read = Tango::string_dup(error_dict_.c_str());
    attr.set_value(&attr_error_dict_read);
}

void VacuumDevice::read_device_position(Tango::Attribute &attr) {
    attr_device_position_read = Tango::string_dup(device_position_.c_str());
    attr.set_value(&attr_device_position_read);
}

void VacuumDevice::read_device_product_date(Tango::Attribute &attr) {
    attr_device_product_date_read = Tango::string_dup(device_product_date_.c_str());
    attr.set_value(&attr_device_product_date_read);
}

void VacuumDevice::read_device_install_date(Tango::Attribute &attr) {
    attr_device_install_date_read = Tango::string_dup(device_install_date_.c_str());
    attr.set_value(&attr_device_install_date_read);
}

void VacuumDevice::read_simulator_mode(Tango::Attribute &attr) {
    attr_sim_mode_ = sim_mode_ ? 1 : 0;
    attr.set_value(&attr_sim_mode_);
}

// ===== 自检状态属性读取函数 =====
void VacuumDevice::read_self_check_state(Tango::Attribute &attr) {
    attr_self_check_state_read = static_cast<Tango::DevLong>(self_check_state_);
    attr.set_value(&attr_self_check_state_read);
}

void VacuumDevice::read_self_check_status(Tango::Attribute &attr) {
    attr_self_check_status_read = Tango::string_dup(self_check_status_.c_str());
    attr.set_value(&attr_self_check_status_read);
}

void VacuumDevice::read_mode(Tango::Attribute &attr) {
    attr_operation_mode_ = static_cast<Tango::DevShort>(mode_);
    attr.set_value(&attr_operation_mode_);
}

void VacuumDevice::read_result_value(Tango::Attribute &attr) {
    attr.set_value(&result_value_);
}

void VacuumDevice::read_group_attribute_json(Tango::Attribute &attr) {
    std::ostringstream oss;
    oss << "{"
        << "\"selfCheckState\":" << static_cast<int>(self_check_state_) << ","
        << "\"selfCheckStatus\":\"" << self_check_status_ << "\","
        << "\"mode\":" << static_cast<int>(mode_) << ","
        << "\"simulatorMode\":" << (sim_mode_ ? "true" : "false") << ","
        << "\"operationMode\":" << static_cast<int>(mode_) << ","
        << "\"systemState\":" << static_cast<int>(system_state_) << ","
        << "\"autoState\":" << (auto_state_ ? "true" : "false") << ","
        << "\"manualState\":" << (manual_state_ ? "true" : "false") << ","
        << "\"remoteState\":" << (remote_state_ ? "true" : "false") << ","
        << "\"vacuumGauge1\":" << vacuum_gauge1_ << ","
        << "\"vacuumGauge2\":" << vacuum_gauge2_ << ","
        << "\"vacuumGauge3\":" << vacuum_gauge3_ << ","
        << "\"screwPumpPower\":" << (screw_pump_power_ ? "true" : "false") << ","
        << "\"rootsPumpPower\":" << (roots_pump_power_ ? "true" : "false")
        << "}";
    attr_group_attribute_json_read = Tango::string_dup(oss.str().c_str());
    attr.set_value(&attr_group_attribute_json_read);
}

// ===== 基于新PLC Tags的属性读取器实现 =====
void VacuumDevice::read_operation_mode(Tango::Attribute &attr) {
    attr_operation_mode_ = static_cast<Tango::DevShort>(mode_);
    attr.set_value(&attr_operation_mode_);
}

void VacuumDevice::read_system_state(Tango::Attribute &attr) {
    attr_system_state_ = static_cast<Tango::DevShort>(system_state_);
    attr.set_value(&attr_system_state_);
}

void VacuumDevice::read_auto_state(Tango::Attribute &attr) {
    attr_auto_state_ = auto_state_ ? 1 : 0;
    attr.set_value(&attr_auto_state_);
}

void VacuumDevice::read_manual_state(Tango::Attribute &attr) {
    attr_manual_state_ = manual_state_ ? 1 : 0;
    attr.set_value(&attr_manual_state_);
}

void VacuumDevice::read_remote_state(Tango::Attribute &attr) {
    attr_remote_state_ = remote_state_ ? 1 : 0;
    attr.set_value(&attr_remote_state_);
}

// 传感器数据属性读取
void VacuumDevice::read_vacuum_gauge1(Tango::Attribute &attr) {
    attr.set_value(&vacuum_gauge1_);
}

void VacuumDevice::read_vacuum_gauge2(Tango::Attribute &attr) {
    attr.set_value(&vacuum_gauge2_);
}

void VacuumDevice::read_vacuum_gauge3(Tango::Attribute &attr) {
    attr.set_value(&vacuum_gauge3_);
}

void VacuumDevice::read_water_pressure(Tango::Attribute &attr) {
    attr.set_value(&water_pressure_);
}

void VacuumDevice::read_air_pressure(Tango::Attribute &attr) {
    attr.set_value(&air_pressure_);
}

// 泵状态属性读取
void VacuumDevice::read_screw_pump_power(Tango::Attribute &attr) {
    attr_screw_pump_power_ = screw_pump_power_ ? 1 : 0;
    attr.set_value(&attr_screw_pump_power_);
}

void VacuumDevice::read_roots_pump_power(Tango::Attribute &attr) {
    attr_roots_pump_power_ = roots_pump_power_ ? 1 : 0;
    attr.set_value(&attr_roots_pump_power_);
}

void VacuumDevice::read_molecular_pump1_power(Tango::Attribute &attr) {
    attr_molecular_pump1_power_ = molecular_pump1_power_ ? 1 : 0;
    attr.set_value(&attr_molecular_pump1_power_);
}

void VacuumDevice::read_molecular_pump2_power(Tango::Attribute &attr) {
    attr_molecular_pump2_power_ = molecular_pump2_power_ ? 1 : 0;
    attr.set_value(&attr_molecular_pump2_power_);
}

void VacuumDevice::read_molecular_pump3_power(Tango::Attribute &attr) {
    attr_molecular_pump3_power_ = molecular_pump3_power_ ? 1 : 0;
    attr.set_value(&attr_molecular_pump3_power_);
}

void VacuumDevice::read_screw_pump_speed(Tango::Attribute &attr) {
    attr.set_value(&screw_pump_speed_);
}

void VacuumDevice::read_molecular_pump1_speed(Tango::Attribute &attr) {
    attr.set_value(&molecular_pump1_speed_);
}

void VacuumDevice::read_molecular_pump2_speed(Tango::Attribute &attr) {
    attr.set_value(&molecular_pump2_speed_);
}

void VacuumDevice::read_molecular_pump3_speed(Tango::Attribute &attr) {
    attr.set_value(&molecular_pump3_speed_);
}

// 参数设置属性读取
void VacuumDevice::read_molecular_pump_start_stop_select(Tango::Attribute &attr) {
    attr.set_value(&molecular_pump_start_stop_select_);
}

void VacuumDevice::read_gauge_criterion(Tango::Attribute &attr) {
    attr.set_value(&gauge_criterion_);
}

void VacuumDevice::read_molecular_pump_criterion(Tango::Attribute &attr) {
    attr.set_value(&molecular_pump_criterion_);
}

// 异常状态属性读取
void VacuumDevice::read_gate_valve1_fault(Tango::Attribute &attr) {
    attr_gate_valve1_fault_ = gate_valve1_fault_ ? 1 : 0;
    attr.set_value(&attr_gate_valve1_fault_);
}

void VacuumDevice::read_gate_valve2_fault(Tango::Attribute &attr) {
    attr_gate_valve2_fault_ = gate_valve2_fault_ ? 1 : 0;
    attr.set_value(&attr_gate_valve2_fault_);
}

void VacuumDevice::read_gate_valve3_fault(Tango::Attribute &attr) {
    attr_gate_valve3_fault_ = gate_valve3_fault_ ? 1 : 0;
    attr.set_value(&attr_gate_valve3_fault_);
}

void VacuumDevice::read_gate_valve4_fault(Tango::Attribute &attr) {
    attr_gate_valve4_fault_ = gate_valve4_fault_ ? 1 : 0;
    attr.set_value(&attr_gate_valve4_fault_);
}

void VacuumDevice::read_gate_valve5_fault(Tango::Attribute &attr) {
    attr_gate_valve5_fault_ = gate_valve5_fault_ ? 1 : 0;
    attr.set_value(&attr_gate_valve5_fault_);
}

void VacuumDevice::read_electromagnetic_valve1_fault(Tango::Attribute &attr) {
    attr_electromagnetic_valve1_fault_ = electromagnetic_valve1_fault_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve1_fault_);
}

void VacuumDevice::read_electromagnetic_valve2_fault(Tango::Attribute &attr) {
    attr_electromagnetic_valve2_fault_ = electromagnetic_valve2_fault_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve2_fault_);
}

void VacuumDevice::read_electromagnetic_valve3_fault(Tango::Attribute &attr) {
    attr_electromagnetic_valve3_fault_ = electromagnetic_valve3_fault_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve3_fault_);
}

void VacuumDevice::read_electromagnetic_valve4_fault(Tango::Attribute &attr) {
    attr_electromagnetic_valve4_fault_ = electromagnetic_valve4_fault_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve4_fault_);
}

void VacuumDevice::read_vent_valve1_fault(Tango::Attribute &attr) {
    attr_vent_valve1_fault_ = vent_valve1_fault_ ? 1 : 0;
    attr.set_value(&attr_vent_valve1_fault_);
}

void VacuumDevice::read_vent_valve2_fault(Tango::Attribute &attr) {
    attr_vent_valve2_fault_ = vent_valve2_fault_ ? 1 : 0;
    attr.set_value(&attr_vent_valve2_fault_);
}

void VacuumDevice::read_phase_sequence_fault(Tango::Attribute &attr) {
    attr_phase_sequence_fault_ = phase_sequence_fault_ ? 1 : 0;
    attr.set_value(&attr_phase_sequence_fault_);
}

void VacuumDevice::read_screw_pump_water_fault(Tango::Attribute &attr) {
    attr_screw_pump_water_fault_ = screw_pump_water_fault_ ? 1 : 0;
    attr.set_value(&attr_screw_pump_water_fault_);
}

void VacuumDevice::read_molecular_pump1_water_fault(Tango::Attribute &attr) {
    attr_molecular_pump1_water_fault_ = molecular_pump1_water_fault_ ? 1 : 0;
    attr.set_value(&attr_molecular_pump1_water_fault_);
}

void VacuumDevice::read_molecular_pump2_water_fault(Tango::Attribute &attr) {
    attr_molecular_pump2_water_fault_ = molecular_pump2_water_fault_ ? 1 : 0;
    attr.set_value(&attr_molecular_pump2_water_fault_);
}

void VacuumDevice::read_molecular_pump3_water_fault(Tango::Attribute &attr) {
    attr_molecular_pump3_water_fault_ = molecular_pump3_water_fault_ ? 1 : 0;
    attr.set_value(&attr_molecular_pump3_water_fault_);
}

// 阀门状态读取函数 - 模拟模式下直接返回内部状态
void VacuumDevice::read_gate_valve1_open(Tango::Attribute &attr) {
    // 非模拟模式下从PLC读取
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve1Open(), gate_valve1_open_);
    }
    attr_gate_valve1_open_ = gate_valve1_open_ ? 1 : 0;
    attr.set_value(&attr_gate_valve1_open_);
}

void VacuumDevice::read_gate_valve1_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve1Close(), gate_valve1_close_);
    }
    attr_gate_valve1_close_ = gate_valve1_close_ ? 1 : 0;
    attr.set_value(&attr_gate_valve1_close_);
}

void VacuumDevice::read_gate_valve2_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve2Open(), gate_valve2_open_);
    }
    attr_gate_valve2_open_ = gate_valve2_open_ ? 1 : 0;
    attr.set_value(&attr_gate_valve2_open_);
}

void VacuumDevice::read_gate_valve2_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve2Close(), gate_valve2_close_);
    }
    attr_gate_valve2_close_ = gate_valve2_close_ ? 1 : 0;
    attr.set_value(&attr_gate_valve2_close_);
}

void VacuumDevice::read_gate_valve3_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve3Open(), gate_valve3_open_);
    }
    attr_gate_valve3_open_ = gate_valve3_open_ ? 1 : 0;
    attr.set_value(&attr_gate_valve3_open_);
}

void VacuumDevice::read_gate_valve3_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve3Close(), gate_valve3_close_);
    }
    attr_gate_valve3_close_ = gate_valve3_close_ ? 1 : 0;
    attr.set_value(&attr_gate_valve3_close_);
}

void VacuumDevice::read_gate_valve4_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve4Open(), gate_valve4_open_);
    }
    attr_gate_valve4_open_ = gate_valve4_open_ ? 1 : 0;
    attr.set_value(&attr_gate_valve4_open_);
}

void VacuumDevice::read_gate_valve4_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve4Close(), gate_valve4_close_);
    }
    attr_gate_valve4_close_ = gate_valve4_close_ ? 1 : 0;
    attr.set_value(&attr_gate_valve4_close_);
}

void VacuumDevice::read_gate_valve5_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve5Open(), gate_valve5_open_);
    }
    attr_gate_valve5_open_ = gate_valve5_open_ ? 1 : 0;
    attr.set_value(&attr_gate_valve5_open_);
}

void VacuumDevice::read_gate_valve5_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::GateValve5Close(), gate_valve5_close_);
    }
    attr_gate_valve5_close_ = gate_valve5_close_ ? 1 : 0;
    attr.set_value(&attr_gate_valve5_close_);
}

void VacuumDevice::read_electromagnetic_valve1_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::ElectromagneticValve1Open(), electromagnetic_valve1_open_);
    }
    attr_electromagnetic_valve1_open_ = electromagnetic_valve1_open_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve1_open_);
}

void VacuumDevice::read_electromagnetic_valve1_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::ElectromagneticValve1Close(), electromagnetic_valve1_close_);
    }
    attr_electromagnetic_valve1_close_ = electromagnetic_valve1_close_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve1_close_);
}

void VacuumDevice::read_electromagnetic_valve2_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::ElectromagneticValve2Open(), electromagnetic_valve2_open_);
    }
    attr_electromagnetic_valve2_open_ = electromagnetic_valve2_open_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve2_open_);
}

void VacuumDevice::read_electromagnetic_valve2_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::ElectromagneticValve2Close(), electromagnetic_valve2_close_);
    }
    attr_electromagnetic_valve2_close_ = electromagnetic_valve2_close_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve2_close_);
}

void VacuumDevice::read_electromagnetic_valve3_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::ElectromagneticValve3Open(), electromagnetic_valve3_open_);
    }
    attr_electromagnetic_valve3_open_ = electromagnetic_valve3_open_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve3_open_);
}

void VacuumDevice::read_electromagnetic_valve3_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::ElectromagneticValve3Close(), electromagnetic_valve3_close_);
    }
    attr_electromagnetic_valve3_close_ = electromagnetic_valve3_close_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve3_close_);
}

void VacuumDevice::read_electromagnetic_valve4_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::ElectromagneticValve4Open(), electromagnetic_valve4_open_);
    }
    attr_electromagnetic_valve4_open_ = electromagnetic_valve4_open_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve4_open_);
}

void VacuumDevice::read_electromagnetic_valve4_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::ElectromagneticValve4Close(), electromagnetic_valve4_close_);
    }
    attr_electromagnetic_valve4_close_ = electromagnetic_valve4_close_ ? 1 : 0;
    attr.set_value(&attr_electromagnetic_valve4_close_);
}

void VacuumDevice::read_vent_valve1_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::VentValve1Open(), vent_valve1_open_);
    }
    attr_vent_valve1_open_ = vent_valve1_open_ ? 1 : 0;
    attr.set_value(&attr_vent_valve1_open_);
}

void VacuumDevice::read_vent_valve1_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::VentValve1Close(), vent_valve1_close_);
    }
    attr_vent_valve1_close_ = vent_valve1_close_ ? 1 : 0;
    attr.set_value(&attr_vent_valve1_close_);
}

void VacuumDevice::read_vent_valve2_open(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::VentValve2Open(), vent_valve2_open_);
    }
    attr_vent_valve2_open_ = vent_valve2_open_ ? 1 : 0;
    attr.set_value(&attr_vent_valve2_open_);
}

void VacuumDevice::read_vent_valve2_close(Tango::Attribute &attr) {
    if (!sim_mode_) {
        readPLCBool(PLC::VacuumPLCMapping::VentValve2Close(), vent_valve2_close_);
    }
    attr_vent_valve2_close_ = vent_valve2_close_ ? 1 : 0;
    attr.set_value(&attr_vent_valve2_close_);
}

// ===== HOOKS =====
void VacuumDevice::specific_self_check() {
    // Check PLC connection
}

void VacuumDevice::always_executed_hook() {
    Common::StandardSystemDevice::always_executed_hook();
    
    // 定期更新PLC数据（节流，避免在高频属性读取时过于频繁）
    const auto now = std::chrono::steady_clock::now();
    if (now - last_plc_update_ < std::chrono::milliseconds(plc_update_interval_ms_)) {
        return;
    }
    last_plc_update_ = now;

    // 如果未连接，尝试重连（不使用线程避免堆积）
    if (plc_comm_ && !plc_comm_->isConnected()) {
        if (now - last_connect_attempt_ >= std::chrono::milliseconds(plc_reconnect_interval_ms_)) {
            last_connect_attempt_ = now;
            // 简单尝试重连，超时由snap7控制，不会阻塞太久
            bool ok = plc_comm_->connect(plc_ip_, plc_port_);
            if (ok) {
                set_state(Tango::ON);
                result_value_ = 0;
            }
        }
        return; // 未连接时不进行数据更新
    }

    try {
        updatePLCData();
    } catch (...) {
        // 忽略PLC通信错误，避免影响其他功能
    }
}

void VacuumDevice::read_attr_hardware(std::vector<long> &/*attr_list*/) {
    // Update from hardware (PLC)
    // 数据已在always_executed_hook中通过updatePLCData更新
}

void VacuumDevice::ensure_unlocked(const char *origin) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (is_locked_) {
        Tango::Except::throw_exception("DEVICE_LOCKED",
            std::string("Device locked by ") + lock_user_, origin);
    }
}

void VacuumDevice::ensure_permission(size_t /*permission_index*/, const char *origin, bool check_mode) {
    // 权限检查：如果不在远程模式，需要检查权限
    // 当前实现：简化处理，仅检查模式
    if (check_mode && mode_ != OperationMode::AUTO && !sim_mode_) {
        Tango::Except::throw_exception("MODE_MISMATCH",
            "Operation only allowed in AUTO mode", origin);
    }
}

void VacuumDevice::log_event(const std::string &event) {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    INFO_STREAM << "[" << buf << "] " << event << std::endl;
}

// 命令日志：打印命令名、参数、执行结果以及PLC连接状态
void VacuumDevice::log_command_result(const std::string &name, const std::string &args, bool ok, bool plc_connected) {
    INFO_STREAM << "[CMD] " << name << "(" << args << ") => "
                << (ok ? "OK" : "FAIL")
                << ", plc_connected=" << (plc_connected ? "YES" : "NO")
                << std::endl;
}

// ===== 自定义属性类 - 用于转发读取请求到设备的 read_attr 方法 =====
// Tango 的 Attr::read() 默认是空实现，必须子类化并重写 read() 方法
// 才能让属性读取时调用到设备的 read_attr() 方法

class VacuumAttr : public Tango::Attr {
public:
    VacuumAttr(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<VacuumDevice *>(dev)->read_attr(att);
    }
};

class VacuumAttrRW : public Tango::Attr {
public:
    VacuumAttrRW(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ_WRITE)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<VacuumDevice *>(dev)->read_attr(att);
    }
    
    virtual void write(Tango::DeviceImpl *dev, Tango::WAttribute &att) override {
        static_cast<VacuumDevice *>(dev)->write_attr(att);
    }
};

// ===== CLASS FACTORY =====
VacuumDeviceClass *VacuumDeviceClass::_instance = nullptr;

VacuumDeviceClass *VacuumDeviceClass::instance() {
    if (_instance == nullptr) {
        std::string class_name = "VacuumDevice";
        _instance = new VacuumDeviceClass(class_name);
    }
    return _instance;
}

VacuumDeviceClass::VacuumDeviceClass(std::string &class_name) : Tango::DeviceClass(class_name) {}

void VacuumDeviceClass::attribute_factory(std::vector<Tango::Attr *> &att_list) {
    // ===== 固有状态属性 (CTS通信表 Sheet: 固有状态) =====
    att_list.push_back(new VacuumAttr("bundleNo", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("laserNo", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("systemNo", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("subDevList", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::SpectrumAttr("modelList", Tango::DEV_STRING, Tango::READ, 16));
    att_list.push_back(new VacuumAttr("currentModel", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("connectString", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("errorDict", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("devicePosition", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("deviceProductDate", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("deviceInstallDate", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new VacuumAttr("simulatorMode", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 自检状态属性 - 按通信表定义
    att_list.push_back(new VacuumAttr("selfCheckState", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumAttr("selfCheckStatus", Tango::DEV_STRING, Tango::READ));
    
    // 运行模式
    att_list.push_back(new VacuumAttr("mode", Tango::DEV_SHORT, Tango::READ));
    
    // ===== 基于新PLC Tags的属性注册 =====
    // 运行状态属性
    att_list.push_back(new VacuumAttr("operationMode", Tango::DEV_SHORT, Tango::READ));
    att_list.push_back(new VacuumAttr("systemState", Tango::DEV_SHORT, Tango::READ));
    att_list.push_back(new VacuumAttr("autoState", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("manualState", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("remoteState", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 传感器数据属性
    att_list.push_back(new VacuumAttr("vacuumGauge1", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new VacuumAttr("vacuumGauge2", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new VacuumAttr("vacuumGauge3", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new VacuumAttr("waterPressure", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new VacuumAttr("airPressure", Tango::DEV_DOUBLE, Tango::READ));
    
    // 泵状态属性
    att_list.push_back(new VacuumAttr("screwPumpPower", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("rootsPumpPower", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump1Power", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump2Power", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump3Power", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("screwPumpSpeed", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump1Speed", Tango::DEV_USHORT, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump2Speed", Tango::DEV_USHORT, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump3Speed", Tango::DEV_USHORT, Tango::READ));
    
    // 参数设置属性 (READ_WRITE)
    att_list.push_back(new VacuumAttrRW("molecularPumpStartStopSelect", Tango::DEV_SHORT, Tango::READ_WRITE));
    att_list.push_back(new VacuumAttrRW("gaugeCriterion", Tango::DEV_SHORT, Tango::READ_WRITE));
    att_list.push_back(new VacuumAttrRW("molecularPumpCriterion", Tango::DEV_SHORT, Tango::READ_WRITE));
    
    // 异常状态属性
    att_list.push_back(new VacuumAttr("gateValve1Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve2Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve3Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve4Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve5Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve1Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve2Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve3Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve4Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("ventValve1Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("ventValve2Fault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("phaseSequenceFault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("screwPumpWaterFault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump1WaterFault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump2WaterFault", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("molecularPump3WaterFault", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 阀门状态属性（开/关位置反馈）
    att_list.push_back(new VacuumAttr("gateValve1Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve1Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve2Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve2Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve3Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve3Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve4Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve4Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve5Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("gateValve5Close", Tango::DEV_BOOLEAN, Tango::READ));
    
    att_list.push_back(new VacuumAttr("electromagneticValve1Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve1Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve2Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve2Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve3Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve3Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve4Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("electromagneticValve4Close", Tango::DEV_BOOLEAN, Tango::READ));
    
    att_list.push_back(new VacuumAttr("ventValve1Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("ventValve1Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("ventValve2Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumAttr("ventValve2Close", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 组合属性
    att_list.push_back(new VacuumAttr("groupAttributeJson", Tango::DEV_STRING, Tango::READ));
    
    // Runtime state
    att_list.push_back(new VacuumAttr("resultValue", Tango::DEV_SHORT, Tango::READ));
    
    // ===== 旧的真空控制属性已删除，仅保留基于新PLC Tags的属性 =====
}

void VacuumDeviceClass::command_factory() {
    // Lock/Unlock commands
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("devLock",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&VacuumDevice::devLock)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("devUnlock",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&VacuumDevice::devUnlock)));
    command_list.push_back(new Tango::TemplCommand("devLockVerify",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::devLockVerify)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("devLockQuery",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&VacuumDevice::devLockQuery)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("devUserConfig",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&VacuumDevice::devUserConfig)));
    
    // System commands
    command_list.push_back(new Tango::TemplCommand("selfCheck",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::selfCheck)));
    command_list.push_back(new Tango::TemplCommand("init",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::init)));
    command_list.push_back(new Tango::TemplCommand("reset",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::reset)));
    
    
    // ===== 基于新PLC Tags的命令注册 =====
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("switchMode",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&VacuumDevice::switchMode)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("setRemoteControl",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&VacuumDevice::setRemoteControl)));
    command_list.push_back(new Tango::TemplCommand("oneKeyVacuumStart",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::oneKeyVacuumStart)));
    command_list.push_back(new Tango::TemplCommand("oneKeyVacuumStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::oneKeyVacuumStop)));
    command_list.push_back(new Tango::TemplCommand("ventStart",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::ventStart)));
    command_list.push_back(new Tango::TemplCommand("ventStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::ventStop)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("setScrewPumpPower",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&VacuumDevice::setScrewPumpPower)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("setScrewPumpStartStop",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&VacuumDevice::setScrewPumpStartStop)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("setRootsPumpPower",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&VacuumDevice::setRootsPumpPower)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarShortArray *>("setMolecularPumpPower",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarShortArray *)>(&VacuumDevice::setMolecularPumpPower)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarShortArray *>("setMolecularPumpStartStop",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarShortArray *)>(&VacuumDevice::setMolecularPumpStartStop)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("setScrewPumpSpeed",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&VacuumDevice::setScrewPumpSpeed)));
    command_list.push_back(new Tango::TemplCommand("resetScrewPumpFault",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::resetScrewPumpFault)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarShortArray *>("setGateValve",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarShortArray *)>(&VacuumDevice::setGateValve)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarShortArray *>("setElectromagneticValve",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarShortArray *)>(&VacuumDevice::setElectromagneticValve)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarShortArray *>("setVentValve",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarShortArray *)>(&VacuumDevice::setVentValve)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarShortArray *>("setWaterElectromagneticValve",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarShortArray *)>(&VacuumDevice::setWaterElectromagneticValve)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("setAirMainElectromagneticValve",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&VacuumDevice::setAirMainElectromagneticValve)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("setMolecularPumpStartStopSelect",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&VacuumDevice::setMolecularPumpStartStopSelect)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("setGaugeCriterion",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&VacuumDevice::setGaugeCriterion)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("setMolecularPumpCriterion",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&VacuumDevice::setMolecularPumpCriterion)));
    command_list.push_back(new Tango::TemplCommand("alarmReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::alarmReset)));
    command_list.push_back(new Tango::TemplCommand("emergencyStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::emergencyStop)));
    command_list.push_back(new Tango::TemplCommand("connectPLC",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::connectPLC)));
    command_list.push_back(new Tango::TemplCommand("disconnectPLC",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::disconnectPLC)));
    command_list.push_back(new Tango::TemplCommand("updatePLCData",
        static_cast<void (Tango::DeviceImpl::*)()>(&VacuumDevice::updatePLCData)));
}

void VacuumDeviceClass::device_factory(const Tango::DevVarStringArray *devlist_ptr) {
    for (unsigned long i = 0; i < devlist_ptr->length(); i++) {
        std::string dev_name = (*devlist_ptr)[i].in();
        VacuumDevice *dev = new VacuumDevice(this, dev_name);
        device_list.push_back(dev);
        export_device(dev);
    }
}

// ===== 基于新PLC Tags的命令实现 =====

void VacuumDevice::connectPLC() {
    ensure_unlocked("VacuumDevice::connectPLC");
    
    INFO_STREAM << "========== VacuumDevice::connectPLC ===========" << std::endl;
    INFO_STREAM << "  PLC IP: " << plc_ip_ << std::endl;
    INFO_STREAM << "  PLC Port: " << plc_port_ << std::endl;
    INFO_STREAM << "  Sim Mode: " << (sim_mode_ ? "YES" : "NO") << std::endl;
    
    if (!plc_comm_) {
        ERROR_STREAM << "  ERROR: PLC communication object not initialized!" << std::endl;
        Tango::Except::throw_exception("PLC_NOT_INITIALIZED",
            "PLC通信未初始化", "VacuumDevice::connectPLC");
    }
    
    INFO_STREAM << "  PLC Comm Type: " << (sim_mode_ ? "MockPLCCommunication" : "S7Communication (snap7)") << std::endl;
    
    // 如果已连接，先断开
    if (plc_comm_->isConnected()) {
        INFO_STREAM << "  Already connected, disconnecting first..." << std::endl;
        disconnectPLC();
    }
    
    INFO_STREAM << "  Connecting to PLC..." << std::endl;
    bool connected = plc_comm_->connect(plc_ip_, plc_port_);
    if (connected) {
        INFO_STREAM << "  SUCCESS: PLC connected!" << std::endl;
        log_event("PLC连接成功: " + plc_ip_ + ":" + std::to_string(plc_port_));
        set_state(Tango::ON);
        result_value_ = 0;
    } else {
        ERROR_STREAM << "  FAILED: Cannot connect to PLC!" << std::endl;
        log_event("PLC连接失败: " + plc_ip_);
        // 不抛出异常，保持设备存活并等待定时循环重试
        set_state(Tango::ALARM);
        result_value_ = 1;
    }
    INFO_STREAM << "================================================" << std::endl;
}

void VacuumDevice::disconnectPLC() {
    ensure_unlocked("VacuumDevice::disconnectPLC");
    
    if (plc_comm_ && plc_comm_->isConnected()) {
        plc_comm_->disconnect();
        log_event("PLC已断开连接");
    }
    result_value_ = 0;
}

// 内部方法：更新PLC数据（在always_executed_hook中调用）
void VacuumDevice::updatePLCData() {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        return;
    }
    
    // 移除大锁，改为在每个readPLC内部加锁，避免长时间阻塞
    try {
        // 只读取关键状态，减少通信量
        // 读取泵状态
        readPLCBool(PLC::VacuumPLCMapping::ScrewPumpPower(), screw_pump_power_);
        readPLCBool(PLC::VacuumPLCMapping::RootsPumpPower(), roots_pump_power_);
        readPLCBool(PLC::VacuumPLCMapping::MolecularPump1Power(), molecular_pump1_power_);
        readPLCBool(PLC::VacuumPLCMapping::MolecularPump2Power(), molecular_pump2_power_);
        readPLCBool(PLC::VacuumPLCMapping::MolecularPump3Power(), molecular_pump3_power_);

        // 模式/本地远程状态反馈
        readPLCBool(PLC::VacuumPLCMapping::AutoState(), auto_state_);
        readPLCBool(PLC::VacuumPLCMapping::ManualState(), manual_state_);
        readPLCBool(PLC::VacuumPLCMapping::RemoteState(), remote_state_);
        
        // 读取传感器数据（最重要）
        float sensor_real = 0.0f;
        if (readPLCReal(PLC::VacuumPLCMapping::VacuumGauge1(), sensor_real)) {
            vacuum_gauge1_ = static_cast<double>(sensor_real);
        }
        if (readPLCReal(PLC::VacuumPLCMapping::VacuumGauge2(), sensor_real)) {
            vacuum_gauge2_ = static_cast<double>(sensor_real);
        }
        if (readPLCReal(PLC::VacuumPLCMapping::VacuumGauge3(), sensor_real)) {
            vacuum_gauge3_ = static_cast<double>(sensor_real);
        }
        
        // 读取水冷故障（重要安全指标）
        readPLCBool(PLC::VacuumPLCMapping::ScrewPumpWaterFault(), screw_pump_water_fault_);
        readPLCBool(PLC::VacuumPLCMapping::MolecularPump1WaterFault(), molecular_pump1_water_fault_);
        readPLCBool(PLC::VacuumPLCMapping::MolecularPump2WaterFault(), molecular_pump2_water_fault_);
        readPLCBool(PLC::VacuumPLCMapping::MolecularPump3WaterFault(), molecular_pump3_water_fault_);
        
        // 其他数据按需读取，不在周期内读取以减少负载
        // 转速、阀门状态、其他故障等可通过属性读取时按需获取
        
        // 读取参数设置
        int16_t param_int = 0;
        if (readPLCInt(PLC::VacuumPLCMapping::MolecularPumpStartStopSelect(), param_int)) {
            molecular_pump_start_stop_select_ = param_int;
        }
        if (readPLCInt(PLC::VacuumPLCMapping::GaugeCriterion(), param_int)) {
            gauge_criterion_ = param_int;
        }
        if (readPLCInt(PLC::VacuumPLCMapping::MolecularPumpCriterion(), param_int)) {
            molecular_pump_criterion_ = param_int;
        }
        
        // 更新系统状态
        updateSystemState();
    } catch (const std::exception& e) {
        // 记录错误但不中断运行
        log_event(std::string("PLC数据更新错误: ") + e.what());
    }
}

void VacuumDevice::syncPLCData() {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(plc_comm_mutex_);
    
    // 同步输出数据到PLC（如果需要）
    // 这里可以根据需要写入控制命令
}

// ===== PLC 读写调试辅助函数 =====
static std::string formatPLCAddress(const Common::PLC::PLCAddress& address) {
    return address.address_string;
}

bool VacuumDevice::readPLCBool(const Common::PLC::PLCAddress& address, bool& value) {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        return false;
    }
    return plc_comm_->readBool(address, value);
}

bool VacuumDevice::readPLCWord(const Common::PLC::PLCAddress& address, uint16_t& value) {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        return false;
    }
    return plc_comm_->readWord(address, value);
}

bool VacuumDevice::readPLCInt(const Common::PLC::PLCAddress& address, int16_t& value) {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        return false;
    }
    return plc_comm_->readInt(address, value);
}

bool VacuumDevice::readPLCReal(const Common::PLC::PLCAddress& address, float& value) {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        return false;
    }
    return plc_comm_->readReal(address, value);
}

bool VacuumDevice::writePLCBool(const Common::PLC::PLCAddress& address, bool value) {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        WARN_STREAM << "[PLC-WRITE] Bool @ " << formatPLCAddress(address) 
                    << " <- " << (value ? "TRUE" : "FALSE")
                    << " - FAILED: PLC not connected" << std::endl;
        return false;
    }
    bool result = plc_comm_->writeBool(address, value);
    INFO_STREAM << "[PLC-WRITE] Bool @ " << formatPLCAddress(address) 
                << " <- " << (value ? "TRUE" : "FALSE")
                << " [" << (result ? "OK" : "FAIL") << "]" << std::endl;
    return result;
}

bool VacuumDevice::writePLCWord(const Common::PLC::PLCAddress& address, uint16_t value) {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        WARN_STREAM << "[PLC-WRITE] Word @ " << formatPLCAddress(address) 
                    << " <- " << value
                    << " - FAILED: PLC not connected" << std::endl;
        return false;
    }
    bool result = plc_comm_->writeWord(address, value);
    INFO_STREAM << "[PLC-WRITE] Word @ " << formatPLCAddress(address) 
                << " <- " << value << " (0x" << std::hex << value << std::dec << ")"
                << " [" << (result ? "OK" : "FAIL") << "]" << std::endl;
    return result;
}

bool VacuumDevice::writePLCInt(const Common::PLC::PLCAddress& address, int16_t value) {
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        WARN_STREAM << "[PLC-WRITE] Int @ " << formatPLCAddress(address) 
                    << " <- " << value
                    << " - FAILED: PLC not connected" << std::endl;
        return false;
    }
    bool result = plc_comm_->writeInt(address, value);
    INFO_STREAM << "[PLC-WRITE] Int @ " << formatPLCAddress(address) 
                << " <- " << value 
                << " [" << (result ? "OK" : "FAIL") << "]" << std::endl;
    return result;
}

void VacuumDevice::updateSystemState() {
    // 检查故障
    checkFaults();
    
    // 根据当前状态更新系统状态
    // 首先检查是否有故障
    if (phase_sequence_fault_ || screw_pump_water_fault_ || 
        molecular_pump1_water_fault_ || molecular_pump2_water_fault_ || 
        molecular_pump3_water_fault_ ||
        gate_valve1_fault_ || gate_valve2_fault_ || gate_valve3_fault_ ||
        gate_valve4_fault_ || gate_valve5_fault_ ||
        electromagnetic_valve1_fault_ || electromagnetic_valve2_fault_ ||
        electromagnetic_valve3_fault_ || electromagnetic_valve4_fault_ ||
        vent_valve1_fault_ || vent_valve2_fault_) {
        system_state_ = SystemState::FAULT;
    } else {
        // 根据PLC反馈的状态判断系统状态
        // 如果有泵在运行，可能是抽真空状态
        // 实际应该根据PLC的反馈状态来判断（需要从PLC读取系统状态反馈）
        // 暂时根据泵的运行状态来判断
        if (screw_pump_power_ || roots_pump_power_ || 
            molecular_pump1_power_ || molecular_pump2_power_ || molecular_pump3_power_) {
            // 如果有泵在运行，可能是抽真空状态
            // 注意：实际应该根据PLC的系统状态反馈来判断，这里只是临时逻辑
            system_state_ = SystemState::PUMPING;
        } else {
            system_state_ = SystemState::IDLE;
        }
    }
}

void VacuumDevice::checkFaults() {
    // 检查所有异常标志，更新故障状态
    // 故障信息已经在updatePLCData中读取
}

} // namespace Vacuum

// Main function
void Tango::DServer::class_factory() {
    add_class(Vacuum::VacuumDeviceClass::instance());
}

int main(int argc, char *argv[]) {
    // 禁用 stdout/stderr 缓冲，确保日志实时输出
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    
    try {
        Common::SystemConfig::loadConfig();
        Tango::Util *tg = Tango::Util::init(argc, argv);
        tg->server_init();
        std::cout << "Vacuum Server Ready" << std::endl;
        tg->server_run();
    } catch (std::bad_alloc &) {
        std::cout << "Can't allocate memory!!!" << std::endl;
        return -1;
    } catch (CORBA::Exception &e) {
        Tango::Except::print_exception(e);
        return -1;
    }
    return 0;
}
