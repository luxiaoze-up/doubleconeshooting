#include "device_services/auxiliary_support_device.h"
#include "common/system_config.h"
#include <iostream>
// #include <server/DServer.h>÷
#include <sstream>
#include <ctime>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {

// Temporarily overrides Tango proxy timeout for a ping and restores it afterwards.
// Throws on ping failure.
void ping_with_temp_timeout_or_throw(Tango::DeviceProxy* proxy, int timeout_ms) {
    if (proxy == nullptr) {
        throw std::runtime_error("ping_with_temp_timeout_or_throw: proxy is null");
    }

    int original_timeout = proxy->get_timeout_millis();
    proxy->set_timeout_millis(timeout_ms);
    try {
        proxy->ping();
    } catch (...) {
        proxy->set_timeout_millis(original_timeout);
        throw;
    }
    proxy->set_timeout_millis(original_timeout);
}

bool ping_with_temp_timeout(Tango::DeviceProxy* proxy, int timeout_ms) noexcept {
    try {
        ping_with_temp_timeout_or_throw(proxy, timeout_ms);
        return true;
    } catch (...) {
        return false;
    }
}

// LargeStroke-style: support shared_ptr
void ping_with_temp_timeout_or_throw(const std::shared_ptr<Tango::DeviceProxy>& proxy, int timeout_ms) {
    if (!proxy) {
        throw std::runtime_error("ping_with_temp_timeout_or_throw: proxy is null");
    }

    int original_timeout = proxy->get_timeout_millis();
    proxy->set_timeout_millis(timeout_ms);
    try {
        proxy->ping();
    } catch (...) {
        proxy->set_timeout_millis(original_timeout);
        throw;
    }
    proxy->set_timeout_millis(original_timeout);
}

bool ping_with_temp_timeout(const std::shared_ptr<Tango::DeviceProxy>& proxy, int timeout_ms) noexcept {
    try {
        ping_with_temp_timeout_or_throw(proxy, timeout_ms);
        return true;
    } catch (...) {
        return false;
    }
}

std::shared_ptr<Tango::DeviceProxy> create_proxy_and_ping(const std::string& name, int timeout_ms) {
    auto proxy = std::make_shared<Tango::DeviceProxy>(name);
    ping_with_temp_timeout_or_throw(proxy, timeout_ms);
    return proxy;
}

}  // namespace

namespace AuxiliarySupport {

AuxiliarySupportDevice::AuxiliarySupportDevice(Tango::DeviceClass *device_class, std::string &device_name)
    : Common::StandardSystemDevice(device_class, device_name),
      is_locked_(false),
      lock_user_(""),
      bundle_no_(""),
      laser_no_(""),
      system_no_(""),
      sub_dev_list_("[]"),
      current_model_(""),
      connect_string_("{}"),
      error_dict_("{}"),
      device_name_("辅助支撑类"),
      device_id_("CTS01"),
      device_position_(""),
      device_product_date_(""),
      device_install_date_(""),
      motion_controller_name_("sys/motion/1"),
      encoder_name_("sys/encoder/1"),
      encoder_channel_(0),            // 默认编码器通道0
      move_range_(0),
      limit_number_(0),
      force_range_(0),
      hold_pos_(0.0),
      support_type_(""),           // 默认为空，从Property读取
      support_position_(""),       // 默认为空，从Property读取
      support_orientation_("vertical"),  // 默认垂直支撑
      force_sensor_channel_(0),    // 默认通道0
      force_sensor_controller_(""), // 默认为空，使用motionControllerName
      force_sensor_scale_(1000.0),  // 默认转换系数1000（电压V→力N）
      force_sensor_offset_(0.0),    // 默认零点偏移0
      token_assist_pos_(0.0),
      dire_pos_(0.0),
      target_force_(0.0),
      assist_lim_org_state_(0),
      assist_state_(false),
      support_logs_("[]"),
      fault_state_("NORMAL"),
      axis_parameter_("{}"),
      result_value_(0),
      position_unit_("mm"),
      self_check_result_(0),
      sim_mode_(true) {
    std::cout << "[DEBUG] AuxiliarySupportDevice constructor called for: " << device_name << std::endl;
    std::cout << "[DEBUG] Initial state (before init_device): " << Tango::DevStateName[get_state()] << std::endl;
    init_device();
    std::cout << "[DEBUG] State after constructor init_device(): " << Tango::DevStateName[get_state()] << std::endl;
}

AuxiliarySupportDevice::~AuxiliarySupportDevice() {
    delete_device();
}

void AuxiliarySupportDevice::init_device() {
    INFO_STREAM << "========== [DEBUG] init_device() START ==========" << std::endl;
    INFO_STREAM << "[DEBUG] Current state before parent init: " << Tango::DevStateName[get_state()] << std::endl;
    
    Common::StandardSystemDevice::init_device();
    
    INFO_STREAM << "[DEBUG] Current state after parent init: " << Tango::DevStateName[get_state()] << std::endl;
    
    // Get properties - 固有状态属性
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
    db_data.push_back(Tango::DbDatum("devicePosition"));
    db_data.push_back(Tango::DbDatum("deviceProductDate"));
    db_data.push_back(Tango::DbDatum("deviceInstallDate"));
    // 运行参数属性
    db_data.push_back(Tango::DbDatum("motionControllerName"));
    db_data.push_back(Tango::DbDatum("encoderName"));
    db_data.push_back(Tango::DbDatum("encoderChannel"));  // 编码器采集器通道号
    db_data.push_back(Tango::DbDatum("axis_ids"));
    db_data.push_back(Tango::DbDatum("axisId"));  // 兼容单轴配置
    db_data.push_back(Tango::DbDatum("moveRange"));
    db_data.push_back(Tango::DbDatum("limitNumber"));
    db_data.push_back(Tango::DbDatum("forceRange"));
    db_data.push_back(Tango::DbDatum("holdPos"));
    // 新增Property
    db_data.push_back(Tango::DbDatum("supportType"));
    db_data.push_back(Tango::DbDatum("supportPosition"));
    db_data.push_back(Tango::DbDatum("supportOrientation"));
    db_data.push_back(Tango::DbDatum("forceSensorChannel"));
    db_data.push_back(Tango::DbDatum("forceSensorController"));
    db_data.push_back(Tango::DbDatum("forceSensorScale"));
    db_data.push_back(Tango::DbDatum("forceSensorOffset"));
    db_data.push_back(Tango::DbDatum("motorStepAngle"));
    db_data.push_back(Tango::DbDatum("motorGearRatio"));
    db_data.push_back(Tango::DbDatum("motorSubdivision"));
    db_data.push_back(Tango::DbDatum("driverPowerPort"));
    db_data.push_back(Tango::DbDatum("driverPowerController"));
    get_db_device()->get_property(db_data);
    
    // 固有状态属性赋值
    if (!db_data[0].is_empty()) db_data[0] >> bundle_no_;
    if (!db_data[1].is_empty()) db_data[1] >> laser_no_;
    if (!db_data[2].is_empty()) db_data[2] >> system_no_;
    if (!db_data[3].is_empty()) db_data[3] >> sub_dev_list_;
    if (!db_data[4].is_empty()) db_data[4] >> model_list_;
    if (!db_data[5].is_empty()) db_data[5] >> current_model_;
    if (!db_data[6].is_empty()) db_data[6] >> connect_string_;
    if (!db_data[7].is_empty()) db_data[7] >> error_dict_;
    if (!db_data[8].is_empty()) db_data[8] >> device_name_;
    if (!db_data[9].is_empty()) db_data[9] >> device_id_;
    if (!db_data[10].is_empty()) db_data[10] >> device_position_;
    if (!db_data[11].is_empty()) db_data[11] >> device_product_date_;
    if (!db_data[12].is_empty()) db_data[12] >> device_install_date_;
    // 运行参数属性赋值
    if (!db_data[13].is_empty()) db_data[13] >> motion_controller_name_;
    if (!db_data[14].is_empty()) db_data[14] >> encoder_name_;
    if (!db_data[15].is_empty()) db_data[15] >> encoder_channel_;
    if (!db_data[16].is_empty()) db_data[16] >> axis_ids_;
    // 兼容单轴配置：如果 axis_ids 为空，从 axisId 读取
    if (axis_ids_.empty() && !db_data[17].is_empty()) {
        long single_axis_id = 0;
        db_data[17] >> single_axis_id;
        axis_ids_.push_back(single_axis_id);
    }
    if (!db_data[18].is_empty()) db_data[18] >> move_range_;
    if (!db_data[19].is_empty()) db_data[19] >> limit_number_;
    if (!db_data[20].is_empty()) db_data[20] >> force_range_;
    if (!db_data[21].is_empty()) db_data[21] >> hold_pos_;
    // 读取新增Property
    if (!db_data[22].is_empty()) db_data[22] >> support_type_;
    if (!db_data[23].is_empty()) db_data[23] >> support_position_;
    if (!db_data[24].is_empty()) db_data[24] >> support_orientation_;
    if (!db_data[25].is_empty()) db_data[25] >> force_sensor_channel_;
    if (!db_data[26].is_empty()) db_data[26] >> force_sensor_controller_;
    if (!db_data[27].is_empty()) db_data[27] >> force_sensor_scale_; else force_sensor_scale_ = 1000.0;
    if (!db_data[28].is_empty()) db_data[28] >> force_sensor_offset_; else force_sensor_offset_ = 0.0;
    if (!db_data[29].is_empty()) db_data[29] >> motor_step_angle_; else motor_step_angle_ = 1.8;
    if (!db_data[30].is_empty()) db_data[30] >> motor_gear_ratio_; else motor_gear_ratio_ = 1.0;
    if (!db_data[31].is_empty()) db_data[31] >> motor_subdivision_; else motor_subdivision_ = 12800.0;
    if (!db_data[32].is_empty()) db_data[32] >> driver_power_port_; else driver_power_port_ = -1;
    if (!db_data[33].is_empty()) db_data[33] >> driver_power_controller_;
    
    // 初始化状态
    driver_power_enabled_ = false;
    
    // 根据supportType和supportPosition更新deviceName和deviceID
    update_device_name_and_id();
    
    // 从系统配置读取模拟模式设置（启动时的初始默认值）
    // 注意：运行时可以通过 simSwitch 命令或GUI切换，但重启后会恢复配置文件的值（不持久化）
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
    
    INFO_STREAM << "[DEBUG] State before connect_proxies: " << Tango::DevStateName[get_state()] << std::endl;
    
    log_event("AuxiliarySupportDevice (辅助支撑类) initialized");
    INFO_STREAM << "AuxiliarySupportDevice initialized. Motion: " << motion_controller_name_ 
                << ", Encoder: " << encoder_name_ << std::endl;
    
    connect_proxies();

    // Start LargeStroke-style background connection monitor: command path reads connection_healthy_ only.
    start_connection_monitor();
    
    INFO_STREAM << "[DEBUG] State after connect_proxies: " << Tango::DevStateName[get_state()] << std::endl;
    INFO_STREAM << "========== [DEBUG] init_device() END ==========" << std::endl;
}

void AuxiliarySupportDevice::delete_device() {
    stop_connection_monitor();
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        motion_controller_proxy_.reset();
        encoder_proxy_.reset();
    }
    Common::StandardSystemDevice::delete_device();
}

// ===== Proxy helpers (lifetime-safe, LargeStroke-style) =====
std::shared_ptr<Tango::DeviceProxy> AuxiliarySupportDevice::get_motion_controller_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return motion_controller_proxy_;
}

std::shared_ptr<Tango::DeviceProxy> AuxiliarySupportDevice::get_encoder_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return encoder_proxy_;
}

void AuxiliarySupportDevice::reset_motion_controller_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    motion_controller_proxy_.reset();
}

void AuxiliarySupportDevice::reset_encoder_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    encoder_proxy_.reset();
}

void AuxiliarySupportDevice::rebuild_motion_controller_proxy(int timeout_ms) {
    if (motion_controller_name_.empty()) {
        return;
    }
    auto new_motion = create_proxy_and_ping(motion_controller_name_, timeout_ms);
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        motion_controller_proxy_ = new_motion;
    }
}

void AuxiliarySupportDevice::rebuild_encoder_proxy(int timeout_ms) {
    if (encoder_name_.empty()) {
        return;
    }
    auto new_enc = create_proxy_and_ping(encoder_name_, timeout_ms);
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        encoder_proxy_ = new_enc;
    }
}

// ===== Background connection monitor (LargeStroke-style) =====
void AuxiliarySupportDevice::start_connection_monitor() {
    if (sim_mode_) {
        connection_healthy_.store(true);
        return;
    }
    if (connection_monitor_thread_.joinable()) {
        return;
    }
    stop_connection_monitor_.store(false);
    connection_monitor_thread_ = std::thread(&AuxiliarySupportDevice::connection_monitor_loop, this);
}

void AuxiliarySupportDevice::stop_connection_monitor() {
    stop_connection_monitor_.store(true);
    if (connection_monitor_thread_.joinable()) {
        connection_monitor_thread_.join();
    }
}

void AuxiliarySupportDevice::connection_monitor_loop() {
    const int ping_timeout_ms = 300;

    while (!stop_connection_monitor_.load()) {
        if (sim_mode_) {
            connection_healthy_.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        bool motion_ok = motion_controller_name_.empty();
        bool encoder_ok = encoder_name_.empty();

        // 1) 快速 ping 现有 proxy（短超时）
        if (!motion_controller_name_.empty()) {
            auto motion = get_motion_controller_proxy();
            if (motion) {
                motion_ok = ping_with_temp_timeout(motion, ping_timeout_ms);
                if (!motion_ok) {
                    reset_motion_controller_proxy();
                }
            } else {
                motion_ok = false;
            }
        }

        if (!encoder_name_.empty()) {
            auto enc = get_encoder_proxy();
            if (enc) {
                encoder_ok = ping_with_temp_timeout(enc, ping_timeout_ms);
                if (!encoder_ok) {
                    reset_encoder_proxy();
                }
            } else {
                encoder_ok = false;
            }
        }

        // 2) 按间隔尝试重连（仅创建 proxy + ping，不做上电/刹车/参数恢复等副作用）
        const int connect_timeout_ms = 500;
        auto now = std::chrono::steady_clock::now();
        auto reconnect_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_reconnect_attempt_).count();
        bool can_reconnect = (reconnect_elapsed >= Common::SystemConfig::PROXY_RECONNECT_INTERVAL_SEC);

        if (can_reconnect && (!motion_ok || !encoder_ok)) {
            last_reconnect_attempt_ = now;
            if (!motion_ok && !motion_controller_name_.empty()) {
                try {
                    rebuild_motion_controller_proxy(connect_timeout_ms);
                    motion_ok = true;
                    // 注意：不立即设置 connection_healthy_ = true，等待恢复操作完成后再设置

                    // Defer side-effects (power/brake/params/sync) to main thread
                    motion_restore_pending_.store(true);
                } catch (...) {
                    // 连接失败，保持不健康
                }
            }
            if (!encoder_ok && !encoder_name_.empty()) {
                try {
                    rebuild_encoder_proxy(connect_timeout_ms);
                    encoder_ok = true;
                } catch (...) {
                    // ignore
                }
            }
        }

        // 3) 更新连接健康标志（命令路径只读此标志，零等待）
        // 只有在 motion 和 encoder 都正常，且没有待处理的恢复操作时，才设置为健康
        bool was_healthy = connection_healthy_.load();
        bool should_be_healthy = motion_ok && encoder_ok && !motion_restore_pending_.load();
        if (should_be_healthy != was_healthy) {
            connection_healthy_.store(should_be_healthy);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void AuxiliarySupportDevice::connect_proxies() {
    INFO_STREAM << "[DEBUG] connect_proxies() called. sim_mode=" << sim_mode_ << std::endl;
    try {
        if (sim_mode_) {
            INFO_STREAM << "[DEBUG] Sim mode: skipping proxy connection" << std::endl;
            set_state(Tango::ON);
            set_status("Simulation Mode");
            connection_healthy_.store(true);
            return;
        }

        // Aggressive simplification: always rebuild proxies when connect_proxies() is called.
        // This avoids duplicating ping/reconnect logic here vs. monitor loop.
        reset_motion_controller_proxy();
        reset_encoder_proxy();

        INFO_STREAM << "[DEBUG] Rebuilding proxies (motion=" << motion_controller_name_
                    << ", encoder=" << encoder_name_ << ")" << std::endl;
        if (!motion_controller_name_.empty()) {
            rebuild_motion_controller_proxy(500);
        }
        if (!encoder_name_.empty()) {
            rebuild_encoder_proxy(500);
        }

        set_state(Tango::ON);
        set_status("Connected");
        connection_healthy_.store(true);
        INFO_STREAM << "[DEBUG] All proxies connected. Device state: ON, status: Connected" << std::endl;

        // Initial connect should perform restore immediately.
        perform_post_motion_reconnect_restore();
        motion_restore_pending_.store(false);
    } catch (Tango::DevFailed &e) {
        // 内层 catch 已经打印了详细的代理名称和错误信息
        // 注意：初始化阶段连接失败不应设置 FAULT，保持 UNKNOWN 状态
        // 用户可以通过 init 命令或 simSwitch 来重新初始化
        WARN_STREAM << "[DEBUG] connect_proxies() FAILED - proxy not available yet" << std::endl;
        WARN_STREAM << "[DEBUG] Exception: " << e.errors[0].desc << std::endl;
        WARN_STREAM << "[DEBUG] Keeping current state (not setting FAULT during init)" << std::endl;
        WARN_STREAM << "[DEBUG] Hint: Start motion controller first, or use simSwitch(1) for simulation mode" << std::endl;
        connection_healthy_.store(false);
        reconnect_pending_.store(true);
        set_status("Proxy connection pending - motion controller not ready");
    }
}

void AuxiliarySupportDevice::perform_post_motion_reconnect_restore() {
    if (sim_mode_) {
        return;
    }
    
    auto motion = get_motion_controller_proxy();
    auto encoder = get_encoder_proxy();
    
    if (!motion) {
        return;  // motion proxy 未连接，无法恢复
    }
    
    // 1. 恢复电机细分参数
    if (!axis_ids_.empty()) {
        try {
            long axis_id = axis_ids_[0];
            INFO_STREAM << "Applying motor subdivision parameters (axis=" << axis_id 
                       << ", stepAngle=" << motor_step_angle_ 
                       << ", gearRatio=" << motor_gear_ratio_ 
                       << ", subdivision=" << motor_subdivision_ << ")" << std::endl;
            Tango::DevVarDoubleArray params;
            params.length(4);
            params[0] = static_cast<double>(axis_id);
            params[1] = motor_step_angle_;
            params[2] = motor_gear_ratio_;
            params[3] = motor_subdivision_;
            Tango::DeviceData arg;
            arg << params;
            motion->command_inout("setStructParameter", arg);
            INFO_STREAM << "Successfully applied motor subdivision parameters to axis " << axis_id << std::endl;
        } catch (Tango::DevFailed &e) {
            WARN_STREAM << "Failed to apply motor subdivision parameters: " << e.errors[0].desc << std::endl;
        }
    }
    
    // 2. 恢复驱动器上电
    INFO_STREAM << "Restoring relay configuration and enabling driver power after reconnection..." << std::endl;
    if (enable_driver_power()) {
        INFO_STREAM << "Driver power enabled successfully after reconnection" << std::endl;
    } else {
        WARN_STREAM << "Failed to enable driver power after reconnection" << std::endl;
    }
    
    // 3. 同步编码器位置
    if (encoder && !axis_ids_.empty() && encoder_channel_ >= 0) {
        try {
            INFO_STREAM << "[DEBUG] Synchronizing encoder position to motion controller..." << std::endl;
            long axis_id = axis_ids_[0];
            short encoder_ch = encoder_channel_;
            
            // 读取编码器位置
            Tango::DeviceData data_in;
            data_in << encoder_ch;
            Tango::DeviceData data_out = encoder->command_inout("readEncoder", data_in);
            double encoder_pos;
            data_out >> encoder_pos;
            
            // 更新运动控制器计数
            Tango::DevVarDoubleArray params;
            params.length(2);
            params[0] = static_cast<double>(axis_id);
            params[1] = encoder_pos;
            Tango::DeviceData arg;
            arg << params;
            motion->command_inout("setEncoderPosition", arg);
            
            INFO_STREAM << "[DEBUG] Axis " << axis_id << " (encoder channel " << encoder_ch 
                       << "): synced position " << encoder_pos << " to motion controller" << std::endl;
            log_event("Encoder position synchronized to motion controller: " + std::to_string(encoder_pos));
        } catch (Tango::DevFailed &e) {
            WARN_STREAM << "[DEBUG] Failed to sync encoder position: " << e.errors[0].desc << std::endl;
        }
    }
}

// ===== LOCK/UNLOCK COMMANDS =====
void AuxiliarySupportDevice::devLock(Tango::DevString user_info) {
    check_state_for_lock_commands();  // 状态机检查: UNKNOWN, OFF, FAULT
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (is_locked_ && lock_user_ != std::string(user_info)) {
        Tango::Except::throw_exception("DEVICE_LOCKED",
            "Device locked by " + lock_user_, "AuxiliarySupportDevice::devLock");
    }
    is_locked_ = true;
    lock_user_ = std::string(user_info);
    log_event("Device locked by " + lock_user_);
    result_value_ = 0;
}

void AuxiliarySupportDevice::devUnlock(Tango::DevBoolean unlock_all) {
    check_state_for_lock_commands();  // 状态机检查: UNKNOWN, OFF, FAULT
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (unlock_all || is_locked_) {
        log_event("Device unlocked (was " + lock_user_ + ")");
        is_locked_ = false;
        lock_user_ = "";
    }
    result_value_ = 0;
}

void AuxiliarySupportDevice::devLockVerify() {
    check_state_for_all_states();  // 状态机检查: 所有状态
    std::lock_guard<std::mutex> lock(lock_mutex_);
    result_value_ = is_locked_ ? 1 : 0;
}

Tango::DevString AuxiliarySupportDevice::devLockQuery() {
    check_state_for_all_states();  // 状态机检查: 所有状态
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string result = is_locked_ ? lock_user_ : "UNLOCKED";
    return CORBA::string_dup(result.c_str());
}

void AuxiliarySupportDevice::devUserConfig(Tango::DevString config) {
    check_state_for_all_states();  // 状态机检查: 所有状态
    std::string cfg(config);
    log_event("User config: " + cfg);
    result_value_ = 0;
}

// ===== SYSTEM COMMANDS =====
void AuxiliarySupportDevice::selfCheck() {
    INFO_STREAM << "[DEBUG] selfCheck() called, current state: " << Tango::DevStateName[get_state()] << std::endl;
    check_state_for_init_commands();  // 状态机检查: UNKNOWN, OFF, FAULT
    log_event("Self check started");
    self_check_result_ = 0;
    
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] selfCheck(): Simulation mode, returning success" << std::endl;
        self_check_result_ = 0;
    } else {
        INFO_STREAM << "[DEBUG] selfCheck(): Real mode, pinging proxies..." << std::endl;
        try {
            auto motion = get_motion_controller_proxy();
            if (motion) {
                INFO_STREAM << "[DEBUG] selfCheck(): Pinging motion_controller_proxy_" << std::endl;
                motion->ping();
            } else {
                WARN_STREAM << "[DEBUG] selfCheck(): motion_controller_proxy_ is NULL!" << std::endl;
            }
            auto encoder = get_encoder_proxy();
            if (encoder) {
                INFO_STREAM << "[DEBUG] selfCheck(): Pinging encoder_proxy_" << std::endl;
                encoder->ping();
            } else {
                WARN_STREAM << "[DEBUG] selfCheck(): encoder_proxy_ is NULL!" << std::endl;
            }
        } catch (...) {
            ERROR_STREAM << "[DEBUG] selfCheck(): Ping failed!" << std::endl;
            self_check_result_ = 1;
        }
    }
    
    result_value_ = (self_check_result_ == 0) ? 0 : 1;
    INFO_STREAM << "[DEBUG] selfCheck() completed, result=" << self_check_result_ << std::endl;
    log_event("Self check completed: " + std::to_string(self_check_result_));
}

void AuxiliarySupportDevice::init() {
    INFO_STREAM << "[DEBUG] init() command called, current state: " << Tango::DevStateName[get_state()] << std::endl;
    check_state_for_init_commands();  // 状态机检查: UNKNOWN, OFF, FAULT
    log_event("Init started");
    
    // 真实模式下，尝试重新连接代理（如果尚未连接）
    if (!sim_mode_) {
        INFO_STREAM << "[DEBUG] init(): Real mode - attempting to connect proxies..." << std::endl;
        
        // 检查并尝试连接运动控制器代理
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            try {
                INFO_STREAM << "[DEBUG] init(): Connecting to motion controller: " << motion_controller_name_ << std::endl;
                rebuild_motion_controller_proxy(500);
                motion = get_motion_controller_proxy();
                if (!motion) {
                    throw std::runtime_error("Failed to create motion controller proxy");
                }
                INFO_STREAM << "[DEBUG] init(): Motion controller connected successfully" << std::endl;
                
                // 应用电机参数
                if (!axis_ids_.empty()) {
                    try {
                        long axis_id = axis_ids_[0];
                        Tango::DevVarDoubleArray params;
                        params.length(4);
                        params[0] = static_cast<double>(axis_id);
                        params[1] = motor_step_angle_;
                        params[2] = motor_gear_ratio_;
                        params[3] = motor_subdivision_;
                        Tango::DeviceData arg;
                        arg << params;
                        motion->command_inout("setStructParameter", arg);
                        INFO_STREAM << "[DEBUG] init(): Motor parameters applied to axis " << axis_id << std::endl;
                    } catch (Tango::DevFailed &e) {
                        WARN_STREAM << "[DEBUG] init(): Failed to apply motor parameters: " << e.errors[0].desc << std::endl;
                    }
                }
                
                // 启动驱动器电源
                enable_driver_power();
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] init(): Failed to connect motion controller: " << e.errors[0].desc << std::endl;
                result_value_ = 1;
                set_state(Tango::FAULT);
                fault_state_ = "PROXY_ERROR";
                Tango::Except::throw_exception("INIT_FAILED",
                    "Cannot connect to motion controller: " + std::string(e.errors[0].desc),
                    "AuxiliarySupportDevice::init");
            } catch (...) {
                ERROR_STREAM << "[DEBUG] init(): Failed to connect motion controller: unknown error" << std::endl;
                result_value_ = 1;
                set_state(Tango::FAULT);
                fault_state_ = "PROXY_ERROR";
                Tango::Except::throw_exception("INIT_FAILED",
                    "Cannot connect to motion controller: unknown error",
                    "AuxiliarySupportDevice::init");
            }
        }
        
        // 检查并尝试连接编码器代理
        auto encoder = get_encoder_proxy();
        if (!encoder) {
            try {
                INFO_STREAM << "[DEBUG] init(): Connecting to encoder: " << encoder_name_ << std::endl;
                rebuild_encoder_proxy(500);
                INFO_STREAM << "[DEBUG] init(): Encoder connected successfully" << std::endl;
            } catch (Tango::DevFailed &e) {
                WARN_STREAM << "[DEBUG] init(): Failed to connect encoder (non-critical): " << e.errors[0].desc << std::endl;
                // 编码器连接失败不阻止初始化，只是警告
            }
        }
    }
    
    token_assist_pos_ = 0.0;
    dire_pos_ = 0.0;
    target_force_ = 0.0;
    assist_state_ = false;
    fault_state_ = "NORMAL";
    INFO_STREAM << "[DEBUG] init(): Setting state to ON" << std::endl;
    set_state(Tango::ON);
    set_status("Device initialized and ready");
    result_value_ = 0;
    INFO_STREAM << "[DEBUG] init() completed, final state: " << Tango::DevStateName[get_state()] << std::endl;
    log_event("Init completed");
}

void AuxiliarySupportDevice::reset() {
    INFO_STREAM << "[DEBUG] reset() called, current state: " << Tango::DevStateName[get_state()] << std::endl;
    check_state_for_reset();  // 状态机检查: ON, FAULT
    log_event("Reset started");
    Common::StandardSystemDevice::reset();

    // Clear latched limit fault
    limit_fault_latched_.store(false);
    limit_fault_el_state_.store(0);
    
    // 真实模式下，调用运动控制器的 reset 命令清除报警
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && motion) {
        INFO_STREAM << "[DEBUG] reset(): Calling motion controller reset for " << axis_ids_.size() << " axes" << std::endl;
        if (!motion) {
            throw std::runtime_error("Motion controller proxy not available");
        }
        try {
            for (long axis : axis_ids_) {
                Tango::DeviceData data_in;
                data_in << static_cast<Tango::DevShort>(axis);
                motion->command_inout("reset", data_in);
            }
        } catch (Tango::DevFailed &e) {
            WARN_STREAM << "reset: Failed to reset motion controller: " << e.errors[0].desc << std::endl;
        }
    }
    
    stop();
    fault_state_ = "NORMAL";
    INFO_STREAM << "[DEBUG] reset(): Setting state to ON" << std::endl;
    set_state(Tango::ON);
    result_value_ = 0;
    INFO_STREAM << "[DEBUG] reset() completed, final state: " << Tango::DevStateName[get_state()] << std::endl;
    log_event("Reset completed");
}

// ===== PARAMETER COMMANDS =====
void AuxiliarySupportDevice::moveAxisSet(const Tango::DevVarDoubleArray *params) {
    check_state_for_param_commands();  // 状态机检查: OFF, ON, FAULT
    // Set movement parameters: [move_range, limit_number]
    if (params->length() >= 2) {
        move_range_ = (short)(*params)[0];
        limit_number_ = (short)(*params)[1];
        log_event("MoveAxisSet: range=" + std::to_string(move_range_) + 
                  ", limit=" + std::to_string(limit_number_));
    }
    result_value_ = 0;
}

void AuxiliarySupportDevice::structAxisSet(const Tango::DevVarDoubleArray *params) {
    check_state_for_param_commands();  // 状态机检查: OFF, ON, FAULT
    // Set structural parameters
    if (params->length() >= 1) {
        force_range_ = (short)(*params)[0];
        log_event("StructAxisSet: force_range=" + std::to_string(force_range_));
    }
    result_value_ = 0;
}

// ===== MOTION COMMANDS =====
void AuxiliarySupportDevice::moveRelative(Tango::DevDouble distance) {
    INFO_STREAM << "[DEBUG] moveRelative(" << distance << ") called, current state: " 
               << Tango::DevStateName[get_state()] << std::endl;
    check_state_for_motion_commands();  // 状态机检查: 仅ON状态
    log_event("MoveRelative: " + std::to_string(distance));
    
    if (sim_mode_) {
        assist_state_ = true;
        set_state(Tango::MOVING);
        dire_pos_ += distance;
        token_assist_pos_ += distance;
        assist_state_ = false;
        set_state(Tango::ON);
        result_value_ = 0;
        return;
    }
    
    // 真实模式下，必须有控制器连接和轴配置
    // 注意：check_state_for_motion_commands()已经检查了状态必须是ON
    // 如果状态不是ON（比如FAULT、OFF等），说明网络未连接或设备异常，会直接抛出异常
    auto motion = get_motion_controller_proxy();
    if (!motion) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_ProxyError",
            "Motion controller not connected. Cannot move in real mode.",
            "AuxiliarySupportDevice::moveRelative");
    }
    if (axis_ids_.empty()) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_ConfigError",
            "No axis configured. Cannot move.",
            "AuxiliarySupportDevice::moveRelative");
    }
    
    assist_state_ = true;
    set_state(Tango::MOVING);
    
    try {
        Tango::DevVarDoubleArray args;
        args.length(2);
        args[0] = static_cast<double>(axis_ids_[0]);
        args[1] = distance;
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            throw std::runtime_error("Motion controller proxy not available");
        }
        Tango::DeviceData data_in;
        data_in << args;
        motion->command_inout("moveRelative", data_in);
        result_value_ = 0;
    } catch (Tango::DevFailed &e) {
        assist_state_ = false;
        ERROR_STREAM << "[FAULT] moveRelative command failed: " << e.errors[0].desc 
                     << ", setting FAULT state" << std::endl;
        set_state(Tango::FAULT);
        fault_state_ = "MOTION_ERROR";
        result_value_ = 1;
        log_event("moveRelative failed: " + std::string(e.errors[0].desc.in()));
        Tango::Except::re_throw_exception(e,
            "API_CommandFailed",
            "moveRelative failed: " + std::string(e.errors[0].desc),
            "AuxiliarySupportDevice::moveRelative");
    }
}

void AuxiliarySupportDevice::moveAbsolute(Tango::DevDouble position) {
    check_state_for_motion_commands();  // 状态机检查: 仅ON状态
    log_event("MoveAbsolute: " + std::to_string(position));
    
    if (sim_mode_) {
        assist_state_ = true;
        set_state(Tango::MOVING);
        dire_pos_ = position;
        token_assist_pos_ = position;
        assist_state_ = false;
        set_state(Tango::ON);
        result_value_ = 0;
        return;
    }
    
    // 真实模式下，必须有控制器连接和轴配置
    // 注意：check_state_for_motion_commands()已经检查了状态必须是ON
    // 如果状态不是ON（比如FAULT、OFF等），说明网络未连接或设备异常，会直接抛出异常
    auto motion = get_motion_controller_proxy();
    if (!motion) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_ProxyError",
            "Motion controller not connected. Cannot move in real mode.",
            "AuxiliarySupportDevice::moveAbsolute");
    }
    if (axis_ids_.empty()) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_ConfigError",
            "No axis configured. Cannot move.",
            "AuxiliarySupportDevice::moveAbsolute");
    }
    
    assist_state_ = true;
    set_state(Tango::MOVING);
    
    try {
        Tango::DevVarDoubleArray args;
        args.length(2);
        args[0] = static_cast<double>(axis_ids_[0]);
        args[1] = position;
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            throw std::runtime_error("Motion controller proxy not available");
        }
        Tango::DeviceData data_in;
        data_in << args;
        motion->command_inout("moveAbsolute", data_in);
        result_value_ = 0;
    } catch (Tango::DevFailed &e) {
        assist_state_ = false;
        set_state(Tango::FAULT);
        fault_state_ = "MOTION_ERROR";
        result_value_ = 1;
        Tango::Except::re_throw_exception(e,
            "API_CommandFailed",
            "moveAbsolute failed: " + std::string(e.errors[0].desc),
            "AuxiliarySupportDevice::moveAbsolute");
    }
}

void AuxiliarySupportDevice::stop() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    log_event("Stop");
    
    if (!sim_mode_) {
        // 真实模式下，必须有控制器连接
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            result_value_ = 1;
            Tango::Except::throw_exception("API_ProxyError",
                "Motion controller not connected. Cannot stop in real mode.",
                "AuxiliarySupportDevice::stop");
        }
        
        try {
            for (long axis : axis_ids_) {
                auto motion = get_motion_controller_proxy();
                if (motion) {
                    Tango::DeviceData data_in;
                    data_in << static_cast<Tango::DevShort>(axis);
                    motion->command_inout("stopMove", data_in);
                }
            }
        } catch (Tango::DevFailed &e) {
            result_value_ = 1;
            Tango::Except::re_throw_exception(e,
                "API_CommandFailed", 
                "stopMove command failed: " + std::string(e.errors[0].desc),
                "AuxiliarySupportDevice::stop");
        }
    }
    
    // 始终清除运动状态标志
    assist_state_ = false;
    if (limit_fault_latched_.load() || get_state() == Tango::FAULT) {
        set_state(Tango::FAULT);
        if (!fault_state_.empty()) {
            set_status(fault_state_);
        }
    } else {
        set_state(Tango::ON);
    }
    result_value_ = 0;
}

// ===== READ COMMANDS =====
Tango::DevDouble AuxiliarySupportDevice::readEncoder() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    if (sim_mode_) {
        return token_assist_pos_;
    }
    
    // 真实模式：通过 encoder_device 的 readEncoder(channel) 命令读取编码器位置
    auto encoder = get_encoder_proxy();
    if (!encoder) {
        WARN_STREAM << "readEncoder: encoder_proxy_ is NULL, returning cached value" << std::endl;
        return token_assist_pos_;
    }
    
    try {
        Tango::DeviceData data_in;
        data_in << encoder_channel_;
        Tango::DeviceData data_out = encoder->command_inout("readEncoder", data_in);
        double pos;
        data_out >> pos;
        token_assist_pos_ = pos;
        return pos;
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "readEncoder failed: " << e.errors[0].desc << " - returning cached value" << std::endl;
        return token_assist_pos_;
    }
}

Tango::DevDouble AuxiliarySupportDevice::readForce() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    if (sim_mode_) {
        return target_force_;
    }
    
    // 真实模式：通过运动控制器的 readAD 命令读取力传感器值
    // 如果配置了独立的力传感器控制器，使用它；否则使用运动控制器
    std::shared_ptr<Tango::DeviceProxy> force_ctrl;
    bool created_new = false;
    
    if (!force_sensor_controller_.empty() && 
        force_sensor_controller_ != motion_controller_name_) {
        // 使用独立的力传感器控制器
        try {
            force_ctrl = create_proxy_and_ping(force_sensor_controller_, 500);
            created_new = true;
            INFO_STREAM << "[ForceSensor] Using dedicated force sensor controller: " << force_sensor_controller_ << std::endl;
        } catch (Tango::DevFailed &e) {
            WARN_STREAM << "readForce: Failed to connect to force sensor controller " << force_sensor_controller_ 
                       << ": " << e.errors[0].desc << ", falling back to motion controller" << std::endl;
            force_ctrl = get_motion_controller_proxy();
        }
    } else {
        // 使用运动控制器
        force_ctrl = get_motion_controller_proxy();
    }
    
    if (!force_ctrl) {
        WARN_STREAM << "readForce: force sensor controller proxy is NULL, returning cached value" << std::endl;
        return target_force_;
    }
    
    try {
        Tango::DeviceData data_in;
        data_in << static_cast<Tango::DevShort>(force_sensor_channel_);
        Tango::DeviceData data_out = force_ctrl->command_inout("readAD", data_in);
        Tango::DevDouble raw_voltage;
        data_out >> raw_voltage;
        
        // 应用力传感器转换公式: 力值 = (原始电压 - 零点偏移) × 转换系数
        Tango::DevDouble force_value = (raw_voltage - force_sensor_offset_) * force_sensor_scale_;
        
        DEBUG_STREAM << "[ForceSensor] raw_voltage=" << raw_voltage 
                    << ", offset=" << force_sensor_offset_ 
                    << ", scale=" << force_sensor_scale_ 
                    << ", force=" << force_value << std::endl;
        
        target_force_ = force_value;
        return force_value;
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "readForce failed: " << e.errors[0].desc << " - returning cached value" << std::endl;
        return target_force_;
    }
}

Tango::DevBoolean AuxiliarySupportDevice::readOrg() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    if (sim_mode_) {
        return true;
    }
    
    // 真实模式：通过运动控制器的 readOrg 命令读取原点状态
    auto motion = get_motion_controller_proxy();
    if (!motion) {
        WARN_STREAM << "readOrg: motion_controller_proxy_ is NULL, returning cached value" << std::endl;
        return (assist_lim_org_state_ & 0x01) != 0;
    }
    
    if (axis_ids_.empty()) {
        WARN_STREAM << "readOrg: axis_ids_ is empty, returning cached value" << std::endl;
        return (assist_lim_org_state_ & 0x01) != 0;
    }
    
    try {
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            throw std::runtime_error("Motion controller proxy not available");
        }
        Tango::DeviceData data_in;
        data_in << static_cast<Tango::DevShort>(axis_ids_[0]);
        Tango::DeviceData data_out = motion->command_inout("readOrg", data_in);
        Tango::DevBoolean org_state;
        data_out >> org_state;
        // 更新缓存
        if (org_state) {
            assist_lim_org_state_ |= 0x01;
        } else {
            assist_lim_org_state_ &= ~0x01;
        }
        return org_state;
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "readOrg failed: " << e.errors[0].desc << " - returning cached value" << std::endl;
        return (assist_lim_org_state_ & 0x01) != 0;
    }
}

Tango::DevShort AuxiliarySupportDevice::readEL() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    if (sim_mode_) {
        return 0; // No limit triggered
    }
    
    // 真实模式：通过运动控制器的 readEL 命令读取限位状态
    auto motion = get_motion_controller_proxy();
    if (!motion) {
        WARN_STREAM << "readEL: motion_controller_proxy_ is NULL, returning cached value" << std::endl;
        return assist_lim_org_state_;
    }
    
    if (axis_ids_.empty()) {
        WARN_STREAM << "readEL: axis_ids_ is empty, returning cached value" << std::endl;
        return assist_lim_org_state_;
    }
    
    try {
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            throw std::runtime_error("Motion controller proxy not available");
        }
        Tango::DeviceData data_in;
        data_in << static_cast<Tango::DevShort>(axis_ids_[0]);
        Tango::DeviceData data_out = motion->command_inout("readEL", data_in);
        Tango::DevShort el_state_raw;
        data_out >> el_state_raw;
        
        // 限位开关低电平有效：硬件读取0（低电平）表示触发，1（高电平）表示未触发
        // 运动控制器当前逻辑：pos_limit==1返回1(EL+), neg_limit==1返回-1(EL-), 否则返回0
        // 如果限位开关是低电平有效，需要反转：当硬件读取0时应该返回限位触发
        // 由于运动控制器返回0时无法区分方向，我们在auxiliary_support_device中反转逻辑
        // 反转规则：当readEL返回0时，认为是限位触发（统一处理为EL+）
        //          当readEL返回非0时，认为是未触发（转换为0）
        Tango::DevShort el_state = 0;
        if (el_state_raw == 0) {
            // 低电平有效：返回0表示限位触发（但无法区分方向，统一处理为EL+）
            el_state = 1;  // 转换为EL+表示限位触发
        }
        // 如果返回非0（1或-1），说明硬件读取是高电平，限位未触发，el_state保持为0
        
        assist_lim_org_state_ = el_state;

        if (el_state != 0) {
            if (!limit_fault_latched_.exchange(true)) {
                limit_fault_el_state_.store(el_state);
                std::string dir = (el_state > 0) ? "EL+" : "EL-";
                fault_state_ = "Limit switch triggered: " + dir;
                set_status(fault_state_);
                set_state(Tango::FAULT);
                // Best-effort stop
                try {
                    for (long axis : axis_ids_) {
                        auto motion = get_motion_controller_proxy();
                        if (motion) {
                            Tango::DeviceData stop_in;
                            stop_in << static_cast<Tango::DevShort>(axis);
                            motion->command_inout("stopMove", stop_in);
                        }
                    }
                } catch (...) {
                }
            }
        }
        return el_state;
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "readEL failed: " << e.errors[0].desc << " - returning cached value" << std::endl;
        return assist_lim_org_state_;
    }
}

// ===== AUTO/FORCE COMMANDS =====
void AuxiliarySupportDevice::assistAuto(Tango::DevDouble position) {
    check_state_for_motion_commands();  // 状态机检查: 仅ON状态
    log_event("AssistAuto to: " + std::to_string(position));
    assist_state_ = true;
    set_state(Tango::MOVING);
    
    if (sim_mode_) {
        dire_pos_ = position;
        token_assist_pos_ = position;
        assist_state_ = false;
        set_state(Tango::ON);
    } else {
        // Automatic movement with force control
        moveAbsolute(position);
    }
    result_value_ = 0;
}

void AuxiliarySupportDevice::setHoldPos(Tango::DevDouble position) {
    check_state_for_motion_commands();  // 状态机检查: 仅ON状态
    hold_pos_ = position;
    log_event("SetHoldPos: " + std::to_string(position));
    result_value_ = 0;
}

void AuxiliarySupportDevice::setForceZero() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    target_force_ = 0.0;
    log_event("Force zeroed");
    result_value_ = 0;
}

// ===== EXPORT COMMANDS =====
Tango::DevString AuxiliarySupportDevice::readtAxis() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    std::ostringstream oss;
    oss << "{\"tokenAssistPos\":" << token_assist_pos_ << ","
        << "\"direPos\":" << dire_pos_ << ","
        << "\"targetForce\":" << target_force_ << ","
        << "\"holdPos\":" << hold_pos_ << ","
        << "\"assistState\":" << (assist_state_ ? "true" : "false") << ","
        << "\"faultState\":\"" << fault_state_ << "\"}";
    return CORBA::string_dup(oss.str().c_str());
}

void AuxiliarySupportDevice::exportAxis() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    log_event("Axis parameters exported");
    result_value_ = 0;
}

void AuxiliarySupportDevice::simSwitch(Tango::DevShort mode) {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    bool was_sim_mode = sim_mode_;
    sim_mode_ = (mode != 0);
    
    // 注意：运行时切换只影响当前会话，server 重启后恢复配置文件的值（不持久化）
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] simSwitch: Enabling SIMULATION MODE (运行时切换，不持久化)" << std::endl;
        reset_motion_controller_proxy();
        reset_encoder_proxy();
        set_status("Simulation Mode");
        log_event("Simulation mode enabled (runtime switch)");
    } else {
        INFO_STREAM << "[DEBUG] simSwitch: Disabling simulation mode, switching to REAL MODE (运行时切换，不持久化)" << std::endl;
        set_status("Connecting to Hardware...");
        connect_proxies();
        if (get_state() != Tango::FAULT) {
            set_status("Connected to Hardware");
        }
        // 如果从模拟模式切换到真实模式，且代理未连接，尝试连接代理
        auto motion = get_motion_controller_proxy();
        auto encoder = get_encoder_proxy();
        if (was_sim_mode && (!motion || !encoder)) {
            INFO_STREAM << "[DEBUG] simSwitch: Switching from sim mode to real mode, connecting proxies..." << std::endl;
            try {
                connect_proxies();
                INFO_STREAM << "[DEBUG] simSwitch: Proxies connected successfully" << std::endl;
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] simSwitch: Failed to connect proxies: " << e.errors[0].desc << std::endl;
            } catch (...) {
                ERROR_STREAM << "[DEBUG] simSwitch: Failed to connect proxies with unknown exception" << std::endl;
            }
        }
        log_event("Simulation mode disabled (runtime switch)");
    }
    result_value_ = 0;
}

// ===== ATTRIBUTE READERS =====
void AuxiliarySupportDevice::read_attr(Tango::Attribute &attr) {
    std::string attr_name = attr.get_name();
    
    // DEBUG: 确认 read_attr 被调用
    INFO_STREAM << "[DEBUG] read_attr called for: " << attr_name << std::endl;
    
    try {
        if (attr_name == "selfCheckResult") {
            read_self_check_result(attr);
        } else if (attr_name == "positionUnit") {
            read_position_unit(attr);
        } else if (attr_name == "groupAttributeJson") {
            read_group_attribute_json(attr);
        } else if (attr_name == "tokenAssistPos") {
            read_token_assist_pos(attr);
        } else if (attr_name == "direPos") {
            read_dire_pos(attr);
        } else if (attr_name == "targetForce") {
            read_target_force(attr);
        } else if (attr_name == "forceValue") {
            read_force_value(attr);
        } else if (attr_name == "supportLogs") {
            read_support_logs(attr);
        } else if (attr_name == "faultState") {
            read_fault_state(attr);
        } else if (attr_name == "axisParameter") {
            read_axis_parameter(attr);
        } else if (attr_name == "AssistLimOrgState") {
            read_assist_lim_org_state(attr);
        } else if (attr_name == "AssistState") {
            read_assist_state(attr);
        } else if (attr_name == "resultValue") {
            read_result_value(attr);
        } else if (attr_name == "driverPowerStatus") {
            read_driver_power_status(attr);
        // 固有状态属性
        } else if (attr_name == "bundleNo") {
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
        } else if (attr_name == "deviceName") {
            read_device_name_attr(attr);
        } else if (attr_name == "deviceID") {
            read_device_id(attr);
        } else if (attr_name == "devicePosition") {
            read_device_position(attr);
        } else if (attr_name == "deviceProductDate") {
            read_device_product_date(attr);
        } else if (attr_name == "deviceInstallDate") {
            read_device_install_date(attr);
        // 新增支撑类型属性
        } else if (attr_name == "supportType") {
            read_support_type(attr);
        } else if (attr_name == "supportPosition") {
            read_support_position(attr);
        } else if (attr_name == "supportOrientation") {
            read_support_orientation(attr);
        } else if (attr_name == "forceSensorChannel") {
            read_force_sensor_channel(attr);
        } else {
            WARN_STREAM << "[DEBUG] read_attr: Unknown attribute: " << attr_name << std::endl;
        }
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "[DEBUG] read_attr exception for " << attr_name << ": " << e.errors[0].desc << std::endl;
        throw;
    } catch (std::exception &e) {
        ERROR_STREAM << "[DEBUG] read_attr std::exception for " << attr_name << ": " << e.what() << std::endl;
        throw;
    } catch (...) {
        ERROR_STREAM << "[DEBUG] read_attr unknown exception for " << attr_name << std::endl;
        throw;
    }
}

void AuxiliarySupportDevice::read_self_check_result(Tango::Attribute &attr) {
    attr.set_value(&self_check_result_);
}

void AuxiliarySupportDevice::read_position_unit(Tango::Attribute &attr) {
    attr_position_unit_read = Tango::string_dup(position_unit_.c_str());
    attr.set_value(&attr_position_unit_read);
}

void AuxiliarySupportDevice::write_position_unit(Tango::WAttribute &attr) {
    Tango::DevString new_unit;
    attr.get_write_value(new_unit);
    position_unit_ = std::string(new_unit);
}

void AuxiliarySupportDevice::write_attr(Tango::WAttribute &attr) {
    std::string attr_name = attr.get_name();
    
    if (attr_name == "positionUnit") {
        write_position_unit(attr);
    } else {
        WARN_STREAM << "[DEBUG] write_attr: Unknown writable attribute: " << attr_name << std::endl;
    }
}

void AuxiliarySupportDevice::read_group_attribute_json(Tango::Attribute &attr) {
    std::ostringstream oss;
    oss << "{\"tokenAssistPos\":" << token_assist_pos_ << ","
        << "\"direPos\":" << dire_pos_ << ","
        << "\"targetForce\":" << target_force_ << ","
        << "\"assistState\":" << (assist_state_ ? "true" : "false") << "}";
    attr_group_attribute_json_read = Tango::string_dup(oss.str().c_str());
    attr.set_value(&attr_group_attribute_json_read);
}

void AuxiliarySupportDevice::read_token_assist_pos(Tango::Attribute &attr) {
    update_position();
    attr.set_value(&token_assist_pos_);
}

void AuxiliarySupportDevice::read_dire_pos(Tango::Attribute &attr) {
    attr.set_value(&dire_pos_);
}

void AuxiliarySupportDevice::read_target_force(Tango::Attribute &attr) {
    attr.set_value(&target_force_);
}

void AuxiliarySupportDevice::read_force_value(Tango::Attribute &attr) {
    // 打印力传感器配置信息
    INFO_STREAM << "========== [ForceSensor] read_force_value 调用 ==========" << std::endl;
    INFO_STREAM << "[ForceSensor] 配置信息:" << std::endl;
    INFO_STREAM << "  - 力传感器通道号 (force_sensor_channel_): " << force_sensor_channel_ << std::endl;
    INFO_STREAM << "  - 力传感器控制器 (force_sensor_controller_): " 
                << (force_sensor_controller_.empty() ? "(空，使用运动控制器 " + motion_controller_name_ + ")" : force_sensor_controller_) << std::endl;
    INFO_STREAM << "  - 转换系数 (force_sensor_scale_): " << force_sensor_scale_ << std::endl;
    INFO_STREAM << "  - 零点偏移 (force_sensor_offset_): " << force_sensor_offset_ << std::endl;
    INFO_STREAM << "  - 当前缓存的目标力值 (target_force_): " << target_force_ << std::endl;
    INFO_STREAM << "  - 仿真模式 (sim_mode_): " << (sim_mode_ ? "是" : "否") << std::endl;
    
    // 调用 readForce() 方法获取实际力传感器值
    // 注意：属性读取不应该抛出异常，需要捕获可能的异常
    // readForce() 会将读取的值存储到成员变量 target_force_ 中
    try {
        Tango::DevDouble force = readForce();  // 这会更新 target_force_
        INFO_STREAM << "[ForceSensor] 读取成功: 实际力值 = " << force << " N" << std::endl;
        INFO_STREAM << "[ForceSensor] 返回的力值 (target_force_): " << target_force_ << " N" << std::endl;
        INFO_STREAM << "========== [ForceSensor] read_force_value 完成 ==========" << std::endl;
        // 使用成员变量 target_force_ 而不是局部变量，确保指针在 Tango 读取时仍然有效
        attr.set_value(&target_force_);
    } catch (Tango::DevFailed &e) {
        // 如果读取失败，返回缓存的目标力值，不抛出异常
        WARN_STREAM << "[ForceSensor] 读取失败 (Tango::DevFailed): " << e.errors[0].desc << std::endl;
        if (e.errors.length() > 1) {
            for (size_t i = 0; i < e.errors.length(); ++i) {
                WARN_STREAM << "  - 错误[" << i << "]: " << e.errors[i].reason << " - " << e.errors[i].desc << std::endl;
            }
        }
        WARN_STREAM << "[ForceSensor] 返回缓存的目标力值: " << target_force_ << " N" << std::endl;
        WARN_STREAM << "========== [ForceSensor] read_force_value 异常处理完成 ==========" << std::endl;
        attr.set_value(&target_force_);
    } catch (...) {
        // 捕获其他异常，返回缓存值
        WARN_STREAM << "[ForceSensor] 读取失败 (未知异常)" << std::endl;
        WARN_STREAM << "[ForceSensor] 返回缓存的目标力值: " << target_force_ << " N" << std::endl;
        WARN_STREAM << "========== [ForceSensor] read_force_value 异常处理完成 ==========" << std::endl;
        attr.set_value(&target_force_);
    }
}

void AuxiliarySupportDevice::read_support_logs(Tango::Attribute &attr) {
    attr_support_logs_read = Tango::string_dup(support_logs_.c_str());
    attr.set_value(&attr_support_logs_read);
}

void AuxiliarySupportDevice::read_fault_state(Tango::Attribute &attr) {
    attr_fault_state_read = Tango::string_dup(fault_state_.c_str());
    attr.set_value(&attr_fault_state_read);
}

void AuxiliarySupportDevice::read_axis_parameter(Tango::Attribute &attr) {
    std::ostringstream oss;
    oss << "{\"moveRange\":" << move_range_ << ","
        << "\"limitNumber\":" << limit_number_ << ","
        << "\"forceRange\":" << force_range_ << ","
        << "\"holdPos\":" << hold_pos_ << "}";
    axis_parameter_ = oss.str();
    attr_axis_parameter_read = Tango::string_dup(axis_parameter_.c_str());
    attr.set_value(&attr_axis_parameter_read);
}

void AuxiliarySupportDevice::read_assist_lim_org_state(Tango::Attribute &attr) {
    attr.set_value(&assist_lim_org_state_);
}

void AuxiliarySupportDevice::read_assist_state(Tango::Attribute &attr) {
    attr.set_value(&assist_state_);
}

void AuxiliarySupportDevice::read_result_value(Tango::Attribute &attr) {
    attr.set_value(&result_value_);
}

void AuxiliarySupportDevice::read_driver_power_status(Tango::Attribute &attr) {
    // 直接使用成员变量 driver_power_enabled_ 的地址，确保指针在 Tango 读取时仍然有效
    // Tango::DevBoolean 是 bool 的 typedef，可以直接传递 bool* 或 Tango::DevBoolean*
    attr.set_value(&driver_power_enabled_);
}

// ===== 固有状态属性读取函数 =====
void AuxiliarySupportDevice::read_bundle_no(Tango::Attribute &attr) {
    attr_bundle_no_read = Tango::string_dup(bundle_no_.c_str());
    attr.set_value(&attr_bundle_no_read);
}

void AuxiliarySupportDevice::read_laser_no(Tango::Attribute &attr) {
    attr_laser_no_read = Tango::string_dup(laser_no_.c_str());
    attr.set_value(&attr_laser_no_read);
}

void AuxiliarySupportDevice::read_system_no(Tango::Attribute &attr) {
    attr_system_no_read = Tango::string_dup(system_no_.c_str());
    attr.set_value(&attr_system_no_read);
}

void AuxiliarySupportDevice::read_sub_dev_list(Tango::Attribute &attr) {
    attr_sub_dev_list_read = Tango::string_dup(sub_dev_list_.c_str());
    attr.set_value(&attr_sub_dev_list_read);
}

void AuxiliarySupportDevice::read_model_list(Tango::Attribute &attr) {
    // Convert vector to JSON array string
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < model_list_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << model_list_[i] << "\"";
    }
    oss << "]";
    attr_model_list_read = Tango::string_dup(oss.str().c_str());
    attr.set_value(&attr_model_list_read);
}

void AuxiliarySupportDevice::read_current_model(Tango::Attribute &attr) {
    attr_current_model_read = Tango::string_dup(current_model_.c_str());
    attr.set_value(&attr_current_model_read);
}

void AuxiliarySupportDevice::read_connect_string(Tango::Attribute &attr) {
    attr_connect_string_read = Tango::string_dup(connect_string_.c_str());
    attr.set_value(&attr_connect_string_read);
}

void AuxiliarySupportDevice::read_error_dict(Tango::Attribute &attr) {
    attr_error_dict_read = Tango::string_dup(error_dict_.c_str());
    attr.set_value(&attr_error_dict_read);
}

void AuxiliarySupportDevice::read_device_name_attr(Tango::Attribute &attr) {
    attr_device_name_read = Tango::string_dup(device_name_.c_str());
    attr.set_value(&attr_device_name_read);
}

void AuxiliarySupportDevice::read_device_id(Tango::Attribute &attr) {
    attr_device_id_read = Tango::string_dup(device_id_.c_str());
    attr.set_value(&attr_device_id_read);
}

void AuxiliarySupportDevice::read_device_position(Tango::Attribute &attr) {
    attr_device_position_read = Tango::string_dup(device_position_.c_str());
    attr.set_value(&attr_device_position_read);
}

void AuxiliarySupportDevice::read_device_product_date(Tango::Attribute &attr) {
    attr_device_product_date_read = Tango::string_dup(device_product_date_.c_str());
    attr.set_value(&attr_device_product_date_read);
}

void AuxiliarySupportDevice::read_device_install_date(Tango::Attribute &attr) {
    attr_device_install_date_read = Tango::string_dup(device_install_date_.c_str());
    attr.set_value(&attr_device_install_date_read);
}

// ===== Support Type Attributes (新增) =====
void AuxiliarySupportDevice::read_support_type(Tango::Attribute &attr) {
    attr_support_type_read = Tango::string_dup(support_type_.empty() ? "" : support_type_.c_str());
    attr.set_value(&attr_support_type_read);
}

void AuxiliarySupportDevice::read_support_position(Tango::Attribute &attr) {
    attr_support_position_read = Tango::string_dup(support_position_.empty() ? "" : support_position_.c_str());
    attr.set_value(&attr_support_position_read);
}

void AuxiliarySupportDevice::read_support_orientation(Tango::Attribute &attr) {
    attr_support_orientation_read = Tango::string_dup(support_orientation_.empty() ? "vertical" : support_orientation_.c_str());
    attr.set_value(&attr_support_orientation_read);
}

void AuxiliarySupportDevice::read_force_sensor_channel(Tango::Attribute &attr) {
    attr.set_value(&force_sensor_channel_);
}

// ===== 日志导出命令 (状态机22) =====
void AuxiliarySupportDevice::exportLogs() {
    check_state_for_operational_commands();  // 状态机检查: OFF, ON, FAULT
    log_event("Logs exported");
    result_value_ = 0;
}

// ===== HOOKS =====
void AuxiliarySupportDevice::specific_self_check() {
    if (!sim_mode_) {
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            throw std::runtime_error("Motion Controller Proxy not initialized");
        }
        motion->ping();
    }
}

void AuxiliarySupportDevice::always_executed_hook() {
    static int hook_call_count = 0;
    hook_call_count++;
    
    // 前5次调用详细打印，之后每100次调用打印一次状态
    if (hook_call_count <= 5 || hook_call_count % 100 == 1) {
        INFO_STREAM << "[DEBUG] always_executed_hook() #" << hook_call_count 
                    << ", state: " << Tango::DevStateName[get_state()] 
                    << ", sim_mode=" << (sim_mode_ ? "true" : "false")
                    << ", assist_state=" << (assist_state_ ? "true" : "false") << std::endl;
    }
    
    Common::StandardSystemDevice::always_executed_hook();

    // LargeStroke-style: do not ping/reconnect in hook; only read connection_healthy_ (zero-wait).
    // 模拟模式：连接始终健康
    if (sim_mode_) {
        connection_healthy_.store(true);
        return;
    }
    
    if (!sim_mode_) {
        // If monitor rebuilt motion proxy, run restore actions in main thread.
        if (motion_restore_pending_.load()) {
            // Only attempt restore when connection is healthy; otherwise keep pending.
            if (connection_healthy_.load()) {
                if (motion_restore_pending_.exchange(false)) {
                    try {
                        perform_post_motion_reconnect_restore();
                        restore_retry_count_.store(0);  // 成功，重置计数
                        // 恢复成功后，设置连接为健康状态
                        connection_healthy_.store(true);
                    } catch (...) {
                        // 恢复失败，增加重试计数
                        int retries = restore_retry_count_.fetch_add(1);
                        if (retries < MAX_RESTORE_RETRIES) {
                            motion_restore_pending_.store(true);  // 重试
                            WARN_STREAM << "[Reconnect] Restore failed, will retry (" 
                                       << (retries + 1) << "/" << MAX_RESTORE_RETRIES << ")" << std::endl;
                        } else {
                            // 超过最大重试次数，记录错误但不再重试
                            ERROR_STREAM << "[Reconnect] Restore failed after " << MAX_RESTORE_RETRIES 
                                        << " attempts, giving up" << std::endl;
                            restore_retry_count_.store(0);  // 重置计数，等待下次重连
                        }
                    }
                }
            }
        }

        // 重要：避免在请求路径做网络 ping/重连。
        // 这里保持"零等待"，只根据后台线程更新的 connection_healthy_ 更新设备状态文本。
        if (!connection_healthy_.load() && get_state() == Tango::ON) {
            ERROR_STREAM << "[FAULT] Network connection lost (connection_healthy_=false), setting FAULT state" << std::endl;
            set_state(Tango::FAULT);
            set_status("Network connection lost");
            log_event("Network connection lost (detected in always_executed_hook)");
        }
    }
    
    // 真实模式下同步运动控制器的运动状态
    // 当本设备处于MOVING状态时，检查运动控制器是否仍在运动
    if (!sim_mode_ && assist_state_) {
        auto motion = get_motion_controller_proxy();
        if (motion) {
            try {
                // Detect limit during motion and latch FAULT
                if (!axis_ids_.empty() && !limit_fault_latched_.load()) {
                    try {
                        Tango::DeviceData data_in;
                        data_in << static_cast<Tango::DevShort>(axis_ids_[0]);
                        Tango::DeviceData data_out = motion->command_inout("readEL", data_in);
                        Tango::DevShort el_state_raw;
                        data_out >> el_state_raw;
                        
                        // 限位开关低电平有效：硬件读取0（低电平）表示触发，1（高电平）表示未触发
                        // 运动控制器当前逻辑：pos_limit==1返回1(EL+), neg_limit==1返回-1(EL-), 否则返回0
                        // 如果限位开关是低电平有效，需要反转：当硬件读取0时应该返回限位触发
                        // 由于运动控制器返回0时无法区分方向，我们在auxiliary_support_device中反转逻辑
                        // 反转规则：当readEL返回0时，认为是限位触发（统一处理为EL+）
                        //          当readEL返回非0时，认为是未触发（转换为0）
                        Tango::DevShort el_state = 0;
                        if (el_state_raw == 0) {
                            // 低电平有效：返回0表示限位触发（但无法区分方向，统一处理为EL+）
                            el_state = 1;  // 转换为EL+表示限位触发
                        }
                        // 如果返回非0（1或-1），说明硬件读取是高电平，限位未触发，el_state保持为0
                        
                        assist_lim_org_state_ = el_state;
                        if (el_state != 0) {
                            limit_fault_latched_.store(true);
                            limit_fault_el_state_.store(el_state);
                            std::string dir = (el_state > 0) ? "EL+" : "EL-";
                            fault_state_ = "Limit switch triggered: " + dir;
                            ERROR_STREAM << "[FAULT] Limit switch detected during motion: " << dir 
                                       << " (el_state_raw=" << el_state_raw << ", inverted to " << el_state << "), setting FAULT state" << std::endl;
                            set_status(fault_state_);
                            set_state(Tango::FAULT);
                            assist_state_ = false;
                            log_event("Limit switch triggered: " + dir + " (detected in always_executed_hook, low-level active)");
                            try {
                                for (long axis : axis_ids_) {
                                    Tango::DeviceData stop_in;
                                    stop_in << static_cast<Tango::DevShort>(axis);
                                    motion->command_inout("stopMove", stop_in);
                                }
                            } catch (...) {
                            }
                            return;
                        }
                    } catch (...) {
                    }
                }
            } catch (Tango::DevFailed &e) {
                // 通信错误时不改变状态，只记录警告
                WARN_STREAM << "always_executed_hook: Failed to check motion controller state: " 
                           << e.errors[0].desc << std::endl;
            } catch (...) {
                // 忽略其他异常
            }

            if (limit_fault_latched_.load()) {
                // 限位故障已锁定，保持FAULT状态
                set_state(Tango::FAULT);
                if (!fault_state_.empty()) {
                    set_status(fault_state_);
                }
                assist_state_ = false;
                return;
            }

            try {
                Tango::DevState mc_state = motion->state();
                DEBUG_STREAM << "[DEBUG] always_executed_hook: motion controller state = " 
                            << Tango::DevStateName[mc_state] << std::endl;
                if (mc_state != Tango::MOVING) {
                    // 运动控制器已停止运动，同步本设备状态
                    INFO_STREAM << "[DEBUG] always_executed_hook: Motion controller stopped, syncing state" << std::endl;
                    assist_state_ = false;
                    if (get_state() == Tango::MOVING) {
                        if (!limit_fault_latched_.load()) {
                            set_state(Tango::ON);
                        } else {
                            set_state(Tango::FAULT);
                        }
                        log_event("Motion completed (state sync from motion controller)");
                    }
                    // 更新位置
                    update_position();
                }
            } catch (Tango::DevFailed &e) {
                // 通信错误时不改变状态，只记录警告
                WARN_STREAM << "always_executed_hook: Failed to check motion controller state: " 
                           << e.errors[0].desc << std::endl;
            } catch (...) {
                // 忽略其他异常
            }
        }
    }
}

void AuxiliarySupportDevice::read_attr_hardware(std::vector<long> &attr_list) {
    // DEBUG: 确认 read_attr_hardware 被调用
    INFO_STREAM << "[DEBUG] read_attr_hardware called, attr_list.size=" << attr_list.size() << std::endl;
    
    // Update from hardware
    // 更新力传感器值（从硬件读取实际值）
    if (!sim_mode_) {
        try {
            // 调用 readForce() 更新 target_force_ 为实际读取的力传感器值
            readForce();
        } catch (...) {
            // 读取失败时保持当前值，不抛出异常
        }
    }
}

void AuxiliarySupportDevice::update_position() {
    if (sim_mode_) {
        // 模拟模式下位置已设置
        return;
    }
    
    // 真实模式：通过 encoder_device 的 readEncoder(channel) 命令读取位置
    auto encoder = get_encoder_proxy();
    if (!encoder) {
        return;  // 无代理，保持当前值
    }
    
    try {
        Tango::DeviceData data_in;
        data_in << encoder_channel_;
        Tango::DeviceData data_out = encoder->command_inout("readEncoder", data_in);
        data_out >> token_assist_pos_;
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "update_position failed: " << e.errors[0].desc << std::endl;
    }
}

void AuxiliarySupportDevice::update_device_name_and_id() {
    // 如果supportType或supportPosition为空，保持默认值
    if (support_type_.empty() || support_position_.empty()) {
        return;
    }
    
    // 根据supportType和supportPosition设置deviceName和deviceID
    if (support_type_ == "ray") {
        if (support_position_ == "upper") {
            device_name_ = "射线表征上辅助支撑";
            device_id_ = "RTS01";
        } else if (support_position_ == "lower") {
            device_name_ = "射线表征下辅助支撑";
            device_id_ = "RBS01";
        }
    } else if (support_type_ == "reflection") {
        if (support_position_ == "upper") {
            device_name_ = "反射光成像表征上辅助支撑";
            device_id_ = "RFS01";
        } else if (support_position_ == "lower") {
            device_name_ = "反射光成像表征下辅助支撑";
            device_id_ = "RFS02";
        }
    } else if (support_type_ == "targeting") {
        device_name_ = "打靶辅助支撑";
        device_id_ = "TGS01";
        support_position_ = "single";  // 确保position为single
    }
}

void AuxiliarySupportDevice::log_event(const std::string &event) {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // 在日志中包含支撑类型和位置信息
    std::string log_prefix = "";
    if (!support_type_.empty() && !support_position_.empty()) {
        log_prefix = "[" + support_type_ + "/" + support_position_ + "] ";
    }
    
    INFO_STREAM << "[" << buf << "] " << log_prefix << event << std::endl;
}

// ===== 状态机检查辅助函数 =====
void AuxiliarySupportDevice::check_state_for_lock_commands() {
    // devLock, devUnlock: 允许 UNKNOWN, OFF, FAULT
    Tango::DevState state = get_state();
    if (state == Tango::ON || state == Tango::MOVING) {
        Tango::Except::throw_exception("STATE_NOT_ALLOWED",
            "Command not allowed in ON/MOVING state",
            "AuxiliarySupportDevice::check_state_for_lock_commands");
    }
}

void AuxiliarySupportDevice::check_state_for_all_states() {
    // devLockVerify, devLockQuery, devUserConfig: 所有状态都允许
    // 不做任何检查
}

void AuxiliarySupportDevice::check_state_for_init_commands() {
    // selfCheck, init: 允许 UNKNOWN, OFF, FAULT
    Tango::DevState state = get_state();
    if (state == Tango::ON || state == Tango::MOVING) {
        Tango::Except::throw_exception("STATE_NOT_ALLOWED",
            "Command not allowed in ON/MOVING state",
            "AuxiliarySupportDevice::check_state_for_init_commands");
    }
}

void AuxiliarySupportDevice::check_state_for_reset() {
    // reset: 仅允许 ON, FAULT
    Tango::DevState state = get_state();
    if (state != Tango::ON && state != Tango::FAULT && state != Tango::MOVING) {
        Tango::Except::throw_exception("STATE_NOT_ALLOWED",
            "Command only allowed in ON or FAULT state",
            "AuxiliarySupportDevice::check_state_for_reset");
    }
}

void AuxiliarySupportDevice::check_state_for_param_commands() {
    // moveAxisSet, structAxisSet: 允许 OFF, ON, FAULT
    Tango::DevState state = get_state();
    if (state == Tango::UNKNOWN || state == Tango::MOVING) {
        Tango::Except::throw_exception("STATE_NOT_ALLOWED",
            "Command not allowed in UNKNOWN/MOVING state",
            "AuxiliarySupportDevice::check_state_for_param_commands");
    }
}

void AuxiliarySupportDevice::check_state_for_motion_commands() {
    // moveRelative, moveAbsolute, assistAuto, setHoldPos: 仅允许 ON
    Tango::DevState state = get_state();
    INFO_STREAM << "[DEBUG] check_state_for_motion_commands(): current state = " 
               << Tango::DevStateName[state] << std::endl;
    if (state != Tango::ON) {
        ERROR_STREAM << "[DEBUG] Motion command REJECTED! State is " << Tango::DevStateName[state] 
                    << ", expected ON" << std::endl;
        Tango::Except::throw_exception("STATE_NOT_ALLOWED",
            "Motion command only allowed in ON state (current: " + std::string(Tango::DevStateName[state]) + ")",
            "AuxiliarySupportDevice::check_state_for_motion_commands");
    }
}

void AuxiliarySupportDevice::check_state_for_operational_commands() {
    // Stop, readEncoder, readForce, readOrg, readEL, setForceZero, exportLogs, simSwitch 等：
    // 允许 OFF, ON, MOVING, FAULT 状态（不允许 UNKNOWN 状态）
    Tango::DevState state = get_state();
    if (state == Tango::UNKNOWN) {
        Tango::Except::throw_exception("STATE_NOT_ALLOWED",
            "Command not allowed in UNKNOWN state",
            "AuxiliarySupportDevice::check_state_for_operational_commands");
    }
}

// ===== CUSTOM ATTRIBUTE CLASSES (for read_attr() dispatch) =====
class AuxiliarySupportAttr : public Tango::Attr {
public:
    AuxiliarySupportAttr(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<AuxiliarySupportDevice *>(dev)->read_attr(att);
    }
};

class AuxiliarySupportAttrRW : public Tango::Attr {
public:
    AuxiliarySupportAttrRW(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ_WRITE)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<AuxiliarySupportDevice *>(dev)->read_attr(att);
    }
    
    virtual void write(Tango::DeviceImpl *dev, Tango::WAttribute &att) override {
        static_cast<AuxiliarySupportDevice *>(dev)->write_attr(att);
    }
};

// ===== CLASS FACTORY =====
AuxiliarySupportDeviceClass *AuxiliarySupportDeviceClass::_instance = nullptr;

AuxiliarySupportDeviceClass *AuxiliarySupportDeviceClass::instance() {
    if (_instance == nullptr) {
        std::string class_name = "AuxiliarySupportDevice";
        _instance = new AuxiliarySupportDeviceClass(class_name);
    }
    return _instance;
}

AuxiliarySupportDeviceClass::AuxiliarySupportDeviceClass(std::string &class_name) : Tango::DeviceClass(class_name) {}

void AuxiliarySupportDeviceClass::attribute_factory(std::vector<Tango::Attr *> &att_list) {
    // Standard attributes - 使用自定义属性类以确保 read_attr() 被调用
    att_list.push_back(new AuxiliarySupportAttr("selfCheckResult", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttrRW("positionUnit", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new AuxiliarySupportAttr("groupAttributeJson", Tango::DEV_STRING, Tango::READ));
    
    // Auxiliary Support specific attributes (辅助支撑类)
    att_list.push_back(new AuxiliarySupportAttr("tokenAssistPos", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("direPos", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("targetForce", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("forceValue", Tango::DEV_DOUBLE, Tango::READ));  // 实际力传感器值
    att_list.push_back(new AuxiliarySupportAttr("supportLogs", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("faultState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("axisParameter", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("AssistLimOrgState", Tango::DEV_SHORT, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("AssistState", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("resultValue", Tango::DEV_SHORT, Tango::READ));
    
    // Power control status attribute (NEW)
    att_list.push_back(new AuxiliarySupportAttr("driverPowerStatus", Tango::DEV_BOOLEAN, Tango::READ));
    
    // ===== 固有状态属性 (Property) =====
    att_list.push_back(new AuxiliarySupportAttr("bundleNo", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("laserNo", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("systemNo", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("subDevList", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("modelList", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("currentModel", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("connectString", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("errorDict", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("deviceName", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("deviceID", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("devicePosition", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("deviceProductDate", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("deviceInstallDate", Tango::DEV_STRING, Tango::READ));
    
    // 新增支撑类型属性
    att_list.push_back(new AuxiliarySupportAttr("supportType", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("supportPosition", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("supportOrientation", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new AuxiliarySupportAttr("forceSensorChannel", Tango::DEV_SHORT, Tango::READ));
}

void AuxiliarySupportDeviceClass::command_factory() {
    // Lock/Unlock commands (1-5)
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("devLock",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&AuxiliarySupportDevice::devLock)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("devUnlock",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&AuxiliarySupportDevice::devUnlock)));
    command_list.push_back(new Tango::TemplCommand("devLockVerify",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::devLockVerify)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("devLockQuery",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::devLockQuery)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("devUserConfig",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&AuxiliarySupportDevice::devUserConfig)));
    
    // System commands (6-8)
    command_list.push_back(new Tango::TemplCommand("selfCheck",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::selfCheck)));
    command_list.push_back(new Tango::TemplCommand("init",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::init)));
    command_list.push_back(new Tango::TemplCommand("reset",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::reset)));
    
    // Parameter commands (9-10)
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("moveAxisSet",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&AuxiliarySupportDevice::moveAxisSet)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("structAxisSet",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&AuxiliarySupportDevice::structAxisSet)));
    
    // Motion commands (11-13)
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("moveRelative",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&AuxiliarySupportDevice::moveRelative)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("moveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&AuxiliarySupportDevice::moveAbsolute)));
    // 接口文档要求命令名为 "Stop" (大写S)
    command_list.push_back(new Tango::TemplCommand("Stop",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::stop)));
    
    // Read commands (14, 17, 20-21)
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevDouble>("readEncoder",
        static_cast<Tango::DevDouble (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::readEncoder)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevDouble>("readForce",
        static_cast<Tango::DevDouble (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::readForce)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevBoolean>("readOrg",
        static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::readOrg)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevShort>("readEL",
        static_cast<Tango::DevShort (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::readEL)));
    
    // Auto/Force commands (15-16, 18)
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("assistAuto",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&AuxiliarySupportDevice::assistAuto)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("setHoldPos",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&AuxiliarySupportDevice::setHoldPos)));
    command_list.push_back(new Tango::TemplCommand("setForceZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::setForceZero)));
    
    // Export commands (22-24)
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("readtAxis",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::readtAxis)));
    command_list.push_back(new Tango::TemplCommand("exportAxis",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::exportAxis)));
    // 日志导出命令 (状态机24)
    command_list.push_back(new Tango::TemplCommand("exportLogs",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::exportLogs)));
    // 模拟运行开关 (状态机25)
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("simSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&AuxiliarySupportDevice::simSwitch)));
    
    // Power Control Commands (NEW - for GUI)
    command_list.push_back(new Tango::TemplCommand("enableDriverPower",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::enableDriverPower)));
    command_list.push_back(new Tango::TemplCommand("disableDriverPower",
        static_cast<void (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::disableDriverPower)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("queryPowerStatus",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&AuxiliarySupportDevice::queryPowerStatus)));
}

// ========== Power Control Commands (for GUI) ==========
void AuxiliarySupportDevice::enableDriverPower() {
    if (!enable_driver_power()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to enable driver power", "AuxiliarySupportDevice::enableDriverPower");
    }
}

void AuxiliarySupportDevice::disableDriverPower() {
    if (!disable_driver_power()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to disable driver power", "AuxiliarySupportDevice::disableDriverPower");
    }
}

Tango::DevString AuxiliarySupportDevice::queryPowerStatus() {
    std::stringstream ss;
    ss << "{"
       << "\"driverPowerEnabled\":" << (driver_power_enabled_ ? "true" : "false") << ","
       << "\"driverPowerPort\":" << driver_power_port_ << ","
       << "\"driverPowerController\":\"" << driver_power_controller_ << "\""
       << "}";
    return CORBA::string_dup(ss.str().c_str());
}

// ========== Power Control Methods ==========
bool AuxiliarySupportDevice::enable_driver_power() {
    INFO_STREAM << "[PowerControl] enable_driver_power() called" << std::endl;
    INFO_STREAM << "[PowerControl] driver_power_port_=" << driver_power_port_ 
               << ", driver_power_controller_=" << driver_power_controller_ << std::endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Driver power enabled (simulated)" << std::endl;
        driver_power_enabled_ = true;
        return true;
    }
    
    if (driver_power_port_ < 0) {
        INFO_STREAM << "[PowerControl] Driver power control not configured (port=" 
                   << driver_power_port_ << "), skipping" << std::endl;
        return true;
    }
    
    try {
        Tango::DeviceProxy* power_ctrl = nullptr;
        bool created_new = false;
        
        // 如果配置了专用控制器且与当前运动控制器不同，则创建新的 proxy
        if (!driver_power_controller_.empty() && 
            driver_power_controller_ != motion_controller_name_) {
            power_ctrl = new Tango::DeviceProxy(driver_power_controller_);
            created_new = true;
            INFO_STREAM << "[PowerControl] Using dedicated controller: " << driver_power_controller_ << std::endl;
        } else {
            auto motion = get_motion_controller_proxy();
            if (motion) {
                power_ctrl = motion.get();
                INFO_STREAM << "[PowerControl] Using connected motion controller" << std::endl;
            } else {
                ERROR_STREAM << "[PowerControl] Motion controller proxy not available!" << std::endl;
                return false;
            }
        }
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(driver_power_port_);
        params[1] = 1.0;  // HIGH = 上电
        Tango::DeviceData data;
        data << params;
        
        INFO_STREAM << "[PowerControl] Calling writeIO with port=" << driver_power_port_ << ", value=1" << std::endl;
        INFO_STREAM << "[PowerControl] Executing hardware writeIO command..." << std::endl;
        power_ctrl->command_inout("writeIO", data);
        INFO_STREAM << "[PowerControl] writeIO command executed successfully" << std::endl;
        
        if (created_new) delete power_ctrl;
        
        driver_power_enabled_ = true;
        INFO_STREAM << "✓ Driver power enabled on port OUT" << driver_power_port_ << std::endl;
        log_event("Driver power enabled on port OUT" + std::to_string(driver_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to enable driver power: " << e.errors[0].desc << std::endl;
        log_event("Failed to enable driver power: " + std::string(e.errors[0].desc.in()));
        return false;
    }
}

bool AuxiliarySupportDevice::disable_driver_power() {
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Driver power disabled (simulated)" << std::endl;
        driver_power_enabled_ = false;
        return true;
    }
    
    if (driver_power_port_ < 0) {
        return true;
    }
    
    try {
        Tango::DeviceProxy* power_ctrl = nullptr;
        bool created_new = false;
        
        if (!driver_power_controller_.empty() && 
            driver_power_controller_ != motion_controller_name_) {
            power_ctrl = new Tango::DeviceProxy(driver_power_controller_);
            created_new = true;
        } else {
            auto motion = get_motion_controller_proxy();
            if (motion) {
                power_ctrl = motion.get();
            } else {
                return true;
            }
        }
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(driver_power_port_);
        params[1] = 0.0;  // LOW = 断电
        Tango::DeviceData data;
        data << params;
        power_ctrl->command_inout("writeIO", data);
        
        if (created_new) delete power_ctrl;
        driver_power_enabled_ = false;
        INFO_STREAM << "✓ Driver power disabled on port OUT" << driver_power_port_ << std::endl;
        log_event("Driver power disabled on port OUT" + std::to_string(driver_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to disable driver power: " << e.errors[0].desc << std::endl;
        return false;
    }
}

void AuxiliarySupportDeviceClass::device_factory(const Tango::DevVarStringArray *devlist_ptr) {
    std::cout << "[DEBUG] device_factory() called, creating " << devlist_ptr->length() << " device(s)" << std::endl;
    for (unsigned long i = 0; i < devlist_ptr->length(); i++) {
        std::string dev_name = (*devlist_ptr)[i].in();
        std::cout << "[DEBUG] Creating device: " << dev_name << std::endl;
        AuxiliarySupportDevice *dev = new AuxiliarySupportDevice(this, dev_name);
        std::cout << "[DEBUG] Device created, state: " << Tango::DevStateName[dev->get_state()] << std::endl;
        device_list.push_back(dev);
        export_device(dev);
        std::cout << "[DEBUG] Device exported: " << dev_name << ", final state: " << Tango::DevStateName[dev->get_state()] << std::endl;
    }
    std::cout << "[DEBUG] device_factory() completed" << std::endl;
}

} // namespace AuxiliarySupport

// Main function
void Tango::DServer::class_factory() {
    add_class(AuxiliarySupport::AuxiliarySupportDeviceClass::instance());
}

int main(int argc, char *argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "[DEBUG] AuxiliarySupport Server Starting..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        std::cout << "[DEBUG] Loading system config..." << std::endl;
        Common::SystemConfig::loadConfig();
        std::cout << "[DEBUG] SystemConfig.SIM_MODE = " << (Common::SystemConfig::SIM_MODE ? "true" : "false") << std::endl;
        
        std::cout << "[DEBUG] Initializing Tango Util..." << std::endl;
        Tango::Util *tg = Tango::Util::init(argc, argv);
        
        std::cout << "[DEBUG] Calling server_init()..." << std::endl;
        tg->server_init();
        std::cout << "========================================" << std::endl;
        std::cout << "AuxiliarySupport (辅助支撑) Server Ready" << std::endl;
        std::cout << "========================================" << std::endl;
        
        tg->server_run();
    } catch (std::bad_alloc &) {
        std::cout << "[ERROR] Can't allocate memory!!!" << std::endl;
        return -1;
    } catch (CORBA::Exception &e) {
        std::cout << "[ERROR] CORBA Exception caught:" << std::endl;
        Tango::Except::print_exception(e);
        return -1;
    } catch (std::exception &e) {
        std::cout << "[ERROR] std::exception: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cout << "[ERROR] Unknown exception caught!" << std::endl;
        return -1;
    }
    return 0;
}
