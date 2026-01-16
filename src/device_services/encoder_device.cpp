#include "device_services/encoder_device.h"
#include "common/system_config.h"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <fstream>

namespace Encoder {

EncoderDevice::EncoderDevice(Tango::DeviceClass *cl, std::string &name)
        : Tango::Device_4Impl(cl, name.c_str()), self_check_result_(-1),
            is_connected_(false), sim_mode_(false), is_locked_(false) {
    motor_pos_.fill(0.0);
    encoder_resolution_arr_.fill(1.0);  // Default resolution
    zero_offset_.fill(0.0);
    position_unit_ = "step";
    init_device();
}

EncoderDevice::EncoderDevice(Tango::DeviceClass *cl, const char *name)
        : Tango::Device_4Impl(cl, name), self_check_result_(-1),
            is_connected_(false), sim_mode_(false), is_locked_(false) {
    motor_pos_.fill(0.0);
    encoder_resolution_arr_.fill(1.0);
    zero_offset_.fill(0.0);
    position_unit_ = "step";
    init_device();
}

EncoderDevice::EncoderDevice(Tango::DeviceClass *cl, const char *name, const char *description)
        : Tango::Device_4Impl(cl, name, description), self_check_result_(-1),
            is_connected_(false), sim_mode_(false), is_locked_(false) {
    motor_pos_.fill(0.0);
    encoder_resolution_arr_.fill(1.0);
    zero_offset_.fill(0.0);
    position_unit_ = "step";
    init_device();
}

EncoderDevice::~EncoderDevice() {
    delete_device();
}

void EncoderDevice::init_device() {
    std::cout << "EncoderDevice::init_device() " << get_name() << std::endl;
    set_state(Tango::INIT);

    if (encoder_manager_) {
        encoder_manager_->stop();
        encoder_manager_.reset();
    }

    // Get properties
    Tango::DbData db_data;
    db_data.push_back(Tango::DbDatum("bundleNo"));
    db_data.push_back(Tango::DbDatum("laserNo"));
    db_data.push_back(Tango::DbDatum("systemNo"));
    db_data.push_back(Tango::DbDatum("subDevList"));
    db_data.push_back(Tango::DbDatum("modelList"));
    db_data.push_back(Tango::DbDatum("currentModel"));
    db_data.push_back(Tango::DbDatum("connectString"));
    db_data.push_back(Tango::DbDatum("errorDict"));
    db_data.push_back(Tango::DbDatum("deviceName"));
    db_data.push_back(Tango::DbDatum("deviceID"));
    db_data.push_back(Tango::DbDatum("deviceModel"));
    db_data.push_back(Tango::DbDatum("devicePosition"));
    db_data.push_back(Tango::DbDatum("deviceProductDate"));
    db_data.push_back(Tango::DbDatum("deviceInstallDate"));
    db_data.push_back(Tango::DbDatum("encoderResolution"));
    db_data.push_back(Tango::DbDatum("axis_ids"));
    // 编码器采集器配置 Property
    db_data.push_back(Tango::DbDatum("encoderCollectorIPs"));
    db_data.push_back(Tango::DbDatum("encoderCollectorPorts"));
    db_data.push_back(Tango::DbDatum("channelsPerCollector"));
    get_db_device()->get_property(db_data);

    int idx = 0;
    if (!db_data[idx].is_empty()) { db_data[idx] >> bundle_no_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> laser_no_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> system_no_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> sub_dev_list_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> model_list_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> current_model_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> connect_string_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> error_dict_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_name_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_id_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_model_; }
    else device_model_ = "AELAN10";  // 默认型号：哈尔滨明快机电 AELAN10
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_position_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_product_date_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_install_date_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_resolution_prop_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> axis_ids_; } idx++;
    // 读取编码器采集器配置
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_collector_ips_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_collector_ports_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> channels_per_collector_; }
    else { channels_per_collector_ = 10; }  // 默认每个采集器10通道
    
    if (axis_ids_.empty()) {
        // 默认使用20个通道（对应2个AELAN10编码器，每个10通道）
        // 第一个AELAN10: 通道 0-9
        // 第二个AELAN10: 通道 10-19
        for (int i = 0; i < 20; i++) {
            axis_ids_.push_back(i);
        }
        log_event("Using default 20 encoder channels (2x AELAN10, 10 channels each)");
    }
    
    // 解析encoderResolution Property（JSON格式）
    parse_encoder_resolution_prop();

    // 从系统配置读取模拟模式设置（启动时的初始默认值）
    // 注意：运行时可以通过 simSwitch 命令或GUI切换，但重启后会恢复配置文件的值（不持久化）
    sim_mode_ = Common::SystemConfig::SIM_MODE;
    if (sim_mode_) {
        INFO_STREAM << "========================================" << std::endl;
        INFO_STREAM << "  模拟模式已启用 (从配置文件加载, SIM_MODE=true)" << std::endl;
        INFO_STREAM << "  不连接真实硬件，使用内部模拟逻辑" << std::endl;
        INFO_STREAM << "========================================" << std::endl;
        is_connected_ = true;
        set_state(Tango::ON);
        set_status("Simulation Mode - Ready");
        log_event("Encoder device initialized in simulation mode");
    } else {
        INFO_STREAM << "[DEBUG] 运行模式: 真实模式 (从配置文件加载, SIM_MODE=false)" << std::endl;
        INFO_STREAM << "[DEBUG] 提示: 可通过 simSwitch 命令或GUI动态切换模拟模式" << std::endl;
        
        // 根据配置创建 EncoderAcquisitionManager
        if (!encoder_collector_ips_.empty()) {
            // 使用配置的 IP 和端口
            std::vector<int> ports;
            for (size_t i = 0; i < encoder_collector_ips_.size(); ++i) {
                int port = (i < encoder_collector_ports_.size()) ? 
                           static_cast<int>(encoder_collector_ports_[i]) : 
                           Common::EncoderAcquisitionManager::kDefaultPort;
                ports.push_back(port);
            }
            encoder_manager_ = std::make_unique<Common::EncoderAcquisitionManager>(
                encoder_collector_ips_, ports, static_cast<int>(channels_per_collector_));
            
            INFO_STREAM << "[DEBUG] 使用配置的编码器采集器:" << std::endl;
            for (size_t i = 0; i < encoder_collector_ips_.size(); ++i) {
                INFO_STREAM << "  [" << i << "] " << encoder_collector_ips_[i] 
                           << ":" << ports[i] << std::endl;
            }
        } else {
            // 使用默认配置
            encoder_manager_ = std::make_unique<Common::EncoderAcquisitionManager>();
            INFO_STREAM << "[DEBUG] 使用默认编码器采集器配置 (192.168.1.15, 192.168.1.16)" << std::endl;
        }
        
        encoder_manager_->start();
        is_connected_ = true; // manager will handle reconnects per device internally
        set_state(Tango::ON);
        set_status("Encoder acquisition started");
        log_event(encoder_manager_->status_summary());
        log_event("Encoder device initialized with " + 
                  std::to_string(encoder_manager_->collector_count()) + " collector(s)");
    }
}

void EncoderDevice::delete_device() {
    if (encoder_manager_) {
        encoder_manager_->stop();
    }
}

void EncoderDevice::check_connection() {
    // 模拟模式下只检查 is_connected_ 标志
    if (sim_mode_) {
        if (!is_connected_) {
            Tango::Except::throw_exception("HardwareError", 
                "Device not connected (Simulation)", "EncoderDevice::check_connection");
        }
        return;
    }
    
    // 真实模式下检查 encoder_manager_
    if (!is_connected_ || !encoder_manager_) {
        Tango::Except::throw_exception("HardwareError", 
            "Encoder acquisition not initialized", "EncoderDevice::check_connection");
    }
    if (!encoder_manager_->has_any_connection()) {
        Tango::Except::throw_exception("HardwareError", 
            "No encoder collector connected", "EncoderDevice::check_connection");
    }
}

bool EncoderDevice::is_valid_channel(short channel) const {
    return std::find(axis_ids_.begin(), axis_ids_.end(), channel) != axis_ids_.end();
}

// ========== State Machine Helpers ==========
bool EncoderDevice::is_state_allowed(StateMachineRule rule) const {
    Tango::DevState current_state = const_cast<EncoderDevice*>(this)->get_state();
    
    switch (rule) {
        case StateMachineRule::ALL_STATES:
            return true;
            
        case StateMachineRule::NOT_ON:
            // UNKNOWN/OFF/FAULT可用，ON不可用
            return (current_state == Tango::UNKNOWN || 
                    current_state == Tango::OFF || 
                    current_state == Tango::FAULT);
            
        case StateMachineRule::NOT_UNKNOWN:
            // OFF/ON/FAULT可用，UNKNOWN不可用
            return (current_state == Tango::OFF || 
                    current_state == Tango::ON || 
                    current_state == Tango::FAULT);
            
        case StateMachineRule::ONLY_ON:
            // 仅ON状态可用
            return (current_state == Tango::ON);
    }
    return false;
}

void EncoderDevice::check_state_for_command(const std::string &cmd_name, StateMachineRule rule) {
    Tango::DevState current_state = get_state();
    std::string state_name = Tango::DevStateName[current_state];
    
    std::string allowed_states;
    switch (rule) {
        case StateMachineRule::ALL_STATES:
            allowed_states = "ALL";
            break;
        case StateMachineRule::NOT_ON:
            allowed_states = "UNKNOWN, OFF, FAULT";
            break;
        case StateMachineRule::NOT_UNKNOWN:
            allowed_states = "OFF, ON, FAULT";
            break;
        case StateMachineRule::ONLY_ON:
            allowed_states = "ON";
            break;
    }
    
    if (!is_state_allowed(rule)) {
        std::string error_msg = "Command " + cmd_name + " not allowed in " + state_name + 
                               " state. Allowed states: " + allowed_states;
        Tango::Except::throw_exception("API_InvalidState", error_msg, 
            "EncoderDevice::check_state_for_command");
    }
}

// ========== JSON Parsing Helpers ==========
std::string EncoderDevice::parse_json_string(const std::string& json, const std::string& key) {
    // Simple JSON string value parser
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) return "";
    
    size_t colon_pos = json.find(':', key_pos + search_key.length());
    if (colon_pos == std::string::npos) return "";
    
    size_t quote_start = json.find('"', colon_pos + 1);
    if (quote_start == std::string::npos) return "";
    
    size_t quote_end = json.find('"', quote_start + 1);
    if (quote_end == std::string::npos) return "";
    
    return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

double EncoderDevice::parse_json_double(const std::string& json, const std::string& key, double default_val) {
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) return default_val;
    
    size_t colon_pos = json.find(':', key_pos + search_key.length());
    if (colon_pos == std::string::npos) return default_val;
    
    // Skip whitespace
    size_t num_start = colon_pos + 1;
    while (num_start < json.length() && (json[num_start] == ' ' || json[num_start] == '\t')) {
        num_start++;
    }
    
    // Find the end of the number (stop at comma, brace, or whitespace)
    size_t num_end = num_start;
    while (num_end < json.length()) {
        char c = json[num_end];
        if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            break;
        }
        num_end++;
    }
    
    if (num_end <= num_start) return default_val;
    
    // Parse double from the extracted substring
    try {
        return std::stod(json.substr(num_start, num_end - num_start));
    } catch (...) {
        return default_val;
    }
}

void EncoderDevice::parse_encoder_resolution_prop() {
    if (encoder_resolution_prop_.empty() || encoder_resolution_prop_ == "{}") {
        // 如果没有配置，使用默认值1.0
        log_event("Using default resolution 1.0 for all channels");
        return;
    }
    
    // 解析JSON格式：{"0": 1.0, "1": 1.0} 或 {"ChannelNumber": "0", "Resolution": "1.0"}
    // 尝试解析每个通道的分辨率
    for (size_t i = 0; i < axis_ids_.size() && i < MAX_ENCODER_CHANNELS; i++) {
        short channel = axis_ids_[i];
        std::string channel_key = std::to_string(channel);
        
        // 尝试格式1: {"0": 1.0, "1": 1.0}
        double resolution = parse_json_double(encoder_resolution_prop_, channel_key, encoder_resolution_arr_[channel]);
        if (resolution != encoder_resolution_arr_[channel]) {
            encoder_resolution_arr_[channel] = resolution;
            log_event("Parsed resolution for channel " + std::to_string(channel) + ": " + std::to_string(resolution));
        }
    }
}

void EncoderDevice::log_event(const std::string &event) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << ": " << event << "\n";
    encoder_logs_ += ss.str();
    
    // 按行截断日志，保持最后约8000字符，但不截断到行中间
    if (encoder_logs_.size() > 10000) {
        size_t trim_pos = encoder_logs_.size() - 8000;
        // 找到下一个换行符，从完整行开始
        size_t newline_pos = encoder_logs_.find('\n', trim_pos);
        if (newline_pos != std::string::npos && newline_pos < encoder_logs_.size()) {
            encoder_logs_ = encoder_logs_.substr(newline_pos + 1);
        } else {
            encoder_logs_ = encoder_logs_.substr(trim_pos);
        }
    }
}

void EncoderDevice::update_motor_positions() {
    if (sim_mode_) {
        return;
    }
    for (size_t i = 0; i < axis_ids_.size() && i < MAX_ENCODER_CHANNELS; i++) {
        short channel = axis_ids_[i];
        motor_pos_[static_cast<size_t>(channel)] = read_position_from_cache(channel, "update_motor_positions");
    }
}

double EncoderDevice::read_raw_from_cache(short channel, const std::string &context) {
    if (!is_valid_channel(channel)) {
        Tango::Except::throw_exception("InvalidChannel",
            "Channel " + std::to_string(channel) + " is not configured", context);
    }
    if (channel >= MAX_ENCODER_CHANNELS || channel < 0) {
        Tango::Except::throw_exception("InvalidChannel",
            "Channel " + std::to_string(channel) + " exceeds maximum", context);
    }

    if (sim_mode_) {
        // Convert current simulated position back to raw counts
        double raw = (motor_pos_[channel] / encoder_resolution_arr_[channel]) + zero_offset_[channel];
        return static_cast<uint32_t>(raw);
    }

    check_connection();

    Common::EncoderReading reading;
    bool ok = encoder_manager_ && encoder_manager_->get_reading(channel, reading);
    auto now = std::chrono::steady_clock::now();
    auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - reading.timestamp).count();

    if (!ok || !reading.valid) {
        fault_state_ = "No encoder data for channel " + std::to_string(channel);
        Tango::Except::throw_exception("DataUnavailable", fault_state_, context);
    }
    if (age_ms > DATA_TIMEOUT_MS) {
        fault_state_ = "Encoder data stale for channel " + std::to_string(channel) +
                       " (age=" + std::to_string(age_ms) + "ms)";
        Tango::Except::throw_exception("DataTimeout", fault_state_, context);
    }

    return reading.combined_value;
}

double EncoderDevice::read_position_from_cache(short channel, const std::string &context) {
    double combined_value = read_raw_from_cache(channel, context);
    size_t idx = static_cast<size_t>(channel);
    
    // 计算减去零点偏移后的值
    double value_after_zero = combined_value - zero_offset_[idx];
    
    // 分离整数部分和小数部分
    double integer_part = std::floor(value_after_zero);
    double fractional_part = value_after_zero - integer_part;
    
    // 小数部分处理: 小数 * 1000000 / 2^17，继续作为小数
    // 2^17 = 131072
    double processed_fraction = (fractional_part * 1000000.0) / 131072.0;
    
    // 最终位置 = 整数部分(圈数) + 处理后的小数部分
    double pos = integer_part + processed_fraction;
    
    motor_pos_[idx] = pos;
    return pos;
}

// ========== Lock/Unlock Commands ==========
void EncoderDevice::devLock(Tango::DevString argin) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string client_info = argin;
    if (is_locked_ && locker_info_ != client_info) {
        Tango::Except::throw_exception("Locked", "Device is already locked by: " + locker_info_, "devLock");
    }
    is_locked_ = true;
    locker_info_ = client_info;
    log_event("Device locked by: " + locker_info_);
}

void EncoderDevice::devUnlock(Tango::DevBoolean /*argin*/) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (is_locked_) {
        log_event("Device unlocked (was: " + locker_info_ + ")");
        is_locked_ = false;
        locker_info_ = "";
    }
}

void EncoderDevice::devLockVerify() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (!is_locked_) {
        Tango::Except::throw_exception("NotLocked", "Device is not locked", "devLockVerify");
    }
}

Tango::DevString EncoderDevice::devLockQuery() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string result = is_locked_ ? 
        "{\"locked\": true, \"locker\": \"" + locker_info_ + "\"}" : "{\"locked\": false}";
    return Tango::string_dup(result.c_str());
}

void EncoderDevice::devUserConfig(Tango::DevString argin) {
    log_event(std::string("User config: ") + argin);
}

// ========== System Commands ==========
void EncoderDevice::selfCheck() {
    // 状态检查：selfCheck在 UNKNOWN/OFF/FAULT 状态可用，ON状态不可用
    check_state_for_command("selfCheck", StateMachineRule::NOT_ON);
    
    log_event("Starting self check...");
    self_check_result_ = -1;

    if (sim_mode_) {
        self_check_result_ = 0;
        log_event("Self check completed successfully (Simulation)");
        return;
    }

    try {
        check_connection();
        for (size_t i = 0; i < axis_ids_.size() && i < MAX_ENCODER_CHANNELS; i++) {
            try {
                (void)read_raw_from_cache(axis_ids_[i], "selfCheck");
            } catch (...) {
                self_check_result_ = 1;
                log_event("Self check failed: channel " + std::to_string(axis_ids_[i]));
                return;
            }
        }
        self_check_result_ = 0;
        log_event("Self check completed successfully");
    } catch (...) {
        self_check_result_ = 4;
        log_event("Self check failed with exception");
    }
}

void EncoderDevice::init() {
    // 状态检查：init在 UNKNOWN/OFF/FAULT 状态可用，ON状态不可用
    check_state_for_command("init", StateMachineRule::NOT_ON);
    init_device();
}

void EncoderDevice::connect() {
    // 状态检查：connect在 UNKNOWN/OFF/FAULT 状态可用，ON状态不可用
    check_state_for_command("connect", StateMachineRule::NOT_ON);
    
    if (is_connected_) return;

    if (sim_mode_) {
        is_connected_ = true;
        fault_state_ = "";
        set_state(Tango::ON);
        set_status("Simulation Mode - Connected");
        log_event("Connected (Simulation)");
        return;
    }

    if (!encoder_manager_) {
        encoder_manager_ = std::make_unique<Common::EncoderAcquisitionManager>();
    }
    encoder_manager_->start();
    is_connected_ = true;
    fault_state_ = "";
    set_state(Tango::ON);
    set_status(encoder_manager_->status_summary());
    log_event("Connected to encoder collectors");
}

void EncoderDevice::disconnect() {
    if (!is_connected_) return;
    if (encoder_manager_) {
        encoder_manager_->stop();
    }
    is_connected_ = false;
    set_state(Tango::OFF);
    log_event("Disconnected");
}

// ========== Encoder Commands ==========
Tango::DevDouble EncoderDevice::readEncoder(Tango::DevShort channel) {
    // 状态检查：readEncoder在 OFF/ON/FAULT 状态可用，UNKNOWN不可用
    check_state_for_command("readEncoder", StateMachineRule::NOT_UNKNOWN);
    
    // 验证通道是否在配置的axis_ids中
    if (!is_valid_channel(channel)) {
        Tango::Except::throw_exception("InvalidChannel", 
            "Channel " + std::to_string(channel) + " is not configured. Valid channels: " + 
            std::to_string(axis_ids_.size()) + " channels", "readEncoder");
    }
    
    if (channel >= MAX_ENCODER_CHANNELS || channel < 0) {
        Tango::Except::throw_exception("InvalidChannel", 
            "Channel " + std::to_string(channel) + " exceeds maximum (" + 
            std::to_string(MAX_ENCODER_CHANNELS) + ")", "readEncoder");
    }
    
    // 模拟模式下直接返回缓存值
    if (sim_mode_) {
        return motor_pos_[channel];
    }
    
    // 真实模式下检查连接并读取
    check_connection();
    return read_position_from_cache(channel, "readEncoder");
}

void EncoderDevice::setEncoderResolution(const Tango::DevVarDoubleArray *argin) {
    // 状态检查：setEncoderResolution在 OFF/ON/FAULT 状态可用，UNKNOWN不可用
    check_state_for_command("setEncoderResolution", StateMachineRule::NOT_UNKNOWN);
    
    if (argin->length() < 2) {
        Tango::Except::throw_exception("InvalidArgs", "Requires [channel, resolution]", "setEncoderResolution");
    }
    short channel = static_cast<short>((*argin)[0]);
    double resolution = (*argin)[1];
    
    // 验证通道
    if (!is_valid_channel(channel)) {
        Tango::Except::throw_exception("InvalidChannel", 
            "Channel " + std::to_string(channel) + " is not configured", "setEncoderResolution");
    }
    
    if (channel >= MAX_ENCODER_CHANNELS || channel < 0) {
        Tango::Except::throw_exception("InvalidChannel", 
            "Channel " + std::to_string(channel) + " exceeds maximum", "setEncoderResolution");
    }
    
    encoder_resolution_arr_[channel] = resolution;
    log_event("Set resolution for channel " + std::to_string(channel) + " to " + std::to_string(resolution));
}

void EncoderDevice::makeZero(Tango::DevShort channel) {
    // 状态检查：makeZero在 ON 状态可用（需要设备正常运行）
    check_state_for_command("makeZero", StateMachineRule::ONLY_ON);
    
    // 验证通道
    if (!is_valid_channel(channel)) {
        Tango::Except::throw_exception("InvalidChannel", 
            "Channel " + std::to_string(channel) + " is not configured", "makeZero");
    }
    
    if (channel >= MAX_ENCODER_CHANNELS || channel < 0) {
        Tango::Except::throw_exception("InvalidChannel", 
            "Channel " + std::to_string(channel) + " exceeds maximum", "makeZero");
    }
    
    // 模拟模式下设置零点
    if (sim_mode_) {
        double current_sim_raw = (motor_pos_[channel] / encoder_resolution_arr_[channel]) + zero_offset_[channel];
        zero_offset_[channel] = current_sim_raw;
        motor_pos_[channel] = 0.0;
        log_event("Set zero for channel " + std::to_string(channel) + " (Simulation)");
        return;
    }

    // 真实模式下检查连接并设置零点
    check_connection();
    double current_raw = read_raw_from_cache(channel, "makeZero");
    zero_offset_[channel] = current_raw;
    log_event("Set zero for channel " + std::to_string(channel));
}

void EncoderDevice::reset(Tango::DevShort channel) {
    // reset 在所有状态下可用，可以清除 FAULT 状态
    check_state_for_command("reset", StateMachineRule::ALL_STATES);
    
    // 验证通道
    if (!is_valid_channel(channel)) {
        Tango::Except::throw_exception("InvalidChannel", 
            "Channel " + std::to_string(channel) + " is not configured", "reset");
    }
    
    if (channel >= MAX_ENCODER_CHANNELS || channel < 0) {
        Tango::Except::throw_exception("InvalidChannel", 
            "Channel " + std::to_string(channel) + " exceeds maximum", "reset");
    }
    
    encoder_resolution_arr_[channel] = 1.0;
    zero_offset_[channel] = 0.0;
    log_event("Reset channel " + std::to_string(channel));
    
    // 如果处于 FAULT 状态，恢复到正常状态
    if (get_state() == Tango::FAULT) {
        fault_state_ = "";
        if (is_connected_) {
            set_state(Tango::ON);
            set_status("Ready - Fault cleared by reset");
        } else {
            set_state(Tango::OFF);
            set_status("Disconnected");
        }
        log_event("Fault state cleared by reset");
    }
}

void EncoderDevice::exportLogs() {
    // 状态检查：exportLogs 在 OFF/ON/FAULT 状态可用，UNKNOWN不可用
    check_state_for_command("exportLogs", StateMachineRule::NOT_UNKNOWN);
    
    log_event("Exporting logs to file...");
    
    // 导出日志到本地文件
    try {
        auto now = std::time(nullptr);
        std::ostringstream filename;
        filename << "encoder_logs_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".txt";
        
        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << encoder_logs_;
            file.close();
            log_event("Logs exported to " + filename.str());
        } else {
            Tango::Except::throw_exception("API_FileError",
                "Failed to create log file", "EncoderDevice::exportLogs");
        }
    } catch (std::exception& e) {
        Tango::Except::throw_exception("API_ExportError",
            std::string("Failed to export logs: ") + e.what(), "EncoderDevice::exportLogs");
    }
}

void EncoderDevice::exportResolution() {
    // 状态检查：exportResolution 在 OFF/ON/FAULT 状态可用，UNKNOWN不可用
    check_state_for_command("exportResolution", StateMachineRule::NOT_UNKNOWN);
    
    log_event("Exporting resolution to file...");
    
    // 导出分辨率到本地文件（JSON格式）
    try {
        auto now = std::time(nullptr);
        std::ostringstream filename;
        filename << "encoder_resolution_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".json";
        
        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << "{";
            for (size_t i = 0; i < axis_ids_.size() && i < MAX_ENCODER_CHANNELS; i++) {
                if (i > 0) file << ",";
                file << "\"" << axis_ids_[i] << "\":" << encoder_resolution_arr_[axis_ids_[i]];
            }
            file << "}";
            file.close();
            log_event("Resolution exported to " + filename.str());
        } else {
            Tango::Except::throw_exception("API_FileError",
                "Failed to create resolution file", "EncoderDevice::exportResolution");
        }
    } catch (std::exception& e) {
        Tango::Except::throw_exception("API_ExportError",
            std::string("Failed to export resolution: ") + e.what(), "EncoderDevice::exportResolution");
    }
}

void EncoderDevice::simSwitch(Tango::DevShort mode) {
    // 状态检查：simSwitch在 OFF/ON/FAULT 状态可用，UNKNOWN不可用
    check_state_for_command("simSwitch", StateMachineRule::NOT_UNKNOWN);
    
    bool was_sim_mode = sim_mode_;
    sim_mode_ = (mode == 1);
    
    // 注意：运行时切换只影响当前会话，server 重启后恢复配置文件的值（不持久化）
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] simSwitch: Enabling SIMULATION MODE (运行时切换，不持久化)" << std::endl;
        if (encoder_manager_) {
            encoder_manager_->stop();
            encoder_manager_.reset();
        }
        is_connected_ = true;
        set_state(Tango::ON);
        set_status("Simulation Mode - Ready");
        log_event("Simulation mode enabled (runtime switch)");
    } else {
        INFO_STREAM << "[DEBUG] simSwitch: Disabling simulation mode, switching to REAL MODE (运行时切换，不持久化)" << std::endl;
        if (was_sim_mode && !encoder_manager_) {
            // 使用配置创建 EncoderAcquisitionManager
            if (!encoder_collector_ips_.empty()) {
                std::vector<int> ports;
                for (size_t i = 0; i < encoder_collector_ips_.size(); ++i) {
                    int port = (i < encoder_collector_ports_.size()) ? 
                               static_cast<int>(encoder_collector_ports_[i]) : 
                               Common::EncoderAcquisitionManager::kDefaultPort;
                    ports.push_back(port);
                }
                encoder_manager_ = std::make_unique<Common::EncoderAcquisitionManager>(
                    encoder_collector_ips_, ports, static_cast<int>(channels_per_collector_));
            } else {
                encoder_manager_ = std::make_unique<Common::EncoderAcquisitionManager>();
            }
            encoder_manager_->start();
            is_connected_ = true;
            set_state(Tango::ON);
            set_status("Encoder acquisition started");
            log_event(encoder_manager_->status_summary());
        }
        log_event("Simulation mode disabled (runtime switch)");
    }
}

// ========== Attribute Methods ==========
void EncoderDevice::read_selfCheckResult(Tango::Attribute &attr) {
    attr_selfCheckResult_read = self_check_result_;
    attr.set_value(&attr_selfCheckResult_read);
}

void EncoderDevice::read_positionUnit(Tango::Attribute &attr) {
    attr_positionUnit_read = Tango::string_dup(position_unit_.c_str());
    attr.set_value(&attr_positionUnit_read);
}

void EncoderDevice::write_positionUnit(Tango::WAttribute &attr) {
    Tango::DevString new_unit;
    attr.get_write_value(new_unit);
    std::string unit_str(new_unit);
    if (unit_str != "step" && unit_str != "mm" && unit_str != "um" &&
        unit_str != "rad" && unit_str != "urad" && unit_str != "mrad") {
        Tango::Except::throw_exception("InvalidValue", "Invalid positionUnit", "write_positionUnit");
    }
    position_unit_ = unit_str;
}

void EncoderDevice::read_groupAttributeJson(Tango::Attribute &attr) {
    update_motor_positions();
    std::stringstream ss;
    ss << "{\"motorPos\":[";
    for (int i = 0; i < MAX_ENCODER_CHANNELS; i++) {
        if (i > 0) ss << ",";
        ss << motor_pos_[i];
    }
    ss << "]}";
    group_attribute_json_ = ss.str();
    attr_groupAttributeJson_read = Tango::string_dup(group_attribute_json_.c_str());
    attr.set_value(&attr_groupAttributeJson_read);
}

void EncoderDevice::read_motorPos(Tango::Attribute &attr) {
    update_motor_positions();
    attr.set_value(motor_pos_.data(), MAX_ENCODER_CHANNELS);
}

void EncoderDevice::read_encoderLogs(Tango::Attribute &attr) {
    attr_encoderLogs_read = Tango::string_dup(encoder_logs_.c_str());
    attr.set_value(&attr_encoderLogs_read);
}

void EncoderDevice::read_faultState(Tango::Attribute &attr) {
    attr_faultState_read = Tango::string_dup(fault_state_.c_str());
    attr.set_value(&attr_faultState_read);
}

void EncoderDevice::read_encoderResolution(Tango::Attribute &attr) {
    attr.set_value(encoder_resolution_arr_.data(), MAX_ENCODER_CHANNELS);
}

void EncoderDevice::always_executed_hook() {}

void EncoderDevice::read_attr_hardware(std::vector<long> &/*attr_list*/) {
    auto now = std::chrono::steady_clock::now();
    
    // 周期性更新motorPos - 每30ms（符合接口规范要求）
    static auto last_pos_update = std::chrono::steady_clock::now();
    auto pos_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_pos_update).count();
    if (pos_elapsed >= 30) {
        last_pos_update = now;
        update_motor_positions();
    }
}

void EncoderDevice::read_attr(Tango::Attribute &attr) {
    std::string attr_name = attr.get_name();
    
    if (attr_name == "selfCheckResult") read_selfCheckResult(attr);
    else if (attr_name == "positionUnit") read_positionUnit(attr);
    else if (attr_name == "groupAttributeJson") read_groupAttributeJson(attr);
    else if (attr_name == "motorPos") read_motorPos(attr);
    else if (attr_name == "encoderLogs") read_encoderLogs(attr);
    else if (attr_name == "faultState") read_faultState(attr);
    else if (attr_name == "encoderResolution") read_encoderResolution(attr);
}

void EncoderDevice::write_attr(Tango::WAttribute &attr) {
    std::string attr_name = attr.get_name();
    if (attr_name == "positionUnit") {
        write_positionUnit(attr);
    }
}

// ========== Class Implementation ==========
EncoderDeviceClass *EncoderDeviceClass::_instance = NULL;

EncoderDeviceClass *EncoderDeviceClass::instance() {
    if (_instance == NULL) {
        if (Tango::Util::instance() == NULL) {
            Tango::Util::init(0, NULL);
        }
        std::string name = "EncoderDevice";
        _instance = new EncoderDeviceClass(name);
    }
    return _instance;
}

EncoderDeviceClass::EncoderDeviceClass(std::string &name)
    : Tango::DeviceClass(name) {
    command_factory();
}

EncoderDeviceClass::~EncoderDeviceClass() {}

void EncoderDeviceClass::command_factory() {
    // Lock/Unlock commands
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>(
        "devLock", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&EncoderDevice::devLock)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>(
        "devUnlock", static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&EncoderDevice::devUnlock)));
    command_list.push_back(new Tango::TemplCommand(
        "devLockVerify", static_cast<void (Tango::DeviceImpl::*)()>(&EncoderDevice::devLockVerify)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>(
        "devLockQuery", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&EncoderDevice::devLockQuery)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>(
        "devUserConfig", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&EncoderDevice::devUserConfig)));
    
    // System commands
    command_list.push_back(new Tango::TemplCommand(
        "selfCheck", static_cast<void (Tango::DeviceImpl::*)()>(&EncoderDevice::selfCheck)));
    command_list.push_back(new Tango::TemplCommand(
        "init", static_cast<void (Tango::DeviceImpl::*)()>(&EncoderDevice::init)));
    command_list.push_back(new Tango::TemplCommand(
        "connect", static_cast<void (Tango::DeviceImpl::*)()>(&EncoderDevice::connect)));
    command_list.push_back(new Tango::TemplCommand(
        "disconnect", static_cast<void (Tango::DeviceImpl::*)()>(&EncoderDevice::disconnect)));
    
    // Encoder commands
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevDouble>(
        "readEncoder", static_cast<Tango::DevDouble (Tango::DeviceImpl::*)(Tango::DevShort)>(&EncoderDevice::readEncoder)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "setEncoderResolution", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&EncoderDevice::setEncoderResolution)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "makeZero", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&EncoderDevice::makeZero)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "reset", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&EncoderDevice::reset)));
    command_list.push_back(new Tango::TemplCommand(
        "exportLogs", static_cast<void (Tango::DeviceImpl::*)()>(&EncoderDevice::exportLogs)));
    command_list.push_back(new Tango::TemplCommand(
        "exportResolution", static_cast<void (Tango::DeviceImpl::*)()>(&EncoderDevice::exportResolution)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "simSwitch", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&EncoderDevice::simSwitch)));
}

void EncoderDeviceClass::device_factory(const Tango::DevVarStringArray *dev_list) {
    for (unsigned long i = 0; i < dev_list->length(); i++) {
        std::string name((*dev_list)[i].in());
        EncoderDevice *dev = new EncoderDevice(this, name);
        device_list.push_back(dev);
        export_device(dev);
    }
}

void EncoderDeviceClass::attribute_factory(std::vector<Tango::Attr *> &att_list) {
    att_list.push_back(new Tango::Attr("selfCheckResult", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new Tango::Attr("positionUnit", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new Tango::Attr("groupAttributeJson", Tango::DEV_STRING, Tango::READ));
    
    Tango::SpectrumAttr *motor_pos_attr = new Tango::SpectrumAttr("motorPos", Tango::DEV_DOUBLE, Tango::READ, MAX_ENCODER_CHANNELS);
    att_list.push_back(motor_pos_attr);
    
    att_list.push_back(new Tango::Attr("encoderLogs", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::Attr("faultState", Tango::DEV_STRING, Tango::READ));
    
    Tango::SpectrumAttr *resolution_attr = new Tango::SpectrumAttr("encoderResolution", Tango::DEV_DOUBLE, Tango::READ, MAX_ENCODER_CHANNELS);
    att_list.push_back(resolution_attr);
}

} // namespace Encoder

// Main function
void Tango::DServer::class_factory() {
    add_class(Encoder::EncoderDeviceClass::instance());
}

int main(int argc, char *argv[]) {
    try {
        Common::SystemConfig::loadConfig();
        Tango::Util *tg = Tango::Util::init(argc, argv);
        tg->server_init();
        std::cout << "Encoder Server Ready" << std::endl;
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

