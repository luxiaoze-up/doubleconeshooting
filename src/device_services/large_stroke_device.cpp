#include "device_services/large_stroke_device.h"
#include "common/system_config.h"
#include <iostream>
#include <string>
#include <cmath>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <stdexcept>

namespace LargeStroke {

namespace {

// Temporarily overrides Tango proxy timeout for a ping and restores it afterwards.
// Throws on ping failure.
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

LargeStrokeDevice::LargeStrokeDevice(Tango::DeviceClass *device_class, std::string &device_name)
    : Common::StandardSystemDevice(device_class, device_name),
    is_locked_(false),
    lock_role_level_(0),
    move_range_(1000),
    limit_number_(2),
    axis_id_(0),
    encoder_channel_(6),  // 默认值：大行程编码器通道 6 (199通道7)
    large_range_pos_(0.0),
    dire_pos_(0.0),
    host_plug_state_("UNKNOWN"),
    large_lim_org_state_(2),
    large_range_state_(false),
    limit_fault_latched_(false),
    limit_fault_el_state_(0),
    result_value_(0),
    position_unit_("step"),
    self_check_result_(-1),
    sim_mode_(false),
    steps_per_mm_(100.0),        // 默认值：每毫米100步
    steps_per_rad_(1000.0),      // 默认值：每弧度1000步
    last_alarm_check_(std::chrono::steady_clock::now()),
    last_state_check_(std::chrono::steady_clock::now()),
    motion_controller_proxy_(nullptr),
    encoder_proxy_(nullptr),
    connection_healthy_(false),
    stop_connection_monitor_(false),
    last_reconnect_attempt_(std::chrono::steady_clock::now() - std::chrono::seconds(10)) {
        init_device();
    }

LargeStrokeDevice::~LargeStrokeDevice() {
    delete_device();
}

void LargeStrokeDevice::init_device() {
    Common::StandardSystemDevice::init_device();
    
    Tango::DbData db_data;
    db_data.push_back(Tango::DbDatum("bundleNo"));
    db_data.push_back(Tango::DbDatum("laserNo"));
    db_data.push_back(Tango::DbDatum("systemNo"));
    db_data.push_back(Tango::DbDatum("subDevList"));
    db_data.push_back(Tango::DbDatum("modelList"));
    db_data.push_back(Tango::DbDatum("currentModel"));
    db_data.push_back(Tango::DbDatum("MotionControllerConnectString"));
    db_data.push_back(Tango::DbDatum("EncoderConnectString"));
    db_data.push_back(Tango::DbDatum("errorDict"));
    db_data.push_back(Tango::DbDatum("deviceName"));
    db_data.push_back(Tango::DbDatum("deviceID"));
    db_data.push_back(Tango::DbDatum("devicePosition"));
    db_data.push_back(Tango::DbDatum("deviceProductDate"));
    db_data.push_back(Tango::DbDatum("deviceInstallDate"));
    db_data.push_back(Tango::DbDatum("moveRange"));
    db_data.push_back(Tango::DbDatum("limitNumber"));
    db_data.push_back(Tango::DbDatum("motionControllerName"));
    db_data.push_back(Tango::DbDatum("encoderName"));
    db_data.push_back(Tango::DbDatum("axisId"));
    db_data.push_back(Tango::DbDatum("encoderChannel"));  // 编码器通道号（可能与运动控制器轴号不同）
    db_data.push_back(Tango::DbDatum("stepsPerMm"));      // 单位转换因子：每毫米步数
    db_data.push_back(Tango::DbDatum("stepsPerRad"));     // 单位转换因子：每弧度步数
    db_data.push_back(Tango::DbDatum("motorStepAngle"));
    db_data.push_back(Tango::DbDatum("motorGearRatio"));
    db_data.push_back(Tango::DbDatum("motorSubdivision"));
    db_data.push_back(Tango::DbDatum("driverPowerPort"));
    db_data.push_back(Tango::DbDatum("driverPowerController"));
    db_data.push_back(Tango::DbDatum("brakePowerPort"));
    db_data.push_back(Tango::DbDatum("brakePowerController"));
    get_db_device()->get_property(db_data);
    
    int idx = 0;
    if (!db_data[idx].is_empty()) { db_data[idx] >> bundle_no_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> laser_no_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> system_no_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> sub_dev_list_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> model_list_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> current_model_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motion_controller_connect_string_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_connect_string_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> error_dict_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_name_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_id_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_position_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_product_date_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_install_date_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> move_range_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> limit_number_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motion_controller_name_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_name_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> axis_id_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_channel_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> steps_per_mm_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> steps_per_rad_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motor_step_angle_; } else { motor_step_angle_ = 1.8; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motor_gear_ratio_; } else { motor_gear_ratio_ = 1.0; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> motor_subdivision_; } else { motor_subdivision_ = 12800.0; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> driver_power_port_; } else { driver_power_port_ = -1; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> driver_power_controller_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> brake_power_port_; } else { brake_power_port_ = -1; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> brake_power_controller_; }
    
    // 初始化状态
    driver_power_enabled_ = false;
    brake_released_ = false;
    
    if (motion_controller_name_.empty()) motion_controller_name_ = "sys/motion/1";  // 默认使用控制器1 (192.168.1.11)
    
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
    
    // 验证转换因子有效性（必须大于0）
    if (steps_per_mm_ <= 0.0) {
        WARN_STREAM << "Invalid stepsPerMm (" << steps_per_mm_ << "), using default 100.0" << std::endl;
        steps_per_mm_ = 100.0;
    }
    if (steps_per_rad_ <= 0.0) {
        WARN_STREAM << "Invalid stepsPerRad (" << steps_per_rad_ << "), using default 1000.0" << std::endl;
        steps_per_rad_ = 1000.0;
    }
    
    INFO_STREAM << "Unit conversion factors: stepsPerMm=" << steps_per_mm_ 
                << ", stepsPerRad=" << steps_per_rad_ << std::endl;
    
    INFO_STREAM << "[DEBUG] LargeStrokeDevice initialized. Motion Controller: " << motion_controller_name_ 
                << ", Encoder: " << encoder_name_ << ", Axis ID: " << axis_id_ 
                << ", Encoder Channel: " << encoder_channel_ << std::endl;
    INFO_STREAM << "[DEBUG] Configuration: moveRange=" << move_range_ 
                << ", limitNumber=" << limit_number_ << ", positionUnit=" << position_unit_ << std::endl;
    log_event("Device initialized");

    connect_proxies();

    // 启动独立后台连接维护线程：命令路径只读 connection_healthy_，不触发重连阻塞
    start_connection_monitor();
}

void LargeStrokeDevice::delete_device() {
    // 停止后台连接维护线程，避免与资源释放并发
    stop_connection_monitor();

    // 设备关闭前自动启用刹车（安全保护）
    if (brake_power_port_ >= 0 && brake_released_ && !sim_mode_) {
        INFO_STREAM << "[BrakeControl] Auto-engaging brake before device shutdown (safety)" << std::endl;
        if (!engage_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to engage brake before shutdown" << std::endl;
        }
    }

    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        motion_controller_proxy_.reset();
        encoder_proxy_.reset();
    }
    Common::StandardSystemDevice::delete_device();
}

// ===== Proxy helpers (lifetime-safe) =====
std::shared_ptr<Tango::DeviceProxy> LargeStrokeDevice::get_motion_controller_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return motion_controller_proxy_;
}

std::shared_ptr<Tango::DeviceProxy> LargeStrokeDevice::get_encoder_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return encoder_proxy_;
}

void LargeStrokeDevice::reset_motion_controller_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    motion_controller_proxy_.reset();
}

void LargeStrokeDevice::reset_encoder_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    encoder_proxy_.reset();
}

void LargeStrokeDevice::rebuild_motion_proxy(int timeout_ms) {
    if (motion_controller_name_.empty()) {
        return;
    }
    auto new_motion = create_proxy_and_ping(motion_controller_name_, timeout_ms);
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        motion_controller_proxy_ = new_motion;
    }
}

void LargeStrokeDevice::rebuild_encoder_proxy(int timeout_ms) {
    if (encoder_name_.empty()) {
        return;
    }
    auto new_enc = create_proxy_and_ping(encoder_name_, timeout_ms);
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        encoder_proxy_ = new_enc;
    }
}

void LargeStrokeDevice::perform_post_motion_reconnect_restore() {
    if (sim_mode_) {
        return;
    }

    // Motion proxy is required for restore actions.
    auto motion = get_motion_controller_proxy();
    if (!motion) {
        return;
    }

    // 1) Apply motor subdivision parameters
    try {
        INFO_STREAM << "Applying motor subdivision parameters (axis=" << axis_id_
                    << ", stepAngle=" << motor_step_angle_
                    << ", gearRatio=" << motor_gear_ratio_
                    << ", subdivision=" << motor_subdivision_ << ")" << std::endl;
        Tango::DevVarDoubleArray params;
        params.length(4);
        params[0] = static_cast<double>(axis_id_);
        params[1] = motor_step_angle_;
        params[2] = motor_gear_ratio_;
        params[3] = motor_subdivision_;
        Tango::DeviceData arg;
        arg << params;
        motion->command_inout("setStructParameter", arg);
        INFO_STREAM << "Successfully applied motor subdivision parameters to axis " << axis_id_ << std::endl;
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "Failed to apply motor subdivision parameters: " << e.errors[0].desc << std::endl;
    } catch (...) {
        WARN_STREAM << "Failed to apply motor subdivision parameters: unknown exception" << std::endl;
    }

    // 2) Restore relay configuration and power/brake states
    try {
        INFO_STREAM << "Restoring relay configuration and enabling driver power after reconnection..." << std::endl;
        if (enable_driver_power()) {
            INFO_STREAM << "Driver power enabled successfully after reconnection" << std::endl;
            if (release_brake()) {
                INFO_STREAM << "Brake released successfully after reconnection, device ready for operation" << std::endl;
            } else {
                WARN_STREAM << "Failed to release brake after reconnection, manual intervention may be required" << std::endl;
            }
        } else {
            WARN_STREAM << "Failed to enable driver power after reconnection" << std::endl;
        }
    } catch (...) {
        WARN_STREAM << "Post-reconnect power/brake restore failed with unknown exception" << std::endl;
    }

    // 3) Sync encoder position to motion controller if available
    auto enc = get_encoder_proxy();
    if (enc) {
        try {
            INFO_STREAM << "[DEBUG] Synchronizing encoder position to motion controller..." << std::endl;
            Tango::DeviceData data_in;
            data_in << encoder_channel_;
            Tango::DeviceData data_out = enc->command_inout("readEncoder", data_in);
            double encoder_pos;
            data_out >> encoder_pos;

            Tango::DevVarDoubleArray sync_params;
            sync_params.length(2);
            sync_params[0] = static_cast<double>(axis_id_);
            sync_params[1] = encoder_pos;
            Tango::DeviceData arg;
            arg << sync_params;
            motion->command_inout("setEncoderPosition", arg);

            INFO_STREAM << "[DEBUG] Axis " << axis_id_ << " (encoder channel " << encoder_channel_
                        << "): synced position " << encoder_pos << " to motion controller" << std::endl;
            log_event("Encoder position synchronized to motion controller: " + std::to_string(encoder_pos));
        } catch (Tango::DevFailed &e) {
            WARN_STREAM << "[DEBUG] Failed to sync encoder position: " << e.errors[0].desc << std::endl;
        } catch (...) {
            WARN_STREAM << "[DEBUG] Failed to sync encoder position: unknown exception" << std::endl;
        }
    }
}

// ===== Background connection monitor =====
void LargeStrokeDevice::start_connection_monitor() {
    // 模拟模式无需启动线程
    if (sim_mode_) {
        connection_healthy_.store(true);
        return;
    }
    // 避免重复启动
    if (connection_monitor_thread_.joinable()) {
        return;
    }
    stop_connection_monitor_.store(false);
    connection_monitor_thread_ = std::thread(&LargeStrokeDevice::connection_monitor_loop, this);
}

void LargeStrokeDevice::stop_connection_monitor() {
    stop_connection_monitor_.store(true);
    if (connection_monitor_thread_.joinable()) {
        connection_monitor_thread_.join();
    }
}

void LargeStrokeDevice::connection_monitor_loop() {
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
                    rebuild_motion_proxy(connect_timeout_ms);
                    motion_ok = true;
                    // 注意：不立即设置 connection_healthy_ = true，等待恢复操作完成后再设置
                    // 这样可以避免在恢复完成前执行命令导致使用未完全恢复的设备状态

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

void LargeStrokeDevice::connect_proxies() {
    INFO_STREAM << "[DEBUG] connect_proxies() called. sim_mode=" << sim_mode_ << std::endl;
    try {
        if (sim_mode_) {
            INFO_STREAM << "[DEBUG] Sim mode: skipping proxy connection" << std::endl;
            set_state(Tango::ON);
            set_status("Simulation Mode");
            connection_healthy_.store(true);
            INFO_STREAM << "[DEBUG] All proxies skipped (SIM MODE). Device state: ON, status: Simulation Mode" << std::endl;
            return;
        }

        // Aggressive simplification: always rebuild proxies when connect_proxies() is called.
        // This avoids duplicating ping/reconnect logic here vs. monitor loop.
        reset_motion_controller_proxy();
        reset_encoder_proxy();

        INFO_STREAM << "[DEBUG] Rebuilding proxies (motion=" << motion_controller_name_
                    << ", encoder=" << encoder_name_ << ")" << std::endl;
        rebuild_motion_proxy(500);
        rebuild_encoder_proxy(500);

        set_state(Tango::ON);
        set_status("Connected");
        connection_healthy_.store(true);  // 连接成功，设置标志为健康（保持现有语义：以 motion 为准）
        INFO_STREAM << "[DEBUG] All proxies connected. Device state: ON, status: Connected, connection_healthy=true" << std::endl;

        // Initial connect should perform restore immediately.
        perform_post_motion_reconnect_restore();
        motion_restore_pending_.store(false);
    } catch (Tango::DevFailed &e) {
        // 内层 catch 已经打印了详细的代理名称和错误信息，这里只设置状态
        // 在模拟模式下，连接失败不应该影响状态（因为我们本来就不需要连接）
        set_state(Tango::FAULT);
        set_status("Proxy connection failed");
        connection_healthy_.store(false);  // 连接失败，设置标志为不健康
        INFO_STREAM << "[DEBUG] connect_proxies: Connection failed, connection_healthy=false" << std::endl;
        motion_restore_pending_.store(false);
    }
}

void LargeStrokeDevice::log_event(const std::string &event) {
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S");
    if (linear_logs_.empty() || linear_logs_ == "{}") {
        linear_logs_ = "{\"" + oss.str() + "\":\"" + event + "\"}";
    } else {
        linear_logs_.insert(linear_logs_.length() - 1, ",\"" + oss.str() + "\":\"" + event + "\"");
    }
}

// ========== JSON Parsing Helpers ==========
std::string LargeStrokeDevice::parse_json_string(const std::string& json, const std::string& key) {
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

int LargeStrokeDevice::parse_json_int(const std::string& json, const std::string& key, int default_val) {
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
    
    // Parse integer
    try {
        return std::stoi(json.substr(num_start));
    } catch (...) {
        return default_val;
    }
}

// ========== Unit Conversion Helpers ==========
// 转换因子定义 (假设 1步 = 0.01mm = 10um 作为基准，可根据实际配置调整)
// step: 脉冲数 (基准单位)
// mm: 毫米
// um: 微米
// rad: 弧度
// urad: 微弧度
// mrad: 毫弧度

double LargeStrokeDevice::convert_to_steps(double value) {
    // 将用户输入值转换为步数（控制器使用的单位）
    // 使用从数据库配置读取的转换因子
    
    double result = value;
    if (position_unit_ == "step") {
        result = value;
    } else if (position_unit_ == "mm") {
        result = value * steps_per_mm_;
    } else if (position_unit_ == "um") {
        result = value * steps_per_mm_ / 1000.0;  // um -> mm -> steps
    } else if (position_unit_ == "rad") {
        result = value * steps_per_rad_;
    } else if (position_unit_ == "mrad") {
        result = value * steps_per_rad_ / 1000.0;  // mrad -> rad -> steps
    } else if (position_unit_ == "urad") {
        result = value * steps_per_rad_ / 1000000.0;  // urad -> rad -> steps
    }
    
    INFO_STREAM << "[DEBUG] convert_to_steps: " << value << " " << position_unit_ 
                << " -> " << result << " steps (stepsPerMm=" << steps_per_mm_ 
                << ", stepsPerRad=" << steps_per_rad_ << ")" << std::endl;
    return result;
}

double LargeStrokeDevice::convert_from_steps(double steps) {
    // 将步数转换为用户设置的单位（用于位置反馈）
    // 使用从数据库配置读取的转换因子
    
    double result = steps;
    if (position_unit_ == "step") {
        result = steps;
    } else if (position_unit_ == "mm") {
        result = steps / steps_per_mm_;
    } else if (position_unit_ == "um") {
        result = steps / steps_per_mm_ * 1000.0;
    } else if (position_unit_ == "rad") {
        result = steps / steps_per_rad_;
    } else if (position_unit_ == "mrad") {
        result = steps / steps_per_rad_ * 1000.0;
    } else if (position_unit_ == "urad") {
        result = steps / steps_per_rad_ * 1000000.0;
    }
    
    INFO_STREAM << "[DEBUG] convert_from_steps: " << steps << " steps -> " 
                << result << " " << position_unit_ << std::endl;
    return result;
}

// ========== State Machine Check ==========
// 状态机规则类型：
// ALL_STATES: 所有状态可用 (UNKNOWN/OFF/ON/MOVING/FAULT)
// NOT_ON: UNKNOWN/OFF/FAULT可用，ON/MOVING状态不可用
// NOT_UNKNOWN: OFF/ON/MOVING/FAULT可用，UNKNOWN状态不可用（stop等命令需要在MOVING状态下执行）
// ONLY_ON: 仅ON状态可用
enum class StateMachineRule {
    ALL_STATES,   // devLockVerify, devLockQuery, devUserConfig
    NOT_ON,       // devLock, devUnlock, selfCheck, init
    NOT_UNKNOWN,  // moveAxisSet, structAxisSet, stop, reset, readEncoder, plugInRead, readOrg, readEL, exportLogs, readtAxis, exportAxis, simSwitch
    ONLY_ON       // moveRelative, moveAbsolute, largeMoveAuto, openValue, runAction
};

void LargeStrokeDevice::check_state_for_command(const std::string& cmd_name, 
                                                 bool require_on,
                                                 bool allow_unknown) {
    Tango::DevState current_state = get_state();
    std::string state_name = Tango::DevStateName[current_state];
    
    INFO_STREAM << "[DEBUG] check_state_for_command: cmd=" << cmd_name 
                << ", current_state=" << state_name << ", require_on=" << require_on 
                << ", allow_unknown=" << allow_unknown << std::endl;
    
    // 根据参数确定规则类型
    StateMachineRule rule;
    if (require_on) {
        rule = StateMachineRule::ONLY_ON;
    } else if (allow_unknown) {
        rule = StateMachineRule::ALL_STATES;
    } else {
        rule = StateMachineRule::NOT_UNKNOWN;
    }
    
    bool allowed = false;
    std::string allowed_states;
    
    switch (rule) {
        case StateMachineRule::ALL_STATES:
            // 所有状态都允许
            allowed = true;
            allowed_states = "ALL_STATES";
            break;
            
        case StateMachineRule::NOT_ON:
            // UNKNOWN/OFF/FAULT可用，ON不可用
            allowed = (current_state == Tango::UNKNOWN || 
                       current_state == Tango::OFF || 
                       current_state == Tango::FAULT);
            allowed_states = "UNKNOWN, OFF, FAULT";
            break;
            
        case StateMachineRule::NOT_UNKNOWN:
            // OFF/ON/MOVING/FAULT可用，UNKNOWN不可用
            // 注意：MOVING状态也应允许，因为stop等命令需要在运动中执行
            allowed = (current_state == Tango::OFF || 
                       current_state == Tango::ON || 
                       current_state == Tango::MOVING ||
                       current_state == Tango::FAULT);
            allowed_states = "OFF, ON, MOVING, FAULT";
            break;
            
        case StateMachineRule::ONLY_ON:
            // 仅ON状态可用
            allowed = (current_state == Tango::ON);
            allowed_states = "ON";
            break;
    }
    
    if (!allowed) {
        ERROR_STREAM << "[DEBUG] check_state_for_command: Command " << cmd_name 
                    << " NOT ALLOWED in state " << state_name 
                    << ". Allowed states: " << allowed_states << std::endl;
        Tango::Except::throw_exception("API_InvalidState",
            "Command " + cmd_name + " not allowed in " + state_name + " state. Allowed states: " + allowed_states,
            "LargeStrokeDevice::check_state_for_command");
    } else {
        INFO_STREAM << "[DEBUG] check_state_for_command: Command " << cmd_name 
                   << " ALLOWED in state " << state_name << std::endl;
    }
}

// 新增：便捷的状态检查方法 - 用于NOT_ON规则
void LargeStrokeDevice::check_state_not_on(const std::string& cmd_name) {
    Tango::DevState current_state = get_state();
    if (current_state == Tango::ON) {
        Tango::Except::throw_exception("API_InvalidState",
            "Command " + cmd_name + " not allowed in ON state. Allowed states: UNKNOWN, OFF, FAULT",
            "LargeStrokeDevice::check_state_not_on");
    }
}

// 快速连接检查：在命令执行前检查连接状态标志（零等待，纯内存操作）
// 连接状态由后台线程 connection_monitor_loop 定期更新
bool LargeStrokeDevice::quick_check_connection() {
    if (sim_mode_) {
        return true;  // 模拟模式下总是返回成功
    }
    
    // 如果状态已经是 FAULT，直接返回失败
    if (get_state() == Tango::FAULT) {
        return false;
    }
    
    // 检查连接状态标志（由后台定期更新，这里只读取，不做任何网络操作）
    if (!connection_healthy_.load()) {
        // 连接不健康，立即设置状态为 FAULT
        set_state(Tango::FAULT);
        set_status("Motion controller not connected");
        return false;
    }
    
    // 检查 proxy 是否存在
    if (!motion_controller_name_.empty() && !get_motion_controller_proxy()) {
        set_state(Tango::FAULT);
        set_status("Motion controller not connected");
        connection_healthy_.store(false);
        return false;
    }
    
    return true;
}

// ========== Lock/Unlock Commands ==========
void LargeStrokeDevice::devLock() {
    check_state_not_on("devLock");  // UNKNOWN/OFF/FAULT可用，ON状态不可用
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (is_locked_) {
        Tango::Except::throw_exception("API_DeviceLocked", 
            "Device already locked by: " + lock_user_, "LargeStrokeDevice::devLock");
    }

    // 设计为无参，保持兼容：若未来需要用户信息可改为从客户端上下文获取
    lock_user_.clear();
    lock_role_.clear();
    lock_role_level_ = 0;

    is_locked_ = true;
    log_event("Device locked");
}

void LargeStrokeDevice::devUnlock(Tango::DevBoolean unlock_all) {
    check_state_not_on("devUnlock");  // UNKNOWN/OFF/FAULT可用，ON状态不可用
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (unlock_all || is_locked_) {
        log_event("Device unlocked");
        is_locked_ = false;
        lock_user_.clear();
        lock_role_.clear();
        lock_role_level_ = 0;
    }
}

void LargeStrokeDevice::devLockVerify() {
    // ALL_STATES: 所有状态可用，无需检查
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (!is_locked_) {
        Tango::Except::throw_exception("API_NotLocked", 
            "Device is not locked", "LargeStrokeDevice::devLockVerify");
    }
}

Tango::DevString LargeStrokeDevice::devLockQuery() {
    // ALL_STATES: 所有状态可用，无需检查
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::ostringstream oss;
    oss << "{\"locked\":" << (is_locked_ ? "true" : "false");
    if (is_locked_) {
        oss << ",\"userName\":\"" << lock_user_ << "\"";
        oss << ",\"roleName\":\"" << lock_role_ << "\"";
        oss << ",\"roleLevel\":" << lock_role_level_;
    }
    oss << "}";
    return Tango::string_dup(oss.str().c_str());
}

void LargeStrokeDevice::devUserConfig() {
    // ALL_STATES: 所有状态可用，无需检查
    log_event("User config updated");
}

// ========== System Commands ==========
void LargeStrokeDevice::selfCheck() {
    INFO_STREAM << "[DEBUG] selfCheck() called" << std::endl;
    check_state_not_on("selfCheck");  // UNKNOWN/OFF/FAULT可用，ON状态不可用
    log_event("Self check started");
    try {
        // 检查电机控制器
        auto motion = get_motion_controller_proxy();
        if (!sim_mode_ && motion) {
            INFO_STREAM << "[DEBUG] selfCheck: checking motion controller..." << std::endl;
            try {
                int original_timeout = motion->get_timeout_millis();
                motion->set_timeout_millis(500);
                motion->ping();
                motion->set_timeout_millis(original_timeout);
                INFO_STREAM << "[DEBUG] selfCheck: motion controller ping OK" << std::endl;
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] selfCheck: motion controller ping failed - " 
                            << e.errors[0].desc << std::endl;
                self_check_result_ = 1;  // 1=电机自检异常
                alarm_state_ = "Self check failed: Motor controller not responding";
                log_event("Self check failed: motor error");
                return;
            } catch (...) {
                ERROR_STREAM << "[DEBUG] selfCheck: motion controller ping failed with unknown exception" << std::endl;
                self_check_result_ = 1;
                alarm_state_ = "Self check failed: Motor controller not responding";
                log_event("Self check failed: motor error");
                return;
            }
        } else if (sim_mode_) {
            INFO_STREAM << "[DEBUG] selfCheck: SIM MODE - skipping motion controller check" << std::endl;
        } else {
            WARN_STREAM << "[DEBUG] selfCheck: motion_controller_proxy_ is NULL" << std::endl;
        }
        
        // 检查编码器连接 (如果有相机则检查相机，这里以编码器代替)
        auto enc = get_encoder_proxy();
        if (!sim_mode_ && enc) {
            INFO_STREAM << "[DEBUG] selfCheck: checking encoder..." << std::endl;
            try {
                int original_timeout = enc->get_timeout_millis();
                enc->set_timeout_millis(500);
                enc->ping();
                enc->set_timeout_millis(original_timeout);
                INFO_STREAM << "[DEBUG] selfCheck: encoder ping OK" << std::endl;
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] selfCheck: encoder ping failed - " << e.errors[0].desc << std::endl;
                self_check_result_ = 2;  // 2=相机/编码器自检异常
                alarm_state_ = "Self check failed: Encoder not responding";
                log_event("Self check failed: encoder error");
                return;
            } catch (...) {
                ERROR_STREAM << "[DEBUG] selfCheck: encoder ping failed with unknown exception" << std::endl;
                self_check_result_ = 2;
                alarm_state_ = "Self check failed: Encoder not responding";
                log_event("Self check failed: encoder error");
                return;
            }
        } else if (sim_mode_) {
            INFO_STREAM << "[DEBUG] selfCheck: SIM MODE - skipping encoder check" << std::endl;
        } else {
            WARN_STREAM << "[DEBUG] selfCheck: encoder_proxy_ is NULL" << std::endl;
        }
        
        // 可以添加光源检查逻辑，返回3表示光源异常
        // 其他检查可以返回4表示其他异常
        
        INFO_STREAM << "[DEBUG] selfCheck: calling specific_self_check()..." << std::endl;
        specific_self_check();
        self_check_result_ = 0;  // 0=自检正常
        alarm_state_.clear();
        INFO_STREAM << "[DEBUG] selfCheck: PASSED" << std::endl;
        log_event("Self check passed");
    } catch (std::exception &e) {
        ERROR_STREAM << "[DEBUG] selfCheck: FAILED with exception - " << e.what() << std::endl;
        self_check_result_ = 4;  // 4=其他异常
        alarm_state_ = std::string("Self check failed: ") + e.what();
        log_event("Self check failed: other error");
    }
}

void LargeStrokeDevice::init() {
    INFO_STREAM << "[DEBUG] init() called, current_state=" << Tango::DevStateName[get_state()] 
                << ", sim_mode=" << (sim_mode_ ? "true" : "false") << std::endl;
    check_state_not_on("init");  // UNKNOWN/OFF/FAULT可用，ON状态不可用
    log_event("Initialization started");
    
    INFO_STREAM << "[DEBUG] init: connecting proxies (sim_mode=" << sim_mode_ << ")..." << std::endl;
    connect_proxies();
    
    INFO_STREAM << "[DEBUG] init: resetting positions and state..." << std::endl;
    large_range_pos_ = 0.0;
    dire_pos_ = 0.0;
    self_check_result_ = -1;
    result_value_ = 0;
    set_state(Tango::ON);
    
    // 确保状态字符串与 sim_mode_ 一致（connect_proxies 已经设置了，这里再次确认）
    if (sim_mode_) {
        set_status("Simulation Mode");
        INFO_STREAM << "[DEBUG] init: Final status set to 'Simulation Mode' (sim_mode=true)" << std::endl;
    } else {
        // 如果不在模拟模式，但 connect_proxies 可能因为连接失败而设置了其他状态
        // 这里确保状态字符串正确反映当前的 sim_mode_
        std::string current_status = get_status();
        if (current_status != "Connected" && current_status != "Proxy connection failed") {
            set_status("Connected");  // 如果连接成功，应该是 Connected
        }
        INFO_STREAM << "[DEBUG] init: Final status = '" << get_status() << "' (sim_mode=false)" << std::endl;
    }
    
    INFO_STREAM << "[DEBUG] init: initialization completed, state set to ON, sim_mode=" << sim_mode_ << std::endl;
    log_event("Initialization completed");
}

// ========== Parameter Commands ==========
void LargeStrokeDevice::moveAxisSet(const Tango::DevVarDoubleArray *params) {
    check_state_for_command("moveAxisSet", false, false);  // OFF/ON/FAULT可用
    if (params->length() < 5) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Need 5 values: startSpeed, maxSpeed, accTime, decTime, stopSpeed", 
            "LargeStrokeDevice::moveAxisSet");
    }
    log_event("Motion parameters set");
    
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && motion) {
        try {
            Tango::DeviceData data_in;
            data_in << *params;
            motion->command_inout("setMoveParameter", data_in);
        } catch (Tango::DevFailed &e) {
            Tango::Except::re_throw_exception(e, "API_ProxyError", 
                "Failed to set motion parameters", "LargeStrokeDevice::moveAxisSet");
        }
    }
    result_value_ = 0;
}

void LargeStrokeDevice::structAxisSet(const Tango::DevVarDoubleArray *params) {
    check_state_for_command("structAxisSet", false, false);  // OFF/ON/FAULT可用
    if (params->length() < 3) {
        Tango::Except::throw_exception("API_InvalidArgument", 
            "Need 3 values: stepAngle, gearRatio, subdivision", 
            "LargeStrokeDevice::structAxisSet");
    }
    log_event("Structure parameters set");
    
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && motion) {
        try {
            Tango::DeviceData data_in;
            data_in << *params;
            motion->command_inout("setStructParameter", data_in);
        } catch (Tango::DevFailed &e) {
            Tango::Except::re_throw_exception(e, "API_ProxyError", 
                "Failed to set structure parameters", "LargeStrokeDevice::structAxisSet");
        }
    }
    result_value_ = 0;
}

// ========== Motion Commands ==========
void LargeStrokeDevice::moveRelative(Tango::DevDouble distance) {
    INFO_STREAM << "[DEBUG] moveRelative() called with distance=" << distance 
                << " " << position_unit_ << ", current_state=" << Tango::DevStateName[get_state()] 
                << ", sim_mode=" << (sim_mode_ ? "true" : "false") << std::endl;
    check_state_for_command("moveRelative", true, false);  // 仅ON状态可用
    
    // 快速连接检查：在命令执行前快速验证连接，避免等待超时
    if (!quick_check_connection()) {
        ERROR_STREAM << "[DEBUG] moveRelative: Connection check failed" << std::endl;
        Tango::Except::throw_exception("API_ProxyError",
            "Motion controller not connected. Cannot execute moveRelative in real mode.",
            "LargeStrokeDevice::moveRelative");
    }
    
    // 单位转换：将用户输入转换为步数
    double distance_in_steps = convert_to_steps(distance);
    INFO_STREAM << "[DEBUG] moveRelative: converted " << distance << " " << position_unit_ 
                << " to " << distance_in_steps << " steps" << std::endl;
    log_event("Relative move: " + std::to_string(distance) + " " + position_unit_ + 
              " (" + std::to_string(distance_in_steps) + " steps)");
    
    // 运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before motion" << std::endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before motion, continuing anyway" << std::endl;
        }
    }
    
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] moveRelative: SIM MODE - updating position (current=" 
                    << large_range_pos_ << ", adding=" << distance_in_steps << ")" << std::endl;
        dire_pos_ += distance_in_steps;
        large_range_pos_ += distance_in_steps;
        large_lim_org_state_ = 2;  // Not at origin
        large_range_state_ = false;
        result_value_ = 0;
        INFO_STREAM << "[DEBUG] moveRelative: SIM MODE - new position=" << large_range_pos_ << std::endl;
        return;  // 模拟模式下直接返回，不执行后续代码
    }
    
    // 真实模式：必须使用运动控制器（此时已经通过 quick_check_connection 验证）
    auto motion = get_motion_controller_proxy();
    if (!motion) {
        ERROR_STREAM << "[DEBUG] moveRelative: motion_controller_proxy_ is NULL in real mode!" << std::endl;
        Tango::Except::throw_exception("API_ProxyError",
            "Motion controller not connected. Cannot execute moveRelative in real mode.",
            "LargeStrokeDevice::moveRelative");
    }
    
    try {
        Tango::DevVarDoubleArray move_params;
        move_params.length(2);
        move_params[0] = static_cast<double>(axis_id_);
        move_params[1] = distance_in_steps;  // 发送转换后的步数
        
        INFO_STREAM << "[DEBUG] moveRelative: sending to motion controller (axis=" 
                    << axis_id_ << ", distance=" << distance_in_steps << " steps)" << std::endl;
        
        Tango::DeviceData data_in;
        data_in << move_params;
        // 对运动控制器命令使用较短超时：控制指令应快速返回；断连时快速失败避免长时间阻塞
        int original_timeout = motion->get_timeout_millis();
        motion->set_timeout_millis(800);
        try {
            motion->command_inout("moveRelative", data_in);
        } catch (...) {
            motion->set_timeout_millis(original_timeout);
            throw;
        }
        motion->set_timeout_millis(original_timeout);
        large_range_state_ = true;
        set_state(Tango::MOVING);
        INFO_STREAM << "[DEBUG] moveRelative: command sent successfully, state set to MOVING" << std::endl;
    } catch (Tango::DevFailed &e) {
        result_value_ = 1;
        ERROR_STREAM << "[DEBUG] moveRelative: command failed - " << e.errors[0].desc << std::endl;
        // 通信失败后立即标记连接不健康，后续命令可快速返回，避免客户端堆积/延时爆窗
        connection_healthy_.store(false);
        reset_motion_controller_proxy();
        set_state(Tango::FAULT);
        set_status("Motion controller communication failed");
        for (unsigned int i = 0; i < e.errors.length(); i++) {
            ERROR_STREAM << "[DEBUG]   Error " << i << ": " << e.errors[i].origin << " - " 
                        << e.errors[i].desc << std::endl;
        }
        Tango::Except::re_throw_exception(e, "API_ProxyError", 
            "Failed to move relative", "LargeStrokeDevice::moveRelative");
    }
}

void LargeStrokeDevice::moveAbsolute(Tango::DevDouble position) {
    INFO_STREAM << "[DEBUG] moveAbsolute() called with position=" << position 
                << " " << position_unit_ << ", current_state=" << Tango::DevStateName[get_state()] 
                << ", sim_mode=" << (sim_mode_ ? "true" : "false")
                << ", current_pos=" << large_range_pos_ << std::endl;
    check_state_for_command("moveAbsolute", true, false);  // 仅ON状态可用
    
    // 快速连接检查：在命令执行前快速验证连接，避免等待超时
    if (!quick_check_connection()) {
        ERROR_STREAM << "[DEBUG] moveAbsolute: Connection check failed" << std::endl;
        Tango::Except::throw_exception("API_ProxyError",
            "Motion controller not connected. Cannot execute moveAbsolute in real mode.",
            "LargeStrokeDevice::moveAbsolute");
    }
    
    // 单位转换：将用户输入转换为步数
    double position_in_steps = convert_to_steps(position);
    INFO_STREAM << "[DEBUG] moveAbsolute: converted " << position << " " << position_unit_ 
                << " to " << position_in_steps << " steps" << std::endl;
    log_event("Absolute move: " + std::to_string(position) + " " + position_unit_ + 
              " (" + std::to_string(position_in_steps) + " steps)");
    
    // 运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before motion" << std::endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before motion, continuing anyway" << std::endl;
        }
    }
    
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] moveAbsolute: SIM MODE - setting position from " 
                    << large_range_pos_ << " to " << position_in_steps << std::endl;
        dire_pos_ = position_in_steps;
        large_range_pos_ = position_in_steps;
        large_lim_org_state_ = (position_in_steps == 0.0) ? 0 : 2;
        large_range_state_ = false;
        result_value_ = 0;
        INFO_STREAM << "[DEBUG] moveAbsolute: SIM MODE - position set to " << large_range_pos_ << std::endl;
        return;  // 模拟模式下直接返回，不执行后续代码
    }
    
    // 真实模式：必须使用运动控制器（此时已经通过 quick_check_connection 验证）
    auto motion = get_motion_controller_proxy();
    if (!motion) {
        ERROR_STREAM << "[DEBUG] moveAbsolute: motion_controller_proxy_ is NULL in real mode!" << std::endl;
        Tango::Except::throw_exception("API_ProxyError",
            "Motion controller not connected. Cannot execute moveAbsolute in real mode.",
            "LargeStrokeDevice::moveAbsolute");
    }
    
    try {
        Tango::DevVarDoubleArray move_params;
        move_params.length(2);
        move_params[0] = static_cast<double>(axis_id_);
        move_params[1] = position_in_steps;  // 发送转换后的步数
        
        INFO_STREAM << "[DEBUG] moveAbsolute: sending to motion controller (axis=" 
                    << axis_id_ << ", target_position=" << position_in_steps << " steps)" << std::endl;
        
        Tango::DeviceData data_in;
        data_in << move_params;
        // 对运动控制器命令使用较短超时：控制指令应快速返回；断连时快速失败避免长时间阻塞
        int original_timeout = motion->get_timeout_millis();
        motion->set_timeout_millis(800);
        try {
            motion->command_inout("moveAbsolute", data_in);
        } catch (...) {
            motion->set_timeout_millis(original_timeout);
            throw;
        }
        motion->set_timeout_millis(original_timeout);
        large_range_state_ = true;
        set_state(Tango::MOVING);
        INFO_STREAM << "[DEBUG] moveAbsolute: command sent successfully, state set to MOVING" << std::endl;
    } catch (Tango::DevFailed &e) {
        result_value_ = 1;
        ERROR_STREAM << "[DEBUG] moveAbsolute: command failed - " << e.errors[0].desc << std::endl;
        // 通信失败后立即标记连接不健康，后续命令可快速返回，避免客户端堆积/延时爆窗
        connection_healthy_.store(false);
        reset_motion_controller_proxy();
        set_state(Tango::FAULT);
        set_status("Motion controller communication failed");
        for (unsigned int i = 0; i < e.errors.length(); i++) {
            ERROR_STREAM << "[DEBUG]   Error " << i << ": " << e.errors[i].origin << " - " 
                        << e.errors[i].desc << std::endl;
        }
        Tango::Except::re_throw_exception(e, "API_ProxyError", 
            "Failed to move absolute", "LargeStrokeDevice::moveAbsolute");
    }
}

void LargeStrokeDevice::stop() {
    INFO_STREAM << "[DEBUG] stop() called, current_state=" << Tango::DevStateName[get_state()] << std::endl;
    check_state_for_command("stop", false, false);  // OFF/ON/FAULT可用
    log_event("Stop");
    
    if (!sim_mode_) {
        // 快速连接检查：如果连接失败，立即返回错误（避免等待超时）
        // 注意：stop 命令允许在 FAULT 状态下执行，但如果连接失败，仍然需要快速返回
        if (get_state() != Tango::FAULT) {
            // 只有在非 FAULT 状态下才进行快速连接检查
            if (!quick_check_connection()) {
                ERROR_STREAM << "[DEBUG] stop: Connection check failed" << std::endl;
                result_value_ = 1;
                Tango::Except::throw_exception("API_ProxyError",
                    "Motion controller not connected. Cannot stop in real mode.",
                    "LargeStrokeDevice::stop");
            }
        }
        
        // 真实模式下，必须有控制器连接
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            ERROR_STREAM << "[DEBUG] stop: motion_controller_proxy_ is NULL in real mode!" << std::endl;
            result_value_ = 1;
            Tango::Except::throw_exception("API_ProxyError",
                "Motion controller not connected. Cannot stop in real mode.",
                "LargeStrokeDevice::stop");
        }
        try {
            INFO_STREAM << "[DEBUG] stop: sending stopMove to motion controller for axis " << axis_id_ << std::endl;
            Tango::DeviceData data_in;
            data_in << static_cast<Tango::DevShort>(axis_id_);
            int original_timeout = motion->get_timeout_millis();
            motion->set_timeout_millis(300);
            try {
                motion->command_inout("stopMove", data_in);
            } catch (...) {
                motion->set_timeout_millis(original_timeout);
                throw;
            }
            motion->set_timeout_millis(original_timeout);
            INFO_STREAM << "[DEBUG] stop: stopMove command sent successfully" << std::endl;
        } catch (Tango::DevFailed &e) {
            ERROR_STREAM << "[DEBUG] stop: stopMove command failed - " << e.errors[0].desc << std::endl;
            result_value_ = 1;
            connection_healthy_.store(false);
            reset_motion_controller_proxy();
            Tango::Except::re_throw_exception(e, "API_ProxyError",
                "Failed to stop motion", "LargeStrokeDevice::stop");
        }
    } else {
        INFO_STREAM << "[DEBUG] stop: SIM MODE - stopping motion" << std::endl;
    }
    
    // 始终清除运动状态标志并设置结果值
    large_range_state_ = false;
    if (limit_fault_latched_.load()) {
        set_state(Tango::FAULT);
    } else if (get_state() == Tango::FAULT) {
        // stop 允许在 FAULT 下执行，但不应无条件清除 FAULT
        set_state(Tango::FAULT);
    } else {
        set_state(Tango::ON);
    }
    result_value_ = 0;
    INFO_STREAM << "[DEBUG] stop: state=" << Tango::DevStateName[get_state()] << ", result_value=0" << std::endl;
    
    // 注意：正常停止后不自动启用刹车，保持刹车释放状态以便快速继续运动
    // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
}

void LargeStrokeDevice::reset() {
    INFO_STREAM << "[DEBUG] reset() called, current_state=" << Tango::DevStateName[get_state()] << std::endl;
    check_state_for_command("reset", false, false);  // OFF/ON/FAULT可用
    log_event("Reset");
    
    // 复位前自动启用刹车（安全保护）
    if (brake_power_port_ >= 0 && brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-engaging brake before reset (safety)" << std::endl;
        if (!engage_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to engage brake before reset" << std::endl;
        }
    }
    
    Common::StandardSystemDevice::reset();
    
    if (!sim_mode_) {
        // 快速连接检查：如果连接失败，立即返回错误（避免等待超时）
        // 注意：reset 命令允许在 FAULT 状态下执行，但如果连接失败，仍然需要快速返回
        if (get_state() != Tango::FAULT) {
            // 只有在非 FAULT 状态下才进行快速连接检查
            if (!quick_check_connection()) {
                ERROR_STREAM << "[DEBUG] reset: Connection check failed" << std::endl;
                result_value_ = 1;
                Tango::Except::throw_exception("API_ProxyError",
                    "Motion controller not connected. Cannot reset in real mode.",
                    "LargeStrokeDevice::reset");
            }
        }
        
        auto motion = get_motion_controller_proxy();
        if (!motion) {
            ERROR_STREAM << "[DEBUG] reset: motion_controller_proxy_ is NULL in real mode!" << std::endl;
            result_value_ = 1;
            Tango::Except::throw_exception("API_ProxyError",
                "Motion controller not connected. Cannot reset in real mode.",
                "LargeStrokeDevice::reset");
        }
        try {
            // 运动控制器的 reset 命令需要传递 axis_id 参数
            INFO_STREAM << "[DEBUG] reset: sending reset to motion controller for axis " << axis_id_ << std::endl;
            Tango::DeviceData data_in;
            data_in << static_cast<Tango::DevShort>(axis_id_);
            int original_timeout = motion->get_timeout_millis();
            motion->set_timeout_millis(500);
            try {
                motion->command_inout("reset", data_in);
            } catch (...) {
                motion->set_timeout_millis(original_timeout);
                throw;
            }
            motion->set_timeout_millis(original_timeout);
            INFO_STREAM << "[DEBUG] reset: reset command sent successfully" << std::endl;
        } catch (Tango::DevFailed &e) {
            ERROR_STREAM << "[DEBUG] reset: reset command failed - " << e.errors[0].desc << std::endl;
            result_value_ = 1;
            connection_healthy_.store(false);
            reset_motion_controller_proxy();
            Tango::Except::re_throw_exception(e, "API_ProxyError",
                "Failed to reset motor", "LargeStrokeDevice::reset");
        }
    } else {
        INFO_STREAM << "[DEBUG] reset: SIM MODE - skipping motor reset" << std::endl;
    }
    
    alarm_state_.clear();
    limit_fault_latched_.store(false);
    limit_fault_el_state_.store(0);
    result_value_ = 0;
    set_state(Tango::ON);
    if (sim_mode_) {
        set_status("Simulation Mode");
    } else {
        set_status("Connected");
    }
    INFO_STREAM << "[DEBUG] reset: completed, state set to ON" << std::endl;
}

// ========== Read Commands ==========
Tango::DevDouble LargeStrokeDevice::readEncoder() {
    INFO_STREAM << "[DEBUG] readEncoder() called" << std::endl;
    check_state_for_command("readEncoder", false, false);  // OFF/ON/FAULT可用
    update_position();
    INFO_STREAM << "[DEBUG] readEncoder: returning position=" << large_range_pos_ << " steps" << std::endl;
    return large_range_pos_;
}

Tango::DevBoolean LargeStrokeDevice::readOrg() {
    check_state_for_command("readOrg", false, false);  // OFF/ON/FAULT可用
    return (large_lim_org_state_ == 0);
}

Tango::DevShort LargeStrokeDevice::readEL() {
    check_state_for_command("readEL", false, false);  // OFF/ON/FAULT可用
    // Return: 0=none, 1=EL+, 2=EL-
    if (large_lim_org_state_ == 1) return 1;
    if (large_lim_org_state_ == -1) return 2;
    return 0;
}

// ========== Auto/Valve Commands ==========
void LargeStrokeDevice::largeMoveAuto() {
    check_state_for_command("largeMoveAuto", true, false);  // 仅ON状态可用
    log_event("Auto move started");
    // Move to predefined position
    if (sim_mode_) {
        dire_pos_ = 0.0;
        large_range_pos_ = 0.0;
        large_lim_org_state_ = 0;
        result_value_ = 0;
    }
    log_event("Auto move completed");
}

void LargeStrokeDevice::openValue(Tango::DevShort state) {
    check_state_for_command("openValue", true, false);  // 仅ON状态可用
    log_event(state == 0 ? "Valve opening" : "Valve closing");
    
    if (sim_mode_) {
        host_plug_state_ = (state == 0) ? "OPENED" : "CLOSED";
        result_value_ = 0;
    }
}

Tango::DevBoolean LargeStrokeDevice::plugInRead() {
    check_state_for_command("plugInRead", false, false);  // OFF/ON/FAULT可用
    return (host_plug_state_ == "OPENED" || host_plug_state_ == "CLOSED");
}

void LargeStrokeDevice::runAction(Tango::DevShort action) {
    check_state_for_command("runAction", true, false);  // 仅ON状态可用
    log_event(action == 0 ? "Action allowed" : "Action not allowed");
    result_value_ = 0;
}

// ========== Export Commands ==========
Tango::DevString LargeStrokeDevice::readtAxis() {
    check_state_for_command("readtAxis", false, false);  // OFF/ON/FAULT可用
    std::ostringstream oss;
    oss << "{\"Axis" << axis_id_ << "\":{";
    oss << "\"StepAngle\":\"1.8\",";
    oss << "\"GearRatio\":\"1.0\",";
    oss << "\"Subdivision\":\"16\",";
    oss << "\"StartSpeed\":\"100\",";
    oss << "\"MaxSpeed\":\"1000\",";
    oss << "\"AccelerationTime\":\"0.1\",";
    oss << "\"DecelerationTime\":\"0.1\",";
    oss << "\"StopSpeed\":\"50\",";
    oss << "\"ChannelNumber\":\"" << axis_id_ << "\"";
    oss << "}}";
    axis_parameter_ = oss.str();
    return Tango::string_dup(axis_parameter_.c_str());
}

void LargeStrokeDevice::exportLogs() {
    check_state_for_command("exportLogs", false, false);  // OFF/ON/FAULT可用
    log_event("Logs exported");
    
    // 导出日志到本地文件
    try {
        auto now = std::time(nullptr);
        std::ostringstream filename;
        filename << "linear_logs_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".json";
        
        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << linear_logs_;
            file.close();
            result_value_ = 0;
        } else {
            result_value_ = 1;
            Tango::Except::throw_exception("API_FileError",
                "Failed to create log file", "LargeStrokeDevice::exportLogs");
        }
    } catch (std::exception& e) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_ExportError",
            std::string("Failed to export logs: ") + e.what(), "LargeStrokeDevice::exportLogs");
    }
}

void LargeStrokeDevice::exportAxis() {
    check_state_for_command("exportAxis", false, false);  // OFF/ON/FAULT可用
    log_event("Parameters exported to file");
    
    // 导出参数到本地文件
    try {
        auto now = std::time(nullptr);
        std::ostringstream filename;
        filename << "axis_params_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".json";
        
        std::ofstream file(filename.str());
        if (file.is_open()) {
            readtAxis();  // 更新axis_parameter_
            file << axis_parameter_;
            file.close();
            result_value_ = 0;
        } else {
            result_value_ = 1;
            Tango::Except::throw_exception("API_FileError",
                "Failed to create parameter file", "LargeStrokeDevice::exportAxis");
        }
    } catch (std::exception& e) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_ExportError",
            std::string("Failed to export parameters: ") + e.what(), "LargeStrokeDevice::exportAxis");
    }
}

void LargeStrokeDevice::simSwitch(Tango::DevShort mode) {
    check_state_for_command("simSwitch", false, false);  // OFF/ON/FAULT可用
    bool was_sim_mode = sim_mode_;
    sim_mode_ = (mode != 0);
    
    // 注意：运行时切换只影响当前会话，server 重启后恢复配置文件的值（不持久化）
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] simSwitch: Enabling SIMULATION MODE (运行时切换，不持久化)" << std::endl;
        set_status("Simulation Mode");
        log_event("Simulation mode enabled (runtime switch)");
    } else {
        INFO_STREAM << "[DEBUG] simSwitch: Disabling simulation mode, switching to REAL MODE (运行时切换，不持久化)" << std::endl;
        set_status("Simulation Mode OFF");
        log_event("Simulation mode disabled (runtime switch)");
        
        // 如果从模拟模式切换到真实模式，且代理未连接，尝试连接代理
        if (was_sim_mode && (!get_motion_controller_proxy() || !get_encoder_proxy())) {
            INFO_STREAM << "[DEBUG] simSwitch: Switching from sim mode to real mode, connecting proxies..." << std::endl;
            try {
                connect_proxies();
                INFO_STREAM << "[DEBUG] simSwitch: Proxies connected successfully" << std::endl;
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] simSwitch: Failed to connect proxies: " << e.errors[0].desc << std::endl;
                // 不抛出异常，允许继续运行，但会有警告
            } catch (...) {
                ERROR_STREAM << "[DEBUG] simSwitch: Failed to connect proxies with unknown exception" << std::endl;
            }
        }
    }
}

// ========== Attribute Reads ==========
void LargeStrokeDevice::read_attr(Tango::Attribute &attr) {
    std::string attr_name = attr.get_name();
    
    if (attr_name == "selfCheckResult") read_self_check_result(attr);
    else if (attr_name == "positionUnit") read_position_unit(attr);
    else if (attr_name == "groupAttributeJson") read_group_attribute_json(attr);
    else if (attr_name == "hostPlugState") read_host_plug_state(attr);
    else if (attr_name == "largeRangePos") read_large_range_pos(attr);
    else if (attr_name == "direPos") read_dire_pos(attr);
    else if (attr_name == "LinearLogs") read_linear_logs(attr);
    else if (attr_name == "alarmState") read_alarm_state(attr);
    else if (attr_name == "axisParameter") read_axis_parameter(attr);
    else if (attr_name == "LargeLimOrgState") read_large_lim_org_state(attr);
    else if (attr_name == "LargeRangeState") read_large_range_state(attr);
    else if (attr_name == "resultValue") read_result_value(attr);
    else if (attr_name == "driverPowerStatus") read_driver_power_status(attr);
    else if (attr_name == "brakeStatus") read_brake_status(attr);
}

void LargeStrokeDevice::read_driver_power_status(Tango::Attribute &attr) {
    // 直接使用成员变量地址，确保指针在 Tango 读取时仍然有效
    attr.set_value(&driver_power_enabled_);
}

void LargeStrokeDevice::read_brake_status(Tango::Attribute &attr) {
    // 直接使用成员变量地址，确保指针在 Tango 读取时仍然有效
    attr.set_value(&brake_released_);
}

// 处理可写属性的写操作
void LargeStrokeDevice::write_attr(Tango::WAttribute &attr) {
    std::string attr_name = attr.get_name();
    
    if (attr_name == "positionUnit") {
        write_position_unit(attr);
    }
}

void LargeStrokeDevice::read_self_check_result(Tango::Attribute &attr) {
    attr_self_check_result_read = self_check_result_;
    attr.set_value(&attr_self_check_result_read);
}

void LargeStrokeDevice::read_position_unit(Tango::Attribute &attr) {
    attr_position_unit_read = Tango::string_dup(position_unit_.c_str());
    attr.set_value(&attr_position_unit_read);
}

void LargeStrokeDevice::write_position_unit(Tango::WAttribute &attr) {
    Tango::DevString new_unit;
    attr.get_write_value(new_unit);
    std::string unit_str(new_unit);
    
    if (unit_str != "step" && unit_str != "mm" && unit_str != "um" && 
        unit_str != "rad" && unit_str != "urad" && unit_str != "mrad") {
        Tango::Except::throw_exception("API_InvalidValue", 
            "Invalid position unit", "LargeStrokeDevice::write_position_unit");
    }
    position_unit_ = unit_str;
}

void LargeStrokeDevice::read_group_attribute_json(Tango::Attribute &attr) {
    // 组合所有关键属性用于一次性获取
    std::ostringstream oss;
    oss << "{";
    oss << "\"State\":\"" << Tango::DevStateName[get_state()] << "\"";
    oss << ",\"largeRangePos\":" << large_range_pos_;
    oss << ",\"direPos\":" << dire_pos_;
    oss << ",\"LargeRangeState\":" << (large_range_state_ ? "true" : "false");
    oss << ",\"hostPlugState\":\"" << host_plug_state_ << "\"";
    oss << ",\"LargeLimOrgState\":" << large_lim_org_state_;
    oss << ",\"selfCheckResult\":" << self_check_result_;
    oss << ",\"resultValue\":" << result_value_;
    oss << ",\"positionUnit\":\"" << position_unit_ << "\"";
    if (!alarm_state_.empty()) {
        oss << ",\"alarmState\":\"" << alarm_state_ << "\"";
    }
    oss << "}";
    std::string json = oss.str();
    attr_group_attribute_json_read = Tango::string_dup(json.c_str());
    attr.set_value(&attr_group_attribute_json_read);
}

void LargeStrokeDevice::read_host_plug_state(Tango::Attribute &attr) {
    attr_host_plug_state_read = Tango::string_dup(host_plug_state_.c_str());
    attr.set_value(&attr_host_plug_state_read);
}

void LargeStrokeDevice::read_large_range_pos(Tango::Attribute &attr) {
    update_position();
    // 转换为用户设置的单位（用于位置反馈）
    // 设备内部存储为步数，需要根据positionUnit转换为用户单位
    attr_large_range_pos_read = convert_from_steps(large_range_pos_);
    attr.set_value(&attr_large_range_pos_read);
}

void LargeStrokeDevice::read_dire_pos(Tango::Attribute &attr) {
    attr_dire_pos_read = dire_pos_;
    attr.set_value(&attr_dire_pos_read);
}

void LargeStrokeDevice::read_linear_logs(Tango::Attribute &attr) {
    attr_linear_logs_read = Tango::string_dup(linear_logs_.c_str());
    attr.set_value(&attr_linear_logs_read);
}

void LargeStrokeDevice::read_alarm_state(Tango::Attribute &attr) {
    attr_alarm_state_read = Tango::string_dup(alarm_state_.c_str());
    attr.set_value(&attr_alarm_state_read);
}

void LargeStrokeDevice::read_axis_parameter(Tango::Attribute &attr) {
    readtAxis();
    attr_axis_parameter_read = Tango::string_dup(axis_parameter_.c_str());
    attr.set_value(&attr_axis_parameter_read);
}

void LargeStrokeDevice::read_large_lim_org_state(Tango::Attribute &attr) {
    attr_large_lim_org_state_read = large_lim_org_state_;
    attr.set_value(&attr_large_lim_org_state_read);
}

void LargeStrokeDevice::read_large_range_state(Tango::Attribute &attr) {
    attr_large_range_state_read = large_range_state_;
    attr.set_value(&attr_large_range_state_read);
}

void LargeStrokeDevice::read_result_value(Tango::Attribute &attr) {
    attr_result_value_read = result_value_;
    attr.set_value(&attr_result_value_read);
}

// ========== Hooks and Helpers ==========
void LargeStrokeDevice::specific_self_check() {
    if (!sim_mode_ && !get_motion_controller_proxy()) {
        throw std::runtime_error("Motion Controller Proxy not initialized");
    }
    auto motion = get_motion_controller_proxy();
    if (!sim_mode_ && motion) {
        int original_timeout = motion->get_timeout_millis();
        motion->set_timeout_millis(500);
        motion->ping();
        motion->set_timeout_millis(original_timeout);
    }
}

void LargeStrokeDevice::always_executed_hook() {
    Common::StandardSystemDevice::always_executed_hook();

    // 模拟模式：连接始终健康
    if (sim_mode_) {
        connection_healthy_.store(true);
        return;
    }

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
    // 这里保持“零等待”，只根据后台线程更新的 connection_healthy_ 更新设备状态文本。
    if (!connection_healthy_.load() && get_state() == Tango::ON) {
        set_state(Tango::FAULT);
        set_status("Network connection lost");
    }
}

void LargeStrokeDevice::read_attr_hardware(std::vector<long> &) {
    auto now = std::chrono::steady_clock::now();
    
    // 周期性报警检测 - 每30ms
    auto alarm_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_alarm_check_).count();
    if (alarm_elapsed >= 30) {
        last_alarm_check_ = now;
        // 检查报警状态逻辑可以在这里实现
        // 例如检查限位开关、过载等
    }
    
    // 周期性状态检测 - 每1000ms
    auto state_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_state_check_).count();
    if (state_elapsed >= 1000) {
        last_state_check_ = now;

        // 连接维护由独立后台线程负责；此处不做 ping/重连，避免属性路径阻塞
        
        auto motion = get_motion_controller_proxy();
        if (!sim_mode_ && motion && connection_healthy_.load()) {
            try {
                // 检测限位触发：读取限位状态
                if (brake_power_port_ >= 0 && brake_released_ && get_state() == Tango::MOVING) {
                    try {
                        Tango::DeviceData data_in;
                        data_in << static_cast<Tango::DevShort>(axis_id_);
                        int original_timeout = motion->get_timeout_millis();
                        motion->set_timeout_millis(300);
                        Tango::DeviceData data_out;
                        try {
                            data_out = motion->command_inout("readEL", data_in);
                        } catch (...) {
                            motion->set_timeout_millis(original_timeout);
                            throw;
                        }
                        motion->set_timeout_millis(original_timeout);
                        Tango::DevShort el_state_raw;
                        data_out >> el_state_raw;
                        
                        // 限位开关低电平有效：硬件读取0（低电平）表示触发，1（高电平）表示未触发
                        // 运动控制器当前逻辑：pos_limit==1返回1(EL+), neg_limit==1返回-1(EL-), 否则返回0
                        // 如果限位开关是低电平有效，需要反转：当硬件读取0时应该返回限位触发
                        // 由于运动控制器返回0时无法区分方向，我们在large_stroke_device中反转逻辑
                        // 反转规则：当readEL返回0时，认为是限位触发（统一处理为EL+）
                        //          当readEL返回非0时，认为是未触发（转换为0）
                        Tango::DevShort el_state = 0;
                        if (el_state_raw == 0) {
                            // 低电平有效：返回0表示限位触发（但无法区分方向，统一处理为EL+）
                            el_state = 1;  // 转换为EL+表示限位触发
                        }
                        // 如果返回非0（1或-1），说明硬件读取是高电平，限位未触发，el_state保持为0
                        
                        // readEL返回: 0=none, 1=EL+, -1=EL- (反转后)
                        if (el_state != 0) {
                            // 锁存限位故障，避免后续状态同步覆盖为 ON
                            if (!limit_fault_latched_.load()) {
                                limit_fault_latched_.store(true);
                                limit_fault_el_state_.store(el_state);
                                std::string direction = (el_state > 0) ? "EL+" : "EL-";
                                alarm_state_ = "Limit switch triggered: " + direction;
                                set_status(alarm_state_);
                                set_state(Tango::FAULT);
                                large_range_state_ = false;
                                log_event(alarm_state_);
                            }

                            // 同步 LargeLimOrgState（仅反映限位触发）
                            large_lim_org_state_ = el_state;
                            INFO_STREAM << "[BrakeControl] Limit switch triggered on axis " << axis_id_ 
                                       << " (el_state_raw=" << el_state_raw << ", inverted to " << el_state << ")" << std::endl;
                            INFO_STREAM << "[BrakeControl] Limit triggered, auto-engaging brake (safety)" << std::endl;
                            if (!engage_brake()) {
                                WARN_STREAM << "[BrakeControl] Failed to engage brake on limit trigger" << std::endl;
                            }
                            // 停止运动
                            try {
                                Tango::DeviceData stop_data;
                                stop_data << static_cast<Tango::DevShort>(axis_id_);
                                int stop_original_timeout = motion->get_timeout_millis();
                                motion->set_timeout_millis(300);
                                try {
                                    motion->command_inout("stopMove", stop_data);
                                } catch (...) {
                                    motion->set_timeout_millis(stop_original_timeout);
                                    throw;
                                }
                                motion->set_timeout_millis(stop_original_timeout);
                            } catch (...) {
                                // 忽略停止失败
                            }
                        } else {
                            // 未触发限位时，若之前处于限位态值，恢复为"无"（2=not at origin/limit）
                            if (large_lim_org_state_ == 1 || large_lim_org_state_ == -1) {
                                large_lim_org_state_ = 2;
                            }
                        }
                    } catch (...) {
                        // 忽略限位读取失败
                    }
                }
                
                // 更新限位和原点状态
                Tango::DevState mc_state;
                int state_original_timeout = motion->get_timeout_millis();
                motion->set_timeout_millis(300);
                try {
                    mc_state = motion->state();
                } catch (...) {
                    // 读取状态失败时不阻塞，直接跳过本周期
                    motion->set_timeout_millis(state_original_timeout);
                    throw;
                }
                motion->set_timeout_millis(state_original_timeout);
                Tango::DevState old_state = get_state();

                if (limit_fault_latched_.load()) {
                    if (old_state != Tango::FAULT) {
                        set_state(Tango::FAULT);
                    }
                    large_range_state_ = false;
                } else if (mc_state == Tango::MOVING) {
                    if (old_state != Tango::MOVING) {
                        set_state(Tango::MOVING);
                        large_range_state_ = true;
                        INFO_STREAM << "[DEBUG] read_attr_hardware: motion controller state changed to MOVING" << std::endl;
                    }
                } else if (mc_state == Tango::FAULT) {
                    if (old_state != Tango::FAULT) {
                        // 故障时自动启用刹车（安全保护）
                        if (brake_power_port_ >= 0 && brake_released_) {
                            INFO_STREAM << "[BrakeControl] Fault detected, auto-engaging brake (safety)" << std::endl;
                            if (!engage_brake()) {
                                WARN_STREAM << "[BrakeControl] Failed to engage brake on fault" << std::endl;
                            }
                        }
                        set_state(Tango::FAULT);
                        large_range_state_ = false;
                        WARN_STREAM << "[DEBUG] read_attr_hardware: motion controller state changed to FAULT" << std::endl;
                    }
                } else {
                    if (old_state == Tango::MOVING) {
                        // 运动完成：从MOVING状态变为ON状态
                        // 注意：正常运动完成后不自动启用刹车，保持刹车释放状态以便快速继续运动
                        // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
                        set_state(Tango::ON);
                        large_range_state_ = false;
                        INFO_STREAM << "[DEBUG] read_attr_hardware: motion controller state changed to ON" << std::endl;
                    } else if (old_state != Tango::ON) {
                        set_state(Tango::ON);
                        large_range_state_ = false;
                        INFO_STREAM << "[DEBUG] read_attr_hardware: motion controller state changed to ON" << std::endl;
                    }
                }
                
                // 可以在这里添加限位状态读取
                // 可以在这里添加原点状态读取
            } catch (Tango::DevFailed &e) {
                ERROR_STREAM << "[DEBUG] read_attr_hardware: failed to read motion controller state - " 
                            << e.errors[0].desc << std::endl;
                set_state(Tango::UNKNOWN);
            } catch (...) {
                ERROR_STREAM << "[DEBUG] read_attr_hardware: failed to read motion controller state with unknown exception" << std::endl;
                set_state(Tango::UNKNOWN);
            }
        }
        
        // 更新位置信息
        update_position();
    }
}

void LargeStrokeDevice::update_position() {
    if (sim_mode_) {
        // INFO_STREAM << "[DEBUG] update_position: SIM MODE - skipping position update" << std::endl;
        return;
    }
    
    auto enc = get_encoder_proxy();
    if (enc) {
        try {
            double old_pos = large_range_pos_;
            // readEncoder 命令需要一个 Tango::DevShort channel 参数
            // 使用 encoder_channel_ 而不是 axis_id_（编码器通道可能与运动控制器轴号不同）
            Tango::DeviceData data_in;
            data_in << encoder_channel_;
            // 位置轮询属于高频/非关键路径：超时必须短，避免阻塞 attribute read 导致客户端卡顿
            int original_timeout = enc->get_timeout_millis();
            enc->set_timeout_millis(300);
            Tango::DeviceData result;
            try {
                result = enc->command_inout("readEncoder", data_in);
            } catch (...) {
                enc->set_timeout_millis(original_timeout);
                throw;
            }
            enc->set_timeout_millis(original_timeout);
            result >> large_range_pos_;
            if (std::abs(large_range_pos_ - old_pos) > 0.001) {  // 只在位置有明显变化时输出
                INFO_STREAM << "[DEBUG] update_position: position updated from " << old_pos 
                           << " to " << large_range_pos_ << " (steps)" << std::endl;
            }
        } catch (Tango::DevFailed &e) {
            // 注释掉：打印太多，影响其他调试
            // ERROR_STREAM << "[DEBUG] update_position: failed to read encoder - " 
            //             << e.errors[0].desc << std::endl;
        } catch (...) {
            // 注释掉：打印太多，影响其他调试
            // ERROR_STREAM << "[DEBUG] update_position: failed to read encoder with unknown exception" << std::endl;
        }
    } else {
        // encoder_proxy_ 为 NULL 可能是正常的（例如在模拟模式下，或编码器未配置）
        // 只在第一次遇到此情况时输出警告，避免日志过多
        static bool warned = false;
        if (!warned) {
            WARN_STREAM << "[DEBUG] update_position: encoder_proxy_ is NULL (this may be normal if encoder is not configured or in simulation mode)" << std::endl;
            warned = true;
        }
    }
}

// ===== CUSTOM ATTRIBUTE CLASSES (for read_attr() dispatch) =====
class LargeStrokeAttr : public Tango::Attr {
public:
    LargeStrokeAttr(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<LargeStrokeDevice *>(dev)->read_attr(att);
    }
};

class LargeStrokeAttrRW : public Tango::Attr {
public:
    LargeStrokeAttrRW(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ_WRITE)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<LargeStrokeDevice *>(dev)->read_attr(att);
    }
    
    virtual void write(Tango::DeviceImpl *dev, Tango::WAttribute &att) override {
        static_cast<LargeStrokeDevice *>(dev)->write_attr(att);
    }
};

// ========== Class Factory ==========
LargeStrokeDeviceClass *LargeStrokeDeviceClass::_instance = nullptr;

LargeStrokeDeviceClass *LargeStrokeDeviceClass::instance() {
    if (_instance == nullptr) {
        std::string class_name = "LargeStrokeDevice";
        _instance = new LargeStrokeDeviceClass(class_name);
    }
    return _instance;
}

LargeStrokeDeviceClass::LargeStrokeDeviceClass(std::string &class_name) : Tango::DeviceClass(class_name) {
    command_factory();
}

void LargeStrokeDeviceClass::attribute_factory(std::vector<Tango::Attr *> &att_list) {
    // Standard attributes - 使用自定义属性类以确保 read_attr() 被调用
    att_list.push_back(new LargeStrokeAttr("selfCheckResult", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new LargeStrokeAttrRW("positionUnit", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new LargeStrokeAttr("groupAttributeJson", Tango::DEV_STRING, Tango::READ));
    
    // Device-specific attributes
    att_list.push_back(new LargeStrokeAttr("hostPlugState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("largeRangePos", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("direPos", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("LinearLogs", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("alarmState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("axisParameter", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("LargeLimOrgState", Tango::DEV_SHORT, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("LargeRangeState", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("resultValue", Tango::DEV_SHORT, Tango::READ));
    
    // Power control status attributes (NEW)
    att_list.push_back(new LargeStrokeAttr("driverPowerStatus", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new LargeStrokeAttr("brakeStatus", Tango::DEV_BOOLEAN, Tango::READ));
}

void LargeStrokeDeviceClass::command_factory() {
    // Lock/Unlock commands
    command_list.push_back(new Tango::TemplCommand(
        "devLock", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::devLock)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>(
        "devUnlock", static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&LargeStrokeDevice::devUnlock)));
    command_list.push_back(new Tango::TemplCommand(
        "devLockVerify", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::devLockVerify)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>(
        "devLockQuery", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::devLockQuery)));
    command_list.push_back(new Tango::TemplCommand(
        "devUserConfig", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::devUserConfig)));
    
    // System commands
    command_list.push_back(new Tango::TemplCommand(
        "selfCheck", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::selfCheck)));
    command_list.push_back(new Tango::TemplCommand(
        "init", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::init)));
    
    // Parameter commands
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "moveAxisSet", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&LargeStrokeDevice::moveAxisSet)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>(
        "structAxisSet", static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&LargeStrokeDevice::structAxisSet)));
    
    // Motion commands
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>(
        "moveRelative", static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&LargeStrokeDevice::moveRelative)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>(
        "moveAbsolute", static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&LargeStrokeDevice::moveAbsolute)));
    command_list.push_back(new Tango::TemplCommand(
        "stop", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::stop)));
    command_list.push_back(new Tango::TemplCommand(
        "reset", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::reset)));
    
    // Read commands
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevDouble>(
        "readEncoder", static_cast<Tango::DevDouble (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::readEncoder)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevBoolean>(
        "readOrg", static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::readOrg)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevShort>(
        "readEL", static_cast<Tango::DevShort (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::readEL)));
    
    // Auto/Valve commands
    command_list.push_back(new Tango::TemplCommand(
        "largeMoveAuto", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::largeMoveAuto)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "openValue", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&LargeStrokeDevice::openValue)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevBoolean>(
        "plugInRead", static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::plugInRead)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "runAction", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&LargeStrokeDevice::runAction)));
    
    // Export commands
    command_list.push_back(new Tango::TemplCommand(
        "exportLogs", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::exportLogs)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>(
        "readtAxis", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::readtAxis)));
    command_list.push_back(new Tango::TemplCommand(
        "exportAxis", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::exportAxis)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>(
        "simSwitch", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&LargeStrokeDevice::simSwitch)));
    
    // Power Control Commands (NEW - for GUI)
    command_list.push_back(new Tango::TemplCommand(
        "enableDriverPower", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::enableDriverPower)));
    command_list.push_back(new Tango::TemplCommand(
        "disableDriverPower", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::disableDriverPower)));
    command_list.push_back(new Tango::TemplCommand(
        "releaseBrake", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::releaseBrake)));
    command_list.push_back(new Tango::TemplCommand(
        "engageBrake", static_cast<void (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::engageBrake)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>(
        "queryPowerStatus", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&LargeStrokeDevice::queryPowerStatus)));
}

// ========== Power Control Commands (for GUI) ==========
void LargeStrokeDevice::enableDriverPower() {
    if (!enable_driver_power()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to enable driver power", "LargeStrokeDevice::enableDriverPower");
    }
}

void LargeStrokeDevice::disableDriverPower() {
    if (!disable_driver_power()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to disable driver power", "LargeStrokeDevice::disableDriverPower");
    }
}

void LargeStrokeDevice::releaseBrake() {
    if (!release_brake()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to release brake", "LargeStrokeDevice::releaseBrake");
    }
}

void LargeStrokeDevice::engageBrake() {
    if (!engage_brake()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to engage brake", "LargeStrokeDevice::engageBrake");
    }
}

Tango::DevString LargeStrokeDevice::queryPowerStatus() {
    std::stringstream ss;
    ss << "{"
       << "\"driverPowerEnabled\":" << (driver_power_enabled_ ? "true" : "false") << ","
       << "\"brakeReleased\":" << (brake_released_ ? "true" : "false") << ","
       << "\"driverPowerPort\":" << driver_power_port_ << ","
       << "\"brakePowerPort\":" << brake_power_port_ << ","
       << "\"driverPowerController\":\"" << driver_power_controller_ << "\","
       << "\"brakePowerController\":\"" << brake_power_controller_ << "\""
       << "}";
    return CORBA::string_dup(ss.str().c_str());
}

// ========== Power Control Methods ==========
bool LargeStrokeDevice::enable_driver_power() {
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
        std::unique_ptr<Tango::DeviceProxy> power_ctrl_owned;
        std::shared_ptr<Tango::DeviceProxy> power_ctrl_shared;
        Tango::DeviceProxy* power_ctrl = nullptr;
        
        // 如果配置了专用控制器且与当前运动控制器不同，则创建新的 proxy
        if (!driver_power_controller_.empty() && 
            driver_power_controller_ != motion_controller_name_) {
            power_ctrl_owned = std::make_unique<Tango::DeviceProxy>(driver_power_controller_);
            power_ctrl = power_ctrl_owned.get();
            INFO_STREAM << "[PowerControl] Using dedicated controller: " << driver_power_controller_ << std::endl;
        } else {
            power_ctrl_shared = get_motion_controller_proxy();
            power_ctrl = power_ctrl_shared.get();
            INFO_STREAM << "[PowerControl] Using connected motion controller" << std::endl;
        }
        if (!power_ctrl) {
            ERROR_STREAM << "[PowerControl] No controller available for power control!" << std::endl;
            return false;
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

bool LargeStrokeDevice::disable_driver_power() {
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Driver power disabled (simulated)" << std::endl;
        driver_power_enabled_ = false;
        return true;
    }
    
    if (driver_power_port_ < 0) {
        return true;
    }
    
    try {
        std::unique_ptr<Tango::DeviceProxy> power_ctrl_owned;
        std::shared_ptr<Tango::DeviceProxy> power_ctrl_shared;
        Tango::DeviceProxy* power_ctrl = nullptr;
        
        if (!driver_power_controller_.empty() && 
            driver_power_controller_ != motion_controller_name_) {
            power_ctrl_owned = std::make_unique<Tango::DeviceProxy>(driver_power_controller_);
            power_ctrl = power_ctrl_owned.get();
        } else {
            power_ctrl_shared = get_motion_controller_proxy();
            power_ctrl = power_ctrl_shared.get();
        }
        if (!power_ctrl) {
            return true;
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
        driver_power_enabled_ = false;
        INFO_STREAM << "✓ Driver power disabled on port OUT" << driver_power_port_ << std::endl;
        log_event("Driver power disabled on port OUT" + std::to_string(driver_power_port_));
        
        // 断电前自动启用刹车（安全保护）
        if (brake_power_port_ >= 0 && brake_released_) {
            INFO_STREAM << "[BrakeControl] Auto-engaging brake before power off (safety)" << std::endl;
            if (!engage_brake()) {
                WARN_STREAM << "[BrakeControl] Failed to engage brake before power off" << std::endl;
            }
        }
        
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to disable driver power: " << e.errors[0].desc << std::endl;
        return false;
    }
}

bool LargeStrokeDevice::release_brake() {
    INFO_STREAM << "[PowerControl] release_brake() called" << std::endl;
    INFO_STREAM << "[PowerControl] brake_power_port_=" << brake_power_port_ 
               << ", brake_power_controller_=" << brake_power_controller_ << std::endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Brake released (simulated)" << std::endl;
        brake_released_ = true;
        return true;
    }
    
    if (brake_power_port_ < 0) {
        INFO_STREAM << "[PowerControl] Brake control not configured (port=" 
                   << brake_power_port_ << "), skipping" << std::endl;
        return true;
    }
    
    try {
        std::unique_ptr<Tango::DeviceProxy> brake_ctrl_owned;
        std::shared_ptr<Tango::DeviceProxy> brake_ctrl_shared;
        Tango::DeviceProxy* brake_ctrl = nullptr;
        
        if (!brake_power_controller_.empty() && 
            brake_power_controller_ != motion_controller_name_) {
            brake_ctrl_owned = std::make_unique<Tango::DeviceProxy>(brake_power_controller_);
            brake_ctrl = brake_ctrl_owned.get();
            INFO_STREAM << "[PowerControl] Using dedicated brake controller: " << brake_power_controller_ << std::endl;
        } else {
            brake_ctrl_shared = get_motion_controller_proxy();
            brake_ctrl = brake_ctrl_shared.get();
            INFO_STREAM << "[PowerControl] Using connected motion controller for brake" << std::endl;
        }
        if (!brake_ctrl) {
            ERROR_STREAM << "[PowerControl] No controller available for brake control!" << std::endl;
            return false;
        }
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(brake_power_port_);
        // 逻辑1=开启刹车供电(硬件LOW)，用于释放刹车
        params[1] = 1.0;
        Tango::DeviceData data;
        data << params;
        
        INFO_STREAM << "[PowerControl] Calling writeIO for brake with port=" << brake_power_port_ << ", value=1" << std::endl;
        brake_ctrl->command_inout("writeIO", data);
        
        brake_released_ = true;
        INFO_STREAM << "✓ Brake released on port OUT" << brake_power_port_ << std::endl;
        log_event("Brake released on port OUT" + std::to_string(brake_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to release brake: " << e.errors[0].desc << std::endl;
        log_event("Failed to release brake: " + std::string(e.errors[0].desc.in()));
        return false;
    }
}

bool LargeStrokeDevice::engage_brake() {
    INFO_STREAM << "[PowerControl] engage_brake() called" << std::endl;
    INFO_STREAM << "[PowerControl] brake_power_port_=" << brake_power_port_ 
               << ", brake_power_controller_=" << brake_power_controller_ << std::endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Brake engaged (simulated)" << std::endl;
        brake_released_ = false;
        return true;
    }
    
    if (brake_power_port_ < 0) {
        INFO_STREAM << "[PowerControl] Brake control not configured (port=" 
                   << brake_power_port_ << "), skipping" << std::endl;
        return true;  // 未配置时认为成功
    }
    
    try {
        std::unique_ptr<Tango::DeviceProxy> brake_ctrl_owned;
        std::shared_ptr<Tango::DeviceProxy> brake_ctrl_shared;
        Tango::DeviceProxy* brake_ctrl = nullptr;
        
        if (!brake_power_controller_.empty() && 
            brake_power_controller_ != motion_controller_name_) {
            brake_ctrl_owned = std::make_unique<Tango::DeviceProxy>(brake_power_controller_);
            brake_ctrl = brake_ctrl_owned.get();
            INFO_STREAM << "[PowerControl] Using dedicated brake controller: " << brake_power_controller_ << std::endl;
        } else {
            brake_ctrl_shared = get_motion_controller_proxy();
            brake_ctrl = brake_ctrl_shared.get();
            INFO_STREAM << "[PowerControl] Using connected motion controller for brake" << std::endl;
        }
        if (!brake_ctrl) {
            ERROR_STREAM << "[PowerControl] No controller available for brake control!" << std::endl;
            return false;
        }
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(brake_power_port_);
        // 逻辑0=关闭刹车供电(硬件HIGH)，用于抱闸/启用刹车
        params[1] = 0.0;
        Tango::DeviceData data;
        data << params;
        
        INFO_STREAM << "[PowerControl] Calling writeIO for brake with port=" << brake_power_port_ << ", value=0" << std::endl;
        brake_ctrl->command_inout("writeIO", data);
        
        brake_released_ = false;
        INFO_STREAM << "✓ Brake engaged on port OUT" << brake_power_port_ << std::endl;
        log_event("Brake engaged on port OUT" + std::to_string(brake_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to engage brake: " << e.errors[0].desc << std::endl;
        log_event("Failed to engage brake: " + std::string(e.errors[0].desc.in()));
        return false;
    }
}

void LargeStrokeDeviceClass::device_factory(const Tango::DevVarStringArray *devlist_ptr) {
    for (unsigned long i = 0; i < devlist_ptr->length(); i++) {
        std::string dev_name = (*devlist_ptr)[i].in();
        LargeStrokeDevice *dev = new LargeStrokeDevice(this, dev_name);
        device_list.push_back(dev);
        export_device(dev);
    }
}

} // namespace LargeStroke

// Main function
void Tango::DServer::class_factory() {
    add_class(LargeStroke::LargeStrokeDeviceClass::instance());
}

int main(int argc, char *argv[]) {
    try {
        Common::SystemConfig::loadConfig();
        Tango::Util *tg = Tango::Util::init(argc, argv);
        tg->server_init();
        std::cout << "LargeStroke Server Ready" << std::endl;
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
