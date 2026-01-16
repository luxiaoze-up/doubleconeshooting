#include "device_services/motion_controller_device.h"
#include "common/system_config.h"
#include "drivers/LTSMC.h"
#include <iostream>
#include <cstdio>
#include <stdexcept>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace MotionController {

MotionControllerDevice::MotionControllerDevice(Tango::DeviceClass *cl, std::string &name)
    : Tango::Device_4Impl(cl, name.c_str()),
      self_check_result_(-1),
      result_value_(0),
      is_disabled_(false),
      controller_ip_(""),
      card_id_(0),
      is_connected_(false),
      sim_mode_(false),
      is_locked_(false),
      reconnect_attempts_(0),
      max_reconnect_attempts_(3),
      reconnect_interval_ms_(2000),
      reconnect_in_progress_(false),
      health_check_interval_count_(0) {
    motor_pos_.fill(0.0);
    analog_out_value_.fill(0.0);
    analog_in_value_.fill(0.0);
    generic_io_value_.fill(0.0);
    special_location_value_.fill(2.0);  // 2 = no special position
    axis_status_.fill(false);
    position_unit_ = "step";
    last_reconnect_time_ = std::chrono::steady_clock::now();
    init_device();
}

MotionControllerDevice::MotionControllerDevice(Tango::DeviceClass *cl, const char *name)
    : Tango::Device_4Impl(cl, name),
      self_check_result_(-1),
      result_value_(0),
      is_disabled_(false),
      controller_ip_(""),
      card_id_(0),
      is_connected_(false),
      sim_mode_(false),
      is_locked_(false),
      reconnect_attempts_(0),
      max_reconnect_attempts_(3),
      reconnect_interval_ms_(2000),
      reconnect_in_progress_(false),
      health_check_interval_count_(0) {
    motor_pos_.fill(0.0);
    analog_out_value_.fill(0.0);
    analog_in_value_.fill(0.0);
    generic_io_value_.fill(0.0);
    special_location_value_.fill(2.0);
    axis_status_.fill(false);
    position_unit_ = "step";
    last_reconnect_time_ = std::chrono::steady_clock::now();
    init_device();
}

MotionControllerDevice::MotionControllerDevice(Tango::DeviceClass *cl, const char *name, const char *description)
    : Tango::Device_4Impl(cl, name, description),
      self_check_result_(-1),
      result_value_(0),
      is_disabled_(false),
      controller_ip_(""),
      card_id_(0),
      is_connected_(false),
      sim_mode_(false),
      is_locked_(false),
      reconnect_attempts_(0),
      max_reconnect_attempts_(3),
      reconnect_interval_ms_(2000),
      reconnect_in_progress_(false),
      health_check_interval_count_(0) {
    motor_pos_.fill(0.0);
    analog_out_value_.fill(0.0);
    analog_in_value_.fill(0.0);
    generic_io_value_.fill(0.0);
    special_location_value_.fill(2.0);
    axis_status_.fill(false);
    position_unit_ = "step";
    last_reconnect_time_ = std::chrono::steady_clock::now();
    init_device();
}

MotionControllerDevice::~MotionControllerDevice() {
    delete_device();
}

void MotionControllerDevice::init_device() {
    INFO_STREAM << "MotionControllerDevice::init_device() " << get_name() << std::endl;
    set_state(Tango::INIT);

    // Get all properties
    Tango::DbData db_data;
    // Standard properties
    db_data.push_back(Tango::DbDatum("bundleNo"));
    db_data.push_back(Tango::DbDatum("laserNo"));
    db_data.push_back(Tango::DbDatum("systemNo"));
    db_data.push_back(Tango::DbDatum("subDevList"));
    db_data.push_back(Tango::DbDatum("modelList"));
    db_data.push_back(Tango::DbDatum("currentModel"));
    db_data.push_back(Tango::DbDatum("connectString"));
    db_data.push_back(Tango::DbDatum("errorDict"));
    // Device info properties
    db_data.push_back(Tango::DbDatum("deviceName"));
    db_data.push_back(Tango::DbDatum("deviceID"));
    db_data.push_back(Tango::DbDatum("deviceModel"));
    db_data.push_back(Tango::DbDatum("devicePosition"));
    db_data.push_back(Tango::DbDatum("deviceProductDate"));
    db_data.push_back(Tango::DbDatum("deviceInstallDate"));
    db_data.push_back(Tango::DbDatum("controllerProperty"));
    db_data.push_back(Tango::DbDatum("analogOutputValue"));
    db_data.push_back(Tango::DbDatum("structParameter"));
    db_data.push_back(Tango::DbDatum("isBrake"));
    db_data.push_back(Tango::DbDatum("moveParameter"));
    // Connection properties
    db_data.push_back(Tango::DbDatum("controller_ip"));
    db_data.push_back(Tango::DbDatum("card_id"));
    
    get_db_device()->get_property(db_data);

    int idx = 0;
    if (!db_data[idx].is_empty()) { db_data[idx] >> bundle_no_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> laser_no_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> system_no_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> sub_dev_list_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> model_list_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> current_model_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> connect_string_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> error_dict_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_name_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_id_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_model_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_position_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_product_date_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_install_date_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> controller_property_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> analog_output_value_prop_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> struct_parameter_prop_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> is_brake_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> move_parameter_prop_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> controller_ip_; } else { controller_ip_ = "192.168.0.11"; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> card_id_; }

    // Initialize attribute caches
    struct_parameter_attr_ = struct_parameter_prop_;
    move_parameter_attr_ = move_parameter_prop_;

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
        log_event("Device initialized in simulation mode");
    } else {
        INFO_STREAM << "[DEBUG] 运行模式: 真实模式 (从配置文件加载, SIM_MODE=false)" << std::endl;
        INFO_STREAM << "[DEBUG] 提示: 可通过 simSwitch 命令或GUI动态切换模拟模式" << std::endl;
        
        // Connect to hardware
        DEBUG_STREAM << "[SMC] smc_board_init(card_id=" << card_id_ << ", ip=" << controller_ip_ << ")" << std::endl;
        short ret = smc_board_init(card_id_, 2, const_cast<char*>(controller_ip_.c_str()), 0);
        DEBUG_STREAM << "[SMC] smc_board_init() returned: " << ret << std::endl;
        if (ret != 0) {
            ERROR_STREAM << "MotionControllerDevice: Failed to init board. Ret=" << ret << std::endl;
            set_state(Tango::FAULT);
            set_status("Hardware initialization failed");
            is_connected_ = false;
            log_event("Hardware init failed with code " + std::to_string(ret));
        } else {
            is_connected_ = true;
            set_state(Tango::ON);
            set_status("Device initialized and ready");
            log_event("Device initialized successfully");
        }
    }
    
    INFO_STREAM << "MotionControllerDevice::init_device() completed for " << get_name() 
              << " [State: " << Tango::DevStateName[get_state()] << "]" << std::endl;
}

void MotionControllerDevice::delete_device() {
    if (is_connected_) {
        DEBUG_STREAM << "[SMC] smc_board_close(card_id=" << card_id_ << ")" << std::endl;
        smc_board_close(card_id_);
        DEBUG_STREAM << "[SMC] smc_board_close() completed" << std::endl;
    }
}

void MotionControllerDevice::check_connection() {
    if (!is_connected_) {
        Tango::Except::throw_exception("HardwareError", "Controller not connected", "MotionControllerDevice::check_connection");
    }
}

void MotionControllerDevice::check_error(short error_code, const std::string &context) {
    if (error_code != 0) {
        std::stringstream ss;
        ss << context << " failed with error code " << error_code;
        fault_state_ = ss.str();
        log_event(ss.str());
        Tango::Except::throw_exception("DriverError", ss.str(), "MotionControllerDevice");
    }
}

void MotionControllerDevice::log_event(const std::string &event) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << ": " << event << "\n";
    controller_logs_ += ss.str();
    // Keep log size reasonable
    if (controller_logs_.size() > 10000) {
        controller_logs_ = controller_logs_.substr(controller_logs_.size() - 8000);
    }
}

// ========== Reconnection mechanism ==========
bool MotionControllerDevice::try_reconnect() {
    if (sim_mode_ || is_connected_ || reconnect_in_progress_) {
        return is_connected_;
    }
    
    // 检查重连间隔
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_reconnect_time_).count();
    if (elapsed < reconnect_interval_ms_) {
        return false;  // 还没到重连时间
    }
    
    // 检查是否超过最大重试次数
    if (reconnect_attempts_ >= max_reconnect_attempts_) {
        // 达到最大重试次数后，等待更长时间再重试（指数退避）
        int extended_interval = reconnect_interval_ms_ * (1 << std::min(reconnect_attempts_ - max_reconnect_attempts_, 4));
        if (elapsed < extended_interval) {
            return false;
        }
    }
    
    reconnect_in_progress_ = true;
    last_reconnect_time_ = now;
    reconnect_attempts_++;
    
    INFO_STREAM << "[Reconnect] 尝试重连控制器 (第 " << reconnect_attempts_ << " 次)..." << std::endl;
    log_event("Attempting reconnection (attempt " + std::to_string(reconnect_attempts_) + ")");
    
    // 先关闭旧连接（如果有）
    DEBUG_STREAM << "[SMC] smc_board_close(card_id=" << card_id_ << ") [reconnect cleanup]" << std::endl;
    smc_board_close(card_id_);
    
    // 尝试重新连接
    DEBUG_STREAM << "[SMC] smc_board_init(card_id=" << card_id_ << ", ip=" << controller_ip_ << ") [reconnect]" << std::endl;
    short ret = smc_board_init(card_id_, 2, const_cast<char*>(controller_ip_.c_str()), 0);
    DEBUG_STREAM << "[SMC] smc_board_init() returned: " << ret << std::endl;
    
    reconnect_in_progress_ = false;
    
    if (ret == 0) {
        is_connected_ = true;
        reconnect_attempts_ = 0;  // 重置重连计数
        set_state(Tango::STANDBY);
        set_status("Reconnected successfully");
        INFO_STREAM << "[Reconnect] 重连成功!" << std::endl;
        log_event("Reconnection successful");
        return true;
    } else {
        is_connected_ = false;
        set_state(Tango::FAULT);
        set_status("Reconnection failed (attempt " + std::to_string(reconnect_attempts_) + ")");
        ERROR_STREAM << "[Reconnect] 重连失败, 错误码: " << ret << std::endl;
        log_event("Reconnection failed with error code " + std::to_string(ret));
        return false;
    }
}

bool MotionControllerDevice::check_connection_health() {
    if (sim_mode_) {
        return true;  // 模拟模式始终健康
    }
    
    if (!is_connected_) {
        return false;
    }
    
    // 尝试读取一个轴的位置来验证连接是否正常
    double pos = 0.0;
    DEBUG_STREAM << "[SMC] smc_get_position_unit(card_id=" << card_id_ << ", axis=0) [health check]" << std::endl;
    short ret = smc_get_position_unit(card_id_, 0, &pos);
    DEBUG_STREAM << "[SMC] smc_get_position_unit() returned: " << ret << " [health check]" << std::endl;
    
    if (ret != 0) {
        // 连接可能已断开
        WARN_STREAM << "[HealthCheck] 连接健康检查失败, 错误码: " << ret << std::endl;
        log_event("Connection health check failed with error code " + std::to_string(ret));
        is_connected_ = false;
        set_state(Tango::FAULT);
        set_status("Connection lost - health check failed");
        return false;
    }
    
    return true;
}

void MotionControllerDevice::update_motor_positions() {
    if (sim_mode_) return;
    for (int i = 0; i < MAX_AXES; i++) {
        double pos = 0.0;
        DEBUG_STREAM << "[SMC] smc_get_position_unit(card_id=" << card_id_ << ", axis=" << i << ")" << std::endl;
        smc_get_position_unit(card_id_, i, &pos);
        DEBUG_STREAM << "[SMC] smc_get_position_unit() returned pos=" << pos << std::endl;
        motor_pos_[i] = pos;
    }
}

void MotionControllerDevice::update_axis_status() {
    if (sim_mode_) {
        // In simulation, if we are in MOVING state, we might want to simulate completion eventually.
        // For now, let's assume immediate completion or handled by always_executed_hook logic if we add sim logic there.
        // But here we just return to avoid hardware calls.
        return;
    }
    for (int i = 0; i < MAX_AXES; i++) {
        DEBUG_STREAM << "[SMC] smc_check_done(card_id=" << card_id_ << ", axis=" << i << ")" << std::endl;
        short state = smc_check_done(card_id_, i);
        // smc_check_done 返回值: 0=运动中, 1=已停止
        DEBUG_STREAM << "[SMC] smc_check_done() returned: " << state << " (0=moving, 1=stopped)" << std::endl;
        axis_status_[i] = (state == 0);  // 0 = 运动中 -> axis_status_ = true 表示正在运动
    }
}

// ========== Lock/Unlock Commands ==========
void MotionControllerDevice::devLock(Tango::DevString argin) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string client_info = argin;
    if (is_locked_) {
        if (locker_info_ == client_info) return;
        Tango::Except::throw_exception("Locked", "Device is already locked by: " + locker_info_, "devLock");
    }
    is_locked_ = true;
    locker_info_ = client_info;
    log_event("Device locked by: " + locker_info_);
}

void MotionControllerDevice::devUnlock(Tango::DevBoolean argin) {
    (void)argin;  // unused
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (is_locked_) {
        is_locked_ = false;
        log_event("Device unlocked (was locked by: " + locker_info_ + ")");
        locker_info_ = "";
    }
}

void MotionControllerDevice::devLockVerify() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (!is_locked_) {
        Tango::Except::throw_exception("NotLocked", "Device is not locked", "devLockVerify");
    }
}

Tango::DevString MotionControllerDevice::devLockQuery() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string result;
    if (is_locked_) {
        result = "{\"locked\": true, \"locker\": \"" + locker_info_ + "\"}";
    } else {
        result = "{\"locked\": false}";
    }
    return Tango::string_dup(result.c_str());
}

void MotionControllerDevice::devUserConfig(Tango::DevString argin) {
    log_event(std::string("User config received: ") + argin);
}

// ========== System Commands ==========
void MotionControllerDevice::selfCheck() {
    log_event("Starting self check...");
    self_check_result_ = -1;  // In progress

    if (sim_mode_) {
        self_check_result_ = 0;
        log_event("Self check completed successfully (Simulation)");
        return;
    }

    try {
        check_connection();
        // Check each axis
        for (int i = 0; i < MAX_AXES; i++) {
            // Simple check - try to read position
            double pos;
            DEBUG_STREAM << "[SMC] smc_get_position_unit(card_id=" << card_id_ << ", axis=" << i << ") [selfCheck]" << std::endl;
            short ret = smc_get_position_unit(card_id_, i, &pos);
            DEBUG_STREAM << "[SMC] smc_get_position_unit() returned: " << ret << ", pos=" << pos << std::endl;
            if (ret != 0) {
                self_check_result_ = 1;  // Motor error
                log_event("Self check failed: axis " + std::to_string(i));
                return;
            }
        }
        self_check_result_ = 0;  // Success
        log_event("Self check completed successfully");
    } catch (...) {
        self_check_result_ = 4;  // Other error
        log_event("Self check failed with exception");
    }
}

void MotionControllerDevice::init() {
    init_device();
}

void MotionControllerDevice::connect() {
    if (is_connected_) return;

    if (sim_mode_) {
        is_connected_ = true;
        is_disabled_ = false;
        moving_axes_.clear();
        reconnect_attempts_ = 0;  // 重置重连计数
        set_state(Tango::STANDBY);
        set_status("Simulation Mode - Connected");
        log_event("Controller connected (Simulation)");
        return;
    }

    DEBUG_STREAM << "[SMC] smc_board_init(card_id=" << card_id_ << ", ip=" << controller_ip_ << ") [connect]" << std::endl;
    short ret = smc_board_init(card_id_, 2, const_cast<char*>(controller_ip_.c_str()), 0);
    DEBUG_STREAM << "[SMC] smc_board_init() returned: " << ret << std::endl;
    check_error(ret, "connect");
    is_connected_ = true;
    is_disabled_ = false;
    moving_axes_.clear();
    reconnect_attempts_ = 0;  // 重置重连计数
    set_state(Tango::STANDBY);
    set_status("Connected - Ready");
    log_event("Controller connected");
}

// 联锁禁用接口
void MotionControllerDevice::setDisabled(bool disabled) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    is_disabled_ = disabled;
    if (disabled) {
        set_state(Tango::DISABLE);
        set_status("Disabled by interlock");
        log_event("Motion disabled by interlock");
    } else {
        if (moving_axes_.empty()) {
            set_state(Tango::STANDBY);
            set_status("Ready - Interlock released");
        }
        log_event("Motion enabled, interlock released");
    }
}

void MotionControllerDevice::disconnect() {
    if (!is_connected_) return;

    if (sim_mode_) {
        is_connected_ = false;
        set_state(Tango::OFF);
        set_status("Simulation Mode - Disconnected");
        log_event("Controller disconnected (Simulation)");
        return;
    }

    DEBUG_STREAM << "[SMC] smc_board_close(card_id=" << card_id_ << ") [disconnect]" << std::endl;
    smc_board_close(card_id_);
    DEBUG_STREAM << "[SMC] smc_board_close() completed" << std::endl;
    is_connected_ = false;
    set_state(Tango::OFF);
    set_status("Disconnected");
    log_event("Controller disconnected");
}

void MotionControllerDevice::reset(Tango::DevShort axis_id) {
    check_connection();
    
    if (sim_mode_) {
        // 模拟模式：清除状态
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.erase(axis_id);
        fault_state_ = "";
        if (moving_axes_.empty()) {
            set_state(Tango::STANDBY);
            set_status("Ready - Reset completed (Simulation)");
        }
        log_event("Simulation: Axis " + std::to_string(axis_id) + " reset");
        result_value_ = 0;
        return;
    }
    
    DEBUG_STREAM << "[SMC] smc_emg_stop(card_id=" << card_id_ << ")" << std::endl;
    short ret = smc_emg_stop(card_id_);
    DEBUG_STREAM << "[SMC] smc_emg_stop() returned: " << ret << std::endl;
    check_error(ret, "reset");
    // Clear error status
    DEBUG_STREAM << "[SMC] smc_clear_stop_reason(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    ret = smc_clear_stop_reason(card_id_, axis_id);
    DEBUG_STREAM << "[SMC] smc_clear_stop_reason() returned: " << ret << std::endl;
    check_error(ret, "reset_clear_stop_reason");
    
    // 状态机：清除故障状态
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.erase(axis_id);
        if (get_state() == Tango::FAULT || get_state() == Tango::ALARM) {
            fault_state_ = "";
            if (moving_axes_.empty()) {
                set_state(Tango::STANDBY);
                set_status("Ready - Reset completed");
            } else {
                set_state(Tango::MOVING);
                set_status("Moving");
            }
            log_event("Axis " + std::to_string(axis_id) + " reset, fault cleared");
        }
    }
    result_value_ = 0;
}

// ========== Motion Commands ==========
void MotionControllerDevice::moveZero(Tango::DevShort axis_id) {
    check_connection();
    
    // 状态机检查：禁用状态不允许运动
    if (is_disabled_) {
        Tango::Except::throw_exception("DISABLED", "Motion disabled by interlock", "moveZero");
    }
    if (get_state() == Tango::FAULT) {
        Tango::Except::throw_exception("FAULT", "Device in fault state, reset required", "moveZero");
    }
    

    if (sim_mode_) {
        motor_pos_[axis_id] = 0.0;
        log_event("Simulation: Home axis " + std::to_string(axis_id));
        // Simulate immediate completion for simplicity, or let always_executed_hook handle it
        // For better UX, we should let it stay moving for a bit, but here we just set pos.
        // The always_executed_hook will see it's in moving_axes_ and clear it if we don't clear it here.
        // But always_executed_hook checks hardware. We need to update always_executed_hook too.
        return;
    }
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.insert(axis_id);
        set_state(Tango::MOVING);
        set_status("Moving - Home axis " + std::to_string(axis_id));
    }
    
    // 自动检查并使能电机（如果未使能）
    DEBUG_STREAM << "[SMC] Checking axis " << axis_id << " enable status..." << std::endl;
    short sevon_status = smc_read_sevon_pin(card_id_, axis_id);
    DEBUG_STREAM << "[SMC] smc_read_sevon_pin() returned: " << sevon_status << " (0=disabled, 1=enabled)" << std::endl;
    if (sevon_status == 0) {  // 0 = 未使能
        WARN_STREAM << "[SMC] Axis " << axis_id << " is not enabled, auto-enabling..." << std::endl;
        DEBUG_STREAM << "[SMC] smc_write_sevon_pin(card_id=" << card_id_ << ", axis=" << axis_id << ", on_off=1)" << std::endl;
        short enable_ret = smc_write_sevon_pin(card_id_, axis_id, 1);
        DEBUG_STREAM << "[SMC] smc_write_sevon_pin() returned: " << enable_ret << std::endl;
        if (enable_ret != 0) {
            WARN_STREAM << "[SMC] Failed to enable axis " << axis_id << ", error code: " << enable_ret 
                       << ". Movement may fail." << std::endl;
        } else {
            INFO_STREAM << "[SMC] Axis " << axis_id << " enabled successfully" << std::endl;
        }
    }
    
    DEBUG_STREAM << "[SMC] smc_home_move(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    short ret = smc_home_move(card_id_, axis_id);
    DEBUG_STREAM << "[SMC] smc_home_move() returned: " << ret << std::endl;
    if (ret != 0) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.erase(axis_id);
        check_error(ret, "moveZero");
    }
    result_value_ = 0;
}

void MotionControllerDevice::moveRelative(const Tango::DevVarDoubleArray *argin) {
    check_connection();
    if (argin->length() < 2) {
        Tango::Except::throw_exception("InvalidArgs", "Requires [axis_id, distance]", "moveRelative");
    }
    short axis = (short)(*argin)[0];
    double dist = (*argin)[1];
    
    // 状态机检查
    if (is_disabled_) {
        Tango::Except::throw_exception("DISABLED", "Motion disabled by interlock", "moveRelative");
    }

    if (sim_mode_) {
        motor_pos_[axis] += dist;
        log_event("Simulation: Move relative axis " + std::to_string(axis) + " by " + std::to_string(dist));
        return;
    }
    if (get_state() == Tango::FAULT) {
        Tango::Except::throw_exception("FAULT", "Device in fault state, reset required", "moveRelative");
    }
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.insert(axis);
        set_state(Tango::MOVING);
        set_status("Moving - Relative axis " + std::to_string(axis));
    }
    
    // 自动检查并使能电机（如果未使能）
    DEBUG_STREAM << "[SMC] Checking axis " << axis << " enable status..." << std::endl;
    short sevon_status = smc_read_sevon_pin(card_id_, axis);
    DEBUG_STREAM << "[SMC] smc_read_sevon_pin() returned: " << sevon_status << " (0=disabled, 1=enabled)" << std::endl;
    if (sevon_status == 0) {  // 0 = 未使能
        WARN_STREAM << "[SMC] Axis " << axis << " is not enabled, auto-enabling..." << std::endl;
        DEBUG_STREAM << "[SMC] smc_write_sevon_pin(card_id=" << card_id_ << ", axis=" << axis << ", on_off=1)" << std::endl;
        short enable_ret = smc_write_sevon_pin(card_id_, axis, 1);
        DEBUG_STREAM << "[SMC] smc_write_sevon_pin() returned: " << enable_ret << std::endl;
        if (enable_ret != 0) {
            WARN_STREAM << "[SMC] Failed to enable axis " << axis << ", error code: " << enable_ret 
                       << ". Movement may fail." << std::endl;
        } else {
            INFO_STREAM << "[SMC] Axis " << axis << " enabled successfully" << std::endl;
        }
    }
    
    DEBUG_STREAM << "[SMC] smc_pmove_unit(card_id=" << card_id_ << ", axis=" << axis << ", dist=" << dist << ", mode=relative)" << std::endl;
    short ret = smc_pmove_unit(card_id_, axis, dist, 0);  // 0 = relative
    DEBUG_STREAM << "[SMC] smc_pmove_unit() returned: " << ret << std::endl;
    if (ret != 0) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.erase(axis);
        check_error(ret, "moveRelative");
    }
    result_value_ = 0;
}

void MotionControllerDevice::moveAbsolute(const Tango::DevVarDoubleArray *argin) {
    check_connection();
    if (argin->length() < 2) {
        Tango::Except::throw_exception("InvalidArgs", "Requires [axis_id, position]", "moveAbsolute");
    }
    short axis = (short)(*argin)[0];
    double pos = (*argin)[1];
    
    // 状态机检查
    if (is_disabled_) {
        Tango::Except::throw_exception("DISABLED", "Motion disabled by interlock", "moveAbsolute");
    }
    if (get_state() == Tango::FAULT) {
        Tango::Except::throw_exception("FAULT", "Device in fault state, reset required", "moveAbsolute");
    }
    

    if (sim_mode_) {
        motor_pos_[axis] = pos;
        log_event("Simulation: Move absolute axis " + std::to_string(axis) + " to " + std::to_string(pos));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.insert(axis);
        set_state(Tango::MOVING);
        set_status("Moving - Absolute axis " + std::to_string(axis) + " to " + std::to_string(pos));
    }
    
    // 自动检查并使能电机（如果未使能）
    DEBUG_STREAM << "[SMC] Checking axis " << axis << " enable status..." << std::endl;
    short sevon_status = smc_read_sevon_pin(card_id_, axis);
    DEBUG_STREAM << "[SMC] smc_read_sevon_pin() returned: " << sevon_status << " (0=disabled, 1=enabled)" << std::endl;
    if (sevon_status == 0) {  // 0 = 未使能
        WARN_STREAM << "[SMC] Axis " << axis << " is not enabled, auto-enabling..." << std::endl;
        DEBUG_STREAM << "[SMC] smc_write_sevon_pin(card_id=" << card_id_ << ", axis=" << axis << ", on_off=1)" << std::endl;
        short enable_ret = smc_write_sevon_pin(card_id_, axis, 1);
        DEBUG_STREAM << "[SMC] smc_write_sevon_pin() returned: " << enable_ret << std::endl;
        if (enable_ret != 0) {
            WARN_STREAM << "[SMC] Failed to enable axis " << axis << ", error code: " << enable_ret 
                       << ". Movement may fail." << std::endl;
        } else {
            INFO_STREAM << "[SMC] Axis " << axis << " enabled successfully" << std::endl;
        }
    }
    
    DEBUG_STREAM << "[SMC] smc_pmove_unit(card_id=" << card_id_ << ", axis=" << axis << ", pos=" << pos << ", mode=absolute)" << std::endl;
    short ret = smc_pmove_unit(card_id_, axis, pos, 1);  // 1 = absolute
    DEBUG_STREAM << "[SMC] smc_pmove_unit() returned: " << ret << std::endl;
    if (ret != 0) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.erase(axis);
        check_error(ret, "moveAbsolute");
    }
    result_value_ = 0;
}

void MotionControllerDevice::stopMove(Tango::DevShort axis_id) {
    check_connection();
    
    if (sim_mode_) {
        // 模拟模式：直接更新状态
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.erase(axis_id);
        if (moving_axes_.empty()) {
            set_state(Tango::STANDBY);
            set_status("Ready - Stopped (Simulation)");
        }
        log_event("Simulation: Stop axis " + std::to_string(axis_id));
        result_value_ = 0;
        return;
    }
    
    DEBUG_STREAM << "[SMC] smc_stop(card_id=" << card_id_ << ", axis=" << axis_id << ", mode=decelerate)" << std::endl;
    short ret = smc_stop(card_id_, axis_id, 0);  // 0 = decelerate stop
    DEBUG_STREAM << "[SMC] smc_stop() returned: " << ret << std::endl;
    check_error(ret, "stopMove");
    
    // 状态机：从运动轴集合中移除，判断是否所有轴都停止
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        moving_axes_.erase(axis_id);
        if (moving_axes_.empty()) {
            set_state(Tango::STANDBY);
            set_status("Ready - Stopped");
        }
    }
    result_value_ = 0;
}

// ========== PVTS Commands ==========
void MotionControllerDevice::setPvts(Tango::DevString argin) {
    check_connection();
    log_event(std::string("setPvts: ") + argin);
    
    if (sim_mode_) {
        log_event("Simulation: setPvts completed");
        result_value_ = 0;
        return;
    }
    
    try {
        // 解析 JSON: {"axes": [0,1,2], "count": N, "time": [...], "pos": [[...],[...],...], "vel": [[...],[...],...]}
        json j = json::parse(argin);
        
        if (j.count("axes") == 0 || j.count("count") == 0 || j.count("time") == 0 || 
            j.count("pos") == 0 || j.count("vel") == 0) {
            Tango::Except::throw_exception("InvalidJSON", 
                "Required fields: axes, count, time, pos, vel", "setPvts");
        }
        
        std::vector<short> axes = j["axes"].get<std::vector<short>>();
        int count = j["count"].get<int>();
        std::vector<double> time_array = j["time"].get<std::vector<double>>();
        std::vector<std::vector<double>> pos_arrays = j["pos"].get<std::vector<std::vector<double>>>();
        std::vector<std::vector<double>> vel_arrays = j["vel"].get<std::vector<std::vector<double>>>();
        
        // 验证数据
        if (time_array.size() != static_cast<size_t>(count)) {
            Tango::Except::throw_exception("InvalidData", "time array size != count", "setPvts");
        }
        if (pos_arrays.size() != axes.size() || vel_arrays.size() != axes.size()) {
            Tango::Except::throw_exception("InvalidData", 
                "pos/vel arrays size must match axes count", "setPvts");
        }
        
        // 检查每个轴的状态
        for (short axis : axes) {
            // 检查运动状态
            DEBUG_STREAM << "[PVT] Checking axis " << axis << " status..." << std::endl;
            short done = smc_check_done(card_id_, axis);
            if (done == 0) {  // 0 = 运动中
                WARN_STREAM << "[PVT] Axis " << axis << " is moving, stopping it..." << std::endl;
                smc_stop(card_id_, axis, 0);  // 减速停止
                // 等待停止完成
                int retry = 0;
                while (smc_check_done(card_id_, axis) == 0 && retry < 50) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    retry++;
                }
            }
            
            // 检查 IO 状态
            DEBUG_STREAM << "[PVT] Checking axis " << axis << " IO status..." << std::endl;
            DWORD io_status = smc_axis_io_status(card_id_, axis);
            // 检查报警、急停、限位
            if (io_status & 0x10) {  // ALM
                WARN_STREAM << "[PVT] Axis " << axis << " has ALM signal" << std::endl;
            }
            if (io_status & 0x20) {  // EMG
                Tango::Except::throw_exception("EMG_Active", 
                    "Axis " + std::to_string(axis) + " EMG active", "setPvts");
            }
            if ((io_status & 0x01) || (io_status & 0x02)) {  // EL+/EL-
                Tango::Except::throw_exception("LIMIT_Active", 
                    "Axis " + std::to_string(axis) + " limit active", "setPvts");
            }
        }
        
        // 下发 PVT 表到每个轴
        for (size_t i = 0; i < axes.size(); ++i) {
            short axis = axes[i];
            
            // 检查并使能电机
            short sevon_status = smc_read_sevon_pin(card_id_, axis);
            if (sevon_status == 0) {
                INFO_STREAM << "[PVT] Enabling axis " << axis << "..." << std::endl;
                smc_write_sevon_pin(card_id_, axis, 1);
            }
            
            DEBUG_STREAM << "[PVT] smc_pvt_table_unit(card_id=" << card_id_ 
                      << ", axis=" << axis << ", count=" << count << ")" << std::endl;
            
            short ret = smc_pvt_table_unit(card_id_, axis, count, 
                                          const_cast<double*>(time_array.data()),
                                          const_cast<double*>(pos_arrays[i].data()),
                                          const_cast<double*>(vel_arrays[i].data()));
            
            DEBUG_STREAM << "[PVT] smc_pvt_table_unit() returned: " << ret << std::endl;
            
            if (ret != 0) {
                check_error(ret, "setPvts_smc_pvt_table_unit_axis_" + std::to_string(axis));
            }
        }
        
        INFO_STREAM << "[PVT] PVT table set successfully for " << axes.size() << " axes" << std::endl;
        result_value_ = 0;
        
    } catch (json::exception& e) {
        Tango::Except::throw_exception("JSONParseError", e.what(), "setPvts");
    }
}

void MotionControllerDevice::movePvts(Tango::DevString argin) {
    check_connection();
    log_event(std::string("movePvts: ") + argin);
    
    // 状态机检查
    if (is_disabled_) {
        Tango::Except::throw_exception("DISABLED", "Motion disabled by interlock", "movePvts");
    }
    if (get_state() == Tango::FAULT) {
        Tango::Except::throw_exception("FAULT", "Device in fault state, reset required", "movePvts");
    }
    
    if (sim_mode_) {
        set_state(Tango::MOVING);
        log_event("Simulation: movePvts started");
        result_value_ = 0;
        return;
    }
    
    try {
        // 解析 JSON: {"axes": [0,1,2]}
        json j = json::parse(argin);
        
        if (j.count("axes") == 0) {
            Tango::Except::throw_exception("InvalidJSON", "Required field: axes", "movePvts");
        }
        
        std::vector<short> axes = j["axes"].get<std::vector<short>>();
        
        if (axes.empty()) {
            Tango::Except::throw_exception("InvalidData", "axes array is empty", "movePvts");
        }
        
        // 准备轴列表（转换为WORD类型）
        short axis_num = static_cast<short>(axes.size());
        std::vector<WORD> axis_list;
        for (short axis : axes) {
            axis_list.push_back(static_cast<WORD>(axis));
        }
        
        // 状态机：添加所有轴到运动集合
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            for (short axis : axes) {
                moving_axes_.insert(axis);
            }
            set_state(Tango::MOVING);
            set_status("Moving - PVT motion on " + std::to_string(axis_num) + " axes");
        }
        
        // 启动 PVT 运动
        DEBUG_STREAM << "[PVT] smc_pvt_move(card_id=" << card_id_ 
                  << ", axis_num=" << axis_num << ", axes=[";
        for (size_t i = 0; i < axes.size(); ++i) {
            if (i > 0) DEBUG_STREAM << ",";
            DEBUG_STREAM << axes[i];
        }
        DEBUG_STREAM << "])" << std::endl;
        
        short ret = smc_pvt_move(card_id_, axis_num, axis_list.data());
        DEBUG_STREAM << "[PVT] smc_pvt_move() returned: " << ret << std::endl;
        
        if (ret != 0) {
            // 失败时清除状态
            std::lock_guard<std::mutex> lock(state_mutex_);
            for (short axis : axes) {
                moving_axes_.erase(axis);
            }
            check_error(ret, "movePvts_smc_pvt_move");
        }
        
        INFO_STREAM << "[PVT] PVT motion started for " << axis_num << " axes" << std::endl;
        result_value_ = 0;
        
    } catch (json::exception& e) {
        Tango::Except::throw_exception("JSONParseError", e.what(), "movePvts");
    }
}

// ========== Parameter Configuration Commands ==========
void MotionControllerDevice::setMoveParameter(const Tango::DevVarDoubleArray *argin) {
    check_connection();
    if (argin->length() < 6) {
        Tango::Except::throw_exception("InvalidArgs", 
            "Requires [axis_id, start_vel, max_vel, acc_time, dec_time, stop_vel]", "setMoveParameter");
    }
    short axis = (short)(*argin)[0];
    double start_vel = (*argin)[1];
    double max_vel = (*argin)[2];
    double acc_time = (*argin)[3];
    double dec_time = (*argin)[4];
    double stop_vel = (*argin)[5];
    
    if (sim_mode_) {
        // 模拟模式：只记录日志
        log_event("Simulation: setMoveParameter axis " + std::to_string(axis) + 
                  " start_vel=" + std::to_string(start_vel) + 
                  " max_vel=" + std::to_string(max_vel));
        result_value_ = 0;
        return;
    }
    
    DEBUG_STREAM << "[SMC] smc_set_profile_unit(card_id=" << card_id_ << ", axis=" << axis 
              << ", start_vel=" << start_vel << ", max_vel=" << max_vel 
              << ", acc_time=" << acc_time << ", dec_time=" << dec_time 
              << ", stop_vel=" << stop_vel << ")" << std::endl;
    short ret = smc_set_profile_unit(card_id_, axis, start_vel, max_vel, acc_time, dec_time, stop_vel);
    DEBUG_STREAM << "[SMC] smc_set_profile_unit() returned: " << ret << std::endl;
    check_error(ret, "setMoveParameter");
    result_value_ = 0;
}

void MotionControllerDevice::setStructParameter(const Tango::DevVarDoubleArray *argin) {
    check_connection();
    if (argin->length() < 4) {
        Tango::Except::throw_exception("InvalidArgs",
            "Requires [axis_id, step_angle, gear_ratio, subdivision]", "setStructParameter");
    }
    short axis = (short)(*argin)[0];
    double step_angle = (*argin)[1];
    double gear_ratio = (*argin)[2];
    double subdivision = (*argin)[3];
    
    // Calculate equivalent pulse
    double equiv = (step_angle * gear_ratio) / (360.0 * subdivision);
    
    if (sim_mode_) {
        // 模拟模式：只记录日志
        log_event("Simulation: setStructParameter axis " + std::to_string(axis) + 
                  " equiv=" + std::to_string(equiv));
        result_value_ = 0;
        return;
    }
    
    DEBUG_STREAM << "[SMC] smc_set_equiv(card_id=" << card_id_ << ", axis=" << axis 
              << ", equiv=" << equiv << " [step_angle=" << step_angle 
              << ", gear_ratio=" << gear_ratio << ", subdivision=" << subdivision << "])" << std::endl;
    short ret = smc_set_equiv(card_id_, axis, equiv);
    DEBUG_STREAM << "[SMC] smc_set_equiv() returned: " << ret << std::endl;
    check_error(ret, "setStructParameter");
    result_value_ = 0;
}

void MotionControllerDevice::setAnalog(const Tango::DevVarDoubleArray *argin) {
    check_connection();
    if (argin->length() < 2) {
        Tango::Except::throw_exception("InvalidArgs", "Requires [channel, value]", "setAnalog");
    }
    short channel = (short)(*argin)[0];
    double value = (*argin)[1];
    if (value < 0 || value > 10) {
        Tango::Except::throw_exception("InvalidValue", "Value must be 0-10", "setAnalog");
    }
    
    if (sim_mode_) {
        // 模拟模式：更新缓存值
        if (channel < MAX_AD_CHANNELS) analog_out_value_[channel] = value;
        log_event("Simulation: setAnalog channel " + std::to_string(channel) + " = " + std::to_string(value));
        result_value_ = 0;
        return;
    }
    
    DEBUG_STREAM << "[SMC] smc_set_da_output(card_id=" << card_id_ << ", channel=" << channel << ", value=" << value << ")" << std::endl;
    short ret = smc_set_da_output(card_id_, channel, value);
    DEBUG_STREAM << "[SMC] smc_set_da_output() returned: " << ret << std::endl;
    check_error(ret, "setAnalog");
    if (channel < MAX_AD_CHANNELS) analog_out_value_[channel] = value;
    result_value_ = 0;
}

// ========== Status Query Commands ==========
Tango::DevShort MotionControllerDevice::checkMoveState(Tango::DevShort axis_id) {
    check_connection();
    
    if (sim_mode_) {
        // 模拟模式：检查该轴是否在运动中
        bool is_moving = (moving_axes_.find(axis_id) != moving_axes_.end());
        return is_moving ? 0 : 1;  // 0=运动中, 1=已停止
    }
    
    DEBUG_STREAM << "[SMC] smc_check_done(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    short state = smc_check_done(card_id_, axis_id);
    // smc_check_done 返回值: 0=运动中, 1=已停止
    DEBUG_STREAM << "[SMC] smc_check_done() returned: " << state << " (0=moving, 1=stopped)" << std::endl;
    return state;
}

Tango::DevBoolean MotionControllerDevice::readOrg(Tango::DevShort axis_id) {
    check_connection();
    
    if (sim_mode_) {
        // 模拟模式：检查缓存的特殊位置值
        if (axis_id < MAX_AXES) {
            return (special_location_value_[axis_id] == 0);  // 0 = origin
        }
        return false;
    }
    
    // DEBUG_STREAM << "[SMC] smc_read_org_pin(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    short ret = smc_read_org_pin(card_id_, axis_id);
    // DEBUG_STREAM << "[SMC] smc_read_org_pin() returned: " << ret << std::endl;
    return (ret == 0);
}

Tango::DevShort MotionControllerDevice::readEL(Tango::DevShort axis_id) {
    check_connection();
    
    if (sim_mode_) {
        // 模拟模式：检查缓存的特殊位置值
        if (axis_id < MAX_AXES) {
            double loc = special_location_value_[axis_id];
            if (loc == 1) return 1;   // 正限位
            if (loc == -1) return -1; // 负限位
        }
        return 0;  // 无限位
    }
    
    short pos_limit = 0, neg_limit = 0;
    // DEBUG_STREAM << "[SMC] smc_read_elp_pin(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    pos_limit = smc_read_elp_pin(card_id_, axis_id);
    // DEBUG_STREAM << "[SMC] smc_read_elp_pin() returned: " << pos_limit << std::endl;
    DEBUG_STREAM << "[SMC] smc_read_eln_pin(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    DEBUG_STREAM << "[SMC] smc_read_elp_pin() returned: " << pos_limit << std::endl;
    neg_limit = smc_read_eln_pin(card_id_, axis_id);
    DEBUG_STREAM << "[SMC] smc_read_eln_pin(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    DEBUG_STREAM << "[SMC] smc_read_eln_pin() returned: " << neg_limit << std::endl;
    // Return: 0=none, 1=EL+, -1=EL-
    if (pos_limit == 0) return 1;
    if (neg_limit == 0) return -1;
    return 0;
}

Tango::DevDouble MotionControllerDevice::readPos(Tango::DevShort axis_id) {
    check_connection();
    
    if (sim_mode_) {
        // 模拟模式：返回缓存的位置值
        if (axis_id < MAX_AXES) {
            return motor_pos_[axis_id];
        }
        return 0.0;
    }
    
    double pos = 0.0;
    DEBUG_STREAM << "[SMC] smc_get_position_unit(card_id=" << card_id_ << ", axis=" << axis_id << ")" << std::endl;
    short ret = smc_get_position_unit(card_id_, axis_id, &pos);
    DEBUG_STREAM << "[SMC] smc_get_position_unit() returned: " << ret << ", pos=" << pos << std::endl;
    check_error(ret, "readPos");
    if (axis_id < MAX_AXES) motor_pos_[axis_id] = pos;
    return pos;
}

void MotionControllerDevice::setEncoderPosition(const Tango::DevVarDoubleArray *argin) {
    check_connection();
    if (argin->length() < 2) {
        Tango::Except::throw_exception("InvalidArgs", "Requires [axis_id, position]", "setEncoderPosition");
    }
    short axis = (short)(*argin)[0];
    double pos = (*argin)[1];
    
    if (sim_mode_) {
        // 模拟模式：只更新缓存值
        if (axis < MAX_AXES) {
            motor_pos_[axis] = pos;
        }
        log_event("Simulation: Set encoder position axis " + std::to_string(axis) + " to " + std::to_string(pos));
        result_value_ = 0;
        return;
    }
    
    DEBUG_STREAM << "[SMC] smc_set_encoder_unit(card_id=" << card_id_ << ", axis=" << axis << ", pos=" << pos << ")" << std::endl;
    short ret = smc_set_encoder_unit(card_id_, axis, pos);
    DEBUG_STREAM << "[SMC] smc_set_encoder_unit() returned: " << ret << std::endl;
    check_error(ret, "setEncoderPosition");
    if (axis < MAX_AXES) motor_pos_[axis] = pos;
    log_event("Set encoder position axis " + std::to_string(axis) + " to " + std::to_string(pos));
    result_value_ = 0;
}

// ========== IO Commands ==========
// 读取输入位状态 (IN0-IN15)
Tango::DevShort MotionControllerDevice::readIO(Tango::DevShort bitno) {
    check_connection();
    
    if (sim_mode_) {
        // 模拟模式：返回缓存值
        if (bitno < MAX_IO_CHANNELS) {
            return static_cast<short>(generic_io_value_[bitno]);
        }
        return 0;
    }
    
    // 使用 smc_read_outbit 读取单个输出位 (OUT0-OUT11)
    DEBUG_STREAM << "[SMC] smc_read_outbit(card_id=" << card_id_ << ", bitno=" << bitno << ")" << std::endl;
    short hardware_value = smc_read_outbit(card_id_, bitno);
    DEBUG_STREAM << "[SMC] smc_read_outbit() returned: " << hardware_value << std::endl;
    
    // OUT0-OUT11 是低电平有效（active low），需要反转硬件值得到逻辑值
    // 硬件值 0 (LOW) -> 逻辑值 1 (开启)
    // 硬件值 1 (HIGH) -> 逻辑值 0 (关闭)
    short logical_value = 1 - hardware_value;
    DEBUG_STREAM << "[SMC] hardware_value=" << hardware_value << " -> logical_value=" << logical_value << " [active low]" << std::endl;
    return logical_value;
}

// 写入输出位状态 (OUT0-OUT11)
void MotionControllerDevice::writeIO(const Tango::DevVarDoubleArray *argin) {
    check_connection();
    if (argin->length() < 2) {
        Tango::Except::throw_exception("InvalidArgs", "Requires [bitno, value]", "writeIO");
    }
    short bitno = (short)(*argin)[0];
    short value = (short)(*argin)[1];
    
    // 缓存值保持用户传入的逻辑值（不取反）
    if (bitno < MAX_IO_CHANNELS) {
        generic_io_value_[bitno] = value;
    }
    
    if (sim_mode_) {
        // 模拟模式：只记录日志
        log_event("Simulation: writeIO bitno " + std::to_string(bitno) + " = " + std::to_string(value));
        return;
    }
    
    // OUT0-OUT11 是低电平有效（active low），需要反转逻辑值
    // 逻辑值 1 (开启) -> 硬件值 0 (LOW)
    // 逻辑值 0 (关闭) -> 硬件值 1 (HIGH)
    // 先规范化逻辑值：非0值视为1，0值保持为0
    short normalized_value = (value != 0) ? 1 : 0;
    short hardware_value = 1 - normalized_value;
    // 使用 smc_write_outbit 写入单个输出位 (OUT0-OUT11)
    DEBUG_STREAM << "[SMC] smc_write_outbit(card_id=" << card_id_ << ", bitno=" << bitno 
                 << ", logical_value=" << value << ", hardware_value=" << hardware_value << ")" << std::endl;
    short ret = smc_write_outbit(card_id_, bitno, hardware_value);
    DEBUG_STREAM << "[SMC] smc_write_outbit() returned: " << ret << std::endl;
    check_error(ret, "writeIO");
}

Tango::DevDouble MotionControllerDevice::readAD(Tango::DevShort channel) {
    check_connection();
    
    if (sim_mode_) {
        // 模拟模式：返回缓存值
        if (channel < MAX_AD_CHANNELS) {
            return analog_in_value_[channel];
        }
        return 0.0;
    }
    
    double value = 0.0;
    // DEBUG_STREAM << "[SMC] smc_get_ain(card_id=" << card_id_ << ", channel=" << channel << ")" << std::endl;
    value = smc_get_ain(card_id_, channel);
    // DEBUG_STREAM << "[SMC] smc_get_ain() returned: " << value << std::endl;
    if (channel < MAX_AD_CHANNELS) analog_in_value_[channel] = value;
    return value;
}

void MotionControllerDevice::writeAD(const Tango::DevVarDoubleArray *argin) {
    check_connection();
    if (argin->length() < 2) {
        Tango::Except::throw_exception("InvalidArgs", "Requires [channel, value]", "writeAD");
    }
    short channel = (short)(*argin)[0];
    double value = (*argin)[1];

    if (sim_mode_) {
        if (channel < MAX_AD_CHANNELS) analog_out_value_[channel] = value;
        return;
    }

    DEBUG_STREAM << "[SMC] smc_set_da_output(card_id=" << card_id_ << ", channel=" << channel << ", value=" << value << ")" << std::endl;
    short ret = smc_set_da_output(card_id_, channel, value);
    DEBUG_STREAM << "[SMC] smc_set_da_output() returned: " << ret << std::endl;
    check_error(ret, "writeAD");
    if (channel < MAX_AD_CHANNELS) analog_out_value_[channel] = value;
}

// ========== Utility Commands ==========
void MotionControllerDevice::exportAxis() {
    log_event("Exporting axis parameters to local file...");
    // TODO: Export to Excel/CSV file
    result_value_ = 0;
}

void MotionControllerDevice::simSwitch(Tango::DevShort mode) {
    bool was_sim_mode = sim_mode_;
    sim_mode_ = (mode == 1);
    
    // 注意：运行时切换只影响当前会话，server 重启后恢复配置文件的值（不持久化）
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] simSwitch: Enabling SIMULATION MODE (运行时切换，不持久化)" << std::endl;
        if (is_connected_ && !was_sim_mode) {
            disconnect();
        }
        is_connected_ = true;
        set_state(Tango::ON);
        set_status("Simulation Mode - Ready");
        log_event("Simulation mode enabled (runtime switch)");
    } else {
        INFO_STREAM << "[DEBUG] simSwitch: Disabling simulation mode, switching to REAL MODE (运行时切换，不持久化)" << std::endl;
        if (was_sim_mode && !is_connected_) {
            connect();
        }
        log_event("Simulation mode disabled (runtime switch)");
    }
}

Tango::DevString MotionControllerDevice::errorParse(Tango::DevShort error_code) {
    // Error code dictionary based on LTSMC API
    std::string msg;
    switch (error_code) {
        case 0: msg = "Success"; break;
        case 1: msg = "Invalid axis number"; break;
        case 2: msg = "Communication error"; break;
        case 3: msg = "Hardware error"; break;
        default: msg = "Unknown error: " + std::to_string(error_code); break;
    }
    return Tango::string_dup(msg.c_str());
}

// ========== Attribute Read/Write Methods ==========
void MotionControllerDevice::read_selfCheckResult(Tango::Attribute &attr) {
    attr_selfCheckResult_read = self_check_result_;
    attr.set_value(&attr_selfCheckResult_read);
}

void MotionControllerDevice::read_positionUnit(Tango::Attribute &attr) {
    attr_positionUnit_read = Tango::string_dup(position_unit_.c_str());
    attr.set_value(&attr_positionUnit_read);
}

void MotionControllerDevice::write_positionUnit(Tango::WAttribute &attr) {
    Tango::DevString new_unit;
    attr.get_write_value(new_unit);
    std::string unit_str(new_unit);
    if (unit_str != "step" && unit_str != "mm" && unit_str != "um" &&
        unit_str != "rad" && unit_str != "urad" && unit_str != "mrad") {
        Tango::Except::throw_exception("InvalidValue",
            "positionUnit must be: step, mm, um, rad, urad, or mrad", "write_positionUnit");
    }
    position_unit_ = unit_str;
}

void MotionControllerDevice::read_groupAttributeJson(Tango::Attribute &attr) {
    update_motor_positions();
    update_axis_status();
    // Build JSON
    std::stringstream ss;
    ss << "{\"motorPos\":[";
    for (int i = 0; i < MAX_AXES; i++) {
        if (i > 0) ss << ",";
        ss << motor_pos_[i];
    }
    ss << "],\"axisStatus\":[";
    for (int i = 0; i < MAX_AXES; i++) {
        if (i > 0) ss << ",";
        ss << (axis_status_[i] ? "true" : "false");
    }
    ss << "]}";
    group_attribute_json_ = ss.str();
    attr_groupAttributeJson_read = Tango::string_dup(group_attribute_json_.c_str());
    attr.set_value(&attr_groupAttributeJson_read);
}

void MotionControllerDevice::read_controllerLogs(Tango::Attribute &attr) {
    attr_controllerLogs_read = Tango::string_dup(controller_logs_.c_str());
    attr.set_value(&attr_controllerLogs_read);
}

void MotionControllerDevice::read_faultState(Tango::Attribute &attr) {
    attr_faultState_read = Tango::string_dup(fault_state_.c_str());
    attr.set_value(&attr_faultState_read);
}

void MotionControllerDevice::read_motorPos(Tango::Attribute &attr) {
    update_motor_positions();
    attr.set_value(motor_pos_.data(), MAX_AXES);
}

void MotionControllerDevice::read_structParameter(Tango::Attribute &attr) {
    attr_structParameter_read = Tango::string_dup(struct_parameter_attr_.c_str());
    attr.set_value(&attr_structParameter_read);
}

void MotionControllerDevice::read_moveParameter(Tango::Attribute &attr) {
    attr_moveParameter_read = Tango::string_dup(move_parameter_attr_.c_str());
    attr.set_value(&attr_moveParameter_read);
}

void MotionControllerDevice::read_analogOutValue(Tango::Attribute &attr) {
    attr.set_value(analog_out_value_.data(), MAX_AD_CHANNELS);
}

void MotionControllerDevice::read_analogInValue(Tango::Attribute &attr) {
    if (!sim_mode_) {
        for (int i = 0; i < MAX_AD_CHANNELS; i++) {
            DEBUG_STREAM << "[SMC] smc_get_ain(card_id=" << card_id_ << ", channel=" << i << ") [read_analogInValue]" << std::endl;
            analog_in_value_[i] = smc_get_ain(card_id_, i);
            DEBUG_STREAM << "[SMC] smc_get_ain() returned: " << analog_in_value_[i] << std::endl;
        }
    }
    // 模拟模式下返回缓存值
    attr.set_value(analog_in_value_.data(), MAX_AD_CHANNELS);
}

void MotionControllerDevice::read_genericIoInputValue(Tango::Attribute &attr) {
    if (!sim_mode_) {
        for (int i = 0; i < MAX_IO_CHANNELS; i++) {
            DEBUG_STREAM << "[SMC] smc_read_inport(card_id=" << card_id_ << ", port=" << i << ") [read_genericIoInputValue]" << std::endl;
            generic_io_value_[i] = smc_read_inport(card_id_, i);
            DEBUG_STREAM << "[SMC] smc_read_inport() returned: " << generic_io_value_[i] << std::endl;
        }
    }
    // 模拟模式下返回缓存值
    attr.set_value(generic_io_value_.data(), MAX_IO_CHANNELS);
}

void MotionControllerDevice::read_specialLocationValue(Tango::Attribute &attr) {
    // Update special location values (limit/origin status)
    if (!sim_mode_) {
        for (int i = 0; i < MAX_AXES; i++) {
            DEBUG_STREAM << "[SMC] smc_read_org_pin(card_id=" << card_id_ << ", axis=" << i << ") [read_specialLocationValue]" << std::endl;
            short org = smc_read_org_pin(card_id_, i);
            DEBUG_STREAM << "[SMC] smc_read_org_pin() returned: " << org << std::endl;
            DEBUG_STREAM << "[SMC] smc_read_elp_pin(card_id=" << card_id_ << ", axis=" << i << ")" << std::endl;
            short pos_lim = smc_read_elp_pin(card_id_, i);
            DEBUG_STREAM << "[SMC] smc_read_elp_pin() returned: " << pos_lim << std::endl;
            DEBUG_STREAM << "[SMC] smc_read_eln_pin(card_id=" << card_id_ << ", axis=" << i << ")" << std::endl;
            short neg_lim = smc_read_eln_pin(card_id_, i);
            DEBUG_STREAM << "[SMC] smc_read_eln_pin() returned: " << neg_lim << std::endl;
            // -1: negative limit, 0: origin, 1: positive limit, 2: none
            if (org == 1) {
                special_location_value_[i] = 0;
            } else if (pos_lim == 1) {
                special_location_value_[i] = 1;
            } else if (neg_lim == 1) {
                special_location_value_[i] = -1;
            } else {
                special_location_value_[i] = 2;
            }
        }
    }
    // 模拟模式下返回缓存值 (默认为2=无特殊位置)
    attr.set_value(special_location_value_.data(), MAX_AXES);
}

void MotionControllerDevice::read_axisStatus(Tango::Attribute &attr) {
    update_axis_status();
    // Use shadow variable array instead of stack array
    for (int i = 0; i < MAX_AXES; i++) {
        attr_axisStatus_read[i] = axis_status_[i];
    }
    attr.set_value(attr_axisStatus_read, MAX_AXES);
}

void MotionControllerDevice::read_resultValue(Tango::Attribute &attr) {
    attr_resultValue_read = result_value_;
    attr.set_value(&attr_resultValue_read);
}

void MotionControllerDevice::always_executed_hook() {
    // ========== 1. 自动重连机制 ==========
    if (!sim_mode_ && !is_connected_) {
        // 连接断开时尝试重连
        try_reconnect();
        return;  // 未连接时跳过后续逻辑
    }
    
    // ========== 2. 定期健康检查 ==========
    if (!sim_mode_ && is_connected_) {
        health_check_interval_count_++;
        if (health_check_interval_count_ >= HEALTH_CHECK_INTERVAL) {
            health_check_interval_count_ = 0;
            if (!check_connection_health()) {
                // 连接不健康，下次 hook 会尝试重连
                return;
            }
        }
    }
    
    // ========== 3. 状态机：轮询运动状态 ==========
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    // 如果当前是 MOVING 状态，检查所有运动中的轴
    if (get_state() == Tango::MOVING && !moving_axes_.empty()) {
        if (sim_mode_) {
            // 模拟模式：立即完成运动
            moving_axes_.clear();
            set_state(Tango::STANDBY);
            set_status("Ready - Motion completed (Simulation)");
            log_event("Simulation: All axes stopped, state -> STANDBY");
            return;
        }
        
        // 真实模式：轮询硬件状态
        std::set<short> still_moving;
        for (short axis : moving_axes_) {
            DEBUG_STREAM << "[SMC] smc_check_done(card_id=" << card_id_ << ", axis=" << axis << ") [always_executed_hook]" << std::endl;
            short state = smc_check_done(card_id_, axis);
            // smc_check_done 返回值: 0=运动中, 1=已停止
            DEBUG_STREAM << "[SMC] smc_check_done() returned: " << state << " (0=moving, 1=stopped)" << std::endl;
            if (state == 0) {  // 0 = still moving
                still_moving.insert(axis);
            }
        }
        moving_axes_ = still_moving;
        
        // 如果所有轴都停止了，恢复到 STANDBY 状态
        if (moving_axes_.empty()) {
            set_state(Tango::STANDBY);
            set_status("Ready - Motion completed");
            log_event("All axes stopped, state -> STANDBY");
        }
    }
}

void MotionControllerDevice::read_attr_hardware(std::vector<long> &/*attr_list*/) {}

void MotionControllerDevice::read_attr(Tango::Attribute &attr) {
    std::string attr_name = attr.get_name();
    
    if (attr_name == "selfCheckResult") read_selfCheckResult(attr);
    else if (attr_name == "positionUnit") read_positionUnit(attr);
    else if (attr_name == "groupAttributeJson") read_groupAttributeJson(attr);
    else if (attr_name == "controllerLogs") read_controllerLogs(attr);
    else if (attr_name == "faultState") read_faultState(attr);
    else if (attr_name == "motorPos") read_motorPos(attr);
    else if (attr_name == "structParameter") read_structParameter(attr);
    else if (attr_name == "moveParameter") read_moveParameter(attr);
    else if (attr_name == "analogOutValue") read_analogOutValue(attr);
    else if (attr_name == "analogInValue") read_analogInValue(attr);
    else if (attr_name == "genericIoInputValue") read_genericIoInputValue(attr);
    else if (attr_name == "specialLocationValue") read_specialLocationValue(attr);
    else if (attr_name == "axisStatus") read_axisStatus(attr);
    else if (attr_name == "resultValue") read_resultValue(attr);
}

void MotionControllerDevice::write_attr(Tango::WAttribute &attr) {
    std::string attr_name = attr.get_name();
    if (attr_name == "positionUnit") {
        write_positionUnit(attr);
    }
}

// ========== Class Implementation ==========
MotionControllerDeviceClass *MotionControllerDeviceClass::_instance = NULL;

MotionControllerDeviceClass *MotionControllerDeviceClass::instance() {
    if (_instance == NULL) {
        if (Tango::Util::instance() == NULL) {
            Tango::Util::init(0, NULL);
        }
        std::string name = "MotionControllerDevice";
        _instance = new MotionControllerDeviceClass(name);
    }
    return _instance;
}

MotionControllerDeviceClass::MotionControllerDeviceClass(std::string &name)
    : Tango::DeviceClass(name) {
    command_factory();
}

MotionControllerDeviceClass::~MotionControllerDeviceClass() {}

void MotionControllerDeviceClass::command_factory() {
    // ========== Lock/Unlock Commands ==========
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>(
        "devLock", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&MotionControllerDevice::devLock)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>(
        "devUnlock", static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&MotionControllerDevice::devUnlock)));
    command_list.push_back(new Tango::TemplCommand(
        "devLockVerify", static_cast<void (Tango::DeviceImpl::*)()>(&MotionControllerDevice::devLockVerify)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>(
        "devLockQuery", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&MotionControllerDevice::devLockQuery)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>(
        "devUserConfig", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&MotionControllerDevice::devUserConfig)));
    
    // ========== System Commands ==========
    command_list.push_back(new Tango::TemplCommand(
        "selfCheck", static_cast<void (Tango::DeviceImpl::*)()>(&MotionControllerDevice::selfCheck)));
    command_list.push_back(new Tango::TemplCommand(
        "init", static_cast<void (Tango::DeviceImpl::*)()>(&MotionControllerDevice::init)));
    command_list.push_back(new Tango::TemplCommand(
        "connect", static_cast<void (Tango::DeviceImpl::*)()>(&MotionControllerDevice::connect)));
    command_list.push_back(new Tango::TemplCommand(
        "disconnect", static_cast<void (Tango::DeviceImpl::*)()>(&MotionControllerDevice::disconnect)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "reset", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::reset)));
    
    // ========== Motion Commands ==========
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "moveZero", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::moveZero)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "moveRelative", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&MotionControllerDevice::moveRelative)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "moveAbsolute", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&MotionControllerDevice::moveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "stopMove", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::stopMove)));
    
    // ========== PVTS Commands ==========
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>(
        "setPvts", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&MotionControllerDevice::setPvts)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>(
        "movePvts", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&MotionControllerDevice::movePvts)));
    
    // ========== Parameter Configuration Commands ==========
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "setMoveParameter", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&MotionControllerDevice::setMoveParameter)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "setStructParameter", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&MotionControllerDevice::setStructParameter)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "setAnalog", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&MotionControllerDevice::setAnalog)));
    
    // ========== Status Query Commands ==========
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevShort>(
        "checkMoveState", static_cast<Tango::DevShort (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::checkMoveState)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevBoolean>(
        "readOrg", static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::readOrg)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevShort>(
        "readEL", static_cast<Tango::DevShort (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::readEL)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevDouble>(
        "readPos", static_cast<Tango::DevDouble (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::readPos)));
    
    // ========== Encoder Commands ==========
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "setEncoderPosition", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&MotionControllerDevice::setEncoderPosition)));
    
    // ========== IO Commands ==========
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevShort>(
        "readIO", static_cast<Tango::DevShort (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::readIO)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "writeIO", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&MotionControllerDevice::writeIO)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevDouble>(
        "readAD", static_cast<Tango::DevDouble (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::readAD)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "writeAD", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&MotionControllerDevice::writeAD)));
    
    // ========== Utility Commands ==========
    command_list.push_back(new Tango::TemplCommand(
        "exportAxis", static_cast<void (Tango::DeviceImpl::*)()>(&MotionControllerDevice::exportAxis)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "simSwitch", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::simSwitch)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevString>(
        "errorParse", static_cast<Tango::DevString (Tango::DeviceImpl::*)(Tango::DevShort)>(&MotionControllerDevice::errorParse)));
}

void MotionControllerDeviceClass::device_factory(const Tango::DevVarStringArray *dev_list) {
    std::cout << "MotionControllerDeviceClass::device_factory() called with " << dev_list->length() << " device(s)" << std::endl;
    for (unsigned long i = 0; i < dev_list->length(); i++) {
        std::string name((*dev_list)[i].in());
        std::cout << "  Creating device: " << name << std::endl;
        MotionControllerDevice *dev = new MotionControllerDevice(this, name);
        device_list.push_back(dev);
        std::cout << "  Exporting device: " << name << std::endl;
        export_device(dev);
        std::cout << "  ✓ Device " << name << " exported successfully" << std::endl;
    }
    std::cout << "MotionControllerDeviceClass::device_factory() completed. Total devices: " << device_list.size() << std::endl;
}

void MotionControllerDeviceClass::attribute_factory(std::vector<Tango::Attr *> &att_list) {
    // ========== Standard Attributes ==========
    att_list.push_back(new Tango::Attr("selfCheckResult", Tango::DEV_LONG, Tango::READ));
    
    // positionUnit - Read/Write
    Tango::Attr *pos_unit_attr = new Tango::Attr("positionUnit", Tango::DEV_STRING, Tango::READ_WRITE);
    att_list.push_back(pos_unit_attr);
    
    att_list.push_back(new Tango::Attr("groupAttributeJson", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::Attr("controllerLogs", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::Attr("faultState", Tango::DEV_STRING, Tango::READ));
    
    // Spectrum attributes
    Tango::SpectrumAttr *motor_pos_attr = new Tango::SpectrumAttr("motorPos", Tango::DEV_DOUBLE, Tango::READ, MAX_AXES);
    att_list.push_back(motor_pos_attr);
    
    att_list.push_back(new Tango::Attr("structParameter", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::Attr("moveParameter", Tango::DEV_STRING, Tango::READ));
    
    Tango::SpectrumAttr *analog_out_attr = new Tango::SpectrumAttr("analogOutValue", Tango::DEV_DOUBLE, Tango::READ, MAX_AD_CHANNELS);
    att_list.push_back(analog_out_attr);
    
    Tango::SpectrumAttr *analog_in_attr = new Tango::SpectrumAttr("analogInValue", Tango::DEV_DOUBLE, Tango::READ, MAX_AD_CHANNELS);
    att_list.push_back(analog_in_attr);
    
    Tango::SpectrumAttr *io_attr = new Tango::SpectrumAttr("genericIoInputValue", Tango::DEV_DOUBLE, Tango::READ, MAX_IO_CHANNELS);
    att_list.push_back(io_attr);
    
    Tango::SpectrumAttr *special_loc_attr = new Tango::SpectrumAttr("specialLocationValue", Tango::DEV_DOUBLE, Tango::READ, MAX_AXES);
    att_list.push_back(special_loc_attr);
    
    Tango::SpectrumAttr *axis_status_attr = new Tango::SpectrumAttr("axisStatus", Tango::DEV_BOOLEAN, Tango::READ, MAX_AXES);
    att_list.push_back(axis_status_attr);
    
    att_list.push_back(new Tango::Attr("resultValue", Tango::DEV_SHORT, Tango::READ));
}

} // namespace MotionController

// Main function
void Tango::DServer::class_factory() {
    add_class(MotionController::MotionControllerDeviceClass::instance());
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
        
        // Debug: List exported devices
        MotionController::MotionControllerDeviceClass *dev_class = MotionController::MotionControllerDeviceClass::instance();
        if (dev_class) {
            std::vector<Tango::DeviceImpl*> devices = dev_class->get_device_list();
            std::cout << "MotionController Server Ready - Exported " << devices.size() << " device(s):" << std::endl;
            for (size_t i = 0; i < devices.size(); i++) {
                std::cout << "  [" << (i+1) << "] " << devices[i]->get_name() << std::endl;
            }
        } else {
            std::cout << "MotionController Server Ready (device class not found)" << std::endl;
        }
        
        tg->server_run();
    } catch (std::bad_alloc &) {
        std::cerr << "Can't allocate memory!!!" << std::endl;
        return -1;
    } catch (CORBA::Exception &e) {
        Tango::Except::print_exception(e);
        return -1;
    }
    return 0;
}

