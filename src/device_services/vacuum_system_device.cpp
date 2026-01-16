/**
 * @file vacuum_system_device.cpp
 * @brief 真空系统 Tango 设备服务 - 实现文件
 * 
 * 设备名: sys/vacuum/2
 * 通讯协议: OPC UA
 */

#include "device_services/vacuum_system_device.h"
#include "device_services/vacuum_system_plc_mapping.h"
#include "common/system_config.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace VacuumSystem;
using namespace VacuumSystem::PLC;

// ============================================================================
// VacuumSystemDevice 构造/析构
// ============================================================================

VacuumSystemDevice::VacuumSystemDevice(Tango::DeviceClass* device_class, std::string& device_name)
    : Tango::Device_4Impl(device_class, device_name.c_str()) {
    init_device();
}

VacuumSystemDevice::VacuumSystemDevice(Tango::DeviceClass* device_class, const char* device_name)
    : Tango::Device_4Impl(device_class, device_name) {
    init_device();
}

VacuumSystemDevice::~VacuumSystemDevice() {
    delete_device();
}

// ============================================================================
// 设备生命周期
// ============================================================================

void VacuumSystemDevice::init_device() {
    INFO_STREAM << "VacuumSystemDevice::init_device - 初始化真空系统设备 sys/vacuum/2" << std::endl;
    
    // 加载配置
    plc_ip_ = Common::SystemConfig::DEFAULT_PLC_IP;
    plc_port_ = 4840;  // OPC UA 默认端口
    sim_mode_ = Common::SystemConfig::SIM_MODE;  // 从配置读取模拟模式
    alarm_log_path_ = "logs/vacuum_system_alarms.json";
    poll_interval_ms_ = 100;  // 100ms 轮询
    
    if (sim_mode_) {
        INFO_STREAM << "========================================" << std::endl;
        INFO_STREAM << "  模拟模式已启用 (SIM_MODE=true)" << std::endl;
        INFO_STREAM << "  不连接真实 PLC，使用内部模拟逻辑" << std::endl;
        INFO_STREAM << "========================================" << std::endl;
    }
    
    // 初始化状态
    operation_mode_ = OperationMode::MANUAL;
    system_state_ = SystemState::IDLE;
    auto_sequence_step_ = 0;
    
    // 初始化泵状态
    screw_pump_power_ = false;
    roots_pump_power_ = false;
    molecular_pump1_power_ = false;
    molecular_pump2_power_ = false;
    molecular_pump3_power_ = false;
    molecular_pump1_speed_ = 0;
    molecular_pump2_speed_ = 0;
    molecular_pump3_speed_ = 0;
    
    // 初始化阀门状态
    // 模拟模式下，阀门初始化为关闭状态（close=true, open=false）
    // 非模拟模式下，状态从PLC读取
    if (sim_mode_) {
        // 模拟模式：阀门初始化为关闭状态
        gate_valve1_open_ = false; gate_valve1_close_ = true;
        gate_valve2_open_ = false; gate_valve2_close_ = true;
        gate_valve3_open_ = false; gate_valve3_close_ = true;
        gate_valve4_open_ = false; gate_valve4_close_ = true;
        gate_valve5_open_ = false; gate_valve5_close_ = true;
        electromagnetic_valve1_open_ = false; electromagnetic_valve1_close_ = true;
        electromagnetic_valve2_open_ = false; electromagnetic_valve2_close_ = true;
        electromagnetic_valve3_open_ = false; electromagnetic_valve3_close_ = true;
        electromagnetic_valve4_open_ = false; electromagnetic_valve4_close_ = true;
        vent_valve1_open_ = false; vent_valve1_close_ = true;
        vent_valve2_open_ = false; vent_valve2_close_ = true;
        INFO_STREAM << "模拟模式：阀门初始化为关闭状态" << std::endl;
    } else {
        // 非模拟模式：阀门状态待从PLC读取
        gate_valve1_open_ = gate_valve1_close_ = false;
        gate_valve2_open_ = gate_valve2_close_ = false;
        gate_valve3_open_ = gate_valve3_close_ = false;
        gate_valve4_open_ = gate_valve4_close_ = false;
        gate_valve5_open_ = gate_valve5_close_ = false;
        electromagnetic_valve1_open_ = electromagnetic_valve1_close_ = false;
        electromagnetic_valve2_open_ = electromagnetic_valve2_close_ = false;
        electromagnetic_valve3_open_ = electromagnetic_valve3_close_ = false;
        electromagnetic_valve4_open_ = electromagnetic_valve4_close_ = false;
        vent_valve1_open_ = vent_valve1_close_ = false;
        vent_valve2_open_ = vent_valve2_close_ = false;
    }
    
    // 初始化传感器
    vacuum_gauge1_ = vacuum_gauge2_ = vacuum_gauge3_ = 101325.0;  // 大气压
    air_pressure_ = 0.5;  // MPa
    screw_pump_frequency_ = 0;  // Hz
    roots_pump_frequency_ = 0;  // Hz
    
    // 初始化系统联锁信号
    phase_sequence_ok_ = true;
    motion_system_online_ = false;
    gate_valve5_permit_ = false;
    motion_req_open_gv5_ = false;
    motion_req_close_gv5_ = false;
    
    // 初始化分子泵启用配置
    // 在非模拟模式下，将从PLC读取；模拟模式下默认全部启用
    if (sim_mode_) {
        molecular_pump1_enabled_ = true;
        molecular_pump2_enabled_ = true;
        molecular_pump3_enabled_ = true;
    } else {
        // 非模拟模式：初始化为true，后续从PLC同步
        molecular_pump1_enabled_ = true;
        molecular_pump2_enabled_ = true;
        molecular_pump3_enabled_ = true;
    }
    
    // 初始化水电磁阀状态
    // 模拟模式下：水电磁阀1-4初始化为开启（表示水路正常），以便一键抽真空功能可以执行
    // 正常模式下：从PLC读取实际状态
    if (sim_mode_) {
        water_valve1_state_ = true;   // 模拟模式：水路1正常
        water_valve2_state_ = true;   // 模拟模式：水路2正常
        water_valve3_state_ = true;   // 模拟模式：水路3正常
        water_valve4_state_ = true;   // 模拟模式：水路4正常
        water_valve5_state_ = false;
        water_valve6_state_ = false;
        INFO_STREAM << "模拟模式：水电磁阀1-4初始化为开启（水路正常）" << std::endl;
    } else {
        water_valve1_state_ = false;
        water_valve2_state_ = false;
        water_valve3_state_ = false;
        water_valve4_state_ = false;
        water_valve5_state_ = false;
        water_valve6_state_ = false;
    }
    air_main_valve_state_ = false;
    
    // 初始化PLC连接相关状态
    last_connect_attempt_ = std::chrono::steady_clock::now() - std::chrono::seconds(PLC_RECONNECT_INTERVAL_SEC);
    plc_was_connected_.store(false);
    
    // 初始化自动流程状态
    auto_sequence_step_ = 0;
    vacuum_sequence_is_low_vacuum_ = false;  // 初始化为非真空流程
    
    // 创建 PLC 通信对象（仅在非模拟模式下）
    if (sim_mode_) {
        INFO_STREAM << "模拟模式：跳过 PLC 通信初始化" << std::endl;
        plc_comm_ = nullptr;  // 模拟模式不需要 PLC 通信对象
        plc_connecting_.store(false);
    } else {
        INFO_STREAM << "使用 OPC UA 通信 -> " << plc_ip_ << std::endl;
        plc_comm_ = std::make_unique<Common::PLC::OPCUACommunication>();
        
        // 尝试快速连接 PLC（设置短超时）
        // 如果连接失败，不阻塞初始化，让轮询线程后续重试
        INFO_STREAM << "尝试连接 PLC（将在后台重试）..." << std::endl;
        
        // 在后台线程中进行初始连接尝试，不阻塞设备初始化
        plc_connecting_.store(true);
        std::thread([this]() {
            try {
                if (connectPLC()) {
                    synchronizeStateFromPLC();
                    INFO_STREAM << "PLC 初始连接成功" << std::endl;
                } else {
                    WARN_STREAM << "初始化时 PLC 连接失败，将在后台重试" << std::endl;
                }
            } catch (const std::exception& e) {
                ERROR_STREAM << "PLC 连接异常: " << e.what() << std::endl;
            }
            plc_connecting_.store(false);
        }).detach();
    }
    
    // 启动后台轮询线程
    poll_running_ = true;
    poll_thread_ = std::thread([this]() {
        while (poll_running_) {
            try {
                pollPLCStatus();
            } catch (const std::exception& e) {
                ERROR_STREAM << "轮询异常: " << e.what() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));
        }
    });
    
    set_state(Tango::ON);
    logEvent("设备初始化完成");
}

void VacuumSystemDevice::delete_device() {
    INFO_STREAM << "VacuumSystemDevice::delete_device" << std::endl;
    
    // 停止轮询线程
    poll_running_ = false;
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
    
    // 断开 PLC
    disconnectPLC();
    
    logEvent("设备已关闭");
}

void VacuumSystemDevice::always_executed_hook() {
    // 每次属性/命令调用前执行
}

void VacuumSystemDevice::read_attr_hardware(std::vector<long>& attr_list) {
    (void)attr_list;
    // 批量读取硬件数据（可优化）
}

void VacuumSystemDevice::read_attr(Tango::Attribute& attr) {
    std::string attr_name = attr.get_name();
    // DEBUG_STREAM << "[DEBUG] read_attr: 读取属性 " << attr_name << std::endl;  // 已禁用：输出过多
    
    // 系统状态
    if (attr_name == "operationMode") read_operationMode(attr);
    else if (attr_name == "systemState") read_systemState(attr);
    else if (attr_name == "simulatorMode") read_simulatorMode(attr);
    
    // 泵状态
    else if (attr_name == "screwPumpPower") read_screwPumpPower(attr);
    else if (attr_name == "rootsPumpPower") read_rootsPumpPower(attr);
    else if (attr_name == "molecularPump1Power") read_molecularPump1Power(attr);
    else if (attr_name == "molecularPump2Power") read_molecularPump2Power(attr);
    else if (attr_name == "molecularPump3Power") read_molecularPump3Power(attr);
    else if (attr_name == "molecularPump1Speed") read_molecularPump1Speed(attr);
    else if (attr_name == "molecularPump2Speed") read_molecularPump2Speed(attr);
    else if (attr_name == "molecularPump3Speed") read_molecularPump3Speed(attr);
    else if (attr_name == "molecularPump1Enabled") read_molecularPump1Enabled(attr);
    else if (attr_name == "molecularPump2Enabled") read_molecularPump2Enabled(attr);
    else if (attr_name == "molecularPump3Enabled") read_molecularPump3Enabled(attr);
    else if (attr_name == "screwPumpFrequency") read_screwPumpFrequency(attr);
    else if (attr_name == "rootsPumpFrequency") read_rootsPumpFrequency(attr);
    
    // 闸板阀状态
    else if (attr_name == "gateValve1Open") read_gateValve1Open(attr);
    else if (attr_name == "gateValve1Close") read_gateValve1Close(attr);
    else if (attr_name == "gateValve2Open") read_gateValve2Open(attr);
    else if (attr_name == "gateValve2Close") read_gateValve2Close(attr);
    else if (attr_name == "gateValve3Open") read_gateValve3Open(attr);
    else if (attr_name == "gateValve3Close") read_gateValve3Close(attr);
    else if (attr_name == "gateValve4Open") read_gateValve4Open(attr);
    else if (attr_name == "gateValve4Close") read_gateValve4Close(attr);
    else if (attr_name == "gateValve5Open") read_gateValve5Open(attr);
    else if (attr_name == "gateValve5Close") read_gateValve5Close(attr);
    
    // 电磁阀状态
    else if (attr_name == "electromagneticValve1Open") read_electromagneticValve1Open(attr);
    else if (attr_name == "electromagneticValve1Close") read_electromagneticValve1Close(attr);
    else if (attr_name == "electromagneticValve2Open") read_electromagneticValve2Open(attr);
    else if (attr_name == "electromagneticValve2Close") read_electromagneticValve2Close(attr);
    else if (attr_name == "electromagneticValve3Open") read_electromagneticValve3Open(attr);
    else if (attr_name == "electromagneticValve3Close") read_electromagneticValve3Close(attr);
    else if (attr_name == "electromagneticValve4Open") read_electromagneticValve4Open(attr);
    else if (attr_name == "electromagneticValve4Close") read_electromagneticValve4Close(attr);
    
    // 放气阀状态
    else if (attr_name == "ventValve1Open") read_ventValve1Open(attr);
    else if (attr_name == "ventValve1Close") read_ventValve1Close(attr);
    else if (attr_name == "ventValve2Open") read_ventValve2Open(attr);
    else if (attr_name == "ventValve2Close") read_ventValve2Close(attr);
    
    // 阀门动作状态
    else if (attr_name == "gateValve1ActionState") read_gateValve1ActionState(attr);
    else if (attr_name == "gateValve2ActionState") read_gateValve2ActionState(attr);
    else if (attr_name == "gateValve3ActionState") read_gateValve3ActionState(attr);
    else if (attr_name == "gateValve4ActionState") read_gateValve4ActionState(attr);
    else if (attr_name == "gateValve5ActionState") read_gateValve5ActionState(attr);
    
    // 传感器
    else if (attr_name == "vacuumGauge1") read_vacuumGauge1(attr);
    else if (attr_name == "vacuumGauge2") read_vacuumGauge2(attr);
    else if (attr_name == "vacuumGauge3") read_vacuumGauge3(attr);
    else if (attr_name == "airPressure") read_airPressure(attr);
    
    // 自动流程
    else if (attr_name == "autoSequenceStep") read_autoSequenceStep(attr);
    else if (attr_name == "plcConnected") read_plcConnected(attr);
    
    // 系统联锁信号
    else if (attr_name == "phaseSequenceOk") read_phaseSequenceOk(attr);
    else if (attr_name == "motionSystemOnline") read_motionSystemOnline(attr);
    else if (attr_name == "gateValve5Permit") read_gateValve5Permit(attr);
    
    // 水电磁阀状态
    else if (attr_name == "waterValve1State") read_waterValve1State(attr);
    else if (attr_name == "waterValve2State") read_waterValve2State(attr);
    else if (attr_name == "waterValve3State") read_waterValve3State(attr);
    else if (attr_name == "waterValve4State") read_waterValve4State(attr);
    else if (attr_name == "waterValve5State") read_waterValve5State(attr);
    else if (attr_name == "waterValve6State") read_waterValve6State(attr);
    else if (attr_name == "airMainValveState") read_airMainValveState(attr);
    
    // 报警
    else if (attr_name == "activeAlarmCount") read_activeAlarmCount(attr);
    else if (attr_name == "hasUnacknowledgedAlarm") read_hasUnacknowledgedAlarm(attr);
    else if (attr_name == "latestAlarmJson") read_latestAlarmJson(attr);
}

// ============================================================================
// PLC 通信方法
// ============================================================================

bool VacuumSystemDevice::connectPLC() {
    std::lock_guard<std::mutex> lock(plc_mutex_);
    return connectPLC_locked();
}

// 内部方法：假设调用者已持有 plc_mutex_
bool VacuumSystemDevice::connectPLC_locked() {
    if (!plc_comm_) {
        DEBUG_STREAM << "[DEBUG] connectPLC_locked: PLC 通信对象不存在" << std::endl;
        return false;
    }
    
    if (plc_comm_->isConnected()) {
        DEBUG_STREAM << "[DEBUG] connectPLC_locked: PLC 已连接" << std::endl;
        plc_was_connected_.store(true);  // 更新连接状态
        return true;
    }
    
    DEBUG_STREAM << "[DEBUG] connectPLC_locked: 尝试连接 PLC (" << plc_ip_ << ":" << plc_port_ << ")" << std::endl;
    if (plc_comm_->connect(plc_ip_, plc_port_)) {
        INFO_STREAM << "PLC 连接成功" << std::endl;
        DEBUG_STREAM << "[DEBUG] connectPLC_locked: 连接成功" << std::endl;
        plc_was_connected_.store(true);  // 更新连接状态
        return true;
    }
    
    ERROR_STREAM << "PLC 连接失败" << std::endl;
    DEBUG_STREAM << "[DEBUG] connectPLC_locked: 连接失败" << std::endl;
    plc_was_connected_.store(false);  // 更新连接状态
    return false;
}

void VacuumSystemDevice::disconnectPLC() {
    std::lock_guard<std::mutex> lock(plc_mutex_);
    
    if (plc_comm_ && plc_comm_->isConnected()) {
        plc_comm_->disconnect();
        INFO_STREAM << "PLC 已断开" << std::endl;
    }
}

bool VacuumSystemDevice::readPLCBool(const Common::PLC::PLCAddress& addr, bool& value) {
    std::lock_guard<std::mutex> lock(plc_mutex_);
    
    // 快速失败：如果正在连接或未连接，立即返回
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        // 检测连接状态变化：如果之前是连接的，现在断开了，记录日志
        if (plc_was_connected_.load()) {
            WARN_STREAM << "PLC 连接断开（在读取 " << addr.address_string << " 时检测到）" << std::endl;
            plc_was_connected_.store(false);
        }
        // DEBUG_STREAM << "[DEBUG] readPLCBool: PLC 未连接，跳过读取 " << addr.address_string << std::endl;
        return false;  // 不尝试重连，让 pollPLCStatus 处理重连
    }
    
    bool result = plc_comm_->readBool(addr, value);
    
    // 如果读取失败，可能是连接已断开，检查连接状态
    if (!result && plc_was_connected_.load()) {
        // 再次检查连接状态（可能在上次检查后断开）
        if (!plc_comm_->isConnected()) {
            WARN_STREAM << "PLC 连接断开（读取 " << addr.address_string << " 失败时检测到）" << std::endl;
            plc_was_connected_.store(false);
        }
    }
    
    // DEBUG_STREAM << "[DEBUG] readPLCBool: " << addr.address_string << " = " << (value ? "true" : "false") 
    //              << " (成功=" << (result ? "是" : "否") << ")" << std::endl;
    return result;
}

bool VacuumSystemDevice::readPLCWord(const Common::PLC::PLCAddress& addr, uint16_t& value) {
    std::lock_guard<std::mutex> lock(plc_mutex_);
    
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        // 检测连接状态变化：如果之前是连接的，现在断开了，记录日志
        if (plc_was_connected_.load()) {
            WARN_STREAM << "PLC 连接断开（在读取 " << addr.address_string << " 时检测到）" << std::endl;
            plc_was_connected_.store(false);
        }
        // DEBUG_STREAM << "[DEBUG] readPLCWord: PLC 未连接，跳过读取 " << addr.address_string << std::endl;
        return false;
    }
    
    bool result = plc_comm_->readWord(addr, value);
    
    // 如果读取失败，可能是连接已断开，检查连接状态
    if (!result && plc_was_connected_.load()) {
        // 再次检查连接状态（可能在上次检查后断开）
        if (!plc_comm_->isConnected()) {
            WARN_STREAM << "PLC 连接断开（读取 " << addr.address_string << " 失败时检测到）" << std::endl;
            plc_was_connected_.store(false);
        }
    }
    
    // DEBUG_STREAM << "[DEBUG] readPLCWord: " << addr.address_string << " = " << value 
    //              << " (成功=" << (result ? "是" : "否") << ")" << std::endl;
    return result;
}

bool VacuumSystemDevice::writePLCBool(const Common::PLC::PLCAddress& addr, bool value) {
    std::lock_guard<std::mutex> lock(plc_mutex_);
    
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        // 检测连接状态变化：如果之前是连接的，现在断开了，记录日志
        if (plc_was_connected_.load()) {
            WARN_STREAM << "PLC 连接断开（在写入 " << addr.address_string << " 时检测到）" << std::endl;
            plc_was_connected_.store(false);
        }
        // DEBUG_STREAM << "[DEBUG] writePLCBool: PLC 未连接，跳过写入 " << addr.address_string << " = " << (value ? "true" : "false") << std::endl;
        return false;
    }
    
    bool result = plc_comm_->writeBool(addr, value);
    
    // 如果写入失败，可能是连接已断开，检查连接状态
    if (!result && plc_was_connected_.load()) {
        // 再次检查连接状态（可能在上次检查后断开）
        if (!plc_comm_->isConnected()) {
            WARN_STREAM << "PLC 连接断开（写入 " << addr.address_string << " 失败时检测到）" << std::endl;
            plc_was_connected_.store(false);
        }
    }
    
    // DEBUG_STREAM << "[DEBUG] writePLCBool: " << addr.address_string << " = " << (value ? "true" : "false") 
    //              << " (成功=" << (result ? "是" : "否") << ")" << std::endl;
    return result;
}

bool VacuumSystemDevice::writePLCWord(const Common::PLC::PLCAddress& addr, uint16_t value) {
    std::lock_guard<std::mutex> lock(plc_mutex_);
    
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        // 检测连接状态变化：如果之前是连接的，现在断开了，记录日志
        if (plc_was_connected_.load()) {
            WARN_STREAM << "PLC 连接断开（在写入 " << addr.address_string << " 时检测到）" << std::endl;
            plc_was_connected_.store(false);
        }
        // DEBUG_STREAM << "[DEBUG] writePLCWord: PLC 未连接，跳过写入 " << addr.address_string << " = " << value << std::endl;
        return false;
    }
    
    bool result = plc_comm_->writeWord(addr, value);
    
    // 如果写入失败，可能是连接已断开，检查连接状态
    if (!result && plc_was_connected_.load()) {
        // 再次检查连接状态（可能在上次检查后断开）
        if (!plc_comm_->isConnected()) {
            WARN_STREAM << "PLC 连接断开（写入 " << addr.address_string << " 失败时检测到）" << std::endl;
            plc_was_connected_.store(false);
        }
    }
    
    // DEBUG_STREAM << "[DEBUG] writePLCWord: " << addr.address_string << " = " << value 
    //              << " (成功=" << (result ? "是" : "否") << ")" << std::endl;
    return result;
}

// ============================================================================
// 状态轮询
// ============================================================================

void VacuumSystemDevice::synchronizeStateFromPLC() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    // 更新一次基础状态
    updatePumpStatus();
    updateValveStatus();
    
    // 同步分子泵启用配置到PLC（确保PLC配置与设备端一致）
    if (!sim_mode_ && plc_comm_ && plc_comm_->isConnected()) {
        writePLCBool(VacuumSystemPLCMapping::MolecularPump1Enabled(), molecular_pump1_enabled_);
        writePLCBool(VacuumSystemPLCMapping::MolecularPump2Enabled(), molecular_pump2_enabled_);
        writePLCBool(VacuumSystemPLCMapping::MolecularPump3Enabled(), molecular_pump3_enabled_);
        DEBUG_STREAM << "[DEBUG] synchronizeStateFromPLC: 已同步分子泵启用配置到PLC" << std::endl;
    }
    
    // 推断系统状态
    if (screw_pump_power_ || roots_pump_power_ || 
        molecular_pump1_power_ || molecular_pump2_power_ || molecular_pump3_power_) {
        system_state_ = SystemState::PUMPING;
        INFO_STREAM << "同步状态: 检测到泵运行，系统状态设为 PUMPING" << std::endl;
    } else if (vent_valve1_open_ || vent_valve2_open_) {
        system_state_ = SystemState::VENTING;
        INFO_STREAM << "同步状态: 检测到放气阀开启，系统状态设为 VENTING" << std::endl;
    } else {
        system_state_ = SystemState::IDLE;
        INFO_STREAM << "同步状态: 系统处于 IDLE" << std::endl;
    }
}

void VacuumSystemDevice::pollPLCStatus() {
    // ============================================================
    // 模拟模式：运行模拟逻辑，不访问真实 PLC
    // ============================================================
    if (sim_mode_) {
        // DEBUG_STREAM << "[DEBUG] pollPLCStatus: 模拟模式，运行模拟逻辑" << std::endl;
        runSimulation();
        return;
    }
    
    // ============================================================
    // 正常模式：从 PLC 读取状态
    // ============================================================
    
    // 如果正在进行连接尝试，跳过本次轮询
    if (plc_connecting_.load()) {
        // DEBUG_STREAM << "[DEBUG] pollPLCStatus: PLC 正在连接中，跳过本次轮询" << std::endl;
        return;
    }
    
    // 检查PLC连接状态
    bool currently_connected = (plc_comm_ && plc_comm_->isConnected());
    
    // 检测连接状态变化
    bool was_connected = plc_was_connected_.load();
    if (was_connected && !currently_connected) {
        // 连接断开：从已连接变为未连接
        WARN_STREAM << "PLC 连接断开（在轮询中检测到）" << std::endl;
        plc_was_connected_.store(false);
    } else if (!was_connected && currently_connected) {
        // 连接恢复：从未连接变为已连接
        INFO_STREAM << "PLC 连接已恢复" << std::endl;
        plc_was_connected_.store(true);
        // 连接恢复后，同步一次状态
        synchronizeStateFromPLC();
    }
    
    if (!currently_connected) {
        // PLC 未连接，尝试重连
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_connect_attempt_).count();
        
        // 每 PLC_RECONNECT_INTERVAL_SEC 秒才尝试重连一次
        if (elapsed >= PLC_RECONNECT_INTERVAL_SEC) {
            DEBUG_STREAM << "[DEBUG] pollPLCStatus: PLC 未连接，尝试重连 (距离上次尝试 " << elapsed << " 秒)" << std::endl;
            last_connect_attempt_ = now;
            
            // 在后台线程中进行连接尝试，不阻塞轮询
            plc_connecting_.store(true);
            std::thread([this]() {
                try {
                    if (connectPLC()) {
                        INFO_STREAM << "PLC 重连成功" << std::endl;
                        plc_was_connected_.store(true);
                        // 重连成功后，同步一次状态
                        synchronizeStateFromPLC();
                    } else {
                        WARN_STREAM << "PLC 重连失败，将在 " << PLC_RECONNECT_INTERVAL_SEC << " 秒后重试" << std::endl;
                    }
                } catch (const std::exception& e) {
                    ERROR_STREAM << "PLC 连接异常: " << e.what() << std::endl;
                    plc_was_connected_.store(false);
                } catch (...) {
                    ERROR_STREAM << "PLC 连接异常: 未知异常" << std::endl;
                    plc_was_connected_.store(false);
                }
                plc_connecting_.store(false);
            }).detach();
        } else {
            // DEBUG_STREAM << "[DEBUG] pollPLCStatus: PLC 未连接，等待重连间隔 (还需等待 " << (PLC_RECONNECT_INTERVAL_SEC - elapsed) << " 秒)" << std::endl;
        }
        return;
    }
    
    // 更新连接状态标志（确保一致性）
    plc_was_connected_.store(true);
    
    // DEBUG_STREAM << "[DEBUG] pollPLCStatus: 开始更新状态 (模式=" 
    //              << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动") 
    //              << ", 状态=" << static_cast<int>(system_state_) << ")" << std::endl;
    
    updatePumpStatus();
    updateValveStatus();
    updateWaterValveStatus();  // 更新水电磁阀和气主阀
    updateSensorReadings();
    checkValveTimeouts();
    checkAlarmConditions();
    
    // 状态机处理（自动模式和手动模式都支持停机、放气流程）
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        switch (system_state_) {
            case SystemState::PUMPING:
                // 抽真空流程仅在自动模式下执行
                if (operation_mode_ == OperationMode::AUTO) {
                    // DEBUG_STREAM << "[DEBUG] pollPLCStatus: 自动模式，处理抽真空流程 (步骤=" << auto_sequence_step_ << ")" << std::endl;
                    processAutoVacuumSequence();
                }
                break;
            case SystemState::STOPPING:
                // 停机流程在自动和手动模式下都支持
                // DEBUG_STREAM << "[DEBUG] pollPLCStatus: " 
                //            << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动") 
                //            << "模式，处理停机流程 (步骤=" << auto_sequence_step_ << ")" << std::endl;
                processAutoStopSequence();
                break;
            case SystemState::VENTING:
                // 放气流程在自动和手动模式下都支持
                // DEBUG_STREAM << "[DEBUG] pollPLCStatus: " 
                //            << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动") 
                //            << "模式，处理放气流程 (步骤=" << auto_sequence_step_ << ")" << std::endl;
                processVentSequence();
                break;
            default:
                break;
        }
    }
}

void VacuumSystemDevice::updatePumpStatus() {
    // DEBUG_STREAM << "[DEBUG] updatePumpStatus: 开始更新泵状态" << std::endl;
    
    // 读取泵状态
    bool old_screw = screw_pump_power_;
    bool old_roots = roots_pump_power_;
    bool old_mp1 = molecular_pump1_power_;
    bool old_mp2 = molecular_pump2_power_;
    bool old_mp3 = molecular_pump3_power_;
    
    readPLCBool(VacuumSystemPLCMapping::ScrewPumpPowerFeedback(), screw_pump_power_);
    readPLCBool(VacuumSystemPLCMapping::RootsPumpPowerFeedback(), roots_pump_power_);
    readPLCBool(VacuumSystemPLCMapping::MolecularPump1PowerFeedback(), molecular_pump1_power_);
    readPLCBool(VacuumSystemPLCMapping::MolecularPump2PowerFeedback(), molecular_pump2_power_);
    readPLCBool(VacuumSystemPLCMapping::MolecularPump3PowerFeedback(), molecular_pump3_power_);
    
    // 记录状态变化
    if (old_screw != screw_pump_power_) {
        DEBUG_STREAM << "[DEBUG] 螺杆泵状态变化: " << (old_screw ? "运行" : "停止") 
                     << " -> " << (screw_pump_power_ ? "运行" : "停止") << std::endl;
    }
    if (old_roots != roots_pump_power_) {
        DEBUG_STREAM << "[DEBUG] 罗茨泵状态变化: " << (old_roots ? "运行" : "停止") 
                     << " -> " << (roots_pump_power_ ? "运行" : "停止") << std::endl;
    }
    if (old_mp1 != molecular_pump1_power_) {
        DEBUG_STREAM << "[DEBUG] 分子泵1状态变化: " << (old_mp1 ? "运行" : "停止") 
                     << " -> " << (molecular_pump1_power_ ? "运行" : "停止") << std::endl;
    }
    if (old_mp2 != molecular_pump2_power_) {
        DEBUG_STREAM << "[DEBUG] 分子泵2状态变化: " << (old_mp2 ? "运行" : "停止") 
                     << " -> " << (molecular_pump2_power_ ? "运行" : "停止") << std::endl;
    }
    if (old_mp3 != molecular_pump3_power_) {
        DEBUG_STREAM << "[DEBUG] 分子泵3状态变化: " << (old_mp3 ? "运行" : "停止") 
                     << " -> " << (molecular_pump3_power_ ? "运行" : "停止") << std::endl;
    }
    
    // 读取分子泵转速
    uint16_t speed;
    if (readPLCWord(VacuumSystemPLCMapping::MolecularPump1Speed(), speed)) {
        if (molecular_pump1_speed_ != speed) {
            DEBUG_STREAM << "[DEBUG] 分子泵1转速变化: " << molecular_pump1_speed_ << " -> " << speed << " RPM" << std::endl;
        }
        molecular_pump1_speed_ = speed;
    }
    if (readPLCWord(VacuumSystemPLCMapping::MolecularPump2Speed(), speed)) {
        if (molecular_pump2_speed_ != speed) {
            DEBUG_STREAM << "[DEBUG] 分子泵2转速变化: " << molecular_pump2_speed_ << " -> " << speed << " RPM" << std::endl;
        }
        molecular_pump2_speed_ = speed;
    }
    if (readPLCWord(VacuumSystemPLCMapping::MolecularPump3Speed(), speed)) {
        if (molecular_pump3_speed_ != speed) {
            DEBUG_STREAM << "[DEBUG] 分子泵3转速变化: " << molecular_pump3_speed_ << " -> " << speed << " RPM" << std::endl;
        }
        molecular_pump3_speed_ = speed;
    }
    
    // 读取螺杆泵频率 (如果有专门的变频器反馈地址，需要添加到PLC映射)
    // 暂时使用模拟值：运行时为110Hz，否则为0
    screw_pump_frequency_ = screw_pump_power_ ? 110 : 0;
    
    // 读取罗茨泵频率 (如果有专门的变频器反馈地址，需要添加到PLC映射)
    // 暂时使用模拟值：运行时为50Hz，否则为0
    roots_pump_frequency_ = roots_pump_power_ ? 50 : 0;
}

void VacuumSystemDevice::updateValveStatus() {
    // 闸板阀
    readPLCBool(VacuumSystemPLCMapping::GateValve1OpenFeedback(), gate_valve1_open_);
    readPLCBool(VacuumSystemPLCMapping::GateValve1CloseFeedback(), gate_valve1_close_);
    readPLCBool(VacuumSystemPLCMapping::GateValve2OpenFeedback(), gate_valve2_open_);
    readPLCBool(VacuumSystemPLCMapping::GateValve2CloseFeedback(), gate_valve2_close_);
    readPLCBool(VacuumSystemPLCMapping::GateValve3OpenFeedback(), gate_valve3_open_);
    readPLCBool(VacuumSystemPLCMapping::GateValve3CloseFeedback(), gate_valve3_close_);
    readPLCBool(VacuumSystemPLCMapping::GateValve4OpenFeedback(), gate_valve4_open_);
    readPLCBool(VacuumSystemPLCMapping::GateValve4CloseFeedback(), gate_valve4_close_);
    readPLCBool(VacuumSystemPLCMapping::GateValve5OpenFeedback(), gate_valve5_open_);
    readPLCBool(VacuumSystemPLCMapping::GateValve5CloseFeedback(), gate_valve5_close_);
    
    // 电磁阀
    readPLCBool(VacuumSystemPLCMapping::ElectromagneticValve1OpenFeedback(), electromagnetic_valve1_open_);
    readPLCBool(VacuumSystemPLCMapping::ElectromagneticValve1CloseFeedback(), electromagnetic_valve1_close_);
    readPLCBool(VacuumSystemPLCMapping::ElectromagneticValve2OpenFeedback(), electromagnetic_valve2_open_);
    readPLCBool(VacuumSystemPLCMapping::ElectromagneticValve2CloseFeedback(), electromagnetic_valve2_close_);
    readPLCBool(VacuumSystemPLCMapping::ElectromagneticValve3OpenFeedback(), electromagnetic_valve3_open_);
    readPLCBool(VacuumSystemPLCMapping::ElectromagneticValve3CloseFeedback(), electromagnetic_valve3_close_);
    readPLCBool(VacuumSystemPLCMapping::ElectromagneticValve4OpenFeedback(), electromagnetic_valve4_open_);
    readPLCBool(VacuumSystemPLCMapping::ElectromagneticValve4CloseFeedback(), electromagnetic_valve4_close_);
    
    // 放气阀
    readPLCBool(VacuumSystemPLCMapping::VentValve1OpenFeedback(), vent_valve1_open_);
    readPLCBool(VacuumSystemPLCMapping::VentValve1CloseFeedback(), vent_valve1_close_);
    readPLCBool(VacuumSystemPLCMapping::VentValve2OpenFeedback(), vent_valve2_open_);
    readPLCBool(VacuumSystemPLCMapping::VentValve2CloseFeedback(), vent_valve2_close_);
    
    // 更新阀门动作状态
    updateValveAction("GateValve1", gate_valve1_open_, gate_valve1_close_);
    updateValveAction("GateValve2", gate_valve2_open_, gate_valve2_close_);
    updateValveAction("GateValve3", gate_valve3_open_, gate_valve3_close_);
    updateValveAction("GateValve4", gate_valve4_open_, gate_valve4_close_);
    updateValveAction("GateValve5", gate_valve5_open_, gate_valve5_close_);
}

void VacuumSystemDevice::updateSensorReadings() {
    uint16_t raw_value;
    
    // 电阻规 -> 真空计1 (电压信号，需要转换)
    if (readPLCWord(VacuumSystemPLCMapping::ResistanceGaugeVoltage(), raw_value)) {
        // 电压转换公式 (根据实际传感器特性调整)
        double voltage = raw_value * 10.0 / 32767.0;  // 假设 0-10V
        vacuum_gauge1_ = pow(10, voltage - 5.0);  // 示例转换

        // 当前 PLC 点位映射仅提供一个真空计模拟量输入（%IW130）。
        // 为保证上层流程（使用 vacuum_gauge2_ 作为“腔室真空计”）在 OPC-UA 模拟联调时能正常变化，
        // 这里将真空计2/3在非模拟模式下与真空计1保持一致。
        vacuum_gauge2_ = vacuum_gauge1_;
        vacuum_gauge3_ = vacuum_gauge1_;
    }
    
    // 气压传感器 (电流信号)
    if (readPLCWord(VacuumSystemPLCMapping::AirPressureSensorCurrent(), raw_value)) {
        // 4-20mA 转换
        double current = 4.0 + (raw_value / 32767.0) * 16.0;  // mA
        air_pressure_ = (current - 4.0) / 16.0;  // 0-1 MPa
    }
}

void VacuumSystemDevice::checkValveTimeouts() {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : valve_trackers_) {
        auto& tracker = pair.second;
        if (tracker.state == ValveActionState::OPENING || 
            tracker.state == ValveActionState::CLOSING) {
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - tracker.start_time).count();
            
            if (elapsed > VALVE_TIMEOUT_MS) {
                if (tracker.state == ValveActionState::OPENING) {
                    tracker.state = ValveActionState::OPEN_TIMEOUT;
                    raiseAlarm(static_cast<int>(AlarmType::GATE_VALVE_1_OPEN_TIMEOUT) + tracker.valve_index,
                              "VALVE_TIMEOUT", 
                              pair.first + " 开到位超时",
                              pair.first);
                } else {
                    tracker.state = ValveActionState::CLOSE_TIMEOUT;
                    raiseAlarm(static_cast<int>(AlarmType::GATE_VALVE_1_CLOSE_TIMEOUT) + tracker.valve_index,
                              "VALVE_TIMEOUT",
                              pair.first + " 关到位超时",
                              pair.first);
                }
            }
        }
    }
}

void VacuumSystemDevice::checkAlarmConditions() {
    // 检查相序保护
    if (readPLCBool(VacuumSystemPLCMapping::PhaseSequenceProtection(), phase_sequence_ok_)) {
        if (!phase_sequence_ok_) {
            raiseAlarm(static_cast<int>(AlarmType::PHASE_SEQUENCE_FAULT),
                      "SYSTEM", "主电源相序异常", "电源系统");
        } else {
            clearAlarm(static_cast<int>(AlarmType::PHASE_SEQUENCE_FAULT));
        }
    }
    
    // 读取运动控制系统联锁信号
    readPLCBool(VacuumSystemPLCMapping::MotionControlSystemOnline(), motion_system_online_);
    readPLCBool(VacuumSystemPLCMapping::GateValve5ActionPermit(), gate_valve5_permit_);
    readPLCBool(VacuumSystemPLCMapping::MotionControlRequestOpenGateValve5(), motion_req_open_gv5_);
    readPLCBool(VacuumSystemPLCMapping::MotionControlRequestCloseGateValve5(), motion_req_close_gv5_);
    
    // 气源压力检查
    if (air_pressure_ < 0.4) {
        raiseAlarm(static_cast<int>(AlarmType::AIR_PRESSURE_LOW),
                  "SYSTEM", "气源压力不足 (<0.4MPa)", "气源");
    } else {
        clearAlarm(static_cast<int>(AlarmType::AIR_PRESSURE_LOW));
    }
}

/**
 * @brief 更新水电磁阀和气主电磁阀状态
 * 
 * 读取水电磁阀1-6和气主电磁阀的输出状态
 */
void VacuumSystemDevice::updateWaterValveStatus() {
    // 注意：这些是输出状态，从PLC读取当前输出值
    // 水电磁阀 %Q12.0-%Q12.5
    readPLCBool(VacuumSystemPLCMapping::WaterValve1Output(), water_valve1_state_);
    readPLCBool(VacuumSystemPLCMapping::WaterValve2Output(), water_valve2_state_);
    readPLCBool(VacuumSystemPLCMapping::WaterValve3Output(), water_valve3_state_);
    readPLCBool(VacuumSystemPLCMapping::WaterValve4Output(), water_valve4_state_);
    readPLCBool(VacuumSystemPLCMapping::WaterValve5Output(), water_valve5_state_);
    readPLCBool(VacuumSystemPLCMapping::WaterValve6Output(), water_valve6_state_);
    
    // 气主电磁阀 %Q12.6
    readPLCBool(VacuumSystemPLCMapping::AirMainValveOutput(), air_main_valve_state_);
}

// ============================================================================
// 阀门动作跟踪
// ============================================================================

void VacuumSystemDevice::startValveAction(const std::string& valve_id, bool target_open) {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    
    ValveActionTracker tracker;
    tracker.target_open = target_open;
    tracker.start_time = std::chrono::steady_clock::now();
    tracker.state = target_open ? ValveActionState::OPENING : ValveActionState::CLOSING;
    
    valve_trackers_[valve_id] = tracker;
}

void VacuumSystemDevice::updateValveAction(const std::string& valve_id, 
                                           bool current_open, bool current_close) {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    
    auto it = valve_trackers_.find(valve_id);
    if (it == valve_trackers_.end()) return;
    
    auto& tracker = it->second;
    
    if (tracker.state == ValveActionState::OPENING && current_open) {
        tracker.state = ValveActionState::IDLE;
    } else if (tracker.state == ValveActionState::CLOSING && current_close) {
        tracker.state = ValveActionState::IDLE;
    }
}

ValveActionState VacuumSystemDevice::getValveActionState(const std::string& valve_id) {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    
    auto it = valve_trackers_.find(valve_id);
    if (it == valve_trackers_.end()) return ValveActionState::IDLE;
    
    return it->second.state;
}

// ============================================================================
// 报警管理
// ============================================================================

void VacuumSystemDevice::raiseAlarm(int code, const std::string& type,
                                    const std::string& desc, const std::string& device) {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    
    // 检查是否已存在
    for (const auto& alarm : active_alarms_) {
        if (alarm.alarm_code == code) {
            DEBUG_STREAM << "[DEBUG] raiseAlarm: 报警 " << code << " 已存在，跳过" << std::endl;
            return;
        }
    }
    
    DEBUG_STREAM << "[DEBUG] raiseAlarm: 触发新报警 (code=" << code << ", type=" << type 
                 << ", desc=" << desc << ", device=" << device << ")" << std::endl;
    
    AlarmInfo alarm(code, type, desc, device);
    active_alarms_.push_back(alarm);
    alarm_history_.push_back(alarm);
    
    saveAlarmToFile(alarm);
    pushAlarmEvent(alarm);
    
    logEvent("报警触发: " + desc);
    
    // 更新设备状态
    if (system_state_ != SystemState::FAULT) {
        DEBUG_STREAM << "[DEBUG] raiseAlarm: 更新设备状态为 ALARM" << std::endl;
        set_state(Tango::ALARM);
    }
}

void VacuumSystemDevice::clearAlarm(int code) {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    
    active_alarms_.erase(
        std::remove_if(active_alarms_.begin(), active_alarms_.end(),
            [code](const AlarmInfo& a) { return a.alarm_code == code; }),
        active_alarms_.end()
    );
    
    if (active_alarms_.empty()) {
        set_state(Tango::ON);
    }
}

void VacuumSystemDevice::saveAlarmToFile(const AlarmInfo& alarm) {
    try {
        json j;
        j["alarm_code"] = alarm.alarm_code;
        j["alarm_type"] = alarm.alarm_type;
        j["description"] = alarm.description;
        j["device_name"] = alarm.device_name;
        j["timestamp"] = getCurrentTimestamp();
        j["acknowledged"] = alarm.acknowledged;
        
        // 读取现有文件
        json all_alarms = json::array();
        std::ifstream ifs(alarm_log_path_);
        if (ifs.is_open()) {
            try {
                ifs >> all_alarms;
            } catch (...) {
                all_alarms = json::array();
            }
            ifs.close();
        }
        
        all_alarms.push_back(j);
        
        // 写回文件
        std::ofstream ofs(alarm_log_path_);
        if (ofs.is_open()) {
            ofs << all_alarms.dump(2);
            ofs.close();
        }
    } catch (const std::exception& e) {
        ERROR_STREAM << "保存报警记录失败: " << e.what() << std::endl;
    }
}

void VacuumSystemDevice::pushAlarmEvent(const AlarmInfo& alarm) {
    // 推送 Tango 事件
    try {
        json j;
        j["alarm_code"] = alarm.alarm_code;
        j["alarm_type"] = alarm.alarm_type;
        j["description"] = alarm.description;
        j["device_name"] = alarm.device_name;
        j["timestamp"] = getCurrentTimestamp();
        
        // 通过属性事件推送
        std::string json_str = j.dump();
        Tango::DevString val = const_cast<char*>(json_str.c_str());
        push_change_event("latestAlarmJson", &val);
    } catch (const std::exception& e) {
        ERROR_STREAM << "推送报警事件失败: " << e.what() << std::endl;
    }
}

// ============================================================================
// Tango 命令实现
// ============================================================================

void VacuumSystemDevice::Init() {
    INFO_STREAM << "VacuumSystemDevice::Init()" << std::endl;
    init_device();
}

void VacuumSystemDevice::Reset() {
    INFO_STREAM << "VacuumSystemDevice::Reset()" << std::endl;
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    system_state_ = SystemState::IDLE;
    auto_sequence_step_ = 0;
    
    set_state(Tango::ON);
    logEvent("系统复位");
}

void VacuumSystemDevice::SelfCheck() {
    INFO_STREAM << "执行自检..." << std::endl;
    
    // 检查 PLC 连接
    if (!plc_comm_ || !plc_comm_->isConnected()) {
        Tango::Except::throw_exception(
            "SELF_CHECK_FAILED",
            "PLC 连接异常",
            "VacuumSystemDevice::SelfCheck");
    }
    
    logEvent("自检完成");
}

void VacuumSystemDevice::SwitchToAuto() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!checkAutoModePrerequisites()) {
        Tango::Except::throw_exception(
            "MODE_SWITCH_FAILED",
            "自动模式先决条件不满足",
            "VacuumSystemDevice::SwitchToAuto");
    }
    
    operation_mode_ = OperationMode::AUTO;
    
    // 推送属性变化事件，通知客户端
    Tango::DevShort mode_val = static_cast<Tango::DevShort>(operation_mode_);
    push_change_event("operationMode", &mode_val);
    
    logEvent("切换至自动模式");
}

void VacuumSystemDevice::SwitchToManual() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    operation_mode_ = OperationMode::MANUAL;
    system_state_ = SystemState::IDLE;
    auto_sequence_step_ = 0;
    
    // 推送属性变化事件，通知客户端
    Tango::DevShort mode_val = static_cast<Tango::DevShort>(operation_mode_);
    push_change_event("operationMode", &mode_val);
    
    Tango::DevShort state_val = static_cast<Tango::DevShort>(system_state_);
    push_change_event("systemState", &state_val);
    
    Tango::DevLong step_val = static_cast<Tango::DevLong>(auto_sequence_step_);
    push_change_event("autoSequenceStep", &step_val);
    
    logEvent("切换至手动模式");
}

void VacuumSystemDevice::OneKeyVacuumStart() {
    DEBUG_STREAM << "[DEBUG] OneKeyVacuumStart: 调用，当前模式=" 
                 << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动") 
                 << ", 当前状态=" << static_cast<int>(system_state_) << std::endl;
    
    if (operation_mode_ != OperationMode::AUTO) {
        DEBUG_STREAM << "[DEBUG] OneKeyVacuumStart: 错误 - 非自动模式" << std::endl;
        Tango::Except::throw_exception(
            "NOT_AUTO_MODE",
            "当前不是自动模式",
            "VacuumSystemDevice::OneKeyVacuumStart");
    }
    
    if (system_state_ != SystemState::IDLE) {
        DEBUG_STREAM << "[DEBUG] OneKeyVacuumStart: 错误 - 系统正忙 (状态=" 
                     << static_cast<int>(system_state_) << ")" << std::endl;
        Tango::Except::throw_exception(
            "SYSTEM_BUSY",
            "系统正忙",
            "VacuumSystemDevice::OneKeyVacuumStart");
    }
    
    // 在判断前先更新传感器读数，确保使用最新的真空度值
    // 在模拟模式下，需要先更新真空度模拟
    if (sim_mode_) {
        simulateVacuumPhysics();
    } else {
        updateSensorReadings();
    }
    
    // 判断当前真空度，选择执行流程
    // 使用vacuum_gauge2_（主真空计1，腔室真空计）作为判断依据
    // 非真空状态：≥3000Pa，使用步骤1-10
    // 低真空状态：<3000Pa，使用步骤100-114
    // 注意：保存初始状态，在整个流程中保持不变，避免流程执行过程中真空度变化导致步骤号不一致
    double current_vacuum = vacuum_gauge2_;  // 腔室真空度
    bool is_low_vacuum = (current_vacuum < 3000.0);
    int start_step = is_low_vacuum ? 100 : 1;
    std::string flow_type = is_low_vacuum ? "低真空状态" : "非真空状态";
    
    DEBUG_STREAM << "[DEBUG] OneKeyVacuumStart: 判断流程类型 - 当前真空度G2=" << current_vacuum 
                 << "Pa, 判断条件: " << current_vacuum << (is_low_vacuum ? " < " : " >= ") 
                 << "3000Pa, 结果=" << (is_low_vacuum ? "低真空" : "非真空") 
                 << ", 起始步骤=" << start_step << std::endl;
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    system_state_ = SystemState::PUMPING;
    auto_sequence_step_ = start_step;
    vacuum_sequence_is_low_vacuum_ = is_low_vacuum;  // 保存流程类型，在整个流程中保持不变
    auto_step_start_time_ = std::chrono::steady_clock::now();
    
    DEBUG_STREAM << "[DEBUG] OneKeyVacuumStart: 启动成功，状态=PUMPING, 流程类型=" 
                 << flow_type << ", 当前真空度=" << current_vacuum << "Pa, 步骤=" << start_step 
                 << ", 流程类型已保存=" << (vacuum_sequence_is_low_vacuum_ ? "低真空" : "非真空") << std::endl;
    logEvent("一键抽真空启动 - " + flow_type + "流程");
}

void VacuumSystemDevice::OneKeyVacuumStop() {
    DEBUG_STREAM << "[DEBUG] OneKeyVacuumStop: 调用，当前模式=" 
                 << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动") 
                 << ", 当前状态=" << static_cast<int>(system_state_) << std::endl;
    
    // 检查系统状态：只有在抽真空、停机中或空闲状态下才能执行停机
    // 如果已经在停机中，忽略重复调用
    // 如果系统处于故障或急停状态，不允许执行停机（应使用故障复位或紧急停止）
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (system_state_ == SystemState::STOPPING) {
        DEBUG_STREAM << "[DEBUG] OneKeyVacuumStop: 系统已在停机中，忽略重复调用" << std::endl;
        return;  // 已在停机中，静默返回
    }
    
    if (system_state_ == SystemState::FAULT || system_state_ == SystemState::EMERGENCY_STOP) {
        DEBUG_STREAM << "[DEBUG] OneKeyVacuumStop: 错误 - 系统处于故障或急停状态 (状态=" 
                     << static_cast<int>(system_state_) << ")" << std::endl;
        Tango::Except::throw_exception(
            "INVALID_STATE",
            "系统处于故障或急停状态，无法执行停机。请先执行故障复位或紧急停止。",
            "VacuumSystemDevice::OneKeyVacuumStop");
    }
    
    // 检查是否有泵在运行（只有在有设备运行时才需要停机）
    bool has_running_pumps = screw_pump_power_ || roots_pump_power_ || 
                             molecular_pump1_power_ || molecular_pump2_power_ || molecular_pump3_power_;
    
    if (!has_running_pumps && system_state_ == SystemState::IDLE) {
        DEBUG_STREAM << "[DEBUG] OneKeyVacuumStop: 警告 - 系统已处于空闲状态，无设备运行" << std::endl;
        // 不抛出异常，允许执行（可能用户想确保所有阀门关闭）
        INFO_STREAM << "一键停机：系统已空闲，将执行阀门关闭检查" << std::endl;
    }
    
    // 移除模式限制：一键停机功能在自动和手动模式下均可使用
    // 在手动模式下，也会执行停机流程以确保安全停机
    
    system_state_ = SystemState::STOPPING;
    auto_sequence_step_ = 1;
    vacuum_sequence_is_low_vacuum_ = false;  // 重置流程类型标志，停机后下次启动时重新判断
    auto_step_start_time_ = std::chrono::steady_clock::now();
    
    // 推送状态变化事件
    Tango::DevShort state_val = static_cast<Tango::DevShort>(SystemState::STOPPING);
    push_change_event("systemState", &state_val);
    Tango::DevLong step_val = 1;
    push_change_event("autoSequenceStep", &step_val);
    
    DEBUG_STREAM << "[DEBUG] OneKeyVacuumStop: 停机流程已启动，流程类型标志已重置" << std::endl;
    logEvent("一键停机启动");
}

void VacuumSystemDevice::ChamberVent() {
    DEBUG_STREAM << "[DEBUG] ChamberVent: 调用，当前模式=" 
                 << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动") << std::endl;
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    system_state_ = SystemState::VENTING;
    auto_sequence_step_ = 1;
    vacuum_sequence_is_low_vacuum_ = false;  // 重置流程类型标志，放气后下次启动时重新判断
    auto_step_start_time_ = std::chrono::steady_clock::now();
    
    DEBUG_STREAM << "[DEBUG] ChamberVent: 状态已设置为 VENTING, 步骤=1" << std::endl;
    logEvent("腔室放气启动");
}

void VacuumSystemDevice::FaultReset() {
    INFO_STREAM << "VacuumSystemDevice::FaultReset - 故障复位执行" << std::endl;
    
    // 螺杆泵故障复位
    if (!sim_mode_) {
        writePLCBool(VacuumSystemPLCMapping::ScrewPumpFaultReset(), true);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        writePLCBool(VacuumSystemPLCMapping::ScrewPumpFaultReset(), false);
    }
    
    // 清除所有报警
    AcknowledgeAllAlarms();
    
    // 清除紧急停止状态和故障状态，恢复到空闲状态
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        SystemState old_state = system_state_;
        if (system_state_ == SystemState::EMERGENCY_STOP || system_state_ == SystemState::FAULT) {
            system_state_ = SystemState::IDLE;
            auto_sequence_step_ = 0;
            vacuum_sequence_is_low_vacuum_ = false;  // 重置流程类型标志，故障复位后下次启动时重新判断
            
            // 推送状态变化事件
            Tango::DevShort state_val = static_cast<Tango::DevShort>(SystemState::IDLE);
            push_change_event("systemState", &state_val);
            Tango::DevLong step_val = 0;
            push_change_event("autoSequenceStep", &step_val);
            
            INFO_STREAM << "故障复位：系统状态从 " 
                       << (old_state == SystemState::EMERGENCY_STOP ? "紧急停止" : "故障")
                       << " 恢复到空闲状态" << std::endl;
        }
    }
    
    // 恢复设备状态为正常
    set_state(Tango::ON);
    
    logEvent("故障复位完成 - 系统已恢复正常");
}

void VacuumSystemDevice::EmergencyStop() {
    INFO_STREAM << "VacuumSystemDevice::EmergencyStop - 紧急停止执行" << std::endl;
    
    // 设置系统状态为紧急停止（需要持有锁）
    {
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        system_state_ = SystemState::EMERGENCY_STOP;
        auto_sequence_step_ = 0;
        operation_mode_ = OperationMode::MANUAL;
    }
    
    // 推送状态变化事件
    Tango::DevShort state_val = static_cast<Tango::DevShort>(SystemState::EMERGENCY_STOP);
    push_change_event("systemState", &state_val);
    Tango::DevShort mode_val = static_cast<Tango::DevShort>(OperationMode::MANUAL);
    push_change_event("operationMode", &mode_val);
    Tango::DevLong step_val = 0;
    push_change_event("autoSequenceStep", &step_val);
    
    // 关闭所有泵
    INFO_STREAM << "紧急停止：关闭所有泵" << std::endl;
    ctrlScrewPump(false);
    ctrlRootsPump(false);
    ctrlMolecularPump(1, false);
    ctrlMolecularPump(2, false);
    ctrlMolecularPump(3, false);
    
    // 关闭所有闸板阀
    INFO_STREAM << "紧急停止：关闭所有闸板阀" << std::endl;
    if (sim_mode_) {
        // 模拟模式：直接设置状态为关闭，不使用跟踪器
        gate_valve1_open_ = false; gate_valve1_close_ = true;
        gate_valve2_open_ = false; gate_valve2_close_ = true;
        gate_valve3_open_ = false; gate_valve3_close_ = true;
        gate_valve4_open_ = false; gate_valve4_close_ = true;
        gate_valve5_open_ = false; gate_valve5_close_ = true;
    } else {
        // 正常模式：发送PLC命令
        for (int i = 1; i <= 5; i++) {
            ctrlGateValve(i, false);
        }
    }
    
    // 关闭所有电磁阀
    INFO_STREAM << "紧急停止：关闭所有电磁阀" << std::endl;
    for (int i = 1; i <= 4; i++) {
        ctrlElectromagneticValve(i, false);
    }
    
    // 关闭所有放气阀
    INFO_STREAM << "紧急停止：关闭所有放气阀" << std::endl;
    for (int i = 1; i <= 2; i++) {
        ctrlVentValve(i, false);
    }
    
    // 清除阀门动作跟踪器
    {
        std::lock_guard<std::mutex> tracker_lock(tracker_mutex_);
        valve_trackers_.clear();
    }
    
    // 设置设备状态为报警
    set_state(Tango::ALARM);
    
    logEvent("紧急停止已执行 - 所有设备已关闭");
    INFO_STREAM << "紧急停止执行完成" << std::endl;
}

// ============================================================================
// 泵控制命令 (手动模式)
// ============================================================================

void VacuumSystemDevice::SetScrewPumpPower(Tango::DevBoolean state) {
    DEBUG_STREAM << "[DEBUG] SetScrewPumpPower: 调用，参数=" << (state ? "true" : "false") 
                 << ", 当前模式=" << (operation_mode_ == OperationMode::MANUAL ? "手动" : "自动") << std::endl;
    
    if (operation_mode_ != OperationMode::MANUAL) {
        DEBUG_STREAM << "[DEBUG] SetScrewPumpPower: 错误 - 非手动模式" << std::endl;
        Tango::Except::throw_exception(
            "NOT_MANUAL_MODE",
            "手动模式才能操作",
            "VacuumSystemDevice::SetScrewPumpPower");
    }
    
    if (state && !checkManualOperationAllowed("ScrewPump", "Start")) {
        DEBUG_STREAM << "[DEBUG] SetScrewPumpPower: 错误 - 先决条件不满足" << std::endl;
        Tango::Except::throw_exception(
            "PRECONDITION_FAILED",
            getPrerequisiteStatus("ScrewPump", "Start"),
            "VacuumSystemDevice::SetScrewPumpPower");
    }
    
    if (sim_mode_) {
        DEBUG_STREAM << "[DEBUG] SetScrewPumpPower: 模拟模式，直接设置状态" << std::endl;
        screw_pump_power_ = state;
    } else {
        DEBUG_STREAM << "[DEBUG] SetScrewPumpPower: 正常模式，写入PLC" << std::endl;
        writePLCBool(VacuumSystemPLCMapping::ScrewPumpPowerOutput(), state);
    }
    logEvent(state ? "螺杆泵上电" : "螺杆泵断电");
}

void VacuumSystemDevice::SetScrewPumpStartStop(Tango::DevBoolean state) {
    if (operation_mode_ != OperationMode::MANUAL) {
        Tango::Except::throw_exception(
            "NOT_MANUAL_MODE",
            "手动模式才能操作",
            "VacuumSystemDevice::SetScrewPumpStartStop");
    }
    
    if (sim_mode_) {
        // 模拟模式下，启停信号直接影响电源状态
        screw_pump_power_ = state;
    } else {
        writePLCBool(VacuumSystemPLCMapping::ScrewPumpStartStop(), state);
    }
    logEvent(state ? "螺杆泵启动" : "螺杆泵停止");
}

void VacuumSystemDevice::SetRootsPumpPower(Tango::DevBoolean state) {
    if (operation_mode_ != OperationMode::MANUAL) {
        Tango::Except::throw_exception(
            "NOT_MANUAL_MODE",
            "手动模式才能操作",
            "VacuumSystemDevice::SetRootsPumpPower");
    }
    
    if (state && !checkManualOperationAllowed("RootsPump", "Start")) {
        Tango::Except::throw_exception(
            "PRECONDITION_FAILED",
            getPrerequisiteStatus("RootsPump", "Start"),
            "VacuumSystemDevice::SetRootsPumpPower");
    }
    
    if (sim_mode_) {
        roots_pump_power_ = state;
    } else {
        writePLCBool(VacuumSystemPLCMapping::RootsPumpPowerOutput(), state);
    }
    logEvent(state ? "罗茨泵上电" : "罗茨泵断电");
}

void VacuumSystemDevice::SetMolecularPumpPower(const Tango::DevVarShortArray* argin) {
    if (argin->length() < 2) {
        Tango::Except::throw_exception(
            "INVALID_ARGUMENT",
            "需要 [index, state]",
            "VacuumSystemDevice::SetMolecularPumpPower");
    }
    
    int index = (*argin)[0];
    bool state = (*argin)[1] != 0;
    
    if (index < 1 || index > 3) {
        Tango::Except::throw_exception(
            "INVALID_INDEX",
            "分子泵索引 1-3",
            "VacuumSystemDevice::SetMolecularPumpPower");
    }
    
    if (sim_mode_) {
        // 模拟模式：直接修改内部状态
        switch (index) {
            case 1: molecular_pump1_power_ = state; break;
            case 2: molecular_pump2_power_ = state; break;
            case 3: molecular_pump3_power_ = state; break;
        }
    } else {
        Common::PLC::PLCAddress addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1: addr = VacuumSystemPLCMapping::MolecularPump1PowerOutput(); break;
            case 2: addr = VacuumSystemPLCMapping::MolecularPump2PowerOutput(); break;
            case 3: addr = VacuumSystemPLCMapping::MolecularPump3PowerOutput(); break;
        }
        writePLCBool(addr, state);
    }
    logEvent("分子泵" + std::to_string(index) + (state ? "上电" : "断电"));
}

void VacuumSystemDevice::SetMolecularPumpStartStop(const Tango::DevVarShortArray* argin) {
    if (argin->length() < 2) {
        Tango::Except::throw_exception(
            "INVALID_ARGUMENT",
            "需要 [index, state]",
            "VacuumSystemDevice::SetMolecularPumpStartStop");
    }
    
    int index = (*argin)[0];
    bool state = (*argin)[1] != 0;
    
    if (index < 1 || index > 3) {
        Tango::Except::throw_exception(
            "INVALID_INDEX",
            "分子泵索引 1-3",
            "VacuumSystemDevice::SetMolecularPumpStartStop");
    }
    
    if (sim_mode_) {
        // 模拟模式：启停信号直接影响电源状态
        switch (index) {
            case 1: molecular_pump1_power_ = state; break;
            case 2: molecular_pump2_power_ = state; break;
            case 3: molecular_pump3_power_ = state; break;
        }
    } else {
        Common::PLC::PLCAddress addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1: addr = VacuumSystemPLCMapping::MolecularPump1StartStop(); break;
            case 2: addr = VacuumSystemPLCMapping::MolecularPump2StartStop(); break;
            case 3: addr = VacuumSystemPLCMapping::MolecularPump3StartStop(); break;
        }
        writePLCBool(addr, state);
    }
    logEvent("分子泵" + std::to_string(index) + (state ? "启动" : "停止"));
}

// ============================================================================
// 阀门控制命令 (手动模式)
// ============================================================================

void VacuumSystemDevice::SetGateValve(const Tango::DevVarShortArray* argin) {
    if (argin->length() < 2) {
        Tango::Except::throw_exception(
            "INVALID_ARGUMENT",
            "需要 [index, operation] (operation: 1=开, 0=关)",
            "VacuumSystemDevice::SetGateValve");
    }
    
    int index = (*argin)[0];
    bool open = (*argin)[1] != 0;
    
    if (index < 1 || index > 5) {
        Tango::Except::throw_exception(
            "INVALID_INDEX",
            "闸板阀索引 1-5",
            "VacuumSystemDevice::SetGateValve");
    }
    
    // 闸板阀5需要允许信号才能操作
    if (index == 5 && !gate_valve5_permit_) {
        Tango::Except::throw_exception(
            "PERMISSION_DENIED",
            "闸板阀5操作需要允许信号（gateValve5Permit）",
            "VacuumSystemDevice::SetGateValve");
    }
    
    // 获取开/关地址
    Common::PLC::PLCAddress open_addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
    Common::PLC::PLCAddress close_addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
    
    switch (index) {
        case 1:
            open_addr = VacuumSystemPLCMapping::GateValve1OpenOutput();
            close_addr = VacuumSystemPLCMapping::GateValve1CloseOutput();
            break;
        case 2:
            open_addr = VacuumSystemPLCMapping::GateValve2OpenOutput();
            close_addr = VacuumSystemPLCMapping::GateValve2CloseOutput();
            break;
        case 3:
            open_addr = VacuumSystemPLCMapping::GateValve3OpenOutput();
            close_addr = VacuumSystemPLCMapping::GateValve3CloseOutput();
            break;
        case 4:
            open_addr = VacuumSystemPLCMapping::GateValve4OpenOutput();
            close_addr = VacuumSystemPLCMapping::GateValve4CloseOutput();
            break;
        case 5:
            open_addr = VacuumSystemPLCMapping::GateValve5OpenOutput();
            close_addr = VacuumSystemPLCMapping::GateValve5CloseOutput();
            break;
    }
    
    if (sim_mode_) {
        // 模拟模式：启动阀门动作跟踪器，由 simulateValveActions() 处理延时
        startValveAction("GateValve" + std::to_string(index), open);
    } else {
        // 正常模式：发送 PLC 命令
        if (open) {
            writePLCBool(close_addr, false);
            writePLCBool(open_addr, true);
        } else {
            writePLCBool(open_addr, false);
            writePLCBool(close_addr, true);
        }
        startValveAction("GateValve" + std::to_string(index), open);
    }
    
    logEvent("闸板阀" + std::to_string(index) + (open ? "开启" : "关闭"));
}

void VacuumSystemDevice::SetElectromagneticValve(const Tango::DevVarShortArray* argin) {
    if (argin->length() < 2) {
        Tango::Except::throw_exception(
            "INVALID_ARGUMENT",
            "需要 [index, state]",
            "VacuumSystemDevice::SetElectromagneticValve");
    }
    
    int index = (*argin)[0];
    bool state = (*argin)[1] != 0;
    
    if (index < 1 || index > 4) {
        Tango::Except::throw_exception(
            "INVALID_INDEX",
            "电磁阀索引 1-4",
            "VacuumSystemDevice::SetElectromagneticValve");
    }
    
    if (sim_mode_) {
        // 模拟模式：电磁阀立即响应
        switch (index) {
            case 1: electromagnetic_valve1_open_ = state; electromagnetic_valve1_close_ = !state; break;
            case 2: electromagnetic_valve2_open_ = state; electromagnetic_valve2_close_ = !state; break;
            case 3: electromagnetic_valve3_open_ = state; electromagnetic_valve3_close_ = !state; break;
            case 4: electromagnetic_valve4_open_ = state; electromagnetic_valve4_close_ = !state; break;
        }
    } else {
        Common::PLC::PLCAddress addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1: addr = VacuumSystemPLCMapping::ElectromagneticValve1Output(); break;
            case 2: addr = VacuumSystemPLCMapping::ElectromagneticValve2Output(); break;
            case 3: addr = VacuumSystemPLCMapping::ElectromagneticValve3Output(); break;
            case 4: addr = VacuumSystemPLCMapping::ElectromagneticValve4Output(); break;
        }
        writePLCBool(addr, state);
    }
    logEvent("电磁阀" + std::to_string(index) + (state ? "开启" : "关闭"));
}

void VacuumSystemDevice::SetVentValve(const Tango::DevVarShortArray* argin) {
    if (argin->length() < 2) {
        Tango::Except::throw_exception(
            "INVALID_ARGUMENT",
            "需要 [index, state]",
            "VacuumSystemDevice::SetVentValve");
    }
    
    int index = (*argin)[0];
    bool state = (*argin)[1] != 0;
    
    if (index < 1 || index > 2) {
        Tango::Except::throw_exception(
            "INVALID_INDEX",
            "放气阀索引 1-2",
            "VacuumSystemDevice::SetVentValve");
    }
    
    if (sim_mode_) {
        // 模拟模式：放气阀立即响应
        switch (index) {
            case 1: vent_valve1_open_ = state; vent_valve1_close_ = !state; break;
            case 2: vent_valve2_open_ = state; vent_valve2_close_ = !state; break;
        }
    } else {
        Common::PLC::PLCAddress addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1: addr = VacuumSystemPLCMapping::VentValve1Output(); break;
            case 2: addr = VacuumSystemPLCMapping::VentValve2Output(); break;
        }
        writePLCBool(addr, state);
    }
    logEvent("放气阀" + std::to_string(index) + (state ? "开启" : "关闭"));
}

void VacuumSystemDevice::SetWaterValve(const Tango::DevVarShortArray* argin) {
    if (argin->length() < 2) {
        Tango::Except::throw_exception(
            "INVALID_ARGUMENT",
            "需要 [index, state]",
            "VacuumSystemDevice::SetWaterValve");
    }
    
    int index = (*argin)[0];
    bool state = (*argin)[1] != 0;
    
    if (index < 1 || index > 6) {
        Tango::Except::throw_exception(
            "INVALID_INDEX",
            "水电磁阀索引 1-6",
            "VacuumSystemDevice::SetWaterValve");
    }
    
    if (sim_mode_) {
        // 模拟模式：水电磁阀立即响应
        switch (index) {
            case 1: water_valve1_state_ = state; break;
            case 2: water_valve2_state_ = state; break;
            case 3: water_valve3_state_ = state; break;
            case 4: water_valve4_state_ = state; break;
            case 5: water_valve5_state_ = state; break;
            case 6: water_valve6_state_ = state; break;
        }
    } else {
        Common::PLC::PLCAddress addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1: addr = VacuumSystemPLCMapping::WaterValve1Output(); break;
            case 2: addr = VacuumSystemPLCMapping::WaterValve2Output(); break;
            case 3: addr = VacuumSystemPLCMapping::WaterValve3Output(); break;
            case 4: addr = VacuumSystemPLCMapping::WaterValve4Output(); break;
            case 5: addr = VacuumSystemPLCMapping::WaterValve5Output(); break;
            case 6: addr = VacuumSystemPLCMapping::WaterValve6Output(); break;
        }
        writePLCBool(addr, state);
    }
    logEvent("水电磁阀" + std::to_string(index) + (state ? "开启" : "关闭"));
}

void VacuumSystemDevice::SetAirMainValve(Tango::DevBoolean state) {
    if (sim_mode_) {
        air_main_valve_state_ = state;
    } else {
        writePLCBool(VacuumSystemPLCMapping::AirMainValveOutput(), state);
    }
    logEvent(state ? "气主电磁阀开启" : "气主电磁阀关闭");
}

// ============================================================================
// 报警管理命令
// ============================================================================

void VacuumSystemDevice::AcknowledgeAlarm(Tango::DevLong alarm_code) {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    
    for (auto& alarm : active_alarms_) {
        if (alarm.alarm_code == alarm_code) {
            alarm.acknowledged = true;
            logEvent("报警已确认: " + alarm.description);
            break;
        }
    }
}

void VacuumSystemDevice::AcknowledgeAllAlarms() {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    
    for (auto& alarm : active_alarms_) {
        alarm.acknowledged = true;
    }
    
    logEvent("所有报警已确认");
}

void VacuumSystemDevice::ClearAlarmHistory() {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    
    alarm_history_.clear();
    
    // 清空文件
    std::ofstream ofs(alarm_log_path_);
    ofs << "[]";
    ofs.close();
    
    logEvent("报警历史已清除");
}

Tango::DevString VacuumSystemDevice::GetOperationConditions(Tango::DevString device_name) {
    std::string device(device_name);
    std::string result = getPrerequisiteStatus(device, "all");
    
    Tango::DevString ret = CORBA::string_dup(result.c_str());
    return ret;
}

Tango::DevString VacuumSystemDevice::GetActiveAlarms() {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    
    json j = json::array();
    for (const auto& alarm : active_alarms_) {
        json item;
        item["alarm_code"] = alarm.alarm_code;
        item["alarm_type"] = alarm.alarm_type;
        item["description"] = alarm.description;
        item["device_name"] = alarm.device_name;
        item["acknowledged"] = alarm.acknowledged;
        j.push_back(item);
    }
    
    Tango::DevString ret = CORBA::string_dup(j.dump().c_str());
    return ret;
}

Tango::DevString VacuumSystemDevice::GetSystemStatus() {
    json j;
    
    j["operation_mode"] = static_cast<int>(operation_mode_);
    j["system_state"] = static_cast<int>(system_state_);
    j["auto_sequence_step"] = auto_sequence_step_;
    
    j["pumps"]["screw_pump_power"] = screw_pump_power_;
    j["pumps"]["roots_pump_power"] = roots_pump_power_;
    j["pumps"]["molecular_pump1_power"] = molecular_pump1_power_;
    j["pumps"]["molecular_pump2_power"] = molecular_pump2_power_;
    j["pumps"]["molecular_pump3_power"] = molecular_pump3_power_;
    j["pumps"]["molecular_pump1_speed"] = molecular_pump1_speed_;
    j["pumps"]["molecular_pump2_speed"] = molecular_pump2_speed_;
    j["pumps"]["molecular_pump3_speed"] = molecular_pump3_speed_;
    
    j["sensors"]["vacuum_gauge1"] = vacuum_gauge1_;
    j["sensors"]["vacuum_gauge2"] = vacuum_gauge2_;
    j["sensors"]["vacuum_gauge3"] = vacuum_gauge3_;
    j["sensors"]["air_pressure"] = air_pressure_;
    
    Tango::DevString ret = CORBA::string_dup(j.dump().c_str());
    return ret;
}

// ============================================================================
// Tango 属性读取
// ============================================================================

void VacuumSystemDevice::read_operationMode(Tango::Attribute& attr) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    attr_operationMode_read = static_cast<Tango::DevShort>(operation_mode_);
    attr.set_value(&attr_operationMode_read);
}

void VacuumSystemDevice::read_systemState(Tango::Attribute& attr) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    attr_systemState_read = static_cast<Tango::DevShort>(system_state_);
    attr.set_value(&attr_systemState_read);
}

void VacuumSystemDevice::read_simulatorMode(Tango::Attribute& attr) {
    attr.set_value(&sim_mode_);
}

void VacuumSystemDevice::read_screwPumpPower(Tango::Attribute& attr) {
    attr.set_value(&screw_pump_power_);
}

void VacuumSystemDevice::read_rootsPumpPower(Tango::Attribute& attr) {
    attr.set_value(&roots_pump_power_);
}

void VacuumSystemDevice::read_molecularPump1Power(Tango::Attribute& attr) {
    attr.set_value(&molecular_pump1_power_);
}

void VacuumSystemDevice::read_molecularPump2Power(Tango::Attribute& attr) {
    attr.set_value(&molecular_pump2_power_);
}

void VacuumSystemDevice::read_molecularPump3Power(Tango::Attribute& attr) {
    attr.set_value(&molecular_pump3_power_);
}

void VacuumSystemDevice::read_molecularPump1Speed(Tango::Attribute& attr) {
    attr_molecularPump1Speed_read = molecular_pump1_speed_;
    attr.set_value(&attr_molecularPump1Speed_read);
}

void VacuumSystemDevice::read_molecularPump2Speed(Tango::Attribute& attr) {
    attr_molecularPump2Speed_read = molecular_pump2_speed_;
    attr.set_value(&attr_molecularPump2Speed_read);
}

void VacuumSystemDevice::read_molecularPump3Speed(Tango::Attribute& attr) {
    attr_molecularPump3Speed_read = molecular_pump3_speed_;
    attr.set_value(&attr_molecularPump3Speed_read);
}

// 分子泵启用配置属性
void VacuumSystemDevice::read_molecularPump1Enabled(Tango::Attribute& attr) {
    // 使用影子变量确保指针在 Tango 读取时仍然有效
    attr_molecularPump1Enabled_read = molecular_pump1_enabled_;
    attr.set_value(&attr_molecularPump1Enabled_read);
}

void VacuumSystemDevice::read_molecularPump2Enabled(Tango::Attribute& attr) {
    // 使用影子变量确保指针在 Tango 读取时仍然有效
    attr_molecularPump2Enabled_read = molecular_pump2_enabled_;
    attr.set_value(&attr_molecularPump2Enabled_read);
}

void VacuumSystemDevice::read_molecularPump3Enabled(Tango::Attribute& attr) {
    // 使用影子变量确保指针在 Tango 读取时仍然有效
    attr_molecularPump3Enabled_read = molecular_pump3_enabled_;
    attr.set_value(&attr_molecularPump3Enabled_read);
}

void VacuumSystemDevice::write_molecularPump1Enabled(Tango::WAttribute& attr) {
    Tango::DevBoolean val;
    attr.get_write_value(val);
    molecular_pump1_enabled_ = val;
    
    // 发送到PLC
    if (!sim_mode_) {
        writePLCBool(VacuumSystemPLCMapping::MolecularPump1Enabled(), val);
        DEBUG_STREAM << "[DEBUG] write_molecularPump1Enabled: 已发送到PLC " 
                     << VacuumSystemPLCMapping::MolecularPump1Enabled().address_string 
                     << " = " << (val ? "true" : "false") << std::endl;
    }
    
    logEvent("分子泵1启用配置: " + std::string(val ? "启用" : "禁用"));
}

void VacuumSystemDevice::write_molecularPump2Enabled(Tango::WAttribute& attr) {
    Tango::DevBoolean val;
    attr.get_write_value(val);
    molecular_pump2_enabled_ = val;
    
    // 发送到PLC
    if (!sim_mode_) {
        writePLCBool(VacuumSystemPLCMapping::MolecularPump2Enabled(), val);
        DEBUG_STREAM << "[DEBUG] write_molecularPump2Enabled: 已发送到PLC " 
                     << VacuumSystemPLCMapping::MolecularPump2Enabled().address_string 
                     << " = " << (val ? "true" : "false") << std::endl;
    }
    
    logEvent("分子泵2启用配置: " + std::string(val ? "启用" : "禁用"));
}

void VacuumSystemDevice::write_molecularPump3Enabled(Tango::WAttribute& attr) {
    Tango::DevBoolean val;
    attr.get_write_value(val);
    molecular_pump3_enabled_ = val;
    
    // 发送到PLC
    if (!sim_mode_) {
        writePLCBool(VacuumSystemPLCMapping::MolecularPump3Enabled(), val);
        DEBUG_STREAM << "[DEBUG] write_molecularPump3Enabled: 已发送到PLC " 
                     << VacuumSystemPLCMapping::MolecularPump3Enabled().address_string 
                     << " = " << (val ? "true" : "false") << std::endl;
    }
    
    logEvent("分子泵3启用配置: " + std::string(val ? "启用" : "禁用"));
}

void VacuumSystemDevice::write_attr(Tango::WAttribute& attr) {
    std::string attr_name = attr.get_name();
    
    // 分子泵启用配置
    if (attr_name == "molecularPump1Enabled") {
        write_molecularPump1Enabled(attr);
    } else if (attr_name == "molecularPump2Enabled") {
        write_molecularPump2Enabled(attr);
    } else if (attr_name == "molecularPump3Enabled") {
        write_molecularPump3Enabled(attr);
    }
}

void VacuumSystemDevice::read_gateValve1Open(Tango::Attribute& attr) {
    attr.set_value(&gate_valve1_open_);
}

void VacuumSystemDevice::read_gateValve1Close(Tango::Attribute& attr) {
    attr.set_value(&gate_valve1_close_);
}

void VacuumSystemDevice::read_gateValve2Open(Tango::Attribute& attr) {
    attr.set_value(&gate_valve2_open_);
}

void VacuumSystemDevice::read_gateValve2Close(Tango::Attribute& attr) {
    attr.set_value(&gate_valve2_close_);
}

void VacuumSystemDevice::read_gateValve3Open(Tango::Attribute& attr) {
    attr.set_value(&gate_valve3_open_);
}

void VacuumSystemDevice::read_gateValve3Close(Tango::Attribute& attr) {
    attr.set_value(&gate_valve3_close_);
}

void VacuumSystemDevice::read_gateValve4Open(Tango::Attribute& attr) {
    attr.set_value(&gate_valve4_open_);
}

void VacuumSystemDevice::read_gateValve4Close(Tango::Attribute& attr) {
    attr.set_value(&gate_valve4_close_);
}

void VacuumSystemDevice::read_gateValve5Open(Tango::Attribute& attr) {
    attr.set_value(&gate_valve5_open_);
}

void VacuumSystemDevice::read_gateValve5Close(Tango::Attribute& attr) {
    attr.set_value(&gate_valve5_close_);
}

void VacuumSystemDevice::read_electromagneticValve1Open(Tango::Attribute& attr) {
    attr.set_value(&electromagnetic_valve1_open_);
}

void VacuumSystemDevice::read_electromagneticValve1Close(Tango::Attribute& attr) {
    attr.set_value(&electromagnetic_valve1_close_);
}

void VacuumSystemDevice::read_electromagneticValve2Open(Tango::Attribute& attr) {
    attr.set_value(&electromagnetic_valve2_open_);
}

void VacuumSystemDevice::read_electromagneticValve2Close(Tango::Attribute& attr) {
    attr.set_value(&electromagnetic_valve2_close_);
}

void VacuumSystemDevice::read_electromagneticValve3Open(Tango::Attribute& attr) {
    attr.set_value(&electromagnetic_valve3_open_);
}

void VacuumSystemDevice::read_electromagneticValve3Close(Tango::Attribute& attr) {
    attr.set_value(&electromagnetic_valve3_close_);
}

void VacuumSystemDevice::read_electromagneticValve4Open(Tango::Attribute& attr) {
    attr.set_value(&electromagnetic_valve4_open_);
}

void VacuumSystemDevice::read_electromagneticValve4Close(Tango::Attribute& attr) {
    attr.set_value(&electromagnetic_valve4_close_);
}

void VacuumSystemDevice::read_ventValve1Open(Tango::Attribute& attr) {
    attr.set_value(&vent_valve1_open_);
}

void VacuumSystemDevice::read_ventValve1Close(Tango::Attribute& attr) {
    attr.set_value(&vent_valve1_close_);
}

void VacuumSystemDevice::read_ventValve2Open(Tango::Attribute& attr) {
    attr.set_value(&vent_valve2_open_);
}

void VacuumSystemDevice::read_ventValve2Close(Tango::Attribute& attr) {
    attr.set_value(&vent_valve2_close_);
}

void VacuumSystemDevice::read_gateValve1ActionState(Tango::Attribute& attr) {
    auto state = getValveActionState("GateValve1");
    attr_gateValve1ActionState_read = static_cast<Tango::DevLong>(state);
    attr.set_value(&attr_gateValve1ActionState_read);
}

void VacuumSystemDevice::read_gateValve2ActionState(Tango::Attribute& attr) {
    auto state = getValveActionState("GateValve2");
    attr_gateValve2ActionState_read = static_cast<Tango::DevLong>(state);
    attr.set_value(&attr_gateValve2ActionState_read);
}

void VacuumSystemDevice::read_gateValve3ActionState(Tango::Attribute& attr) {
    auto state = getValveActionState("GateValve3");
    attr_gateValve3ActionState_read = static_cast<Tango::DevLong>(state);
    attr.set_value(&attr_gateValve3ActionState_read);
}

void VacuumSystemDevice::read_gateValve4ActionState(Tango::Attribute& attr) {
    auto state = getValveActionState("GateValve4");
    attr_gateValve4ActionState_read = static_cast<Tango::DevLong>(state);
    attr.set_value(&attr_gateValve4ActionState_read);
}

void VacuumSystemDevice::read_gateValve5ActionState(Tango::Attribute& attr) {
    auto state = getValveActionState("GateValve5");
    attr_gateValve5ActionState_read = static_cast<Tango::DevLong>(state);
    attr.set_value(&attr_gateValve5ActionState_read);
}

void VacuumSystemDevice::read_vacuumGauge1(Tango::Attribute& attr) {
    attr.set_value(&vacuum_gauge1_);
}

void VacuumSystemDevice::read_vacuumGauge2(Tango::Attribute& attr) {
    attr.set_value(&vacuum_gauge2_);
}

void VacuumSystemDevice::read_vacuumGauge3(Tango::Attribute& attr) {
    attr.set_value(&vacuum_gauge3_);
}

void VacuumSystemDevice::read_airPressure(Tango::Attribute& attr) {
    attr.set_value(&air_pressure_);
}

// ----- 新增属性读取方法 -----

void VacuumSystemDevice::read_screwPumpFrequency(Tango::Attribute& attr) {
    // 使用影子变量确保指针在 Tango 读取时仍然有效
    attr_screwPumpFrequency_read = static_cast<Tango::DevLong>(screw_pump_frequency_);
    attr.set_value(&attr_screwPumpFrequency_read);
}

void VacuumSystemDevice::read_rootsPumpFrequency(Tango::Attribute& attr) {
    // 使用影子变量确保指针在 Tango 读取时仍然有效
    attr_rootsPumpFrequency_read = static_cast<Tango::DevLong>(roots_pump_frequency_);
    attr.set_value(&attr_rootsPumpFrequency_read);
}

void VacuumSystemDevice::read_autoSequenceStep(Tango::Attribute& attr) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    attr_autoSequenceStep_read = static_cast<Tango::DevLong>(auto_sequence_step_);
    attr.set_value(&attr_autoSequenceStep_read);
}

void VacuumSystemDevice::read_plcConnected(Tango::Attribute& attr) {
    // 强制使用静态变量以满足 Tango 的指针要求
    static Tango::DevBoolean connected;
    if (sim_mode_) {
        connected = true;
    } else {
        // 优先使用 plc_was_connected_ 标志（在 pollPLCStatus 中维护，更可靠）
        // 但如果 isConnected() 返回 true，也认为连接正常（处理刚恢复的情况）
        bool was_connected = plc_was_connected_.load();
        bool currently_connected = (plc_comm_ && plc_comm_->isConnected());
        
        // 如果两者不一致，以 isConnected() 为准（更实时），并更新标志
        if (currently_connected != was_connected) {
            plc_was_connected_.store(currently_connected);
            if (currently_connected) {
                DEBUG_STREAM << "[DEBUG] read_plcConnected: 检测到连接恢复，更新标志" << std::endl;
            } else {
                DEBUG_STREAM << "[DEBUG] read_plcConnected: 检测到连接断开，更新标志" << std::endl;
            }
        }
        
        connected = currently_connected;
    }
    attr.set_value(&connected);
}

void VacuumSystemDevice::read_phaseSequenceOk(Tango::Attribute& attr) {
    attr.set_value(&phase_sequence_ok_);
}

void VacuumSystemDevice::read_motionSystemOnline(Tango::Attribute& attr) {
    attr.set_value(&motion_system_online_);
}

void VacuumSystemDevice::read_gateValve5Permit(Tango::Attribute& attr) {
    attr.set_value(&gate_valve5_permit_);
}

void VacuumSystemDevice::read_waterValve1State(Tango::Attribute& attr) {
    attr.set_value(&water_valve1_state_);
}

void VacuumSystemDevice::read_waterValve2State(Tango::Attribute& attr) {
    attr.set_value(&water_valve2_state_);
}

void VacuumSystemDevice::read_waterValve3State(Tango::Attribute& attr) {
    attr.set_value(&water_valve3_state_);
}

void VacuumSystemDevice::read_waterValve4State(Tango::Attribute& attr) {
    attr.set_value(&water_valve4_state_);
}

void VacuumSystemDevice::read_waterValve5State(Tango::Attribute& attr) {
    attr.set_value(&water_valve5_state_);
}

void VacuumSystemDevice::read_waterValve6State(Tango::Attribute& attr) {
    attr.set_value(&water_valve6_state_);
}

void VacuumSystemDevice::read_airMainValveState(Tango::Attribute& attr) {
    attr.set_value(&air_main_valve_state_);
}

void VacuumSystemDevice::read_activeAlarmCount(Tango::Attribute& attr) {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    // 使用影子变量确保指针在 Tango 读取时仍然有效
    attr_activeAlarmCount_read = static_cast<Tango::DevLong>(active_alarms_.size());
    attr.set_value(&attr_activeAlarmCount_read);
}

void VacuumSystemDevice::read_hasUnacknowledgedAlarm(Tango::Attribute& attr) {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    bool has_unack = false;
    for (const auto& alarm : active_alarms_) {
        if (!alarm.acknowledged) {
            has_unack = true;
            break;
        }
    }
    attr.set_value(&has_unack);
}

void VacuumSystemDevice::read_latestAlarmJson(Tango::Attribute& attr) {
    std::lock_guard<std::mutex> lock(alarm_mutex_);
    
    std::string result = "{}";
    if (!active_alarms_.empty()) {
        const auto& alarm = active_alarms_.back();
        json j;
        j["alarm_code"] = alarm.alarm_code;
        j["alarm_type"] = alarm.alarm_type;
        j["description"] = alarm.description;
        j["device_name"] = alarm.device_name;
        j["acknowledged"] = alarm.acknowledged;
        result = j.dump();
    }
    
    // 需要使用静态存储以返回 char*
    static std::string static_result;
    static_result = result;
    Tango::DevString ptr = const_cast<char*>(static_result.c_str());
    attr.set_value(&ptr);
}

// ============================================================================
// 条件检查
// ============================================================================

bool VacuumSystemDevice::checkAutoModePrerequisites() {
    // 自动模式前提条件
    // 1. 无活跃报警
    // 2. 相序正常
    // 3. 气源压力 >= 0.4 MPa
    
    if (!active_alarms_.empty()) return false;
    
    bool phase_ok;
    if (readPLCBool(VacuumSystemPLCMapping::PhaseSequenceProtection(), phase_ok)) {
        if (!phase_ok) return false;
    }
    
    if (air_pressure_ < 0.4) return false;
    
    return true;
}

bool VacuumSystemDevice::checkManualOperationAllowed(const std::string& device, 
                                                      const std::string& operation) {
    // 根据设备和操作类型检查先决条件
    // 此处简化实现，实际应根据 OperationConditions 检查
    
    if (device == "ScrewPump" && operation == "Start") {
        // 检查电磁阀4是否开启
        return electromagnetic_valve4_open_;
    }
    
    if (device == "RootsPump" && operation == "Start") {
        // 检查螺杆泵是否运行
        return screw_pump_power_ && electromagnetic_valve4_open_;
    }
    
    return true;
}

std::string VacuumSystemDevice::getPrerequisiteStatus(const std::string& device, 
                                                       const std::string& operation) {
    json j;
    j["device"] = device;
    j["operation"] = operation;
    j["conditions"] = json::array();
    
    // 根据 OperationConditions 类获取条件列表
    if (device == "ScrewPump") {
        auto conditions = OperationConditions::ScrewPumpStartConditions();
        for (const auto& cond : conditions) {
            json item;
            item["description"] = cond;
            item["satisfied"] = true;  // 实际应检查
            j["conditions"].push_back(item);
        }
    }
    
    return j.dump();
}

// ============================================================================
// 自动流程状态机
// ============================================================================

void VacuumSystemDevice::processAutoVacuumSequence() {
    // 一键抽真空流程（按照《真空系统操作全流程及配置规范》）
    // 
    // 非真空状态流程（≥3000Pa，步骤1-20）：
    //  步骤1: 开启电磁阀4
    //  步骤2: 开电磁阀123（检测开到位）
    //  步骤3: 开闸板阀123（检测开到位）
    //  步骤4: 启动螺杆泵
    //  步骤5: 等待螺杆泵达110Hz稳定
    //  步骤6: 等待真空度<7000Pa，启动罗茨泵
    //  步骤7: 等待真空度<45Pa，启动分子泵123
    //  步骤8: 等待分子泵满转（518Hz）
    //  步骤9: 延时1分钟后关闭罗茨泵
    //  步骤10: 流程完成
    //
    // 低真空状态流程（<3000Pa，步骤100-120）：
    //  步骤100: 开启电磁阀4
    //  步骤101: 开放气阀1
    //  步骤102: 平衡至大气压（等待真空计3≥80000Pa）
    //  步骤103: 关放气阀1
    //  步骤104: 启动螺杆泵
    //  步骤105: 等待螺杆泵达110Hz
    //  步骤106: 等待真空度<7000Pa，启动罗茨泵
    //  步骤107: 等待真空度<3000Pa，开闸板阀4
    //  步骤108: 开电磁阀123（检测开到位）
    //  步骤109: 开闸板阀123（检测开到位）
    //  步骤110: 关闸板阀4
    //  步骤111: 等待真空度<45Pa，启动分子泵123
    //  步骤112: 等待分子泵满转（518Hz）
    //  步骤113: 延时1分钟后关闭罗茨泵
    //  步骤114: 流程完成
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - auto_step_start_time_).count();
    
    DEBUG_STREAM << "[DEBUG] processAutoVacuumSequence: 步骤=" << auto_sequence_step_ 
                 << ", 已耗时=" << elapsed << "秒"
                 << ", 流程类型=" << (vacuum_sequence_is_low_vacuum_ ? "低真空" : "非真空")
                 << ", 当前真空度G2=" << vacuum_gauge2_ << "Pa" << std::endl;
    
    switch (auto_sequence_step_) {
        // ========================================================================
        // 非真空状态流程（≥3000Pa，步骤1-10）
        // ========================================================================
        case 1:  // 开启电磁阀4
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤1: 开启电磁阀4" << std::endl;
            ctrlElectromagneticValve(4, true);
            auto_sequence_step_ = 2;
            auto_step_start_time_ = now;
            logEvent("自动抽真空 - 步骤1: 开启电磁阀4");
            break;
            
        case 2:  // 开电磁阀123（检测开到位）- 规范要求：先全部开启电磁阀123
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤2: 开启电磁阀1、2、3" << std::endl;
            ctrlElectromagneticValve(1, true);
            ctrlElectromagneticValve(2, true);
            ctrlElectromagneticValve(3, true);
            auto_sequence_step_ = 3;
            auto_step_start_time_ = now;
            logEvent("自动抽真空 - 步骤2: 开启电磁阀1、2、3");
            break;
            
        case 3:  // 等待电磁阀123全部开到位
            if (elapsed % 2 == 0) {  // 每2秒输出一次状态
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤3: 等待电磁阀1、2、3全部开到位 (已等待=" << elapsed 
                             << "秒, EMV1=" << (electromagnetic_valve1_open_ ? "开" : "关")
                             << ", EMV2=" << (electromagnetic_valve2_open_ ? "开" : "关")
                             << ", EMV3=" << (electromagnetic_valve3_open_ ? "开" : "关") << ")" << std::endl;
            }
            if (electromagnetic_valve1_open_ && electromagnetic_valve2_open_ && electromagnetic_valve3_open_) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤3: 电磁阀1、2、3已全部开启，开启闸板阀1、2、3" << std::endl;
                ctrlGateValve(1, true);
                ctrlGateValve(2, true);
                ctrlGateValve(3, true);
                auto_sequence_step_ = 4;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤3: 开启闸板阀1、2、3");
            } else if (elapsed > 10) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤3: 超时，电磁阀1、2、3未全部开启 (EMV1=" 
                             << (electromagnetic_valve1_open_ ? "开" : "关")
                             << ", EMV2=" << (electromagnetic_valve2_open_ ? "开" : "关")
                             << ", EMV3=" << (electromagnetic_valve3_open_ ? "开" : "关") << ")" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 电磁阀1、2、3开到位超时");
                ctrlElectromagneticValve(1, false);
                ctrlElectromagneticValve(2, false);
                ctrlElectromagneticValve(3, false);
            }
            break;
            
        case 4:  // 等待闸板阀123全部开到位
            if (elapsed % 2 == 0) {  // 每2秒输出一次状态
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤4: 等待闸板阀1、2、3全部开到位 (已等待=" << elapsed 
                             << "秒, GV1=" << (gate_valve1_open_ ? "开" : "关")
                             << ", GV2=" << (gate_valve2_open_ ? "开" : "关")
                             << ", GV3=" << (gate_valve3_open_ ? "开" : "关") << ")" << std::endl;
            }
            if (gate_valve1_open_ && gate_valve2_open_ && gate_valve3_open_) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤4: 闸板阀1、2、3已全部开启，启动螺杆泵" << std::endl;
                ctrlScrewPump(true);
                auto_sequence_step_ = 5;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤4: 启动螺杆泵");
            } else if (elapsed > 10) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤4: 超时，闸板阀1、2、3未全部开启 (GV1=" 
                             << (gate_valve1_open_ ? "开" : "关")
                             << ", GV2=" << (gate_valve2_open_ ? "开" : "关")
                             << ", GV3=" << (gate_valve3_open_ ? "开" : "关") << ")" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 闸板阀1、2、3开到位超时");
                ctrlGateValve(1, false);
                ctrlGateValve(2, false);
                ctrlGateValve(3, false);
            }
            break;
            
        case 5:  // 等待螺杆泵达110Hz稳定（规范要求：达110赫兹稳定）
            if (elapsed % 5 == 0) {  // 每5秒输出一次状态
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤5: 等待螺杆泵达110Hz稳定 (已等待=" << elapsed 
                             << "秒, 当前=" << screw_pump_frequency_ 
                             << " Hz, 目标≥110 Hz, 进度=" << (screw_pump_frequency_ * 100 / 110) << "%)" << std::endl;
            }
            if (screw_pump_frequency_ >= 110) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤5: 螺杆泵已达110Hz稳定 (耗时=" << elapsed << "秒)，进入下一步" << std::endl;
                auto_sequence_step_ = 6;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤5: 螺杆泵已达110Hz稳定");
            } else if (elapsed > 60) {  // 1分钟超时
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤5: 超时，螺杆泵未达到110Hz (当前=" << screw_pump_frequency_ 
                             << " Hz, 已等待=" << elapsed << "秒)" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 螺杆泵未达到110Hz超时");
                ctrlScrewPump(false);
                ctrlElectromagneticValve(4, false);
            } else if (elapsed > 50) {  // 超时前10秒警告
                DEBUG_STREAM << "[WARN] 自动抽真空步骤5: 即将超时，螺杆泵频率=" << screw_pump_frequency_ 
                             << " Hz (还需" << (60 - elapsed) << "秒)" << std::endl;
            }
            break;
            
        case 6:  // 等待真空度 < 7000Pa，启动罗茨泵
            if (elapsed % 10 == 0) {  // 每10秒输出一次状态
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤6: 等待真空度达标 (已等待=" << elapsed 
                             << "秒, 当前G3=" << vacuum_gauge3_ 
                             << " Pa, 目标<7000 Pa, 进度=" << ((7000 - vacuum_gauge3_) * 100 / 7000) << "%)" << std::endl;
            }
            if (vacuum_gauge3_ < 7000) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤6: 真空度达标 (G3=" << vacuum_gauge3_ 
                             << " Pa, 耗时=" << elapsed << "秒)，启动罗茨泵" << std::endl;
                ctrlRootsPump(true);
                auto_sequence_step_ = 7;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤6: 启动罗茨泵");
            } else if (elapsed > 300) {  // 5分钟超时
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤6: 超时，前级抽气失败 (当前G3=" << vacuum_gauge3_ 
                             << " Pa, 已等待=" << elapsed << "秒)" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 前级抽气超时");
                ctrlRootsPump(false);
                ctrlScrewPump(false);
                ctrlElectromagneticValve(4, false);
            } else if (elapsed > 270) {  // 超时前30秒警告
                DEBUG_STREAM << "[WARN] 自动抽真空步骤6: 即将超时，当前G3=" << vacuum_gauge3_ 
                             << " Pa (还需" << (300 - elapsed) << "秒)" << std::endl;
            }
            break;
            
        case 7:  // 等待真空度 < 45Pa，启动分子泵123
            if (elapsed % 10 == 0) {  // 每10秒输出一次状态
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤7: 等待真空度达标 (已等待=" << elapsed 
                             << "秒, 当前: G1=" << vacuum_gauge1_ 
                             << " Pa, G2=" << vacuum_gauge2_ 
                             << " Pa, 目标<=45 Pa)" << std::endl;
            }
            if (vacuum_gauge1_ <= 45 && vacuum_gauge2_ <= 45) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤7: 真空度达标 (G1=" << vacuum_gauge1_ 
                             << " Pa, G2=" << vacuum_gauge2_ 
                             << " Pa, 耗时=" << elapsed << "秒)，启动启用的分子泵" << std::endl;
                std::string enabled_pumps = "";
                if (molecular_pump1_enabled_) {
                    ctrlMolecularPump(1, true);
                    enabled_pumps += "1 ";
                }
                if (molecular_pump2_enabled_) {
                    ctrlMolecularPump(2, true);
                    enabled_pumps += "2 ";
                }
                if (molecular_pump3_enabled_) {
                    ctrlMolecularPump(3, true);
                    enabled_pumps += "3 ";
                }
                auto_sequence_step_ = 8;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤7: 启动分子泵" + enabled_pumps + "(根据配置)");
            }
            break;
            
        case 8:  // 等待分子泵满转 (518Hz ≈ 31080 RPM)
        {
            if (elapsed % 10 == 0) {  // 每10秒输出一次状态
                int progress1 = (molecular_pump1_speed_ * 100 / 30000);
                int progress2 = (molecular_pump2_speed_ * 100 / 30000);
                int progress3 = (molecular_pump3_speed_ * 100 / 30000);
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤8: 等待分子泵满转 (已等待=" << elapsed 
                             << "秒, MP1=" << molecular_pump1_speed_ << " RPM (" << progress1 << "%)"
                             << ", MP2=" << molecular_pump2_speed_ << " RPM (" << progress2 << "%)"
                             << ", MP3=" << molecular_pump3_speed_ << " RPM (" << progress3 << "%)"
                             << ", 目标≥30000 RPM)" << std::endl;
            }
            // 只检查启用的分子泵是否满转
            bool all_enabled_pumps_ready = true;
            if (molecular_pump1_enabled_ && molecular_pump1_speed_ < 30000) {
                all_enabled_pumps_ready = false;
            }
            if (molecular_pump2_enabled_ && molecular_pump2_speed_ < 30000) {
                all_enabled_pumps_ready = false;
            }
            if (molecular_pump3_enabled_ && molecular_pump3_speed_ < 30000) {
                all_enabled_pumps_ready = false;
            }
            
            if (all_enabled_pumps_ready) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤8: 启用的分子泵已满转 (MP1=" << molecular_pump1_speed_ 
                             << ", MP2=" << molecular_pump2_speed_ 
                             << ", MP3=" << molecular_pump3_speed_ 
                             << " RPM, 耗时=" << elapsed << "秒)，等待1分钟后关闭罗茨泵" << std::endl;
                auto_sequence_step_ = 9;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤8: 启用的分子泵已满转，等待1分钟后关闭罗茨泵");
            }
            break;
        }
            
        case 9:  // 延时1分钟后关闭罗茨泵（规范要求：分子泵满518赫兹后关罗茨泵，延时1分钟）
            if (elapsed % 10 == 0) {  // 每10秒输出一次状态
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤9: 等待1分钟后关闭罗茨泵 (已等待=" << elapsed 
                             << "秒, 还需等待=" << (60 - elapsed) << "秒)" << std::endl;
            }
            if (elapsed >= 60) {  // 1分钟 = 60秒
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤9: 延时已到，关闭罗茨泵" << std::endl;
                ctrlRootsPump(false);
                auto_sequence_step_ = 10;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤9: 关闭罗茨泵");
            }
            break;
            
        case 10:  // 流程完成
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤10: 流程完成" << std::endl;
            system_state_ = SystemState::IDLE;
            auto_sequence_step_ = 0;
            vacuum_sequence_is_low_vacuum_ = false;  // 重置流程类型标志
            logEvent("自动抽真空完成");
            break;
            
        // ========================================================================
        // 低真空状态流程（<3000Pa，步骤100-114）
        // ========================================================================
        case 100:  // 开启电磁阀4
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤100: 开启电磁阀4（低真空流程）" << std::endl;
            ctrlElectromagneticValve(4, true);
            auto_sequence_step_ = 101;
            auto_step_start_time_ = now;
            logEvent("自动抽真空 - 步骤100: 开启电磁阀4（低真空流程）");
            break;
            
        case 101:  // 等待电磁阀4到位，开放气阀1
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤101: 等待电磁阀4到位，开放气阀1" << std::endl;
            if (electromagnetic_valve4_open_) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤101: 电磁阀4已开启，开放气阀1" << std::endl;
                ctrlVentValve(1, true);
                auto_sequence_step_ = 102;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤101: 开放气阀1");
            } else if (elapsed > 10) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤101: 超时，电磁阀4未开启" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 电磁阀4开到位超时");
                ctrlElectromagneticValve(4, false);
            }
            break;
            
        case 102:  // 平衡至大气压（等待真空计3≥80000Pa）
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤102: 等待平衡至大气压 (当前: G3=" << vacuum_gauge3_ 
                         << " Pa, 目标≥80000 Pa)" << std::endl;
            if (vacuum_gauge3_ >= 80000) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤102: 已平衡至大气压，关闭放气阀1" << std::endl;
                ctrlVentValve(1, false);
                auto_sequence_step_ = 103;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤102: 关闭放气阀1");
            } else if (elapsed > 60) {  // 1分钟超时
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤102: 超时，未平衡至大气压" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 平衡至大气压超时");
                ctrlVentValve(1, false);
                ctrlElectromagneticValve(4, false);
            }
            break;
            
        case 103:  // 等待放气阀1关闭，启动螺杆泵
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤103: 等待放气阀1关闭" << std::endl;
            if (vent_valve1_close_) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤103: 放气阀1已关闭，启动螺杆泵" << std::endl;
                ctrlScrewPump(true);
                auto_sequence_step_ = 104;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤103: 启动螺杆泵");
            } else if (elapsed > 10) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤103: 超时，放气阀1未关闭" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 放气阀1关闭超时");
                ctrlVentValve(1, false);
            }
            break;
            
        case 104:  // 等待螺杆泵达110Hz
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤104: 等待螺杆泵达110Hz (当前=" << screw_pump_frequency_ 
                         << " Hz, 目标≥110 Hz)" << std::endl;
            if (screw_pump_frequency_ >= 110) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤104: 螺杆泵已达110Hz，进入下一步" << std::endl;
                auto_sequence_step_ = 105;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤104: 螺杆泵已达110Hz");
            } else if (elapsed > 60) {  // 1分钟超时
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤104: 超时，螺杆泵未达到110Hz" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 螺杆泵未达到110Hz超时");
                ctrlScrewPump(false);
                ctrlElectromagneticValve(4, false);
            }
            break;
            
        case 105:  // 等待真空度<7000Pa，启动罗茨泵
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤105: 等待真空度<7000Pa (当前前级G1=" << vacuum_gauge1_ 
                         << " Pa, 目标<7000 Pa)" << std::endl;
            if (vacuum_gauge1_ < 7000) { // 在低真空隔离状态下，应检查前级真空计G1
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤105: 前级真空达标，启动罗茨泵" << std::endl;
                ctrlRootsPump(true);
                auto_sequence_step_ = 106;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤105: 启动罗茨泵");
            } else if (elapsed > 300) {  // 5分钟超时
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤105: 超时，前级抽气失败" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 前级抽气超时");
                ctrlRootsPump(false);
                ctrlScrewPump(false);
                ctrlElectromagneticValve(4, false);
            }
            break;
            
        case 106:  // 等待真空度<3000Pa，开闸板阀4
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤106: 等待真空度<3000Pa (当前: G2=" << vacuum_gauge2_ 
                         << " Pa, 目标<3000 Pa)" << std::endl;
            if (vacuum_gauge2_ < 3000) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤106: 真空度达标，开启闸板阀4" << std::endl;
                ctrlGateValve(4, true);
                auto_sequence_step_ = 107;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤106: 开启闸板阀4");
            } else if (elapsed > 300) {  // 5分钟超时
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤106: 超时，真空度未达到<3000Pa" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 真空度未达到<3000Pa超时");
                ctrlRootsPump(false);
                ctrlScrewPump(false);
                ctrlElectromagneticValve(4, false);
            }
            break;
            
        case 107:  // 等待闸板阀4开启，开电磁阀123
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤107: 等待闸板阀4开启" << std::endl;
            if (gate_valve4_open_) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤107: 闸板阀4已开启，开启电磁阀1、2、3" << std::endl;
                ctrlElectromagneticValve(1, true);
                ctrlElectromagneticValve(2, true);
                ctrlElectromagneticValve(3, true);
                auto_sequence_step_ = 108;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤107: 开启电磁阀1、2、3");
            } else if (elapsed > 10) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤107: 超时，闸板阀4未开启" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 闸板阀4开到位超时");
                ctrlGateValve(4, false);
            }
            break;
            
        case 108:  // 等待电磁阀123全部开到位，开闸板阀123
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤108: 等待电磁阀1、2、3全部开到位" << std::endl;
            if (electromagnetic_valve1_open_ && electromagnetic_valve2_open_ && electromagnetic_valve3_open_) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤108: 电磁阀1、2、3已全部开启，开启闸板阀1、2、3" << std::endl;
                ctrlGateValve(1, true);
                ctrlGateValve(2, true);
                ctrlGateValve(3, true);
                auto_sequence_step_ = 109;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤108: 开启闸板阀1、2、3");
            } else if (elapsed > 10) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤108: 超时，电磁阀1、2、3未全部开启" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 电磁阀1、2、3开到位超时");
                ctrlElectromagneticValve(1, false);
                ctrlElectromagneticValve(2, false);
                ctrlElectromagneticValve(3, false);
            }
            break;
            
        case 109:  // 等待闸板阀123全部开到位，关闸板阀4
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤109: 等待闸板阀1、2、3全部开到位" << std::endl;
            if (gate_valve1_open_ && gate_valve2_open_ && gate_valve3_open_) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤109: 闸板阀1、2、3已全部开启，关闭闸板阀4" << std::endl;
                ctrlGateValve(4, false);
                auto_sequence_step_ = 110;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤109: 关闭闸板阀4");
            } else if (elapsed > 10) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤109: 超时，闸板阀1、2、3未全部开启" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 闸板阀1、2、3开到位超时");
                ctrlGateValve(1, false);
                ctrlGateValve(2, false);
                ctrlGateValve(3, false);
            }
            break;
            
        case 110:  // 等待闸板阀4关闭，等待真空度<45Pa，启动分子泵123
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤110: 等待闸板阀4关闭" << std::endl;
            if (gate_valve4_close_) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤110: 闸板阀4已关闭，等待真空度<45Pa" << std::endl;
                auto_sequence_step_ = 111;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤110: 等待真空度<45Pa");
            } else if (elapsed > 10) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤110: 超时，闸板阀4未关闭" << std::endl;
                system_state_ = SystemState::FAULT;
                logEvent("自动抽真空失败 - 闸板阀4关闭超时");
                ctrlGateValve(4, false);
            }
            break;
            
        case 111:  // 等待真空度<45Pa，启动分子泵123
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤111: 等待真空度<=45Pa (当前: G1=" << vacuum_gauge1_ 
                         << ", G2=" << vacuum_gauge2_ << ")" << std::endl;
            if (vacuum_gauge1_ <= 45 && vacuum_gauge2_ <= 45) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤111: 真空度达标，启动启用的分子泵" << std::endl;
                std::string enabled_pumps = "";
                if (molecular_pump1_enabled_) {
                    ctrlMolecularPump(1, true);
                    enabled_pumps += "1 ";
                }
                if (molecular_pump2_enabled_) {
                    ctrlMolecularPump(2, true);
                    enabled_pumps += "2 ";
                }
                if (molecular_pump3_enabled_) {
                    ctrlMolecularPump(3, true);
                    enabled_pumps += "3 ";
                }
                auto_sequence_step_ = 112;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤111: 启动分子泵" + enabled_pumps + "(根据配置)");
            }
            break;
            
        case 112:  // 等待分子泵满转 (518Hz ≈ 31080 RPM)
        {
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤112: 等待启用的分子泵满转 (当前: MP1=" << molecular_pump1_speed_ 
                         << ", MP2=" << molecular_pump2_speed_ << ", MP3=" << molecular_pump3_speed_ << " RPM)" << std::endl;
            // 只检查启用的分子泵是否满转
            bool all_enabled_pumps_ready = true;
            if (molecular_pump1_enabled_ && molecular_pump1_speed_ < 30000) {
                all_enabled_pumps_ready = false;
            }
            if (molecular_pump2_enabled_ && molecular_pump2_speed_ < 30000) {
                all_enabled_pumps_ready = false;
            }
            if (molecular_pump3_enabled_ && molecular_pump3_speed_ < 30000) {
                all_enabled_pumps_ready = false;
            }
            
            if (all_enabled_pumps_ready) {
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤112: 启用的分子泵已满转，等待1分钟后关闭罗茨泵" << std::endl;
                auto_sequence_step_ = 113;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤112: 启用的分子泵已满转，等待1分钟后关闭罗茨泵");
            }
            break;
        }
            
        case 113:  // 延时1分钟后关闭罗茨泵（规范要求：分子泵满518赫兹后关罗茨泵，延时1分钟）
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤113: 等待1分钟后关闭罗茨泵 (已等待=" << elapsed << "秒)" << std::endl;
            if (elapsed >= 60) {  // 1分钟 = 60秒
                DEBUG_STREAM << "[DEBUG] 自动抽真空步骤113: 延时已到，关闭罗茨泵" << std::endl;
                ctrlRootsPump(false);
                auto_sequence_step_ = 114;
                auto_step_start_time_ = now;
                logEvent("自动抽真空 - 步骤113: 关闭罗茨泵");
            }
            break;
            
        case 114:  // 流程完成
            DEBUG_STREAM << "[DEBUG] 自动抽真空步骤114: 流程完成" << std::endl;
            system_state_ = SystemState::IDLE;
            auto_sequence_step_ = 0;
            vacuum_sequence_is_low_vacuum_ = false;  // 重置流程类型标志
            logEvent("自动抽真空完成（低真空流程）");
            break;
    }
}

void VacuumSystemDevice::processAutoStopSequence() {
    // 一键停机流程
    // 步骤1: 停止分子泵1-3
    // 步骤2: 等待分子泵转速降至0
    // 步骤3: 关闭闸板阀1-3
    // 步骤4: 关闭电磁阀1-3
    // 步骤5: 停止罗茨泵
    // 步骤6: 停止螺杆泵
    // 步骤7: 关闭电磁阀4
    // 步骤8: 关闭闸板阀4和5（确保所有闸板阀关闭）
    // 步骤9: 关闭放气阀1-2（确保所有放气阀关闭）
    // 步骤10: 完成
    
    auto now = std::chrono::steady_clock::now();
    
    switch (auto_sequence_step_) {
        case 1:  // 停止启用的分子泵
            {
                std::string stopped_pumps = "";
                if (molecular_pump1_enabled_) {
                    ctrlMolecularPump(1, false);
                    stopped_pumps += "1 ";
                }
                if (molecular_pump2_enabled_) {
                    ctrlMolecularPump(2, false);
                    stopped_pumps += "2 ";
                }
                if (molecular_pump3_enabled_) {
                    ctrlMolecularPump(3, false);
                    stopped_pumps += "3 ";
                }
                auto_sequence_step_ = 2;
                auto_step_start_time_ = now;
                logEvent("自动停机 - 步骤1: 停止分子泵" + stopped_pumps + "(根据配置)");
            }
            break;
            
        case 2:  // 等待启用的分子泵停止
        {
            // 只检查启用的分子泵是否已停止
            bool all_enabled_pumps_stopped = true;
            if (molecular_pump1_enabled_ && molecular_pump1_speed_ != 0) {
                all_enabled_pumps_stopped = false;
            }
            if (molecular_pump2_enabled_ && molecular_pump2_speed_ != 0) {
                all_enabled_pumps_stopped = false;
            }
            if (molecular_pump3_enabled_ && molecular_pump3_speed_ != 0) {
                all_enabled_pumps_stopped = false;
            }
            
            if (all_enabled_pumps_stopped) {
                // 关闭闸板阀1-3
                ctrlGateValve(1, false);
                ctrlGateValve(2, false);
                ctrlGateValve(3, false);
                auto_sequence_step_ = 3;
                auto_step_start_time_ = now;
                logEvent("自动停机 - 步骤2: 关闭闸板阀1-3");
            }
            break;
        }
            
        case 3:  // 等待闸板阀1-3关闭
            if (gate_valve1_close_ && gate_valve2_close_ && gate_valve3_close_) {
                ctrlElectromagneticValve(1, false);
                ctrlElectromagneticValve(2, false);
                ctrlElectromagneticValve(3, false);
                auto_sequence_step_ = 4;
                auto_step_start_time_ = now;
                logEvent("自动停机 - 步骤3: 关闭电磁阀1-3");
            }
            break;
            
        case 4:  // 等待电磁阀1-3关闭，停止罗茨泵
            if (electromagnetic_valve1_close_ && electromagnetic_valve2_close_ && 
                electromagnetic_valve3_close_) {
                ctrlRootsPump(false);
                auto_sequence_step_ = 5;
                auto_step_start_time_ = now;
                logEvent("自动停机 - 步骤4: 停止罗茨泵");
            }
            break;
            
        case 5:  // 停止螺杆泵
            if (!roots_pump_power_) {
                ctrlScrewPump(false);
                auto_sequence_step_ = 6;
                auto_step_start_time_ = now;
                logEvent("自动停机 - 步骤5: 停止螺杆泵");
            }
            break;
            
        case 6:  // 关闭电磁阀4
            if (!screw_pump_power_) {
                ctrlElectromagneticValve(4, false);
                auto_sequence_step_ = 7;
                auto_step_start_time_ = now;
                logEvent("自动停机 - 步骤6: 关闭电磁阀4");
            }
            break;
            
        case 7:  // 关闭闸板阀4和5（确保所有闸板阀关闭）
            if (electromagnetic_valve4_close_) {
                ctrlGateValve(4, false);
                // 闸板阀5需要允许信号，如果没有允许信号则跳过（记录警告）
                if (gate_valve5_permit_) {
                    ctrlGateValve(5, false);
                } else {
                    WARN_STREAM << "自动停机 - 步骤7: 闸板阀5缺少允许信号，跳过关闭操作" << std::endl;
                    logEvent("自动停机 - 步骤7: 闸板阀5缺少允许信号，跳过关闭");
                }
                auto_sequence_step_ = 8;
                auto_step_start_time_ = now;
                {
                    std::string msg = "自动停机 - 步骤7: 关闭闸板阀4";
                    msg += (gate_valve5_permit_ ? "和5" : "（闸板阀5跳过）");
                    logEvent(msg);
                }
            }
            break;
            
        case 8:  // 等待闸板阀4关闭（和5，如果有允许信号），关闭放气阀1-2
        {
            // 闸板阀5如果没有允许信号，不等待其关闭
            bool gv4_ready = gate_valve4_close_;
            // 如果闸板阀5有允许信号，需要等待其关闭；如果没有允许信号，则认为已就绪（跳过）
            bool gv5_ready = !gate_valve5_permit_ || gate_valve5_close_;
            
            if (gv4_ready && gv5_ready) {
                ctrlVentValve(1, false);
                ctrlVentValve(2, false);
                auto_sequence_step_ = 9;
                auto_step_start_time_ = now;
                logEvent("自动停机 - 步骤8: 关闭放气阀1-2");
            }
            break;
        }
            
        case 9:  // 等待放气阀关闭，完成停机
            if (vent_valve1_close_ && vent_valve2_close_) {
                system_state_ = SystemState::IDLE;
                auto_sequence_step_ = 0;
                vacuum_sequence_is_low_vacuum_ = false;  // 重置流程类型标志，停机后下次启动时重新判断
                logEvent("自动停机完成");
            }
            break;
    }
}

void VacuumSystemDevice::processVentSequence() {
    // 腔室放气流程
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - auto_step_start_time_).count();
    
    DEBUG_STREAM << "[DEBUG] processVentSequence: 步骤=" << auto_sequence_step_ 
                 << ", 已耗时=" << elapsed << "秒" << std::endl;
    
    switch (auto_sequence_step_) {
        case 1: {
            DEBUG_STREAM << "[DEBUG] 放气步骤1: 检查闸板阀状态" << std::endl;
            // 确保所有闸板阀关闭（闸板阀5如果没有允许信号，尝试关闭它；如果仍无法关闭，则跳过检查）
            // 先尝试关闭所有未关闭的闸板阀
            if (!gate_valve1_close_) ctrlGateValve(1, false);
            if (!gate_valve2_close_) ctrlGateValve(2, false);
            if (!gate_valve3_close_) ctrlGateValve(3, false);
            if (!gate_valve4_close_) ctrlGateValve(4, false);
            if (!gate_valve5_close_ && gate_valve5_permit_) {
                ctrlGateValve(5, false);
            }
            
            // 检查闸板阀关闭状态（闸板阀5如果没有允许信号，跳过检查）
            bool gv1_ready = gate_valve1_close_;
            bool gv2_ready = gate_valve2_close_;
            bool gv3_ready = gate_valve3_close_;
            bool gv4_ready = gate_valve4_close_;
            bool gv5_ready = !gate_valve5_permit_ || gate_valve5_close_;  // 如果没有允许信号，认为已就绪
            
            if (gv1_ready && gv2_ready && gv3_ready && gv4_ready && gv5_ready) {
                DEBUG_STREAM << "[DEBUG] 放气步骤1: 所有闸板阀已关闭，开启放气阀2" << std::endl;
                ctrlVentValve(2, true);
                // 立即更新一次放气阀状态（确保状态同步）
                if (!sim_mode_ && plc_comm_ && plc_comm_->isConnected()) {
                    readPLCBool(VacuumSystemPLCMapping::VentValve2OpenFeedback(), vent_valve2_open_);
                    readPLCBool(VacuumSystemPLCMapping::VentValve2CloseFeedback(), vent_valve2_close_);
                    DEBUG_STREAM << "[DEBUG] 放气步骤1: 已更新放气阀2状态 - 开=" << vent_valve2_open_ 
                                 << ", 关=" << vent_valve2_close_ << std::endl;
                }
                auto_sequence_step_ = 2;
                auto_step_start_time_ = now;
                logEvent("腔室放气 - 开启放气阀2");
            } else {
                DEBUG_STREAM << "[DEBUG] 放气步骤1: 等待闸板阀关闭 (GV1=" << gate_valve1_close_
                             << ", GV2=" << gate_valve2_close_ << ", GV3=" << gate_valve3_close_
                             << ", GV4=" << gate_valve4_close_ << ", GV5=" << gate_valve5_close_
                             << ", GV5允许信号=" << gate_valve5_permit_ << ")" << std::endl;
            }
            break;
        }
            
        case 2:
            DEBUG_STREAM << "[DEBUG] 放气步骤2: 等待压力达到大气压 (当前: G1=" << vacuum_gauge1_
                         << ", G2=" << vacuum_gauge2_ << " Pa)" << std::endl;
            // 等待腔室压力达到大气压
            if (vacuum_gauge1_ >= 80000 && vacuum_gauge2_ >= 80000) {
                DEBUG_STREAM << "[DEBUG] 放气步骤2: 压力已达标，关闭放气阀，流程完成" << std::endl;
                ctrlVentValve(2, false);
                system_state_ = SystemState::IDLE;
                auto_sequence_step_ = 0;
                vacuum_sequence_is_low_vacuum_ = false;  // 重置流程类型标志，放气完成后下次启动时重新判断
                logEvent("腔室放气完成");
            }
            break;
    }
}

// ============================================================================
// 辅助方法
// ============================================================================

void VacuumSystemDevice::logEvent(const std::string& event) {
    INFO_STREAM << "[" << getCurrentTimestamp() << "] " << event << std::endl;
}

std::string VacuumSystemDevice::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

// ============================================================================
// 模拟模式实现 (sim_mode_=true)
// ============================================================================

/**
 * @brief 运行模拟逻辑 - 替代 PLC 读取
 * 
 * 在模拟模式下调用，模拟真空系统的物理行为
 */
void VacuumSystemDevice::runSimulation() {
    // DEBUG_STREAM << "[DEBUG] runSimulation: 开始模拟逻辑" << std::endl;
    
    // 模拟物理行为
    simulatePumpBehavior();
    simulateVacuumPhysics();
    simulateValveActions();
    
    // 检查报警条件（模拟模式也需要）
    // 简化版：只检查气压
    if (air_pressure_ < 0.4) {
        DEBUG_STREAM << "[DEBUG] runSimulation: 检测到气压不足 (" << air_pressure_ << " MPa)" << std::endl;
        raiseAlarm(static_cast<int>(AlarmType::AIR_PRESSURE_LOW),
                  "SYSTEM", "气源压力不足 (<0.4MPa)", "气源");
    } else {
        clearAlarm(static_cast<int>(AlarmType::AIR_PRESSURE_LOW));
    }
    
    // 状态机处理（VENTING 状态可以在任何模式下处理）
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        // DEBUG_STREAM << "[DEBUG] runSimulation: 检查状态机 (模式=" 
        //              << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动")
        //              << ", 状态=" << static_cast<int>(system_state_) 
        //              << ", 步骤=" << auto_sequence_step_ << ")" << std::endl;
        
        switch (system_state_) {
            case SystemState::PUMPING:
                // 抽真空流程仅在自动模式下执行
                if (operation_mode_ == OperationMode::AUTO) {
                    DEBUG_STREAM << "[DEBUG] runSimulation: 自动模式，处理抽真空流程" << std::endl;
                    processAutoVacuumSequence();
                }
                break;
            case SystemState::STOPPING:
                // 停机流程在自动和手动模式下都支持
                DEBUG_STREAM << "[DEBUG] runSimulation: " 
                           << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动") 
                           << "模式，处理停机流程" << std::endl;
                processAutoStopSequence();
                break;
            case SystemState::VENTING:
                // 放气流程在自动和手动模式下都支持
                DEBUG_STREAM << "[DEBUG] runSimulation: 处理放气流程 (模式=" 
                             << (operation_mode_ == OperationMode::AUTO ? "自动" : "手动") << ")" << std::endl;
                processVentSequence();
                break;
            default:
                break;
        }
    }
}

/**
 * @brief 模拟泵频率/转速变化
 * 
 * - 螺杆泵：启动后频率逐渐升到 110Hz
 * - 罗茨泵：启动后频率逐渐升到 50Hz
 * - 分子泵：启动后转速逐渐升到 31000 RPM
 */
void VacuumSystemDevice::simulatePumpBehavior() {
    // 螺杆泵频率
    if (screw_pump_power_) {
        if (screw_pump_frequency_ < 110) {
            screw_pump_frequency_ = std::min(110, screw_pump_frequency_ + 10);
        }
    } else {
        if (screw_pump_frequency_ > 0) {
            screw_pump_frequency_ = std::max(0, screw_pump_frequency_ - 20);
        }
    }
    
    // 罗茨泵频率
    if (roots_pump_power_) {
        if (roots_pump_frequency_ < 50) {
            roots_pump_frequency_ = std::min(50, roots_pump_frequency_ + 10);
        }
    } else {
        if (roots_pump_frequency_ > 0) {
            roots_pump_frequency_ = std::max(0, roots_pump_frequency_ - 15);
        }
    }
    
    // 分子泵1转速
    if (molecular_pump1_power_) {
        if (molecular_pump1_speed_ < 31000) {
            molecular_pump1_speed_ = std::min(31000, molecular_pump1_speed_ + 3000);
        }
    } else {
        if (molecular_pump1_speed_ > 0) {
            molecular_pump1_speed_ = std::max(0, molecular_pump1_speed_ - 5000);
        }
    }
    
    // 分子泵2转速
    if (molecular_pump2_power_) {
        if (molecular_pump2_speed_ < 31000) {
            molecular_pump2_speed_ = std::min(31000, molecular_pump2_speed_ + 3000);
        }
    } else {
        if (molecular_pump2_speed_ > 0) {
            molecular_pump2_speed_ = std::max(0, molecular_pump2_speed_ - 5000);
        }
    }
    
    // 分子泵3转速
    if (molecular_pump3_power_) {
        if (molecular_pump3_speed_ < 31000) {
            molecular_pump3_speed_ = std::min(31000, molecular_pump3_speed_ + 3000);
        }
    } else {
        if (molecular_pump3_speed_ > 0) {
            molecular_pump3_speed_ = std::max(0, molecular_pump3_speed_ - 5000);
        }
    }
}

/**
 * @brief 模拟真空度变化
 * 
 * - 螺杆泵运行：真空度降低（粗抽）
 * - 罗茨泵运行：真空度进一步降低
 * - 分子泵运行：真空度大幅降低（高真空）
 * - 放气阀打开：真空度升高（恢复大气压）
 */
void VacuumSystemDevice::simulateVacuumPhysics() {
    const double ATMOSPHERIC = 101325.0;  // 大气压 Pa
    
    // 1. 检查连通状态：如果任何主抽或旁路闸板阀打开，腔室与泵组连通
    bool connected = gate_valve1_open_ || gate_valve2_open_ || gate_valve3_open_ || gate_valve4_open_;
    
    // 2. 检查动作状态
    bool venting = vent_valve1_open_ || vent_valve2_open_;
    bool screw_running = screw_pump_power_ && (screw_pump_frequency_ > 0);
    bool roots_running = roots_pump_power_ && (roots_pump_frequency_ > 0);
    bool mp_running = (molecular_pump1_power_ || molecular_pump2_power_ || molecular_pump3_power_) && 
                     (molecular_pump1_speed_ > 5000);

    // 3. 计算泵组产生的“真空拉力”（目标值和速度）
    double pump_target = ATMOSPHERIC;
    double pump_speed = 1.0;

    if (mp_running && connected) {
        pump_target = 0.001;
        pump_speed = 0.85;
    } else if (roots_running && (connected || !connected)) { // 罗茨泵始终作用于前级，无论是否连通腔室
        pump_target = 40.0; // 下调至40Pa，确保能触发45Pa的判定
        pump_speed = 0.92;
    } else if (screw_running) {
        // 螺杆泵始终作用于前级真空计 (G1)
        pump_target = 1000.0;
        pump_speed = 0.95;
    }

    // 4. 更新前级真空计 G1 (Foreline)
    if (screw_running) {
        if (vacuum_gauge1_ > pump_target) {
            vacuum_gauge1_ = std::max(pump_target, vacuum_gauge1_ * pump_speed);
        }
    } else if (!venting) {
        // 停机漏气
        vacuum_gauge1_ = std::min(ATMOSPHERIC, vacuum_gauge1_ * 1.001 + 1.0);
    }

    // 5. 更新腔室真空计 G2, G3 (Main 1, Main 2)
    if (venting) {
        // 放气优先
        vacuum_gauge2_ = std::min(ATMOSPHERIC, vacuum_gauge2_ * 1.5 + 2000);
        vacuum_gauge3_ = std::min(ATMOSPHERIC, vacuum_gauge3_ * 1.5 + 2000);
        // 如果连通，前级也一起回升
        if (connected) vacuum_gauge1_ = std::min(ATMOSPHERIC, vacuum_gauge1_ * 1.5 + 2000);
    } else if (connected) {
        // 连通状态：腔室 G2, G3 随泵组下降，并与前级 G1 同步
        if (vacuum_gauge2_ > pump_target) {
            vacuum_gauge2_ = std::max(pump_target, vacuum_gauge2_ * pump_speed);
        }
        // 压差平衡：G2, G3 与 G1 趋于一致
        double avg = (vacuum_gauge2_ + vacuum_gauge1_) / 2.0;
        vacuum_gauge2_ = vacuum_gauge2_ * 0.7 + avg * 0.3;
        vacuum_gauge1_ = vacuum_gauge1_ * 0.7 + avg * 0.3;
        vacuum_gauge3_ = vacuum_gauge2_; // G2 和 G3 视为腔室一体
    } else {
        // 隔离状态：腔室 G2, G3 缓慢回升（漏气）
        vacuum_gauge2_ = std::min(ATMOSPHERIC, vacuum_gauge2_ * 1.0005 + 0.1);
        vacuum_gauge3_ = std::min(ATMOSPHERIC, vacuum_gauge3_ * 1.0005 + 0.1);
    }

    // 模拟气压（始终保持正常）
    air_pressure_ = 0.6;  // 0.6 MPa
}

/**
 * @brief 模拟阀门动作延时
 * 
 * 阀门开关需要一定时间，根据 valve_trackers_ 状态更新开/关状态
 */
void VacuumSystemDevice::simulateValveActions() {
    std::lock_guard<std::mutex> lock(tracker_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    const int VALVE_ACTION_TIME_MS = 2000;  // 阀门动作时间 2 秒
    
    for (auto& pair : valve_trackers_) {
        auto& tracker = pair.second;
        const std::string& valve_id = pair.first;
        
        if (tracker.state == ValveActionState::OPENING || 
            tracker.state == ValveActionState::CLOSING) {
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - tracker.start_time).count();
            
            DEBUG_STREAM << "[DEBUG] simulateValveActions: " << valve_id 
                         << " " << (tracker.state == ValveActionState::OPENING ? "开启" : "关闭")
                         << " 中，已耗时 " << elapsed << " ms" << std::endl;
            
            // 检查是否超时
            if (elapsed > VALVE_TIMEOUT_MS) {
                DEBUG_STREAM << "[DEBUG] simulateValveActions: " << valve_id << " 动作超时" << std::endl;
                if (tracker.state == ValveActionState::OPENING) {
                    tracker.state = ValveActionState::OPEN_TIMEOUT;
                    raiseAlarm(static_cast<int>(AlarmType::GATE_VALVE_1_OPEN_TIMEOUT) + tracker.valve_index,
                              "VALVE_TIMEOUT", valve_id + " 开到位超时", valve_id);
                } else {
                    tracker.state = ValveActionState::CLOSE_TIMEOUT;
                    raiseAlarm(static_cast<int>(AlarmType::GATE_VALVE_1_CLOSE_TIMEOUT) + tracker.valve_index,
                              "VALVE_TIMEOUT", valve_id + " 关到位超时", valve_id);
                }
            }
            // 正常完成动作
            else if (elapsed >= VALVE_ACTION_TIME_MS) {
                DEBUG_STREAM << "[DEBUG] simulateValveActions: " << valve_id 
                             << " 动作完成，更新状态为 " << (tracker.target_open ? "开启" : "关闭") << std::endl;
                // 更新阀门状态
                if (valve_id == "GateValve1") {
                    gate_valve1_open_ = tracker.target_open;
                    gate_valve1_close_ = !tracker.target_open;
                } else if (valve_id == "GateValve2") {
                    gate_valve2_open_ = tracker.target_open;
                    gate_valve2_close_ = !tracker.target_open;
                } else if (valve_id == "GateValve3") {
                    gate_valve3_open_ = tracker.target_open;
                    gate_valve3_close_ = !tracker.target_open;
                } else if (valve_id == "GateValve4") {
                    gate_valve4_open_ = tracker.target_open;
                    gate_valve4_close_ = !tracker.target_open;
                } else if (valve_id == "GateValve5") {
                    gate_valve5_open_ = tracker.target_open;
                    gate_valve5_close_ = !tracker.target_open;
                }
                
                tracker.state = ValveActionState::IDLE;
            }
        }
    }
    
    // 电磁阀和放气阀在模拟模式下立即响应（已在命令处理中设置）
}

// ============================================================================
// 自动流程使用的控制辅助方法（自动处理模拟模式）
// ============================================================================

void VacuumSystemDevice::ctrlScrewPump(bool power) {
    if (sim_mode_) {
        screw_pump_power_ = power;
    } else {
        writePLCBool(VacuumSystemPLCMapping::ScrewPumpPowerOutput(), power);
        writePLCBool(VacuumSystemPLCMapping::ScrewPumpStartStop(), power);
    }
}

void VacuumSystemDevice::ctrlRootsPump(bool power) {
    if (sim_mode_) {
        roots_pump_power_ = power;
    } else {
        writePLCBool(VacuumSystemPLCMapping::RootsPumpPowerOutput(), power);
    }
}

void VacuumSystemDevice::ctrlMolecularPump(int index, bool power) {
    if (sim_mode_) {
        switch (index) {
            case 1: molecular_pump1_power_ = power; break;
            case 2: molecular_pump2_power_ = power; break;
            case 3: molecular_pump3_power_ = power; break;
        }
    } else {
        Common::PLC::PLCAddress power_addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        Common::PLC::PLCAddress start_addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1: 
                power_addr = VacuumSystemPLCMapping::MolecularPump1PowerOutput();
                start_addr = VacuumSystemPLCMapping::MolecularPump1StartStop();
                break;
            case 2: 
                power_addr = VacuumSystemPLCMapping::MolecularPump2PowerOutput();
                start_addr = VacuumSystemPLCMapping::MolecularPump2StartStop();
                break;
            case 3: 
                power_addr = VacuumSystemPLCMapping::MolecularPump3PowerOutput();
                start_addr = VacuumSystemPLCMapping::MolecularPump3StartStop();
                break;
        }
        writePLCBool(power_addr, power);
        writePLCBool(start_addr, power);
    }
}

void VacuumSystemDevice::ctrlElectromagneticValve(int index, bool state) {
    if (sim_mode_) {
        switch (index) {
            case 1: electromagnetic_valve1_open_ = state; electromagnetic_valve1_close_ = !state; break;
            case 2: electromagnetic_valve2_open_ = state; electromagnetic_valve2_close_ = !state; break;
            case 3: electromagnetic_valve3_open_ = state; electromagnetic_valve3_close_ = !state; break;
            case 4: electromagnetic_valve4_open_ = state; electromagnetic_valve4_close_ = !state; break;
        }
    } else {
        Common::PLC::PLCAddress addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1: addr = VacuumSystemPLCMapping::ElectromagneticValve1Output(); break;
            case 2: addr = VacuumSystemPLCMapping::ElectromagneticValve2Output(); break;
            case 3: addr = VacuumSystemPLCMapping::ElectromagneticValve3Output(); break;
            case 4: addr = VacuumSystemPLCMapping::ElectromagneticValve4Output(); break;
        }
        writePLCBool(addr, state);
    }
}

void VacuumSystemDevice::ctrlGateValve(int index, bool open) {
    // 闸板阀5需要允许信号才能操作
    if (index == 5 && !gate_valve5_permit_) {
        WARN_STREAM << "闸板阀5操作被拒绝：缺少允许信号（gateValve5Permit=false）" << std::endl;
        logEvent("闸板阀5操作被拒绝：缺少允许信号");
        return;  // 静默失败，不抛出异常（自动流程中可能需要跳过）
    }
    
    if (sim_mode_) {
        // 模拟模式：启动阀门动作跟踪器
        startValveAction("GateValve" + std::to_string(index), open);
    } else {
        Common::PLC::PLCAddress open_addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        Common::PLC::PLCAddress close_addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1:
                open_addr = VacuumSystemPLCMapping::GateValve1OpenOutput();
                close_addr = VacuumSystemPLCMapping::GateValve1CloseOutput();
                break;
            case 2:
                open_addr = VacuumSystemPLCMapping::GateValve2OpenOutput();
                close_addr = VacuumSystemPLCMapping::GateValve2CloseOutput();
                break;
            case 3:
                open_addr = VacuumSystemPLCMapping::GateValve3OpenOutput();
                close_addr = VacuumSystemPLCMapping::GateValve3CloseOutput();
                break;
            case 4:
                open_addr = VacuumSystemPLCMapping::GateValve4OpenOutput();
                close_addr = VacuumSystemPLCMapping::GateValve4CloseOutput();
                break;
            case 5:
                open_addr = VacuumSystemPLCMapping::GateValve5OpenOutput();
                close_addr = VacuumSystemPLCMapping::GateValve5CloseOutput();
                break;
        }
        if (open) {
            writePLCBool(close_addr, false);
            writePLCBool(open_addr, true);
        } else {
            writePLCBool(open_addr, false);
            writePLCBool(close_addr, true);
        }
        startValveAction("GateValve" + std::to_string(index), open);
    }
}

void VacuumSystemDevice::ctrlVentValve(int index, bool state) {
    if (sim_mode_) {
        switch (index) {
            case 1: vent_valve1_open_ = state; vent_valve1_close_ = !state; break;
            case 2: vent_valve2_open_ = state; vent_valve2_close_ = !state; break;
        }
        DEBUG_STREAM << "[DEBUG] ctrlVentValve: 模拟模式，放气阀" << index 
                     << " 状态设置为 " << (state ? "开启" : "关闭") << std::endl;
    } else {
        Common::PLC::PLCAddress addr(Common::PLC::PLCAddressType::OUTPUT, 0, 0);
        switch (index) {
            case 1: addr = VacuumSystemPLCMapping::VentValve1Output(); break;
            case 2: addr = VacuumSystemPLCMapping::VentValve2Output(); break;
        }
        writePLCBool(addr, state);
        DEBUG_STREAM << "[DEBUG] ctrlVentValve: 已发送PLC命令，放气阀" << index 
                     << " = " << (state ? "true" : "false") 
                     << " (地址: " << addr.address_string << ")" << std::endl;
        
        // 立即尝试读取一次反馈（如果PLC已响应）
        if (plc_comm_ && plc_comm_->isConnected()) {
            if (index == 1) {
                readPLCBool(VacuumSystemPLCMapping::VentValve1OpenFeedback(), vent_valve1_open_);
                readPLCBool(VacuumSystemPLCMapping::VentValve1CloseFeedback(), vent_valve1_close_);
                DEBUG_STREAM << "[DEBUG] ctrlVentValve: 读取反馈 - 放气阀1 开到位=" << vent_valve1_open_ 
                             << ", 关到位=" << vent_valve1_close_ << std::endl;
            } else if (index == 2) {
                readPLCBool(VacuumSystemPLCMapping::VentValve2OpenFeedback(), vent_valve2_open_);
                readPLCBool(VacuumSystemPLCMapping::VentValve2CloseFeedback(), vent_valve2_close_);
                DEBUG_STREAM << "[DEBUG] ctrlVentValve: 读取反馈 - 放气阀2 开到位=" << vent_valve2_open_ 
                             << ", 关到位=" << vent_valve2_close_ << std::endl;
            }
        }
    }
}

// ============================================================================
// 自定义属性类 - 用于转发读取请求到设备的 read_attr 方法
// ============================================================================

class VacuumSystemAttr : public Tango::Attr {
public:
    VacuumSystemAttr(const char* name, long data_type, Tango::AttrWriteType w_type = Tango::READ)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl* dev, Tango::Attribute& att) override {
        static_cast<VacuumSystemDevice*>(dev)->read_attr(att);
    }
    
    virtual void write(Tango::DeviceImpl* dev, Tango::WAttribute& att) override {
        static_cast<VacuumSystemDevice*>(dev)->write_attr(att);
    }
};

// ============================================================================
// VacuumSystemDeviceClass 实现
// ============================================================================

VacuumSystemDeviceClass* VacuumSystemDeviceClass::_instance = nullptr;

VacuumSystemDeviceClass::VacuumSystemDeviceClass(const std::string& class_name)
    : Tango::DeviceClass(class_name) {
    std::cout << "创建 VacuumSystemDeviceClass" << std::endl;
}

VacuumSystemDeviceClass* VacuumSystemDeviceClass::instance() {
    if (_instance == nullptr) {
        std::string class_name = "VacuumSystemDevice";
        _instance = new VacuumSystemDeviceClass(class_name);
    }
    return _instance;
}

VacuumSystemDeviceClass* VacuumSystemDeviceClass::init(const char* class_name) {
    if (!_instance) {
        _instance = new VacuumSystemDeviceClass(class_name);
    }
    return _instance;
}

void VacuumSystemDeviceClass::attribute_factory(std::vector<Tango::Attr*>& att_list) {
    // 系统状态
    att_list.push_back(new VacuumSystemAttr("operationMode", Tango::DEV_SHORT, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("systemState", Tango::DEV_SHORT, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("simulatorMode", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 泵状态
    att_list.push_back(new VacuumSystemAttr("screwPumpPower", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("rootsPumpPower", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("molecularPump1Power", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("molecularPump2Power", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("molecularPump3Power", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("molecularPump1Speed", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("molecularPump2Speed", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("molecularPump3Speed", Tango::DEV_LONG, Tango::READ));
    
    // 分子泵启用配置
    att_list.push_back(new VacuumSystemAttr("molecularPump1Enabled", Tango::DEV_BOOLEAN, Tango::READ_WRITE));
    att_list.push_back(new VacuumSystemAttr("molecularPump2Enabled", Tango::DEV_BOOLEAN, Tango::READ_WRITE));
    att_list.push_back(new VacuumSystemAttr("molecularPump3Enabled", Tango::DEV_BOOLEAN, Tango::READ_WRITE));
    
    // 闸板阀状态
    att_list.push_back(new VacuumSystemAttr("gateValve1Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve1Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve2Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve2Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve3Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve3Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve4Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve4Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve5Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve5Close", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 电磁阀状态
    att_list.push_back(new VacuumSystemAttr("electromagneticValve1Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("electromagneticValve1Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("electromagneticValve2Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("electromagneticValve2Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("electromagneticValve3Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("electromagneticValve3Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("electromagneticValve4Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("electromagneticValve4Close", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 放气阀状态
    att_list.push_back(new VacuumSystemAttr("ventValve1Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("ventValve1Close", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("ventValve2Open", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("ventValve2Close", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 阀门动作状态
    att_list.push_back(new VacuumSystemAttr("gateValve1ActionState", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve2ActionState", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve3ActionState", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve4ActionState", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve5ActionState", Tango::DEV_LONG, Tango::READ));
    
    // 传感器
    att_list.push_back(new VacuumSystemAttr("vacuumGauge1", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("vacuumGauge2", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("vacuumGauge3", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("airPressure", Tango::DEV_DOUBLE, Tango::READ));
    
    // 新增属性 - 泵频率和自动流程
    att_list.push_back(new VacuumSystemAttr("screwPumpFrequency", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("rootsPumpFrequency", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("autoSequenceStep", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("plcConnected", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 新增属性 - 系统联锁信号
    att_list.push_back(new VacuumSystemAttr("phaseSequenceOk", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("motionSystemOnline", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("gateValve5Permit", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 新增属性 - 水电磁阀状态
    att_list.push_back(new VacuumSystemAttr("waterValve1State", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("waterValve2State", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("waterValve3State", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("waterValve4State", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("waterValve5State", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("waterValve6State", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("airMainValveState", Tango::DEV_BOOLEAN, Tango::READ));
    
    // 报警
    att_list.push_back(new VacuumSystemAttr("activeAlarmCount", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("hasUnacknowledgedAlarm", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new VacuumSystemAttr("latestAlarmJson", Tango::DEV_STRING, Tango::READ));
}

// ============================================================================
// Command Templates
// ============================================================================

template<typename F>
class VoidVoidCmd : public Tango::Command {
    F func_;
public:
    VoidVoidCmd(const char* name, Tango::CmdArgType in, Tango::CmdArgType out, F func)
        : Tango::Command(name, in, out), func_(func) {}
    virtual CORBA::Any* execute(Tango::DeviceImpl* dev, const CORBA::Any& /*in_any*/) {
        (static_cast<VacuumSystemDevice*>(dev)->*func_)();
        return new CORBA::Any();
    }
};

template<typename F>
class BoolVoidCmd : public Tango::Command {
    F func_;
public:
    BoolVoidCmd(const char* name, Tango::CmdArgType in, Tango::CmdArgType out, F func)
        : Tango::Command(name, in, out), func_(func) {}
    virtual CORBA::Any* execute(Tango::DeviceImpl* dev, const CORBA::Any& in_any) {
        Tango::DevBoolean argin;
        extract(in_any, argin);
        (static_cast<VacuumSystemDevice*>(dev)->*func_)(argin);
        return new CORBA::Any();
    }
};

template<typename F>
class ShortArrayVoidCmd : public Tango::Command {
    F func_;
public:
    ShortArrayVoidCmd(const char* name, Tango::CmdArgType in, Tango::CmdArgType out, F func)
        : Tango::Command(name, in, out), func_(func) {}
    virtual CORBA::Any* execute(Tango::DeviceImpl* dev, const CORBA::Any& in_any) {
        const Tango::DevVarShortArray* argin;
        extract(in_any, argin);
        (static_cast<VacuumSystemDevice*>(dev)->*func_)(argin);
        return new CORBA::Any();
    }
};

template<typename F>
class LongVoidCmd : public Tango::Command {
    F func_;
public:
    LongVoidCmd(const char* name, Tango::CmdArgType in, Tango::CmdArgType out, F func)
        : Tango::Command(name, in, out), func_(func) {}
    virtual CORBA::Any* execute(Tango::DeviceImpl* dev, const CORBA::Any& in_any) {
        Tango::DevLong argin;
        extract(in_any, argin);
        (static_cast<VacuumSystemDevice*>(dev)->*func_)(argin);
        return new CORBA::Any();
    }
};

template<typename F>
class StringStringCmd : public Tango::Command {
    F func_;
public:
    StringStringCmd(const char* name, Tango::CmdArgType in, Tango::CmdArgType out, F func)
        : Tango::Command(name, in, out), func_(func) {}
    virtual CORBA::Any* execute(Tango::DeviceImpl* dev, const CORBA::Any& in_any) {
        Tango::DevString argin;
        extract(in_any, argin);
        Tango::DevString argout = (static_cast<VacuumSystemDevice*>(dev)->*func_)(argin);
        return insert(argout);
    }
};

template<typename F>
class VoidStringCmd : public Tango::Command {
    F func_;
public:
    VoidStringCmd(const char* name, Tango::CmdArgType in, Tango::CmdArgType out, F func)
        : Tango::Command(name, in, out), func_(func) {}
    virtual CORBA::Any* execute(Tango::DeviceImpl* dev, const CORBA::Any& /*in_any*/) {
        Tango::DevString argout = (static_cast<VacuumSystemDevice*>(dev)->*func_)();
        return insert(argout);
    }
};

void VacuumSystemDeviceClass::command_factory() {
    // 系统命令
    command_list.push_back(new VoidVoidCmd("Init", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::Init));
    command_list.push_back(new VoidVoidCmd("Reset", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::Reset));
    command_list.push_back(new VoidVoidCmd("SelfCheck", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::SelfCheck));
    
    // 模式切换
    command_list.push_back(new VoidVoidCmd("SwitchToAuto", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::SwitchToAuto));
    command_list.push_back(new VoidVoidCmd("SwitchToManual", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::SwitchToManual));
    
    // 自动操作
    command_list.push_back(new VoidVoidCmd("OneKeyVacuumStart", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::OneKeyVacuumStart));
    command_list.push_back(new VoidVoidCmd("OneKeyVacuumStop", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::OneKeyVacuumStop));
    command_list.push_back(new VoidVoidCmd("ChamberVent", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::ChamberVent));
    command_list.push_back(new VoidVoidCmd("FaultReset", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::FaultReset));
    command_list.push_back(new VoidVoidCmd("EmergencyStop", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::EmergencyStop));
    
    // 泵控制
    command_list.push_back(new BoolVoidCmd("SetScrewPumpPower", Tango::DEV_BOOLEAN, Tango::DEV_VOID, &VacuumSystemDevice::SetScrewPumpPower));
    command_list.push_back(new BoolVoidCmd("SetScrewPumpStartStop", Tango::DEV_BOOLEAN, Tango::DEV_VOID, &VacuumSystemDevice::SetScrewPumpStartStop));
    command_list.push_back(new BoolVoidCmd("SetRootsPumpPower", Tango::DEV_BOOLEAN, Tango::DEV_VOID, &VacuumSystemDevice::SetRootsPumpPower));
    command_list.push_back(new ShortArrayVoidCmd("SetMolecularPumpPower", Tango::DEVVAR_SHORTARRAY, Tango::DEV_VOID, &VacuumSystemDevice::SetMolecularPumpPower));
    command_list.push_back(new ShortArrayVoidCmd("SetMolecularPumpStartStop", Tango::DEVVAR_SHORTARRAY, Tango::DEV_VOID, &VacuumSystemDevice::SetMolecularPumpStartStop));
    
    // 阀门控制
    command_list.push_back(new ShortArrayVoidCmd("SetGateValve", Tango::DEVVAR_SHORTARRAY, Tango::DEV_VOID, &VacuumSystemDevice::SetGateValve));
    command_list.push_back(new ShortArrayVoidCmd("SetElectromagneticValve", Tango::DEVVAR_SHORTARRAY, Tango::DEV_VOID, &VacuumSystemDevice::SetElectromagneticValve));
    command_list.push_back(new ShortArrayVoidCmd("SetVentValve", Tango::DEVVAR_SHORTARRAY, Tango::DEV_VOID, &VacuumSystemDevice::SetVentValve));
    command_list.push_back(new ShortArrayVoidCmd("SetWaterValve", Tango::DEVVAR_SHORTARRAY, Tango::DEV_VOID, &VacuumSystemDevice::SetWaterValve));
    command_list.push_back(new BoolVoidCmd("SetAirMainValve", Tango::DEV_BOOLEAN, Tango::DEV_VOID, &VacuumSystemDevice::SetAirMainValve));
    
    // 报警管理
    command_list.push_back(new LongVoidCmd("AcknowledgeAlarm", Tango::DEV_LONG, Tango::DEV_VOID, &VacuumSystemDevice::AcknowledgeAlarm));
    command_list.push_back(new VoidVoidCmd("AcknowledgeAllAlarms", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::AcknowledgeAllAlarms));
    command_list.push_back(new VoidVoidCmd("ClearAlarmHistory", Tango::DEV_VOID, Tango::DEV_VOID, &VacuumSystemDevice::ClearAlarmHistory));
    
    // 查询命令
    command_list.push_back(new StringStringCmd("GetOperationConditions", Tango::DEV_STRING, Tango::DEV_STRING, &VacuumSystemDevice::GetOperationConditions));
    command_list.push_back(new VoidStringCmd("GetActiveAlarms", Tango::DEV_VOID, Tango::DEV_STRING, &VacuumSystemDevice::GetActiveAlarms));
    command_list.push_back(new VoidStringCmd("GetSystemStatus", Tango::DEV_VOID, Tango::DEV_STRING, &VacuumSystemDevice::GetSystemStatus));
}

void VacuumSystemDeviceClass::device_factory(const Tango::DevVarStringArray* devlist_ptr) {
    for (unsigned int i = 0; i < devlist_ptr->length(); i++) {
        std::string device_name((*devlist_ptr)[i]);
        VacuumSystemDevice* dev = new VacuumSystemDevice(this, device_name);
        device_list.push_back(dev);
        export_device(dev);  // 导出设备，使其可被客户端访问
    }
}

// ============================================================================
// Tango 类工厂
// ============================================================================

extern "C" {

Tango::DeviceClass* _create_VacuumSystemDevice_class(const char* class_name) {
    return VacuumSystem::VacuumSystemDeviceClass::init(class_name);
}

} // extern "C"

// ============================================================================
// Tango DServer 类工厂 - 必须定义此函数来注册设备类
// ============================================================================

void Tango::DServer::class_factory() {
    add_class(VacuumSystem::VacuumSystemDeviceClass::instance());
}

// ============================================================================
// Main 函数
// ============================================================================

int main(int argc, char* argv[]) {
    // 禁用 stdout/stderr 缓冲，确保日志实时输出
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    
    try {
        // 加载系统配置
        Common::SystemConfig::loadConfig();
        
        // 初始化 Tango 设备服务器
        Tango::Util* tg = Tango::Util::init(argc, argv);
        
        // 启动服务器事件循环
        tg->server_init();
        std::cout << "VacuumSystemDevice server started" << std::endl;
        tg->server_run();
        
    } catch (std::bad_alloc&) {
        std::cerr << "Can't allocate memory!!!" << std::endl;
        return -1;
    } catch (CORBA::Exception& e) {
        Tango::Except::print_exception(e);
        return -1;
    }
    
    return 0;
}

