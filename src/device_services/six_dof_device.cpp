#include "device_services/six_dof_device.h"
#include "common/system_config.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <server/log4tango.h>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <nlohmann/json.hpp>

using namespace std;

namespace SixDof {

const double SixDofDevice::POS_LIMIT = 17.0;
const double SixDofDevice::ROT_LIMIT = 4.0;

namespace {
struct AllowedStates {
    bool allow_unknown;
    bool allow_off;
    bool allow_on;
    bool allow_fault;
};

const std::unordered_map<std::string, AllowedStates> kStateMatrix = {
    // 锁定 / 用户
    {"devLock",          {true,  true,  false, true}},
    {"devUnlock",        {true,  true,  false, true}},
    {"devLockVerify",    {true,  true,  true,  true}},
    {"devLockQuery",     {true,  true,  true,  true}},
    {"devUserConfig",    {true,  true,  true,  true}},

    // 系统
    {"selfCheck",        {true,  true,  false, true}},
    {"init",             {true,  true,  false, true}},

    // 参数
    {"moveAxisSet",      {false, true,  true,  true}},
    {"structAxisSet",    {false, true,  true,  true}},

    // 位姿运动
    {"movePoseRelative", {false, false, true,  false}},
    {"movePoseAbsolute", {false, false, true,  false}},

    // 复位 / 回零
    {"reset",            {false, true,  true,  true}},
    {"sixMoveZero",      {false, true,  true,  true}},
    {"singleReset",      {false, true,  true,  true}},

    // 单轴运动
    {"singleMoveRelative", {false, false, true,  false}},
    {"singleMoveAbsolute", {false, false, true,  false}},

    // 读取
    {"readEncoder",      {false, true,  true,  true}},
    {"readOrg",          {false, true,  true,  true}},
    {"readEL",           {false, true,  true,  true}},

    // 控制
    {"stop",             {false, true,  true,  true}},
    {"openBrake",        {false, false, true,  false}},

    // 导出
    {"readtAxis",        {false, true,  true,  true}},
    {"exportAxis",       {false, true,  true,  true}},
};
} // namespace

SixDofDevice::SixDofDevice(Tango::DeviceClass *device_class, std::string &device_name)
    : Common::StandardSystemDevice(device_class, device_name),
      is_locked_(false),
      limit_number_(2),
      open_brake_state_(false),
      result_value_(0),
      position_unit_("mm"),
      self_check_result_(-1),
      sim_mode_(true)
{
    
    // Initialize arrays
    axis_pos_.fill(0.0);
    dire_pos_.fill(0.0);
    six_freedom_pose_.fill(0.0);
    current_leg_lengths_.fill(0.0);
    lim_org_state_.fill(2);  // 2 = not at limit
    sdof_state_.fill(false); // false = idle
    
    // // Initialize Kinematics with default values
    // Common::PlatformGeometry default_geom;
    // default_geom.base_radius = 200.0;
    // default_geom.platform_radius = 100.0;
    // default_geom.initial_height = 300.0;
    // default_geom.min_leg_length = 250.0;
    // default_geom.max_leg_length = 400.0;
    // kinematics_ = std::unique_ptr<Common::StewartPlatformKinematics>(
    //     new Common::StewartPlatformKinematics(default_geom));
    
    // // 用 kinematics 的 normalleg 初始化 leg 长度
    // double nominal_leg = kinematics_->getNominalLegLength();
    // current_leg_lengths_.fill(nominal_leg);
    // axis_pos_ = current_leg_lengths_;  // axis_pos 也初始化为 normalleg
    
    init_device();
}

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

// 辅助函数：将double值四舍五入到指定小数位数
static double round_to_decimals(double value, int decimals) {
    double multiplier = std::pow(10.0, decimals);
    return std::round(value * multiplier) / multiplier;
}

SixDofDevice::~SixDofDevice() {
    delete_device();
}

void SixDofDevice::init_device() {
    Common::StandardSystemDevice::init_device();
    
    // Get properties from database
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
    db_data.push_back(Tango::DbDatum("moveRange"));
    db_data.push_back(Tango::DbDatum("limitNumber"));
    db_data.push_back(Tango::DbDatum("sdofConfig"));
    db_data.push_back(Tango::DbDatum("motionControllerName"));
    db_data.push_back(Tango::DbDatum("encoderName"));
    db_data.push_back(Tango::DbDatum("encoderChannels"));  // 每个轴对应的编码器通道号
    db_data.push_back(Tango::DbDatum("motorStepAngle"));
    db_data.push_back(Tango::DbDatum("motorGearRatio"));
    db_data.push_back(Tango::DbDatum("motorSubdivision"));
    db_data.push_back(Tango::DbDatum("driverPowerPort"));
    db_data.push_back(Tango::DbDatum("driverPowerController"));
    db_data.push_back(Tango::DbDatum("brakePowerPort"));
    db_data.push_back(Tango::DbDatum("brakePowerController"));
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
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_position_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_product_date_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_install_date_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> move_range_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> limit_number_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> sdof_config_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motion_controller_name_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_name_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_channels_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motor_step_angle_; } else { motor_step_angle_ = 1.8; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motor_gear_ratio_; } else { motor_gear_ratio_ = 1.0; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motor_subdivision_; } else { motor_subdivision_ = 12800.0; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> driver_power_port_; } else { driver_power_port_ = -1; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> driver_power_controller_; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> brake_power_port_; } else { brake_power_port_ = -1; }
    idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> brake_power_controller_; }
    idx++;
    
    // 初始化状态
    driver_power_enabled_ = false;
    brake_released_ = false;
    
    // 如果未配置 encoderChannels，使用默认值 0-5
    if (encoder_channels_.empty()) {
        encoder_channels_.resize(NUM_AXES);
        for (int i = 0; i < NUM_AXES; ++i) {
            encoder_channels_[i] = static_cast<short>(i);
        }
        INFO_STREAM << "Using default encoder channels 0-5" << endl;
    } else if (encoder_channels_.size() < NUM_AXES) {
        WARN_STREAM << "encoderChannels has " << encoder_channels_.size() 
                   << " elements, expected " << NUM_AXES << ", padding with defaults" << endl;
        for (size_t i = encoder_channels_.size(); i < NUM_AXES; ++i) {
            encoder_channels_.push_back(static_cast<short>(i));
        }
    }
    
    if (motion_controller_name_.empty()) motion_controller_name_ = "sys/motion/1";
    
    // 从系统配置读取模拟模式设置（启动时的初始默认值）
    // 注意：运行时可以通过 simSwitch 命令或GUI切换，但重启后会恢复配置文件的值（不持久化）
    sim_mode_ = Common::SystemConfig::SIM_MODE;
    if (sim_mode_) {
        INFO_STREAM << "========================================" << endl;
        INFO_STREAM << "  模拟模式已启用 (从配置文件加载, SIM_MODE=true)" << endl;
        INFO_STREAM << "  不连接真实硬件，使用内部模拟逻辑" << endl;
        INFO_STREAM << "========================================" << endl;
    } else {
        INFO_STREAM << "[DEBUG] 运行模式: 真实模式 (从配置文件加载, SIM_MODE=false)" << endl;
        INFO_STREAM << "[DEBUG] 提示: 可通过 simSwitch 命令或GUI动态切换模拟模式" << endl;
    }

    INFO_STREAM << "SixDofDevice initialized. Motion Controller: " << motion_controller_name_ << endl;
    log_event("Device initialized");
    
    configure_kinematics();
    connect_proxies();

    // Start LargeStroke-style background connection monitor
    start_connection_monitor();
}

void SixDofDevice::delete_device() {
    stop_connection_monitor();
    // 设备关闭前自动启用刹车（安全保护）
    if (brake_power_port_ >= 0 && brake_released_ && !sim_mode_) {
        INFO_STREAM << "[BrakeControl] Auto-engaging brake before device shutdown (safety)" << endl;
        if (!engage_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to engage brake before shutdown" << endl;
        }
    }
    
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        motion_controller_proxy_.reset();
        encoder_proxy_.reset();
    }
    Common::StandardSystemDevice::delete_device();
}

// ===== Proxy helpers (lifetime-safe, LargeStroke-style) =====
std::shared_ptr<Tango::DeviceProxy> SixDofDevice::get_motion_controller_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return motion_controller_proxy_;
}

std::shared_ptr<Tango::DeviceProxy> SixDofDevice::get_encoder_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return encoder_proxy_;
}

void SixDofDevice::reset_motion_controller_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    motion_controller_proxy_.reset();
}

void SixDofDevice::reset_encoder_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    encoder_proxy_.reset();
}

void SixDofDevice::rebuild_motion_controller_proxy(int timeout_ms) {
    if (motion_controller_name_.empty()) {
        return;
    }
    auto new_motion = create_proxy_and_ping(motion_controller_name_, timeout_ms);
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        motion_controller_proxy_ = new_motion;
    }
}

void SixDofDevice::rebuild_encoder_proxy(int timeout_ms) {
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
void SixDofDevice::start_connection_monitor() {
    if (sim_mode_) {
        connection_healthy_.store(true);
        return;
    }
    if (connection_monitor_thread_.joinable()) {
        return;
    }
    stop_connection_monitor_.store(false);
    connection_monitor_thread_ = std::thread(&SixDofDevice::connection_monitor_loop, this);
}

void SixDofDevice::stop_connection_monitor() {
    stop_connection_monitor_.store(true);
    if (connection_monitor_thread_.joinable()) {
        connection_monitor_thread_.join();
    }
}

void SixDofDevice::connection_monitor_loop() {
    const int ping_timeout_ms = 300;
    const int connect_timeout_ms = 500;

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
        // std::cout << "[ConnectionMonitor] Previous healthy=" << was_healthy << std::endl;
        bool should_be_healthy = motion_ok && encoder_ok && !motion_restore_pending_.load();
        // std::cout << "[ConnectionMonitor] motion_ok=" << motion_ok 
        //           << ", encoder_ok=" << encoder_ok 
        //           << ", motion_restore_pending=" << motion_restore_pending_.load()
        //           << ", should_be_healthy=" << should_be_healthy << std::endl;
        if (should_be_healthy != was_healthy) {
            connection_healthy_.store(should_be_healthy);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void SixDofDevice::connect_proxies() {
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
        // 内层 catch 已经打印了详细的代理名称和错误信息，这里只设置状态
        set_state(Tango::FAULT);
        set_status("Proxy connection failed");
        connection_healthy_.store(false);
        reconnect_pending_.store(true);
    }
}

void SixDofDevice::perform_post_motion_reconnect_restore() {
    if (sim_mode_) {
        return;
    }
    
    auto motion = get_motion_controller_proxy();
    auto encoder = get_encoder_proxy();
    
    if (!motion) {
        return;  // motion proxy 未连接，无法恢复
    }
    
    // 1. 恢复电机细分参数（对每个轴单独处理，记录详细错误信息）
    // INFO_STREAM << "Applying motor subdivision parameters (stepAngle=" << motor_step_angle_ 
    //            << ", gearRatio=" << motor_gear_ratio_ 
    //            << ", subdivision=" << motor_subdivision_ << ")" << endl;
    // int success_count = 0;
    // int fail_count = 0;
    // for (int axis = 0; axis < NUM_AXES; ++axis) {
    //     try {
    //         Tango::DevVarDoubleArray params;
    //         params.length(4);
    //         params[0] = static_cast<double>(axis);
    //         params[1] = motor_step_angle_;
    //         params[2] = motor_gear_ratio_;
    //         params[3] = motor_subdivision_;
    //         Tango::DeviceData arg;
    //         arg << params;
    //         motion->command_inout("setStructParameter", arg);
    //         INFO_STREAM << "Successfully applied motor subdivision parameters to axis " << axis << endl;
    //         success_count++;
    //     } catch (Tango::DevFailed &e) {
    //         ERROR_STREAM << "Failed to apply motor subdivision parameters to axis " << axis 
    //                     << ": " << e.errors[0].desc << " (error code: " << e.errors[0].reason << ")" << endl;
    //         fail_count++;
    //         // 继续处理其他轴，不中断循环
    //     } catch (...) {
    //         ERROR_STREAM << "Failed to apply motor subdivision parameters to axis " << axis 
    //                     << ": unknown error" << endl;
    //         fail_count++;
    //     }
    // }
    // if (success_count == NUM_AXES) {
    //     INFO_STREAM << "Successfully applied motor subdivision parameters to all " << NUM_AXES << " axes" << endl;
    // } else {
    //     WARN_STREAM << "Motor subdivision parameters: " << success_count << " succeeded, " 
    //                << fail_count << " failed (axes with failures may not work properly)" << endl;
    // }
    
    // 2. 恢复驱动器上电和释放刹车
    INFO_STREAM << "Restoring relay configuration and enabling driver power after reconnection..." << endl;
    if (enable_driver_power()) {
        INFO_STREAM << "Driver power enabled successfully after reconnection" << endl;
        // 自动释放刹车
        if (release_brake()) {
            INFO_STREAM << "Brake released successfully after reconnection, device ready for operation" << endl;
        } else {
            WARN_STREAM << "Failed to release brake after reconnection, manual intervention may be required" << endl;
        }
    } else {
        WARN_STREAM << "Failed to enable driver power after reconnection" << endl;
    }
    
    // 3. 同步编码器位置
    if (encoder) {
        try {
            INFO_STREAM << "Synchronizing encoder positions to motion controller..." << endl;
            for (int axis = 0; axis < NUM_AXES; ++axis) {
                if (axis < static_cast<int>(encoder_channels_.size())) {
                    short encoder_channel = encoder_channels_[axis];
                    try {
                        // 读取编码器位置
                        Tango::DeviceData data_in;
                        data_in << encoder_channel;
                        Tango::DeviceData data_out = encoder->command_inout("readEncoder", data_in);
                        double encoder_pos;
                        data_out >> encoder_pos;
                        
                        // 更新运动控制器计数
                        Tango::DevVarDoubleArray params;
                        params.length(2);
                        params[0] = static_cast<double>(axis);
                        params[1] = encoder_pos;
                        Tango::DeviceData arg;
                        arg << params;
                        motion->command_inout("setEncoderPosition", arg);
                        
                        INFO_STREAM << "Axis " << axis << " (encoder channel " << encoder_channel 
                                   << "): synced position " << encoder_pos << " to motion controller" << endl;
                    } catch (Tango::DevFailed &e) {
                        WARN_STREAM << "Failed to sync encoder position for axis " << axis 
                                   << " (channel " << encoder_channels_[axis] << "): " << e.errors[0].desc << endl;
                    }
                }
            }
            
            INFO_STREAM << "Encoder positions synchronized to motion controller successfully" << endl;
            log_event("Encoder positions synchronized to motion controller");
        } catch (Tango::DevFailed &e) {
            WARN_STREAM << "Failed to synchronize encoder positions: " << e.errors[0].desc << endl;
        }
    }
}

void SixDofDevice::log_event(const std::string &event) {
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S");
    if (six_logs_.empty() || six_logs_ == "{}") {
        six_logs_ = "{\"" + oss.str() + "\":\"" + event + "\"}";
    } else {
        six_logs_.insert(six_logs_.length() - 1, ",\"" + oss.str() + "\":\"" + event + "\"");
    }
}

void SixDofDevice::check_state(const std::string& cmd_name) {
    auto it = kStateMatrix.find(cmd_name);
    if (it == kStateMatrix.end()) {
        Tango::Except::throw_exception("API_StateViolation",
            cmd_name + " state rule not defined", "SixDofDevice::check_state");
    }
    const auto allow = it->second;

    Tango::DevState s = get_state();
    switch (s) {
        case Tango::UNKNOWN:
            if (!allow.allow_unknown) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: device in UNKNOWN", "SixDofDevice::check_state");
            }
            break;
        case Tango::OFF:
            if (!allow.allow_off) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: device in OFF", "SixDofDevice::check_state");
            }
            break;
        case Tango::ON:
        case Tango::MOVING:
            if (!allow.allow_on) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: device not allowed in ON/MOVING", "SixDofDevice::check_state");
            }
            break;
        case Tango::FAULT:
            if (!allow.allow_fault) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: device in FAULT", "SixDofDevice::check_state");
            }
            break;
        default:
            if (!allow.allow_on) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: state not allowed", "SixDofDevice::check_state");
            }
            break;
    }
}

// ========== Lock/Unlock Commands ==========
void SixDofDevice::devLock() {
    check_state("devLock");
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (is_locked_) {
        Tango::Except::throw_exception("API_DeviceLocked", 
            "Device already locked by: " + lock_user_, "SixDofDevice::devLock");
    }
    lock_user_.clear();
    is_locked_ = true;
    log_event("Device locked");
}

void SixDofDevice::devUnlock(Tango::DevBoolean /*unlock_all*/) {
    check_state("devUnlock");
    std::lock_guard<std::mutex> lock(lock_mutex_);
    // LargeStroke-style: do not ping/reconnect in hook; only read connection_healthy_ (zero-wait).
    if (!sim_mode_) {
        if (!connection_healthy_.load() && get_state() == Tango::ON) {
            // 连接丢失时自动启用刹车（安全保护）
            if (brake_power_port_ >= 0 && brake_released_) {
                INFO_STREAM << "[BrakeControl] Connection lost, auto-engaging brake (safety)" << endl;
                if (!engage_brake()) {
                    WARN_STREAM << "[BrakeControl] Failed to engage brake on connection loss" << endl;
                }
            }
            set_state(Tango::FAULT);
            set_status("Network connection lost");
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_reconnect_attempt_).count();
        if (reconnect_pending_.load() && elapsed >= Common::SystemConfig::PROXY_RECONNECT_INTERVAL_SEC) {
            last_reconnect_attempt_ = now;
            INFO_STREAM << "[Reconnect] attempting to (re)connect proxies..." << endl;
            connect_proxies();
        }
    }
    
    is_locked_ = false;
    lock_user_.clear();
    result_value_ = 0;
}

void SixDofDevice::devLockVerify() {
    // ALL_STATES: 所有状态可用，无需检查
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (!is_locked_) {
        Tango::Except::throw_exception("API_NotLocked", 
            "Device is not locked", "SixDofDevice::devLockVerify");
    }
}

Tango::DevString SixDofDevice::devLockQuery() {
    // ALL_STATES: 所有状态可用，无需检查
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::ostringstream oss;
    oss << "{\"locked\":" << (is_locked_ ? "true" : "false");
    if (is_locked_) {
        oss << ",\"userName\":\"" << lock_user_ << "\"";
    }
    oss << "}";
    return Tango::string_dup(oss.str().c_str());
}

void SixDofDevice::devUserConfig() {
    // ALL_STATES: 所有状态可用，无需检查
    log_event("User config updated");
}

// ========== System Commands ==========
void SixDofDevice::selfCheck() {
    INFO_STREAM << "[DEBUG] selfCheck() called" << endl;
    check_state("selfCheck");  // UNKNOWN/OFF/FAULT可用，ON状态不可用
    log_event("Self check started");
    try {
        // 检查电机控制器
        auto motion = get_motion_controller_proxy();
        if (!sim_mode_ && motion) {
            INFO_STREAM << "[DEBUG] selfCheck: checking motion controller..." << endl;
            try {
                int original_timeout = motion->get_timeout_millis();
                motion->set_timeout_millis(500);
                motion->ping();
                motion->set_timeout_millis(original_timeout);
                INFO_STREAM << "[DEBUG] selfCheck: motion controller ping OK" << endl;
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] selfCheck: motion controller ping failed - " 
                            << e.errors[0].desc << endl;
                self_check_result_ = 1;  // 1=电机自检异常
                alarm_state_ = "Self check failed: Motor controller not responding";
                log_event("Self check failed: motor error");
                return;
            } catch (...) {
                ERROR_STREAM << "[DEBUG] selfCheck: motion controller ping failed with unknown exception" << endl;
                self_check_result_ = 1;
                alarm_state_ = "Self check failed: Motor controller not responding";
                log_event("Self check failed: motor error");
                return;
            }
        } else if (sim_mode_) {
            INFO_STREAM << "[DEBUG] selfCheck: SIM MODE - skipping motion controller check" << endl;
        } else {
            WARN_STREAM << "[DEBUG] selfCheck: motion_controller_proxy_ is NULL" << endl;
        }
        
        // 检查编码器连接
        auto enc = get_encoder_proxy();
        if (!sim_mode_ && enc) {
            INFO_STREAM << "[DEBUG] selfCheck: checking encoder..." << endl;
            try {
                int original_timeout = enc->get_timeout_millis();
                enc->set_timeout_millis(500);
                enc->ping();
                enc->set_timeout_millis(original_timeout);
                INFO_STREAM << "[DEBUG] selfCheck: encoder ping OK" << endl;
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] selfCheck: encoder ping failed - " << e.errors[0].desc << endl;
                self_check_result_ = 2;  // 2=编码器自检异常
                alarm_state_ = "Self check failed: Encoder not responding";
                log_event("Self check failed: encoder error");
                return;
            } catch (...) {
                ERROR_STREAM << "[DEBUG] selfCheck: encoder ping failed with unknown exception" << endl;
                self_check_result_ = 2;
                alarm_state_ = "Self check failed: Encoder not responding";
                log_event("Self check failed: encoder error");
                return;
            }
        } else if (sim_mode_) {
            INFO_STREAM << "[DEBUG] selfCheck: SIM MODE - skipping encoder check" << endl;
        } else {
            WARN_STREAM << "[DEBUG] selfCheck: encoder_proxy_ is NULL" << endl;
        }
        
        INFO_STREAM << "[DEBUG] selfCheck: calling specific_self_check()..." << endl;
        specific_self_check();
        self_check_result_ = 0;  // 0=自检正常
        alarm_state_.clear();
        INFO_STREAM << "[DEBUG] selfCheck: PASSED" << endl;
        log_event("Self check passed");
    } catch (std::exception &e) {
        ERROR_STREAM << "[DEBUG] selfCheck: FAILED with exception - " << e.what() << endl;
        self_check_result_ = 4;  // 4=其他异常
        alarm_state_ = std::string("Self check failed: ") + e.what();
        log_event("Self check failed: other error");
    }
}

void SixDofDevice::init() {
    INFO_STREAM << "[DEBUG] init() called, current_state=" << Tango::DevStateName[get_state()] 
                << ", sim_mode=" << (sim_mode_ ? "true" : "false") << endl;
    check_state("init");  // UNKNOWN/OFF/FAULT可用，ON状态不可用
    log_event("Initialization started");
    
    INFO_STREAM << "[DEBUG] init: connecting proxies (sim_mode=" << sim_mode_ << ")..." << endl;
    connect_proxies();
    
    INFO_STREAM << "[DEBUG] init: resetting positions and state..." << endl;
    for (int i = 0; i < NUM_AXES; ++i) {
        axis_pos_[i] = 0.0;
        dire_pos_[i] = 0.0;
        lim_org_state_[i] = 0;
    }
    six_freedom_pose_.fill(0.0);
    self_check_result_ = -1;
    result_value_ = 0;
    set_state(Tango::ON);
    
    // 确保状态字符串与 sim_mode_ 一致
    if (sim_mode_) {
        set_status("Simulation Mode");
        INFO_STREAM << "[DEBUG] init: Final status set to 'Simulation Mode' (sim_mode=true)" << endl;
    } else {
        std::string current_status = get_status();
        if (current_status != "Connected" && current_status != "Proxy connection failed") {
            set_status("Connected");
        }
        INFO_STREAM << "[DEBUG] init: Final status = '" << get_status() << "' (sim_mode=false)" << endl;
    }
    
    INFO_STREAM << "[DEBUG] init: initialization completed, state set to ON, sim_mode=" << sim_mode_ << endl;
    log_event("Initialization completed");
}

// ========== Parameter Commands ==========
void SixDofDevice::moveAxisSet(const Tango::DevVarDoubleArray *params) {
    check_state("moveAxisSet");  // OFF/ON/FAULT可用
    if (params->length() < 5) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Need 5 values: startSpeed, maxSpeed, accTime, decTime, stopSpeed", 
            "SixDofDevice::moveAxisSet");
    }
    // 六自由度设备有6个轴，但参数命令通常只设置一个轴的参数
    // 这里假设参数是 [axis, startSpeed, maxSpeed, accTime, decTime, stopSpeed]
    if (params->length() < 6) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Need 6 values: [axis, startSpeed, maxSpeed, accTime, decTime, stopSpeed]", 
            "SixDofDevice::moveAxisSet");
    }
    int axis = static_cast<int>((*params)[0]);
    if (axis < 0 || axis >= NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Axis must be 0-5", "SixDofDevice::moveAxisSet");
    }
    
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && motion) {
        try {
            Tango::DeviceData data_in;
            data_in << *params;  // 解引用指针，传递数组内容
            motion->command_inout("setMoveParameter", data_in);
        } catch (Tango::DevFailed &e) {
            Tango::Except::re_throw_exception(e, 
                "API_ProxyError", 
                "Failed to set move parameters", 
                "SixDofDevice::moveAxisSet");
        }
    }
    log_event("Move parameters set for axis " + std::to_string(axis));
}

void SixDofDevice::structAxisSet(const Tango::DevVarDoubleArray *params) {
    check_state("structAxisSet");  // OFF/ON/FAULT可用
    if (params->length() < 4) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Need 4 values: [axis, stepAngle, gearRatio, subdivision]", 
            "SixDofDevice::structAxisSet");
    }
    int axis = static_cast<int>((*params)[0]);
    if (axis < 0 || axis >= NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Axis must be 0-5", "SixDofDevice::structAxisSet");
    }
    
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && motion) {
        try {
            Tango::DeviceData data_in;
            data_in << params;
            motion->command_inout("setStructParameter", data_in);
        } catch (Tango::DevFailed &e) {
            Tango::Except::re_throw_exception(e, 
                "API_ProxyError", 
                "Failed to set structure parameters", 
                "SixDofDevice::structAxisSet");
        }
    }
    log_event("Structure parameters set for axis " + std::to_string(axis));
}

// ========== Pose Motion Commands ==========
void SixDofDevice::movePoseRelative(const Tango::DevVarDoubleArray *pose) {
    check_state("movePoseRelative");
    if (pose->length() != NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Input must be 6 values [X,Y,Z,ThetaX,ThetaY,ThetaZ]", 
            "SixDofDevice::movePoseRelative");
    }
    
    std::array<double, NUM_AXES> target_pose;
    for (int i = 0; i < NUM_AXES; ++i) {
        target_pose[i] = six_freedom_pose_[i] + (*pose)[i];
    }
    
    if (!validate_pose(target_pose)) {
        Tango::Except::throw_exception("API_OutOfRange", 
            "Target pose out of limits", "SixDofDevice::movePoseRelative");
    }
    
    log_event("Pose relative move started");
    
    // 运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before motion, continuing anyway" << endl;
        }
    }
    
    // Calculate inverse kinematics
    // Note: GUI sends angles in radians, but kinematics expects degrees
    // const double rad_to_deg = 180.0 / M_PI;
    Common::Pose p;
    p.x = target_pose[0]; p.y = target_pose[1]; p.z = target_pose[2];
    p.rx = target_pose[3];
    p.ry = target_pose[4];
    p.rz = target_pose[5];
    
    std::array<double, 6> leg_lengths;
    if (!kinematics_->calculateInverseKinematics(p, leg_lengths)) {
        Tango::Except::throw_exception("API_KinematicsError", 
            "Unreachable pose", "SixDofDevice::movePoseRelative");
    }
    
    // 对腿长进行4位小数舍入
    for (int i = 0; i < NUM_AXES; ++i) {
        leg_lengths[i] = round_to_decimals(leg_lengths[i], 4);
    }
    
    if (sim_mode_) {
        for (int i = 0; i < NUM_AXES; ++i) {
            current_leg_lengths_[i] = round_to_decimals(leg_lengths[i], 4);
            dire_pos_[i] = leg_lengths[i];
            axis_pos_[i] = leg_lengths[i];
            sdof_state_[i] = false;
        }
        six_freedom_pose_ = target_pose;
        result_value_ = 0;
    } else {
        std::cout << "get in motion" << endl;
        auto motion = get_motion_controller_proxy();
        if (motion) {
            for (int i = 0; i < NUM_AXES; ++i) {
                // 计算增量：目标腿长 - 当前存储的leg长度
                double delta = leg_lengths[i] - current_leg_lengths_[i];
                std::cout << std::fixed << std::setprecision(4);
                std::cout << std::fixed << std::setprecision(4);
                std::cout << "current leg length " << current_leg_lengths_[i] << endl;
                std::cout << "target leg length " << leg_lengths[i] << endl;
                std::cout << "axis " << i << " delta: " << delta << endl;
                int pulse = static_cast<int>(std::round(29793.103448275 * delta)); // 四舍五入到最接近的整数步数
                std::cout << "axis " << i << " delta steps: " << pulse << endl;
                // 使用相对运动
                send_move_command(i, pulse, true);
                sdof_state_[i] = true;
                // 更新存储的leg长度（保疙4位小数）
                current_leg_lengths_[i] = round_to_decimals(leg_lengths[i], 4);
                axis_pos_[i] = leg_lengths[i];
            }
            set_state(Tango::MOVING);
        }
    }
    log_event("Pose relative move completed");
}

void SixDofDevice::movePoseAbsolute(const Tango::DevVarDoubleArray *pose) {
    check_state("movePoseAbsolute");
    if (pose->length() != NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Input must be 6 values [X,Y,Z,ThetaX,ThetaY,ThetaZ]", 
            "SixDofDevice::movePoseAbsolute");
    }
    std::array<double, NUM_AXES> target_pose;
    for (int i = 0; i < NUM_AXES; ++i) {
        target_pose[i] = (*pose)[i];
    }
    
    if (!validate_pose(target_pose)) {
        Tango::Except::throw_exception("API_OutOfRange", 
            "Target pose out of limits", "SixDofDevice::movePoseAbsolute");
    }
    log_event("Pose absolute move started");
    
    // 运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before motion, continuing anyway" << endl;
        }
    }
    // Note: GUI sends angles in radians, but kinematics expects degrees
    const double rad_to_deg = 180.0 / M_PI;
    Common::Pose p;
    p.x = target_pose[0]; p.y = target_pose[1]; p.z = target_pose[2];
    p.rx = target_pose[3] * rad_to_deg;  // Convert radians to degrees
    p.ry = target_pose[4] * rad_to_deg;
    p.rz = target_pose[5] * rad_to_deg;
    
    std::array<double, 6> leg_lengths;
    if (!kinematics_->calculateInverseKinematics(p, leg_lengths)) {
        Tango::Except::throw_exception("API_KinematicsError", 
            "Unreachable pose", "SixDofDevice::movePoseAbsolute");
    }
    
    // 对腿长进行4位小数舍入
    for (int i = 0; i < NUM_AXES; ++i) {
        leg_lengths[i] = round_to_decimals(leg_lengths[i], 4);
    }
    
    if (sim_mode_) {
        for (int i = 0; i < NUM_AXES; ++i) {
            dire_pos_[i] = leg_lengths[i];
            current_leg_lengths_[i] = round_to_decimals(leg_lengths[i], 4);
            dire_pos_[i] = leg_lengths[i];
            axis_pos_[i] = leg_lengths[i];
            sdof_state_[i] = false;
        }
        six_freedom_pose_ = target_pose;
        result_value_ = 0;
    } else {
        std::cout << "get in motion" << endl;
        auto motion = get_motion_controller_proxy();
        if (motion) {
            for (int i = 0; i < NUM_AXES; ++i) {
                // TODO:这里不是计算相对增量，而是绝对增量：目标腿长 - 当前存储的leg长度.
                double delta = leg_lengths[i] - current_leg_lengths_[i];
                std::cout << "current leg length " << current_leg_lengths_[i] << endl;
                std::cout << "target leg length " << leg_lengths[i] << endl;
                std::cout << "axis " << i << " delta: " << delta << endl;
                int pulse = static_cast<int>(std::round(29793.103448275 * delta)); // 四舍五入到最接近的整数步数
                std::cout << "axis " << i << " delta steps: " << pulse << endl;
                // 使用相对运动
                send_move_command(i, pulse, false);
                sdof_state_[i] = true;
                // 更新存储的leg长度（保疙4位小数）
                current_leg_lengths_[i] = round_to_decimals(leg_lengths[i], 4);
                axis_pos_[i] = leg_lengths[i];
            }
            set_state(Tango::MOVING);
        }
    }
    log_event("Pose absolute move completed");
}

// ========== Reset/Zero Commands ==========
void SixDofDevice::reset() {
    INFO_STREAM << "[DEBUG] reset() called, current_state=" << Tango::DevStateName[get_state()] << endl;
    check_state("reset");
    log_event("Reset started");
    
    // 复位前自动启用刹车（安全保护）
    if (brake_power_port_ >= 0 && brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-engaging brake before reset (safety)" << endl;
        if (!engage_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to engage brake before reset" << endl;
        }
    }
    
    Common::StandardSystemDevice::reset();
    
    if (!sim_mode_) {
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            ERROR_STREAM << "[DEBUG] reset: motion_controller_proxy_ is NULL in real mode!" << endl;
            result_value_ = 1;
            Tango::Except::throw_exception("API_ProxyError",
                "Motion controller not connected. Cannot reset in real mode.",
                "SixDofDevice::reset");
        }
        try {
            // 六自由度设备有 6 个轴，需要对所有轴进行 reset
            INFO_STREAM << "[DEBUG] reset: sending reset to motion controller for all 6 axes" << endl;
            auto motion = get_motion_controller_proxy();
            if (!motion) {
                throw std::runtime_error("Motion controller proxy not available");
            }
            for (int axis = 0; axis < NUM_AXES; ++axis) {
                Tango::DeviceData data_in;
                data_in << static_cast<Tango::DevShort>(axis);
                motion->command_inout("reset", data_in);
                INFO_STREAM << "[DEBUG] reset: axis " << axis << " reset completed" << endl;
            }
            INFO_STREAM << "[DEBUG] reset: all axes reset successfully" << endl;
        } catch (Tango::DevFailed &e) {
            ERROR_STREAM << "[DEBUG] reset: reset command failed - " << e.errors[0].desc << endl;
            result_value_ = 1;
            Tango::Except::throw_exception("API_ProxyError",
                "Failed to reset motors", "SixDofDevice::reset");
        }
    } else {
        INFO_STREAM << "[DEBUG] reset: SIM MODE - skipping motor reset" << endl;
    }

    // Clear latched limit fault
    limit_fault_latched_.store(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    
    alarm_state_.clear();
    result_value_ = 0;
    set_state(Tango::ON);
    log_event("Reset completed");
    INFO_STREAM << "[DEBUG] reset: completed, state set to ON" << endl;
}

void SixDofDevice::sixMoveZero() {
    check_state("sixMoveZero");
    log_event("Move to zero started");

    if (limit_fault_latched_.load()) {
        Tango::Except::throw_exception(
            "LIMIT_FAULT_LATCHED",
            "Limit fault is latched; please run reset before moving.",
            "SixDofDevice::sixMoveZero");
    }
    
    if (sim_mode_) {
        axis_pos_.fill(0.0);
        dire_pos_.fill(0.0);
        six_freedom_pose_.fill(0.0);
        lim_org_state_.fill(0);  // At origin
        result_value_ = 0;
    } else {
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            result_value_ = 1;
            Tango::Except::throw_exception("API_ProxyError",
                "Motion controller not connected. Cannot move to zero in real mode.",
                "SixDofDevice::sixMoveZero");
        }
        try {
            // 循环调用每个轴的带参 moveZero 命令
            INFO_STREAM << "[DEBUG] sixMoveZero: sending moveZero to motion controller for all 6 axes" << endl;
            for (int axis = 0; axis < NUM_AXES; ++axis) {
                Tango::DeviceData data_in;
                data_in << static_cast<Tango::DevShort>(axis);
                motion->command_inout("moveZero", data_in);
                INFO_STREAM << "[DEBUG] sixMoveZero: axis " << axis << " moveZero command sent" << endl;
            }
            set_state(Tango::MOVING);
            INFO_STREAM << "[DEBUG] sixMoveZero: all axes moveZero commands sent successfully" << endl;
        } catch (Tango::DevFailed &e) {
            result_value_ = 1;
            ERROR_STREAM << "[DEBUG] sixMoveZero: moveZero command failed - " << e.errors[0].desc << endl;
            Tango::Except::re_throw_exception(e, 
                "API_ProxyError", 
                "Failed to move to zero", 
                "SixDofDevice::sixMoveZero");
        }
    }
    log_event("Move to zero completed");
}

void SixDofDevice::singleReset(Tango::DevShort axis) {
    check_state("singleReset");
    if (axis < 0 || axis >= NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Axis must be 0-5", "SixDofDevice::singleReset");
    }
    
    log_event("Single axis reset: " + std::to_string(axis));
    
    if (sim_mode_) {
        axis_pos_[axis] = 0.0;
        dire_pos_[axis] = 0.0;
        lim_org_state_[axis] = 0;
        result_value_ = 0;
    } else {
        auto motion = get_motion_controller_proxy();
        if (motion) {
            try {
                Tango::DeviceData data_in;
                data_in << axis;
                motion->command_inout("moveZero", data_in);
            } catch (Tango::DevFailed &e) {
                result_value_ = 1;
                Tango::Except::re_throw_exception(e, 
                    "API_ProxyError", 
                    "Failed to reset axis", 
                    "SixDofDevice::singleReset");
            }
        }
    }

    // Any reset-like operation should clear a latched limit fault
    if (limit_fault_latched_.load()) {
        limit_fault_latched_.store(false);
        limit_fault_axis_.store(-1);
        limit_fault_el_state_.store(0);
        alarm_state_.clear();
    }
}

// ========== Single Axis Motion Commands ==========
void SixDofDevice::singleMoveRelative(const Tango::DevVarDoubleArray *params) {
    check_state("singleMoveRelative");
    if (params->length() < 2) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Need 2 values: [axis, distance]", "SixDofDevice::singleMoveRelative");
    }
    
    int axis = static_cast<int>((*params)[0]);
    double distance = (*params)[1];
    
    if (axis < 0 || axis >= NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Axis must be 0-5", "SixDofDevice::singleMoveRelative");
    }
    
    log_event("Single axis relative move: axis=" + std::to_string(axis) + " dist=" + std::to_string(distance));
    
    // 运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        dire_pos_[axis] += distance;
        axis_pos_[axis] += distance;
        lim_org_state_[axis] = 2;  // Not at origin
        result_value_ = 0;
    } else {
        send_move_command(axis, distance, true);
    }
}

void SixDofDevice::singleMoveAbsolute(const Tango::DevVarDoubleArray *params) {
    check_state("singleMoveAbsolute");
    if (params->length() < 2) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Need 2 values: [axis, position]", "SixDofDevice::singleMoveAbsolute");
    }
    
    int axis = static_cast<int>((*params)[0]);
    double position = (*params)[1];
    
    if (axis < 0 || axis >= NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Axis must be 0-5", "SixDofDevice::singleMoveAbsolute");
    }
    
    log_event("Single axis absolute move: axis=" + std::to_string(axis) + " pos=" + std::to_string(position));
    
    // 运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        dire_pos_[axis] = position;
        axis_pos_[axis] = position;
        lim_org_state_[axis] = (position == 0.0) ? 0 : 2;
        result_value_ = 0;
    } else {
        send_move_command(axis, position, false);
    }
}

void SixDofDevice::movePosePvt(Tango::DevString argin) {
    check_state("movePosePvt");
    
    log_event("PVT trajectory motion started");
    
    // 运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before PVT motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        INFO_STREAM << "[Simulation] PVT trajectory motion: " << argin << endl;
        set_state(Tango::MOVING);
        result_value_ = 0;
        return;
    }
    
    try {
        // 解析JSON输入: {"poses": [[x,y,z,rx,ry,rz],...], "times": [t0,t1,...], "velocities": [[vx,vy,vz,vrx,vry,vrz],...]}
        nlohmann::json j = nlohmann::json::parse(argin);
        
        if (j.count("poses") == 0 || j.count("times") == 0) {
            Tango::Except::throw_exception("InvalidJSON",
                "Required fields: poses, times", "movePosePvt");
        }
        
        auto poses = j["poses"].get<std::vector<std::array<double, 6>>>();
        auto times = j["times"].get<std::vector<double>>();
        
        // 速度可选，如果没有提供则自动计算
        std::vector<std::array<double, 6>> velocities;
        if (j.count("velocities") > 0) {
            velocities = j["velocities"].get<std::vector<std::array<double, 6>>>();
        }
        
        int count = poses.size();
        if (count < 2) {
            Tango::Except::throw_exception("InvalidData",
                "At least 2 poses required", "movePosePvt");
        }
        if (times.size() != static_cast<size_t>(count)) {
            Tango::Except::throw_exception("InvalidData",
                "times array size must match poses count", "movePosePvt");
        }
        
        // 验证所有姿态是否在范围内
        for (const auto& pose : poses) {
            if (!validate_pose(pose)) {
                Tango::Except::throw_exception("API_OutOfRange",
                    "Trajectory pose out of limits", "movePosePvt");
            }
        }
        
        // 第一步：对每个姿态进行逆运动学计算，得到绝对腿长
        const double rad_to_deg = 180.0 / M_PI;
        std::vector<std::array<double, 6>> leg_lengths_abs_trajectory;  // 绝对腿长序列
        
        for (size_t i = 0; i < poses.size(); ++i) {
            Common::Pose p;
            p.x = poses[i][0];
            p.y = poses[i][1];
            p.z = poses[i][2];
            p.rx = poses[i][3] * rad_to_deg;
            p.ry = poses[i][4] * rad_to_deg;
            p.rz = poses[i][5] * rad_to_deg;
            
            std::array<double, 6> leg_lengths_abs;
            if (!kinematics_->calculateInverseKinematics(p, leg_lengths_abs)) {
                Tango::Except::throw_exception("API_KinematicsError",
                    "Unreachable pose at index " + std::to_string(i), "movePosePvt");
            }
            
            // 对腿长进行4位小数舍入
            for (int j = 0; j < NUM_AXES; ++j) {
                leg_lengths_abs[j] = round_to_decimals(leg_lengths_abs[j], 4);
            }
            
            leg_lengths_abs_trajectory.push_back(leg_lengths_abs);
        }
        
        // 第二步：转换为相对增量（相对于轨迹起点）
        std::vector<std::array<double, 6>> leg_lengths_trajectory;  // 相对增量序列
        for (size_t i = 0; i < leg_lengths_abs_trajectory.size(); ++i) {
            std::array<double, 6> leg_lengths_rel;
            for (int j = 0; j < NUM_AXES; ++j) {
                // 所有点都相对于轨迹起点（第一个点的绝对位置）
                leg_lengths_rel[j] = leg_lengths_abs_trajectory[i][j] - leg_lengths_abs_trajectory[0][j];
            }
            leg_lengths_trajectory.push_back(leg_lengths_rel);
        }
        
        // 计算每个轴的速度（如果没有提供）
        std::vector<std::array<double, 6>> leg_velocities;
        if (velocities.empty()) {
            // 根据位置差和时间差自动计算速度
            for (size_t i = 0; i < leg_lengths_trajectory.size(); ++i) {
                std::array<double, 6> vel;
                if (i == 0) {
                    // 第一个点：使用到下一个点的平均速度
                    double dt = times[1] - times[0];
                    for (int axis = 0; axis < NUM_AXES; ++axis) {
                        vel[axis] = (leg_lengths_trajectory[1][axis] - leg_lengths_trajectory[0][axis]) / dt;
                    }
                } else if (i == leg_lengths_trajectory.size() - 1) {
                    // 最后一个点：使用从上一个点的平均速度
                    double dt = times[i] - times[i-1];
                    for (int axis = 0; axis < NUM_AXES; ++axis) {
                        vel[axis] = (leg_lengths_trajectory[i][axis] - leg_lengths_trajectory[i-1][axis]) / dt;
                    }
                } else {
                    // 中间点：使用中心差分
                    double dt = times[i+1] - times[i-1];
                    for (int axis = 0; axis < NUM_AXES; ++axis) {
                        vel[axis] = (leg_lengths_trajectory[i+1][axis] - leg_lengths_trajectory[i-1][axis]) / dt;
                    }
                }
                leg_velocities.push_back(vel);
            }
        }
        
        // 构造JSON发送给运动控制器
        nlohmann::json pvt_data;
        pvt_data["axes"] = {0, 1, 2, 3, 4, 5};  // 所有6个轴
        pvt_data["count"] = count;
        pvt_data["time"] = times;
        
        // 转置：从[point][axis]到[axis][point]
        std::vector<std::vector<double>> pos_by_axis(6);
        std::vector<std::vector<double>> vel_by_axis(6);
        for (int axis = 0; axis < NUM_AXES; ++axis) {
            for (int i = 0; i < count; ++i) {
                pos_by_axis[axis].push_back(leg_lengths_trajectory[i][axis]);
                vel_by_axis[axis].push_back(leg_velocities[i][axis]);
            }
        }
        
        pvt_data["pos"] = pos_by_axis;
        pvt_data["vel"] = vel_by_axis;
        
        std::string pvt_json = pvt_data.dump();
        
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            Tango::Except::throw_exception("API_ProxyError",
                "Motion controller proxy not available", "movePosePvt");
        }
        
        // 先下发PVT表
        INFO_STREAM << "[PVT] Setting PVT table..." << endl;
        Tango::DeviceData data_in;
        data_in << Tango::string_dup(pvt_json.c_str());
        motion->command_inout("setPvts", data_in);
        
        // 再启动运动
        INFO_STREAM << "[PVT] Starting PVT motion..." << endl;
        nlohmann::json move_cmd;
        move_cmd["axes"] = {0, 1, 2, 3, 4, 5};
        std::string move_json = move_cmd.dump();
        Tango::DeviceData move_data;
        move_data << Tango::string_dup(move_json.c_str());
        motion->command_inout("movePvts", move_data);
        
        // 更新状态
        for (int i = 0; i < NUM_AXES; ++i) {
            sdof_state_[i] = true;
            // 更新目标腿长为轨迹终点的绝对值
            current_leg_lengths_[i] = leg_lengths_abs_trajectory.back()[i];
        }
        six_freedom_pose_ = poses.back();
        set_state(Tango::MOVING);
        result_value_ = 0;
        
        INFO_STREAM << "[PVT] Trajectory motion started with " << count << " points" << endl;
        
    } catch (nlohmann::json::exception& e) {
        Tango::Except::throw_exception("JSONParseError", e.what(), "movePosePvt");
    } catch (Tango::DevFailed& e) {
        Tango::Except::re_throw_exception(e, "API_ProxyError",
            "Failed to execute PVT motion", "movePosePvt");
    }
}

void SixDofDevice::send_move_command(int axis, int position, bool relative) {
    auto motion = get_motion_controller_proxy();
    if (!motion) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_ProxyError",
            "Motion controller proxy not available", 
            "SixDofDevice::send_move_command");
    }
    
    try {
        Tango::DevVarDoubleArray move_params;
        move_params.length(2);
        move_params[0] = static_cast<double>(axis);
        move_params[1] = position;
        
        Tango::DeviceData data_in;
        data_in << move_params;
        
        std::string cmd = relative ? "moveRelative" : "moveAbsolute";
        INFO_STREAM << "[DEBUG] send_move_command: axis=" << axis 
                   << ", position=" << position 
                   << ", relative=" << (relative ? "true" : "false")
                   << ", cmd=" << cmd << endl;
        motion->command_inout(cmd.c_str(), data_in);
        sdof_state_[axis] = true;
        set_state(Tango::MOVING);
        INFO_STREAM << "[DEBUG] send_move_command: command sent successfully for axis " << axis << endl;
    } catch (Tango::DevFailed &e) {
        result_value_ = 1;
        ERROR_STREAM << "[DEBUG] send_move_command: Failed for axis " << axis 
                    << ", error: " << e.errors[0].desc << endl;
        std::string error_msg = "Failed to send move command for axis " + std::to_string(axis) + ": " + std::string(e.errors[0].desc.in());
        Tango::Except::re_throw_exception(e, 
            "API_ProxyError", 
            error_msg.c_str(), 
            "SixDofDevice::send_move_command");
    }
}

void SixDofDevice::simSwitch(Tango::DevShort mode) {
    bool was_sim_mode = sim_mode_;
    sim_mode_ = (mode != 0);
    
    // 注意：运行时切换只影响当前会话，server 重启后恢复配置文件的值（不持久化）
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] simSwitch: Enabling SIMULATION MODE (运行时切换，不持久化)" << endl;
        reset_motion_controller_proxy();
        reset_encoder_proxy();
        set_status("Simulation Mode");
        log_event("Simulation mode enabled (runtime switch)");
    } else {
        INFO_STREAM << "[DEBUG] simSwitch: Disabling simulation mode, switching to REAL MODE (运行时切换，不持久化)" << endl;
        set_status("Connecting to Hardware...");
        connect_proxies();
        if (get_state() != Tango::FAULT) {
            set_status("Connected to Hardware");
        }
        log_event("Simulation mode disabled (runtime switch)");
        
        // 如果从模拟模式切换到真实模式，且代理未连接，尝试连接代理
        auto motion = get_motion_controller_proxy();
        auto encoder = get_encoder_proxy();
        if (was_sim_mode && (!motion || !encoder)) {
            INFO_STREAM << "[DEBUG] simSwitch: Switching from sim mode to real mode, connecting proxies..." << endl;
            try {
                connect_proxies();
                INFO_STREAM << "[DEBUG] simSwitch: Proxies connected successfully" << endl;
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] simSwitch: Failed to connect proxies: " << e.errors[0].desc << endl;
                // 不抛出异常，允许继续运行，但会有警告
            } catch (...) {
                ERROR_STREAM << "[DEBUG] simSwitch: Failed to connect proxies with unknown exception" << endl;
            }
        }
    }
}

void SixDofDevice::saveEncoderPositions() {
    INFO_STREAM << "[SaveEncoder] Saving current encoder positions to database..." << endl;
    
    try {
        // 首先更新编码器值
        update_axis_positions();
        
        // 准备属性数据
        std::vector<std::string> axis_pos_str;
        axis_pos_str.reserve(NUM_AXES);
        
        for (int i = 0; i < NUM_AXES; ++i) {
            axis_pos_str.push_back(std::to_string(axis_pos_[i]));
        }
        
        // 写入数据库
        Tango::DbData db_data;
        db_data.push_back(Tango::DbDatum("axis_pos"));
        db_data[0] << axis_pos_str;
        
        get_db_device()->put_property(db_data);
        
        // 记录日志
        std::ostringstream oss;
        oss << "Encoder positions saved: [";
        for (int i = 0; i < NUM_AXES; ++i) {
            if (i > 0) oss << ", ";
            oss << std::fixed << std::setprecision(3) << axis_pos_[i];
        }
        oss << "]";
        
        INFO_STREAM << "[SaveEncoder] " << oss.str() << endl;
        log_event(oss.str());
        
        result_value_ = 0; // Success
        
    } catch (const Tango::DevFailed &e) {
        ERROR_STREAM << "[SaveEncoder] Failed to save encoder positions: " << e.errors[0].desc << endl;
        result_value_ = 1; // Failure
        // Tango::Except::re_throw_exception(e,
        //                                  "DATABASE_ERROR",
        //                                  "Failed to save encoder positions to database",
        //                                  "SixDofDevice::saveEncoderPositions()");
    } catch (const std::exception &e) {
        ERROR_STREAM << "[SaveEncoder] Exception: " << e.what() << endl;
        result_value_ = 1; // Failure
        Tango::Except::throw_exception("SAVE_ERROR",
                                      e.what(),
                                      "SixDofDevice::saveEncoderPositions()");
    }
}

// ========== Read Commands ==========
Tango::DevVarDoubleArray* SixDofDevice::readEncoder() {
    check_state("readEncoder");
    update_axis_positions();
    
    Tango::DevVarDoubleArray *result = new Tango::DevVarDoubleArray();
    result->length(NUM_AXES);
    for (int i = 0; i < NUM_AXES; ++i) {
        (*result)[i] = axis_pos_[i];
    }
    return result;
}

Tango::DevBoolean SixDofDevice::readOrg(Tango::DevShort axis) {
    check_state("readOrg");
    if (axis < 0 || axis >= NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Axis must be 0-5", "SixDofDevice::readOrg");
    }
    return (lim_org_state_[axis] == 0);  // 0 = at origin
}

Tango::DevShort SixDofDevice::readEL(Tango::DevShort axis) {
    check_state("readEL");
    if (axis < 0 || axis >= NUM_AXES) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Axis must be 0-5", "SixDofDevice::readEL");
    }
    // Return: 0=none, 1=EL+, 2=EL-
    short state = lim_org_state_[axis];
    if (state == 1) return 1;   // Positive limit
    if (state == -1) return 2;  // Negative limit
    return 0;
}

// ========== Control Commands ==========
void SixDofDevice::stop() {
    check_state("stop");
    log_event("Stop all axes");
    
    if (!sim_mode_) {
        // 真实模式下，必须有控制器连接
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            result_value_ = 1;
            Tango::Except::throw_exception("API_ProxyError",
                "Motion controller not connected. Cannot stop in real mode.",
                "SixDofDevice::stop");
        }
        try {
            // 循环停止所有6个轴
            for (int axis = 0; axis < NUM_AXES; ++axis) {
                Tango::DeviceData data_in;
                data_in << static_cast<Tango::DevShort>(axis);
                motion->command_inout("stopMove", data_in);
            }
        } catch (Tango::DevFailed &e) {
            result_value_ = 1;
            Tango::Except::re_throw_exception(e, 
                "API_ProxyError",
                "Failed to stop motion", 
                "SixDofDevice::stop");
        }
    }
    
    // 始终清除运动状态标志并设置结果值
    sdof_state_.fill(false);
    if (limit_fault_latched_.load() || get_state() == Tango::FAULT) {
        set_state(Tango::FAULT);
        if (!alarm_state_.empty()) {
            set_status(alarm_state_);
        }
    } else {
        set_state(Tango::ON);
    }
    result_value_ = 0;
    
    // 注意：正常停止后不自动启用刹车，保持刹车释放状态以便快速继续运动
    // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
}

void SixDofDevice::openBrake(Tango::DevBoolean open) {
    check_state("openBrake");
    log_event(open ? "Brake opened" : "Brake closed");
    open_brake_state_ = open;
    
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && motion) {
        // Send brake command to motion controller if available
    }
    result_value_ = 0;
}

// ========== Parameter Commands ==========
Tango::DevString SixDofDevice::readtAxis() {
    check_state("readtAxis");
    std::ostringstream oss;
    oss << "{";
    for (int i = 0; i < NUM_AXES; ++i) {
        if (i > 0) oss << ",";
        oss << "\"Axis" << i << "\":{";
        oss << "\"StepAngle\":\"1.8\",";
        oss << "\"GearRatio\":\"1.0\",";
        oss << "\"Subdivision\":\"16\",";
        oss << "\"StartSpeed\":\"100\",";
        oss << "\"MaxSpeed\":\"1000\",";
        oss << "\"AccelerationTime\":\"0.1\",";
        oss << "\"DecelerationTime\":\"0.1\",";
        oss << "\"StopSpeed\":\"50\",";
        oss << "\"ChannelNumber\":\"" << i << "\"";
        oss << "}";
    }
    oss << "}";
    axis_parameter_ = oss.str();
    return Tango::string_dup(axis_parameter_.c_str());
}

void SixDofDevice::exportAxis() {
    check_state("exportAxis");
    log_event("Parameters exported to file");

    try {
        auto now = std::time(nullptr);
        std::ostringstream filename;
        filename << "axis_params_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".json";

        // 确保参数为最新
        readtAxis();

        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << axis_parameter_;
            file.close();
            result_value_ = 0;
        } else {
            result_value_ = 1;
            Tango::Except::throw_exception("API_FileError",
                "Failed to create parameter file", "SixDofDevice::exportAxis");
        }
    } catch (std::exception& e) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_ExportError",
            std::string("Failed to export parameters: ") + e.what(), "SixDofDevice::exportAxis");
    }
}

// ========== Attribute Reads ==========
void SixDofDevice::read_attr(Tango::Attribute &attr) {
    std::string attr_name = attr.get_name();
    
    if (attr_name == "selfCheckResult") read_self_check_result(attr);
    else if (attr_name == "positionUnit") read_position_unit(attr);
    else if (attr_name == "groupAttributeJson") read_group_attribute_json(attr);
    else if (attr_name == "axisPos") read_axis_pos(attr);
    else if (attr_name == "direPos") read_dire_pos(attr);
    else if (attr_name == "openBrakeState") read_open_brake_state(attr);
    else if (attr_name == "sixFreedomPose") read_six_freedom_pose(attr);
    else if (attr_name == "sixLogs") read_six_logs(attr);
    else if (attr_name == "alarmState") read_alarm_state(attr);
    else if (attr_name == "axisParameter") read_axis_parameter(attr);
    else if (attr_name == "limOrgState") read_lim_org_state(attr);
    else if (attr_name == "sdofState") read_sdof_state(attr);
    else if (attr_name == "resultValue") read_result_value(attr);
    else if (attr_name == "driverPowerStatus") read_driver_power_status(attr);
    else if (attr_name == "brakeStatus") read_brake_status(attr);
}

void SixDofDevice::read_driver_power_status(Tango::Attribute &attr) {
    // 直接使用成员变量地址，确保指针在 Tango 读取时仍然有效
    attr.set_value(&driver_power_enabled_);
}

void SixDofDevice::read_brake_status(Tango::Attribute &attr) {
    // 直接使用成员变量地址，确保指针在 Tango 读取时仍然有效
    attr.set_value(&brake_released_);
}

void SixDofDevice::read_self_check_result(Tango::Attribute &attr) {
    attr_selfCheckResult_read = self_check_result_;
    attr.set_value(&attr_selfCheckResult_read);
}

void SixDofDevice::read_position_unit(Tango::Attribute &attr) {
    attr_positionUnit_read = Tango::string_dup(position_unit_.c_str());
    attr.set_value(&attr_positionUnit_read);
}

void SixDofDevice::write_position_unit(Tango::WAttribute &attr) {
    Tango::DevString new_unit;
    attr.get_write_value(new_unit);
    std::string unit_str(new_unit);
    
    if (unit_str != "step" && unit_str != "mm" && unit_str != "um" && 
        unit_str != "rad" && unit_str != "urad" && unit_str != "mrad") {
        Tango::Except::throw_exception("API_InvalidValue", 
            "Invalid position unit", "SixDofDevice::write_position_unit");
    }
    position_unit_ = unit_str;
}

void SixDofDevice::write_attr(Tango::WAttribute &attr) {
    std::string attr_name = attr.get_name();
    
    if (attr_name == "positionUnit") {
        write_position_unit(attr);
    } else {
        WARN_STREAM << "[DEBUG] write_attr: Unknown writable attribute: " << attr_name << std::endl;
    }
}

void SixDofDevice::read_group_attribute_json(Tango::Attribute &attr) {
    std::ostringstream oss;
    oss << "{\"axisPos\":[";
    for (int i = 0; i < NUM_AXES; ++i) {
        if (i > 0) oss << ",";
        oss << axis_pos_[i];
    }
    oss << "],\"sixFreedomPose\":[";
    for (int i = 0; i < NUM_AXES; ++i) {
        if (i > 0) oss << ",";
        oss << six_freedom_pose_[i];
    }
    oss << "],\"sdofState\":[";
    for (int i = 0; i < NUM_AXES; ++i) {
        if (i > 0) oss << ",";
        oss << (sdof_state_[i] ? "true" : "false");
    }
    oss << "]}";
    std::string json = oss.str();
    attr_groupAttributeJson_read = Tango::string_dup(json.c_str());
    attr.set_value(&attr_groupAttributeJson_read);
}

void SixDofDevice::read_axis_pos(Tango::Attribute &attr) {
    update_axis_positions();
    attr.set_value(axis_pos_.data(), NUM_AXES);
}

void SixDofDevice::read_dire_pos(Tango::Attribute &attr) {
    attr.set_value(dire_pos_.data(), NUM_AXES);
}

void SixDofDevice::read_open_brake_state(Tango::Attribute &attr) {
    attr_openBrakeState_read = open_brake_state_;
    attr.set_value(&attr_openBrakeState_read);
}

void SixDofDevice::read_six_freedom_pose(Tango::Attribute &attr) {
    update_pose_from_encoders();
    attr.set_value(six_freedom_pose_.data(), NUM_AXES);
}

void SixDofDevice::read_six_logs(Tango::Attribute &attr) {
    attr_sixLogs_read = Tango::string_dup(six_logs_.c_str());
    attr.set_value(&attr_sixLogs_read);
}

void SixDofDevice::read_alarm_state(Tango::Attribute &attr) {
    attr_alarmState_read = Tango::string_dup(alarm_state_.c_str());
    attr.set_value(&attr_alarmState_read);
}

void SixDofDevice::read_axis_parameter(Tango::Attribute &attr) {
    readtAxis();  // Update axis_parameter_
    attr_axisParameter_read = Tango::string_dup(axis_parameter_.c_str());
    attr.set_value(&attr_axisParameter_read);
}

void SixDofDevice::read_lim_org_state(Tango::Attribute &attr) {
    attr.set_value(lim_org_state_.data(), NUM_AXES);
}

void SixDofDevice::read_sdof_state(Tango::Attribute &attr) {
    for (int i = 0; i < NUM_AXES; ++i) {
        attr_sdofState_read[i] = sdof_state_[i];
    }
    attr.set_value(attr_sdofState_read, NUM_AXES);
}

void SixDofDevice::read_result_value(Tango::Attribute &attr) {
    attr_resultValue_read = result_value_;
    attr.set_value(&attr_resultValue_read);
}

// ========== Hooks and Helpers ==========
void SixDofDevice::specific_self_check() {
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && !motion) {
        throw std::runtime_error("Motion Controller Proxy not initialized");
    }
    if (!sim_mode_ && motion) {
        motion->ping();
    }
}

void SixDofDevice::always_executed_hook() {
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
                                       << (retries + 1) << "/" << MAX_RESTORE_RETRIES << ")" << endl;
                        } else {
                            // 超过最大重试次数，记录错误但不再重试
                            ERROR_STREAM << "[Reconnect] Restore failed after " << MAX_RESTORE_RETRIES 
                                        << " attempts, giving up" << endl;
                            restore_retry_count_.store(0);  // 重置计数，等待下次重连
                        }
                    }
                }
            }
        }

        // 重要：避免在请求路径做网络 ping/重连。
        // 这里保持"零等待"，只根据后台线程更新的 connection_healthy_ 更新设备状态文本。
        if (!connection_healthy_.load() && get_state() == Tango::ON) {
            // 连接丢失时自动启用刹车（安全保护）
            if (brake_power_port_ >= 0 && brake_released_) {
                INFO_STREAM << "[BrakeControl] Connection lost, auto-engaging brake (safety)" << endl;
                if (!engage_brake()) {
                    WARN_STREAM << "[BrakeControl] Failed to engage brake on connection loss" << endl;
                }
            }
            set_state(Tango::FAULT);
            set_status("Network connection lost");
        }
    }
}

void SixDofDevice::read_attr_hardware(std::vector<long> &/*attr_list*/) {
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && motion) {
        try {
            // 检测限位触发：读取所有轴的限位状态
            if (brake_power_port_ >= 0 && brake_released_ && get_state() == Tango::MOVING) {
                bool limit_triggered = false;
                Tango::DevShort first_axis = -1;
                Tango::DevShort first_el_state = 0;
                for (int i = 0; i < NUM_AXES; ++i) {
                    try {
                        Tango::DeviceData data_in;
                        data_in << static_cast<Tango::DevShort>(i);
                        Tango::DeviceData data_out = motion->command_inout("readEL", data_in);
                        Tango::DevShort el_state_raw;
                        data_out >> el_state_raw;
                        
                        // 限位开关低电平有效：运动控制器已经处理了低电平有效逻辑
                        // 运动控制器 readEL 返回值：
                        //   1  = EL+（正限位触发）
                        //   -1 = EL-（负限位触发）
                        //   0  = 无限位触发
                        // 直接使用运动控制器返回的值，不需要再反转
                        Tango::DevShort el_state = el_state_raw;
                        
                        // readEL返回: 0=none, 1=EL+, -1=EL-
                        if (el_state != 0) {
                            limit_triggered = true;
                            if (first_axis < 0) {
                                first_axis = static_cast<Tango::DevShort>(i);
                                first_el_state = el_state;
                            }
                            lim_org_state_[i] = el_state;
                            INFO_STREAM << "[BrakeControl] Limit switch triggered on axis " << i 
                                       << " (el_state=" << el_state << ")" << endl;
                        } else if (lim_org_state_[i] == 1 || lim_org_state_[i] == -1) {
                            lim_org_state_[i] = 2;
                        }
                    } catch (...) {
                        // 忽略单个轴读取失败
                    }
                }
                // 如果检测到限位触发，立即启用刹车并停止运动
                if (limit_triggered) {
                    if (!limit_fault_latched_.exchange(true)) {
                        limit_fault_axis_.store(first_axis);
                        limit_fault_el_state_.store(first_el_state);
                        std::string dir = (first_el_state > 0) ? "EL+" : ((first_el_state < 0) ? "EL-" : "EL");
                        alarm_state_ = "Limit switch triggered: axis " + std::to_string(first_axis) + " (" + dir + ")";
                        set_status(alarm_state_);
                        set_state(Tango::FAULT);
                    }
                    INFO_STREAM << "[BrakeControl] Limit triggered, auto-engaging brake (safety)" << endl;
                    if (!engage_brake()) {
                        WARN_STREAM << "[BrakeControl] Failed to engage brake on limit trigger" << endl;
                    }
                    // 停止运动（停止所有轴以确保安全）
                    try {
                        auto motion = get_motion_controller_proxy();
                        if (motion) {
                            for (int axis = 0; axis < NUM_AXES; ++axis) {
                                Tango::DeviceData data_in;
                                data_in << static_cast<Tango::DevShort>(axis);
                                try {
                                    motion->command_inout("stopMove", data_in);
                                } catch (...) {
                                    // 忽略单个轴停止失败，继续停止其他轴
                                }
                            }
                        }
                    } catch (...) {
                        // 忽略停止失败
                    }
                }
            }

            // If limit fault latched, keep FAULT regardless of controller state.
            if (limit_fault_latched_.load()) {
                set_state(Tango::FAULT);
                if (!alarm_state_.empty()) {
                    set_status(alarm_state_);
                }
                return;
            }
            
            auto motion = get_motion_controller_proxy();
            if (!motion) {
                return;
            }
            Tango::DevState mc_state = motion->state();
            if (mc_state == Tango::MOVING) {
                set_state(Tango::MOVING);
            } else if (mc_state == Tango::FAULT) {
                // 故障时自动启用刹车（安全保护）
                if (get_state() != Tango::FAULT && brake_power_port_ >= 0 && brake_released_) {
                    INFO_STREAM << "[BrakeControl] Fault detected, auto-engaging brake (safety)" << endl;
                    if (!engage_brake()) {
                        WARN_STREAM << "[BrakeControl] Failed to engage brake on fault" << endl;
                    }
                }
                set_state(Tango::FAULT);
            } else {
                // 运动完成：从MOVING状态变为ON状态
                // 注意：正常运动完成后不自动启用刹车，保持刹车释放状态以便快速继续运动
                // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
                set_state(Tango::ON);
            }
        } catch (...) {
            set_state(Tango::UNKNOWN);
        }
    }
}

bool SixDofDevice::validate_pose(const std::array<double, NUM_AXES> &pose) {
    for (int i = 0; i < 3; ++i) {
        if (std::abs(pose[i]) > POS_LIMIT) return false;
    }
    for (int i = 3; i < NUM_AXES; ++i) {
        if (std::abs(pose[i]) > ROT_LIMIT) return false;
    }
    return true;
}

void SixDofDevice::update_axis_positions() {
    // 从编码器设备读取真实位置
    if (sim_mode_) {
        // 模拟模式：使用存储的 current_leg_lengths_
        axis_pos_ = current_leg_lengths_;
        return;
    }
    
    auto encoder = get_encoder_proxy();
    if (!encoder) {
        // 编码器不可用，使用存储的值
        axis_pos_ = current_leg_lengths_;
        return;
    }
    
    try {
        // 从编码器设备读取所有6个轴的位置
        for (int axis = 0; axis < NUM_AXES; ++axis) {
            if (axis < static_cast<int>(encoder_channels_.size())) {
                short encoder_channel = encoder_channels_[axis];
                try {
                    // 调用编码器设备的readEncoder命令，传入通道号
                    Tango::DeviceData data_in;
                    data_in << encoder_channel;
                    Tango::DeviceData data_out = encoder->command_inout("readEncoder", data_in);
                    double encoder_pos;
                    data_out >> encoder_pos;
                    
                    // 更新轴位置
                    axis_pos_[axis] = encoder_pos;
                    current_leg_lengths_[axis] = encoder_pos;  // 同步更新leg长度
                } catch (Tango::DevFailed &e) {
                    // 读取失败，使用上次的值
                    WARN_STREAM << "Failed to read encoder channel " << encoder_channel 
                               << " for axis " << axis << ": " << e.errors[0].desc << endl;
                }
            }
        }
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "Failed to read encoder positions: " << e.errors[0].desc << endl;
        // 读取失败，使用存储的值
        axis_pos_ = current_leg_lengths_;
    } catch (...) {
        WARN_STREAM << "Unknown error reading encoder positions" << endl;
        axis_pos_ = current_leg_lengths_;
    }
}

void SixDofDevice::update_pose_from_encoders() {
    update_axis_positions();
    // NOTE: Forward kinematics (computing pose from leg lengths) is complex for Stewart platform
    // and requires iterative numerical solving. Currently not implemented.
    // The pose is updated from motion commands (movePoseAbsolute/Relative) instead.
    // For single-axis moves, the pose cannot be directly computed from one leg length change,
    // so six_freedom_pose_ is not updated in that case.
}

void SixDofDevice::configure_kinematics() {
    if (sdof_config_.empty()) {
        WARN_STREAM << "sdofConfig is empty, using default kinematics parameter." << endl;
        return;
    }

    try {
        nlohmann::json obj = nlohmann::json::parse(sdof_config_);
        
        if (!obj.is_object()) {
            ERROR_STREAM << "Failed to parse sdofConfig JSON: " << sdof_config_ << endl;
            return;
        }

        Common::PlatformGeometry geom;

        // Helper lambda to get double from string or number
        auto get_val = [&](const std::string& key, double default_val) -> double {
            if (obj.find(key) == obj.end()) return default_val;
            
            auto& val = obj[key];
            if (val.is_string()) {
                try {
                    return std::stod(val.get<std::string>());
                } catch (...) {
                    return default_val;
                }
            }
            if (val.is_number()) {
                return val.get<double>();
            }
            return default_val;
        };

        // Parse geometry parameters
        geom.platform_radius = get_val("r1", 110.0);       // r1;上平台半径
        geom.base_radius = get_val("r2", 193.0);           // r2;下平台
        geom.initial_height = get_val("hh", 408.0);        // hh;上下平台铰点面间距离
        std::cout << "Initial height (hh): " << geom.initial_height << std::endl;
        // 保持字段语义一致：hh就是"上铰点面到下铰点面距离"
        geom.H_up_down = geom.initial_height;
        geom.platform_half_angle = get_val("a1", 40.0);    // a1;上平台第一点与X轴夹角
        geom.base_half_angle = get_val("a2", 14.0);        // a2;下平台第一点与X轴夹角
        geom.H_upd_up = get_val("h3", 57.0);               // h3;铰点到上平台下表面垂直高度
        geom.H_target_upd = get_val("h", 575.5);            // h;动坐标点与上平台下表面的垂直距离
        
        // Calculate leg limits roughly if not specified (could use "ll")
        double ll = get_val("ll", 421.4857);
        if (ll > 0) {
            geom.nominal_leg_length = ll;
            geom.min_leg_length = ll  - 40; // Heuristic
            geom.max_leg_length = ll  + 40; // Heuristic
        }

        INFO_STREAM << "Kinematics configured: r1=" << geom.platform_radius 
                   << ", r2=" << geom.base_radius 
                   << ", hh=" << geom.initial_height << endl;

        kinematics_ = std::unique_ptr<Common::StewartPlatformKinematics>(
            new Common::StewartPlatformKinematics(geom));

        // 用 kinematics 的 normalleg 初始化 leg 长度
        double normal_leg_lengths_ = kinematics_->getNominalLegLength();
        current_leg_lengths_.fill(round_to_decimals(normal_leg_lengths_, 4));
        axis_pos_ = current_leg_lengths_;
    } catch (nlohmann::json::parse_error& e) {
        ERROR_STREAM << "JSON parse error in sdofConfig: " << e.what() << endl;
    } catch (std::exception& e) {
        ERROR_STREAM << "Error configuring kinematics: " << e.what() << endl;
    }
}

// ===== CUSTOM ATTRIBUTE CLASSES (for read_attr() dispatch) =====
class SixDofAttr : public Tango::Attr {
public:
    SixDofAttr(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<SixDofDevice *>(dev)->read_attr(att);
    }
};

class SixDofAttrRW : public Tango::Attr {
public:
    SixDofAttrRW(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ_WRITE)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<SixDofDevice *>(dev)->read_attr(att);
    }
    
    virtual void write(Tango::DeviceImpl *dev, Tango::WAttribute &att) override {
        static_cast<SixDofDevice *>(dev)->write_attr(att);
    }
};

class SixDofSpectrumAttr : public Tango::SpectrumAttr {
public:
    SixDofSpectrumAttr(const char *name, long data_type, Tango::AttrWriteType w_type, long max_x)
        : Tango::SpectrumAttr(name, data_type, w_type, max_x) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<SixDofDevice *>(dev)->read_attr(att);
    }
};

// ========== Class Factory ==========
SixDofDeviceClass *SixDofDeviceClass::_instance = nullptr;

SixDofDeviceClass *SixDofDeviceClass::instance() {
    if (_instance == nullptr) {
        std::string class_name = "SixDofDevice";
        _instance = new SixDofDeviceClass(class_name);
    }
    return _instance;
}

SixDofDeviceClass::SixDofDeviceClass(std::string &class_name) : Tango::DeviceClass(class_name) {
    command_factory();
}

void SixDofDeviceClass::attribute_factory(std::vector<Tango::Attr *> &att_list) {
    // Standard attributes - 使用自定义属性类以确保 read_attr() 被调用
    att_list.push_back(new SixDofAttr("selfCheckResult", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new SixDofAttrRW("positionUnit", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new SixDofAttr("groupAttributeJson", Tango::DEV_STRING, Tango::READ));
    
    // Position attributes - 使用自定义 SpectrumAttr 类
    att_list.push_back(new SixDofSpectrumAttr("axisPos", Tango::DEV_DOUBLE, Tango::READ, NUM_AXES));
    att_list.push_back(new SixDofSpectrumAttr("direPos", Tango::DEV_DOUBLE, Tango::READ, NUM_AXES));
    att_list.push_back(new SixDofSpectrumAttr("sixFreedomPose", Tango::DEV_DOUBLE, Tango::READ, NUM_AXES));
    
    // State attributes
    att_list.push_back(new SixDofAttr("openBrakeState", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new SixDofSpectrumAttr("limOrgState", Tango::DEV_SHORT, Tango::READ, NUM_AXES));
    att_list.push_back(new SixDofSpectrumAttr("sdofState", Tango::DEV_BOOLEAN, Tango::READ, NUM_AXES));
    
    // Log and status attributes
    att_list.push_back(new SixDofAttr("sixLogs", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new SixDofAttr("alarmState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new SixDofAttr("axisParameter", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new SixDofAttr("resultValue", Tango::DEV_SHORT, Tango::READ));
    
    // Power control status attributes (NEW)
    att_list.push_back(new SixDofAttr("driverPowerStatus", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new SixDofAttr("brakeStatus", Tango::DEV_BOOLEAN, Tango::READ));
}

void SixDofDeviceClass::command_factory() {
    // Lock/Unlock commands
    command_list.push_back(new Tango::TemplCommand(
        "devLock", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::devLock)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>(
        "devUnlock", static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&SixDofDevice::devUnlock)));
    command_list.push_back(new Tango::TemplCommand(
        "devLockVerify", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::devLockVerify)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>(
        "devLockQuery", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&SixDofDevice::devLockQuery)));
    command_list.push_back(new Tango::TemplCommand(
        "devUserConfig", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::devUserConfig)));
    
    // System commands
    command_list.push_back(new Tango::TemplCommand(
        "selfCheck", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::selfCheck)));
    command_list.push_back(new Tango::TemplCommand(
        "init", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::init)));
    
    // Parameter commands
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "moveAxisSet", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&SixDofDevice::moveAxisSet)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "structAxisSet", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&SixDofDevice::structAxisSet)));
    
    // Pose motion commands
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "movePoseRelative", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&SixDofDevice::movePoseRelative)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "movePoseAbsolute", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&SixDofDevice::movePoseAbsolute)));
    
    // Reset/Zero commands
    command_list.push_back(new Tango::TemplCommand(
        "reset", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::reset)));
    command_list.push_back(new Tango::TemplCommand(
        "sixMoveZero", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::sixMoveZero)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "singleReset", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&SixDofDevice::singleReset)));
    
    // Single axis commands
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "singleMoveRelative", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&SixDofDevice::singleMoveRelative)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "singleMoveAbsolute", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&SixDofDevice::singleMoveAbsolute)));
    
    // Read commands
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevVarDoubleArray *>(
        "readEncoder", static_cast<Tango::DevVarDoubleArray * (Tango::DeviceImpl::*)()>(&SixDofDevice::readEncoder)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevBoolean>(
        "readOrg", static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)(Tango::DevShort)>(&SixDofDevice::readOrg)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevShort, Tango::DevShort>(
        "readEL", static_cast<Tango::DevShort (Tango::DeviceImpl::*)(Tango::DevShort)>(&SixDofDevice::readEL)));
    
    // Control commands
    command_list.push_back(new Tango::TemplCommand(
        "stop", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::stop)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>(
        "openBrake", static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&SixDofDevice::openBrake)));
    
    // Parameter export commands
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>(
        "readtAxis", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&SixDofDevice::readtAxis)));
    command_list.push_back(new Tango::TemplCommand(
        "exportAxis", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::exportAxis)));

    // Simulation command
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "simSwitch", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&SixDofDevice::simSwitch)));
    
    // // Database commands
    // command_list.push_back(new Tango::TemplCommand(
    //     "saveEncoderPositions", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::saveEncoderPositions)));
    
    // Power Control Commands (NEW - for GUI)
    command_list.push_back(new Tango::TemplCommand(
        "enableDriverPower", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::enableDriverPower)));
    command_list.push_back(new Tango::TemplCommand(
        "disableDriverPower", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::disableDriverPower)));
    command_list.push_back(new Tango::TemplCommand(
        "releaseBrake", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::releaseBrake)));
    command_list.push_back(new Tango::TemplCommand(
        "engageBrake", static_cast<void (Tango::DeviceImpl::*)()>(&SixDofDevice::engageBrake)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>(
        "queryPowerStatus", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&SixDofDevice::queryPowerStatus)));
}

// ========== Power Control Commands (for GUI) ==========
void SixDofDevice::enableDriverPower() {
    if (!enable_driver_power()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to enable driver power", "SixDofDevice::enableDriverPower");
    }
}

void SixDofDevice::disableDriverPower() {
    if (!disable_driver_power()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to disable driver power", "SixDofDevice::disableDriverPower");
    }
}

void SixDofDevice::releaseBrake() {
    if (!release_brake()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to release brake", "SixDofDevice::releaseBrake");
    }
}

void SixDofDevice::engageBrake() {
    if (!engage_brake()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to engage brake", "SixDofDevice::engageBrake");
    }
}

Tango::DevString SixDofDevice::queryPowerStatus() {
    std::stringstream ss;
    ss << "{"
       << "\"driverPowerEnabled\":" << (driver_power_enabled_ ? "true" : "false") << ","
       << "\"brakeReleased\":" << (brake_released_ ? "true" : "false") << ","
       << "\"driverPowerPort\":" << driver_power_port_ << ","
       << "\"brakePowerPort\":" << brake_power_port_ << ","
       << "\"driverPowerController\":\"" << driver_power_controller_ << "\","
       << "\"brakePowerController\":\"" << brake_power_controller_ << "\""
       << "}";
    return Tango::string_dup(ss.str().c_str());
}

// ========== Power Control Methods ==========
bool SixDofDevice::enable_driver_power() {
    INFO_STREAM << "[PowerControl] enable_driver_power() called" << endl;
    INFO_STREAM << "[PowerControl] driver_power_port_=" << driver_power_port_ 
               << ", driver_power_controller_=" << driver_power_controller_ << endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Driver power enabled (simulated)" << endl;
        driver_power_enabled_ = true;
        return true;
    }
    
    if (driver_power_port_ < 0) {
        INFO_STREAM << "[PowerControl] Driver power control not configured (port=" 
                   << driver_power_port_ << "), skipping" << endl;
        return true;  // 未配置时认为成功
    }
    
    try {
        Tango::DeviceProxy* power_ctrl = nullptr;
        bool created_new = false;
        
        // 如果配置了专用控制器且与当前运动控制器不同，则创建新的 proxy
        if (!driver_power_controller_.empty() && 
            driver_power_controller_ != motion_controller_name_) {
            power_ctrl = new Tango::DeviceProxy(driver_power_controller_);
            created_new = true;
            INFO_STREAM << "[PowerControl] Using dedicated controller: " << driver_power_controller_ << endl;
        } else {
            auto motion = get_motion_controller_proxy();
            if (motion) {
                power_ctrl = motion.get();
                INFO_STREAM << "[PowerControl] Using connected motion controller" << endl;
            } else {
                ERROR_STREAM << "[PowerControl] Motion controller proxy not available!" << endl;
                return false;
            }
        }
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(driver_power_port_);
        params[1] = 1.0;  // HIGH = 上电
        Tango::DeviceData data;
        data << params;
        
        INFO_STREAM << "[PowerControl] Calling writeIO with port=" << driver_power_port_ << ", value=1" << endl;
        INFO_STREAM << "[PowerControl] Executing hardware writeIO command..." << endl;
        power_ctrl->command_inout("writeIO", data);
        INFO_STREAM << "[PowerControl] writeIO command executed successfully" << endl;
        
        if (created_new) delete power_ctrl;
        
        driver_power_enabled_ = true;
        INFO_STREAM << "✓ Driver power enabled on port OUT" << driver_power_port_ << endl;
        log_event("Driver power enabled on port OUT" + std::to_string(driver_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to enable driver power: " << e.errors[0].desc << endl;
        log_event("Failed to enable driver power: " + std::string(e.errors[0].desc.in()));
        return false;
    }
}

bool SixDofDevice::disable_driver_power() {
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Driver power disabled (simulated)" << endl;
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
        // 注意：OUTx 为低电平有效，MotionControllerDevice::writeIO 会做 active-low 反相
        // 这里写入的是“逻辑值”：1=开启(硬件LOW)，0=关闭(硬件HIGH)
        params[1] = 0.0;  // 逻辑0=关闭驱动器上电
        Tango::DeviceData data;
        data << params;
        power_ctrl->command_inout("writeIO", data);
        
        if (created_new) delete power_ctrl;
        driver_power_enabled_ = false;
        INFO_STREAM << "✓ Driver power disabled on port OUT" << driver_power_port_ << endl;
        log_event("Driver power disabled on port OUT" + std::to_string(driver_power_port_));
        
        // 断电前自动启用刹车（安全保护）
        if (brake_power_port_ >= 0 && brake_released_) {
            INFO_STREAM << "[BrakeControl] Auto-engaging brake before power off (safety)" << endl;
            if (!engage_brake()) {
                WARN_STREAM << "[BrakeControl] Failed to engage brake before power off" << endl;
            }
        }
        
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to disable driver power: " << e.errors[0].desc << endl;
        return false;
    }
}

bool SixDofDevice::release_brake() {
    INFO_STREAM << "[PowerControl] release_brake() called" << endl;
    INFO_STREAM << "[PowerControl] brake_power_port_=" << brake_power_port_ 
               << ", brake_power_controller_=" << brake_power_controller_ << endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Brake released (simulated)" << endl;
        brake_released_ = true;
        return true;
    }
    
    if (brake_power_port_ < 0) {
        INFO_STREAM << "[PowerControl] Brake control not configured (port=" 
                   << brake_power_port_ << "), skipping" << endl;
        return true;  // 未配置时认为成功
    }
    
    try {
        Tango::DeviceProxy* brake_ctrl = nullptr;
        bool created_new = false;
        
        if (!brake_power_controller_.empty() && 
            brake_power_controller_ != motion_controller_name_) {
            brake_ctrl = new Tango::DeviceProxy(brake_power_controller_);
            created_new = true;
            INFO_STREAM << "[PowerControl] Using dedicated brake controller: " << brake_power_controller_ << endl;
        } else {
            auto motion = get_motion_controller_proxy();
            if (motion) {
                brake_ctrl = motion.get();
                INFO_STREAM << "[PowerControl] Using connected motion controller for brake" << endl;
            } else {
                ERROR_STREAM << "[PowerControl] Motion controller proxy not available!" << endl;
                return false;
            }
        }
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(brake_power_port_);
        // 逻辑1=开启刹车供电(硬件LOW)，用于释放刹车
        params[1] = 1.0;
        Tango::DeviceData data;
        data << params;
        
        INFO_STREAM << "[PowerControl] Calling writeIO for brake with port=" << brake_power_port_ << ", value=1" << endl;
        brake_ctrl->command_inout("writeIO", data);
        
        if (created_new) delete brake_ctrl;
        
        brake_released_ = true;
        INFO_STREAM << "✓ Brake released on port OUT" << brake_power_port_ << endl;
        log_event("Brake released on port OUT" + std::to_string(brake_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to release brake: " << e.errors[0].desc << endl;
        log_event("Failed to release brake: " + std::string(e.errors[0].desc.in()));
        return false;
    }
}

bool SixDofDevice::engage_brake() {
    INFO_STREAM << "[PowerControl] engage_brake() called" << endl;
    INFO_STREAM << "[PowerControl] brake_power_port_=" << brake_power_port_ 
               << ", brake_power_controller_=" << brake_power_controller_ << endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Brake engaged (simulated)" << endl;
        brake_released_ = false;
        return true;
    }
    
    if (brake_power_port_ < 0) {
        INFO_STREAM << "[PowerControl] Brake control not configured (port=" 
                   << brake_power_port_ << "), skipping" << endl;
        return true;  // 未配置时认为成功
    }
    
    try {
        Tango::DeviceProxy* brake_ctrl = nullptr;
        bool created_new = false;
        
        if (!brake_power_controller_.empty() && 
            brake_power_controller_ != motion_controller_name_) {
            brake_ctrl = new Tango::DeviceProxy(brake_power_controller_);
            created_new = true;
            INFO_STREAM << "[PowerControl] Using dedicated brake controller: " << brake_power_controller_ << endl;
        } else {
            auto motion = get_motion_controller_proxy();
            if (motion) {
                brake_ctrl = motion.get();
                INFO_STREAM << "[PowerControl] Using connected motion controller for brake" << endl;
            } else {
                ERROR_STREAM << "[PowerControl] Motion controller proxy not available!" << endl;
                return false;
            }
        }
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(brake_power_port_);
        // 逻辑0=关闭刹车供电(硬件HIGH)，用于抱闸/启用刹车
        params[1] = 0.0;
        Tango::DeviceData data;
        data << params;
        
        INFO_STREAM << "[PowerControl] Calling writeIO for brake with port=" << brake_power_port_ << ", value=0" << endl;
        brake_ctrl->command_inout("writeIO", data);
        
        if (created_new) delete brake_ctrl;
        
        brake_released_ = false;
        INFO_STREAM << "✓ Brake engaged on port OUT" << brake_power_port_ << endl;
        log_event("Brake engaged on port OUT" + std::to_string(brake_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to engage brake: " << e.errors[0].desc << endl;
        log_event("Failed to engage brake: " + std::string(e.errors[0].desc.in()));
        return false;
    }
}

void SixDofDeviceClass::device_factory(const Tango::DevVarStringArray *devlist_ptr) {
    for (unsigned long i = 0; i < devlist_ptr->length(); i++) {
        std::string dev_name = (*devlist_ptr)[i].in();
        SixDofDevice *dev = new SixDofDevice(this, dev_name);
        device_list.push_back(dev);
        export_device(dev);
    }
}

} // namespace SixDof

// Main function
void Tango::DServer::class_factory() {
    add_class(SixDof::SixDofDeviceClass::instance());
}

int main(int argc, char *argv[]) {
    try {
        Common::SystemConfig::loadConfig();
        Tango::Util *tg = Tango::Util::init(argc, argv);
        tg->server_init();
        std::cout << "SixDof Server Ready" << std::endl;
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

