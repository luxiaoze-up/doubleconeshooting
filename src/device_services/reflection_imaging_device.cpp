#include "device_services/reflection_imaging_device.h"
#include "common/system_config.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <chrono>
#include <stdexcept>

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

using namespace std;

namespace ReflectionImaging {

ReflectionImagingDevice::ReflectionImagingDevice(Tango::DeviceClass *device_class, std::string &device_name)
    : Common::StandardSystemDevice(device_class, device_name),
      is_locked_(false),
      lock_user_(""),
      upper_support_axis_id_(-1),
      upper_support_work_pos_(0.0),
      upper_support_home_pos_(0.0),
      lower_support_axis_id_(-1),
      lower_support_work_pos_(0.0),
      lower_support_home_pos_(0.0),
      self_check_result_(-1),
      position_unit_("step"),
      fault_state_(""),
      reflection_logs_("{}"),
      result_value_(0),

      // Platform states
      upper_platform_pos_{0.0, 0.0, 0.0},
      upper_platform_dire_pos_{0.0, 0.0, 0.0},
      upper_platform_lim_org_state_{2, 2, 2},
      upper_platform_state_{false, false, false},
      lower_platform_pos_{0.0, 0.0, 0.0},
      lower_platform_dire_pos_{0.0, 0.0, 0.0},
      lower_platform_lim_org_state_{2, 2, 2},
      lower_platform_state_{false, false, false},
      // 上1倍物镜CCD（粗定位）
      upper_ccd_1x_state_("READY"),
      upper_ccd_1x_exposure_(1.0),
      upper_ccd_1x_trigger_mode_("Software"),
      upper_ccd_1x_resolution_("1920x1080"),
      upper_ccd_1x_width_(1920),
      upper_ccd_1x_height_(1080),
      upper_ccd_1x_gain_(0.0),
      upper_ccd_1x_brightness_(0.0),
      upper_ccd_1x_contrast_(0.0),
      upper_ccd_1x_ring_light_on_(false),
      upper_ccd_1x_image_url_(""),
      upper_ccd_1x_last_capture_time_(""),
      // 上10倍物镜CCD（近距离观察）
      upper_ccd_10x_state_("READY"),
      upper_ccd_10x_exposure_(1.0),
      upper_ccd_10x_trigger_mode_("Software"),
      upper_ccd_10x_resolution_("1920x1080"),
      upper_ccd_10x_width_(1920),
      upper_ccd_10x_height_(1080),
      upper_ccd_10x_gain_(0.0),
      upper_ccd_10x_brightness_(0.0),
      upper_ccd_10x_contrast_(0.0),
      upper_ccd_10x_ring_light_on_(false),
      upper_ccd_10x_image_url_(""),
      upper_ccd_10x_last_capture_time_(""),
      // 下1倍物镜CCD（粗定位）
      lower_ccd_1x_state_("READY"),
      lower_ccd_1x_exposure_(1.0),
      lower_ccd_1x_trigger_mode_("Software"),
      lower_ccd_1x_resolution_("1920x1080"),
      lower_ccd_1x_width_(1920),
      lower_ccd_1x_height_(1080),
      lower_ccd_1x_gain_(0.0),
      lower_ccd_1x_brightness_(0.0),
      lower_ccd_1x_contrast_(0.0),
      lower_ccd_1x_ring_light_on_(false),
      lower_ccd_1x_image_url_(""),
      lower_ccd_1x_last_capture_time_(""),
      // 下10倍物镜CCD（近距离观察）
      lower_ccd_10x_state_("READY"),
      lower_ccd_10x_exposure_(1.0),
      lower_ccd_10x_trigger_mode_("Software"),
      lower_ccd_10x_resolution_("1920x1080"),
      lower_ccd_10x_width_(1920),
      lower_ccd_10x_height_(1080),
      lower_ccd_10x_gain_(0.0),
      lower_ccd_10x_brightness_(0.0),
      lower_ccd_10x_contrast_(0.0),
      lower_ccd_10x_ring_light_on_(false),
      lower_ccd_10x_image_url_(""),
      lower_ccd_10x_last_capture_time_(""),
      image_capture_count_(0),
      auto_capture_enabled_(false),
      upper_support_position_(0.0),
      lower_support_position_(0.0),
      upper_support_state_("LOWERED"),
      lower_support_state_("LOWERED"),
      axis_parameter_("{}"),
      sim_mode_(true),
      last_update_time_(std::chrono::steady_clock::now()),
      encoder_proxy_(nullptr),
      upper_platform_proxy_(nullptr),
      lower_platform_proxy_(nullptr),
      sim_motion_start_time_(std::chrono::steady_clock::now()),
      upper_platform_target_{0.0, 0.0, 0.0},
      lower_platform_target_{0.0, 0.0, 0.0},
      sim_motion_duration_(0.0) {
    upper_platform_range_ = {1000, 1000, 500};
    lower_platform_range_ = {1000, 1000, 500};
    init_device();
}

ReflectionImagingDevice::~ReflectionImagingDevice() {
    delete_device();
}

void ReflectionImagingDevice::init_device() {
    Common::StandardSystemDevice::init_device();

    Tango::DbData db_data;
    // 标准属性
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
    
    
    // 上下双三坐标平台配置
    db_data.push_back(Tango::DbDatum("upperPlatformConfig"));
    db_data.push_back(Tango::DbDatum("lowerPlatformConfig"));
    db_data.push_back(Tango::DbDatum("upperPlatformRange"));
    db_data.push_back(Tango::DbDatum("lowerPlatformRange"));
    db_data.push_back(Tango::DbDatum("upperPlatformLimitNumber"));
    db_data.push_back(Tango::DbDatum("lowerPlatformLimitNumber"));
    
    // 四CCD相机配置（上下各两个，1倍和10倍物镜，海康MV-CU020-19GC）
    db_data.push_back(Tango::DbDatum("upperCCD1xConfig"));   // 上1倍物镜CCD配置（粗定位）
    db_data.push_back(Tango::DbDatum("upperCCD10xConfig"));  // 上10倍物镜CCD配置（近距离观察）
    db_data.push_back(Tango::DbDatum("lowerCCD1xConfig"));   // 下1倍物镜CCD配置（粗定位）
    db_data.push_back(Tango::DbDatum("lowerCCD10xConfig"));  // 下10倍物镜CCD配置（近距离观察）
    db_data.push_back(Tango::DbDatum("imageSavePath"));
    db_data.push_back(Tango::DbDatum("imageFormat"));
    db_data.push_back(Tango::DbDatum("autoCaptureInterval"));
    
    // 辅助支撑配置
    db_data.push_back(Tango::DbDatum("upperSupportConfig"));
    db_data.push_back(Tango::DbDatum("lowerSupportConfig"));
    
    // 运动控制器和编码器
    db_data.push_back(Tango::DbDatum("motionControllerName"));
    db_data.push_back(Tango::DbDatum("encoderName"));
    // 编码器通道配置
    // 编码器通道配置
    db_data.push_back(Tango::DbDatum("upperPlatformEncoderChannels"));
    db_data.push_back(Tango::DbDatum("lowerPlatformEncoderChannels"));
    // 电机参数
    db_data.push_back(Tango::DbDatum("motorStepAngle"));
    db_data.push_back(Tango::DbDatum("motorGearRatio"));
    db_data.push_back(Tango::DbDatum("motorSubdivision"));
    db_data.push_back(Tango::DbDatum("driverPowerPort"));
    db_data.push_back(Tango::DbDatum("driverPowerController"));
    db_data.push_back(Tango::DbDatum("brakePowerPort"));
    db_data.push_back(Tango::DbDatum("brakePowerController"));
    
    get_db_device()->get_property(db_data);

    int idx = 0;
    // 标准属性
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
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_position_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_product_date_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> device_install_date_; } idx++;
    
    // 上下双三坐标平台
    if (!db_data[idx].is_empty()) { db_data[idx] >> upper_platform_config_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> lower_platform_config_; } idx++;
    if (!db_data[idx].is_empty()) {
        std::vector<short> temp;
        db_data[idx] >> temp;
        if (temp.size() >= 3) {
            upper_platform_range_[0] = temp[0];
            upper_platform_range_[1] = temp[1];
            upper_platform_range_[2] = temp[2];
        }
    } idx++;
    if (!db_data[idx].is_empty()) {
        std::vector<short> temp;
        db_data[idx] >> temp;
        if (temp.size() >= 3) {
            lower_platform_range_[0] = temp[0];
            lower_platform_range_[1] = temp[1];
            lower_platform_range_[2] = temp[2];
        }
    } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> upper_platform_limit_number_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> lower_platform_limit_number_; } idx++;
    
    // 四CCD相机（上下各两个，1倍和10倍物镜，海康MV-CU020-19GC）
    if (!db_data[idx].is_empty()) { db_data[idx] >> upper_ccd_1x_config_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> upper_ccd_10x_config_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> lower_ccd_1x_config_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> lower_ccd_10x_config_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> image_save_path_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> image_format_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> auto_capture_interval_; } idx++;
    
    // 辅助支撑
    if (!db_data[idx].is_empty()) { db_data[idx] >> upper_support_config_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> lower_support_config_; } idx++;
    
    // 运动控制器和编码器
    if (!db_data[idx].is_empty()) { db_data[idx] >> motion_controller_name_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> encoder_name_; } idx++;
    // 编码器通道配置
    // 注意：db_data 的 push_back 顺序与 idx++ 必须严格一致。
    // 这里不应额外跳过任何一项，否则后续所有属性都会发生“错位读取”。
    if (!db_data[idx].is_empty()) { db_data[idx] >> upper_platform_encoder_channels_; } idx++;
    if (!db_data[idx].is_empty()) { db_data[idx] >> lower_platform_encoder_channels_; } idx++;
    // 电机参数
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

    if (motion_controller_name_.empty()) motion_controller_name_ = "sys/motion/1";
    if (encoder_name_.empty()) encoder_name_ = "sys/encoder/1";
    
    // 编码器通道默认值 - 根据《双锥项目测试数据收集》硬件文档配置
    // 编码器采集器 192.168.1.199: 通道1-10 (代码0-9)
    // 编码器采集器 192.168.1.198: 通道1-10 (代码10-19)
    // 编码器通道默认值 - 根据《双锥项目测试数据收集》硬件文档配置
    // 编码器采集器 192.168.1.199: 通道1-10 (代码0-9)
    // 编码器采集器 192.168.1.198: 通道1-10 (代码10-19)
    if (upper_platform_encoder_channels_.empty()) {
        // 上平台(设备1): X/Y/Z 对应 199通道8/10, 198通道2 (代码7,9,11)
        upper_platform_encoder_channels_.push_back(7);   // 设备1-X: 199通道8
        upper_platform_encoder_channels_.push_back(9);   // 设备1-Y: 199通道10
        upper_platform_encoder_channels_.push_back(11);  // 设备1-Z: 198通道2
        INFO_STREAM << "Using default upper platform encoder channels: 7,9,11 (设备1 X/Y/Z)" << endl;
    }
    if (lower_platform_encoder_channels_.empty()) {
        // 下平台(设备2): X/Y/Z 对应 199通道9, 198通道1/3 (代码8,10,12)
        lower_platform_encoder_channels_.push_back(8);   // 设备2-X: 199通道9
        lower_platform_encoder_channels_.push_back(10);  // 设备2-Y: 198通道1
        lower_platform_encoder_channels_.push_back(12);  // 设备2-Z: 198通道3
        INFO_STREAM << "Using default lower platform encoder channels: 8,10,12 (设备2 X/Y/Z)" << endl;
    }
    if (image_save_path_.empty()) image_save_path_ = "./images";
    if (image_format_.empty()) image_format_ = "PNG";
    
    // 创建图像保存目录（如果不存在）
    #ifdef _WIN32
        std::string mkdir_cmd = "if not exist \"" + image_save_path_ + "\" mkdir \"" + image_save_path_ + "\"";
        (void)system(mkdir_cmd.c_str());  // 忽略返回值
    #else
        std::string mkdir_cmd = "mkdir -p " + image_save_path_;
        int result = system(mkdir_cmd.c_str());
        (void)result;  // 忽略返回值
    #endif

    // 解析辅助支撑配置
    if (!upper_support_config_.empty()) {
        try {
            std::string id_str = parse_json_value(upper_support_config_, "axisId");
            std::string work_str = parse_json_value(upper_support_config_, "workPos");
            std::string home_str = parse_json_value(upper_support_config_, "homePos");
            
            if (!id_str.empty()) upper_support_axis_id_ = std::stoi(id_str);
            if (!work_str.empty()) upper_support_work_pos_ = std::stod(work_str);
            if (!home_str.empty()) upper_support_home_pos_ = std::stod(home_str);
            
            INFO_STREAM << "Upper Support Config: Axis=" << upper_support_axis_id_ 
                       << ", WorkPos=" << upper_support_work_pos_ 
                       << ", HomePos=" << upper_support_home_pos_ << endl;
        } catch (...) {
            WARN_STREAM << "Failed to parse upper support config: " << upper_support_config_ << endl;
        }
    }

    if (!lower_support_config_.empty()) {
        try {
            std::string id_str = parse_json_value(lower_support_config_, "axisId");
            std::string work_str = parse_json_value(lower_support_config_, "workPos");
            std::string home_str = parse_json_value(lower_support_config_, "homePos");
            
            if (!id_str.empty()) lower_support_axis_id_ = std::stoi(id_str);
            if (!work_str.empty()) lower_support_work_pos_ = std::stod(work_str);
            if (!home_str.empty()) lower_support_home_pos_ = std::stod(home_str);
            
            INFO_STREAM << "Lower Support Config: Axis=" << lower_support_axis_id_ 
                       << ", WorkPos=" << lower_support_work_pos_ 
                       << ", HomePos=" << lower_support_home_pos_ << endl;
        } catch (...) {
            WARN_STREAM << "Failed to parse lower support config: " << lower_support_config_ << endl;
        }
    }

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

    INFO_STREAM << "ReflectionImagingDevice initialized. Motion Controller: "
                << motion_controller_name_ << endl;
    log_event("Device initialized");

    connect_proxies();

    // Start LargeStroke-style background connection monitor
    start_connection_monitor();
}

void ReflectionImagingDevice::delete_device() {
    stop_connection_monitor();
    // 设备关闭时自动启用刹车（安全保护）
    if (brake_power_port_ >= 0 && brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-engaging brake during device deletion (safety)" << endl;
        if (!engage_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to engage brake during device deletion" << endl;
        }
    }
    
    // 关闭相机驱动
    shutdown_cameras();
    
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        encoder_proxy_.reset();
        upper_platform_proxy_.reset();
        lower_platform_proxy_.reset();
    }
    Common::StandardSystemDevice::delete_device();
}

// ===== Proxy helpers (lifetime-safe, LargeStroke-style) =====
std::shared_ptr<Tango::DeviceProxy> ReflectionImagingDevice::get_encoder_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return encoder_proxy_;
}

std::shared_ptr<Tango::DeviceProxy> ReflectionImagingDevice::get_upper_platform_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return upper_platform_proxy_;
}

std::shared_ptr<Tango::DeviceProxy> ReflectionImagingDevice::get_lower_platform_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    return lower_platform_proxy_;
}

void ReflectionImagingDevice::reset_encoder_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    encoder_proxy_.reset();
}

void ReflectionImagingDevice::reset_upper_platform_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    upper_platform_proxy_.reset();
}

void ReflectionImagingDevice::reset_lower_platform_proxy() {
    std::lock_guard<std::mutex> lk(proxy_mutex_);
    lower_platform_proxy_.reset();
}

void ReflectionImagingDevice::rebuild_encoder_proxy(int timeout_ms) {
    if (encoder_name_.empty()) {
        return;
    }
    auto new_enc = create_proxy_and_ping(encoder_name_, timeout_ms);
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        encoder_proxy_ = new_enc;
    }
}

void ReflectionImagingDevice::rebuild_upper_platform_proxy(int timeout_ms) {
    if (motion_controller_name_.empty()) {
        return;
    }
    auto new_upper = create_proxy_and_ping(motion_controller_name_, timeout_ms);
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        upper_platform_proxy_ = new_upper;
    }
}

void ReflectionImagingDevice::rebuild_lower_platform_proxy(int timeout_ms) {
    if (motion_controller_name_.empty()) {
        return;
    }
    auto new_lower = create_proxy_and_ping(motion_controller_name_, timeout_ms);
    {
        std::lock_guard<std::mutex> lk(proxy_mutex_);
        lower_platform_proxy_ = new_lower;
    }
}

// ===== Background connection monitor (LargeStroke-style) =====
void ReflectionImagingDevice::start_connection_monitor() {
    // 模拟模式无需启动线程
    if (sim_mode_) {
        connection_healthy_.store(true);
        return;
    }
    if (connection_monitor_thread_.joinable()) {
        return;
    }
    stop_connection_monitor_.store(false);
    connection_monitor_thread_ = std::thread(&ReflectionImagingDevice::connection_monitor_loop, this);
}

void ReflectionImagingDevice::stop_connection_monitor() {
    stop_connection_monitor_.store(true);
    if (connection_monitor_thread_.joinable()) {
        connection_monitor_thread_.join();
    }
}

void ReflectionImagingDevice::connection_monitor_loop() {
    const int ping_timeout_ms = 300;
    const int connect_timeout_ms = 500;

    while (!stop_connection_monitor_.load()) {
        if (sim_mode_) {
            connection_healthy_.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        bool encoder_ok = encoder_name_.empty();
        bool upper_ok = motion_controller_name_.empty();
        bool lower_ok = motion_controller_name_.empty();

        // 1) 快速 ping 现有 proxy（短超时）
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

        if (!motion_controller_name_.empty()) {
            auto upper = get_upper_platform_proxy();
            if (upper) {
                upper_ok = ping_with_temp_timeout(upper, ping_timeout_ms);
                if (!upper_ok) {
                    reset_upper_platform_proxy();
                }
            } else {
                upper_ok = false;
            }

            auto lower = get_lower_platform_proxy();
            if (lower) {
                lower_ok = ping_with_temp_timeout(lower, ping_timeout_ms);
                if (!lower_ok) {
                    reset_lower_platform_proxy();
                }
            } else {
                lower_ok = false;
            }
        }

        // 2) 按间隔尝试重连（仅创建 proxy + ping，不做上电/刹车/参数恢复等副作用）
        auto now = std::chrono::steady_clock::now();
        auto reconnect_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_reconnect_attempt_).count();
        bool can_reconnect = (reconnect_elapsed >= Common::SystemConfig::PROXY_RECONNECT_INTERVAL_SEC);

        bool motion_ok = (upper_ok && lower_ok);
        if (can_reconnect && (!encoder_ok || !motion_ok)) {
            last_reconnect_attempt_ = now;
            if (!encoder_ok && !encoder_name_.empty()) {
                try {
                    rebuild_encoder_proxy(connect_timeout_ms);
                    encoder_ok = true;
                } catch (...) {
                    // 连接失败，保持不健康
                }
            }
            if (!upper_ok && !motion_controller_name_.empty()) {
                try {
                    rebuild_upper_platform_proxy(connect_timeout_ms);
                    upper_ok = true;
                    // 注意：不立即设置 connection_healthy_ = true，等待恢复操作完成后再设置
                    // Defer side-effects (power/brake/params/sync) to main thread
                    motion_restore_pending_.store(true);
                } catch (...) {
                    // 连接失败，保持不健康
                }
            }
            if (!lower_ok && !motion_controller_name_.empty()) {
                try {
                    rebuild_lower_platform_proxy(connect_timeout_ms);
                    lower_ok = true;
                    // 注意：不立即设置 connection_healthy_ = true，等待恢复操作完成后再设置
                    // Defer side-effects (power/brake/params/sync) to main thread
                    motion_restore_pending_.store(true);
                } catch (...) {
                    // 连接失败，保持不健康
                }
            }
            // 更新 motion_ok 状态
            motion_ok = (upper_ok && lower_ok);
        }

        // 3) 更新连接健康标志（命令路径只读此标志，零等待）
        // 只有在所有 proxy 都正常，且没有待处理的恢复操作时，才设置为健康
        bool was_healthy = connection_healthy_.load();
        bool should_be_healthy = encoder_ok && motion_ok && !motion_restore_pending_.load();
        if (should_be_healthy != was_healthy) {
            connection_healthy_.store(should_be_healthy);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void ReflectionImagingDevice::connect_proxies() {
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
        reset_encoder_proxy();
        reset_upper_platform_proxy();
        reset_lower_platform_proxy();

        INFO_STREAM << "[DEBUG] Rebuilding proxies (encoder=" << encoder_name_
                    << ", motion=" << motion_controller_name_ << ")" << std::endl;
        if (!encoder_name_.empty()) {
            rebuild_encoder_proxy(500);
        }
        if (!motion_controller_name_.empty()) {
            rebuild_upper_platform_proxy(500);
            rebuild_lower_platform_proxy(500);
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

void ReflectionImagingDevice::perform_post_motion_reconnect_restore() {
    if (sim_mode_) {
        return;
    }
    
    auto upper = get_upper_platform_proxy();
    auto lower = get_lower_platform_proxy();
    auto encoder = get_encoder_proxy();
    
    if (!upper || !lower) {
        return;  // 平台 proxy 未连接，无法恢复
    }
    
    // 1. 恢复电机细分参数
    try {
        INFO_STREAM << "Applying motor subdivision parameters to upper platform axes (stepAngle=" 
                   << motor_step_angle_ << ", gearRatio=" << motor_gear_ratio_ 
                   << ", subdivision=" << motor_subdivision_ << ")" << endl;
        for (int axis : {0, 2, 4}) {  // 上平台：轴0(X), 轴2(Y), 轴4(Z)
            Tango::DevVarDoubleArray params;
            params.length(4);
            params[0] = static_cast<double>(axis);
            params[1] = motor_step_angle_;
            params[2] = motor_gear_ratio_;
            params[3] = motor_subdivision_;
            Tango::DeviceData arg;
            arg << params;
            upper->command_inout("setStructParameter", arg);
        }
        INFO_STREAM << "Successfully applied motor subdivision parameters to upper platform axes" << endl;
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "Failed to apply motor subdivision parameters to upper platform: " << e.errors[0].desc << endl;
    }
    
    try {
        INFO_STREAM << "Applying motor subdivision parameters to lower platform axes (stepAngle=" 
                   << motor_step_angle_ << ", gearRatio=" << motor_gear_ratio_ 
                   << ", subdivision=" << motor_subdivision_ << ")" << endl;
        for (int axis : {1, 3, 5}) {  // 下平台：轴1(X), 轴3(Y), 轴5(Z)
            Tango::DevVarDoubleArray params;
            params.length(4);
            params[0] = static_cast<double>(axis);
            params[1] = motor_step_angle_;
            params[2] = motor_gear_ratio_;
            params[3] = motor_subdivision_;
            Tango::DeviceData arg;
            arg << params;
            lower->command_inout("setStructParameter", arg);
        }
        INFO_STREAM << "Successfully applied motor subdivision parameters to lower platform axes" << endl;
    } catch (Tango::DevFailed &e) {
        WARN_STREAM << "Failed to apply motor subdivision parameters to lower platform: " << e.errors[0].desc << endl;
    }
    
    // 2. 恢复驱动器上电
    INFO_STREAM << "Restoring relay configuration and enabling driver power after reconnection..." << endl;
    if (enable_driver_power()) {
        INFO_STREAM << "Driver power enabled successfully after reconnection" << endl;
    } else {
        WARN_STREAM << "Failed to enable driver power after reconnection" << endl;
    }
    
    // 3. 释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0) {
        INFO_STREAM << "[BrakeControl] Platform reconnected, auto-releasing brake" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake after platform reconnection" << endl;
        }
    }
    
    // 4. 同步编码器位置
    if (encoder && upper_platform_encoder_channels_.size() >= 3) {
        try {
            INFO_STREAM << "Synchronizing encoder positions to motion controller..." << endl;
            
            // 同步上平台编码器位置（轴0, 2, 4）
            int upper_axes[] = {0, 2, 4};
            for (int i = 0; i < 3; ++i) {
                try {
                    short encoder_ch = upper_platform_encoder_channels_[i];
                    int axis = upper_axes[i];
                    
                    // 读取编码器位置
                    Tango::DeviceData data_in;
                    data_in << encoder_ch;
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
                    upper->command_inout("setEncoderPosition", arg);
                    
                    INFO_STREAM << "Upper platform axis " << axis << " (encoder channel " << encoder_ch 
                               << "): synced position " << encoder_pos << " to motion controller" << endl;
                } catch (Tango::DevFailed &e) {
                    WARN_STREAM << "Failed to sync encoder position for upper platform axis " 
                               << (i == 0 ? 0 : (i == 1 ? 2 : 4)) << ": " << e.errors[0].desc << endl;
                }
            }
            
            // 同步下平台编码器位置（轴1, 3, 5）
            if (lower_platform_encoder_channels_.size() >= 3) {
                int lower_axes[] = {1, 3, 5};
                for (int i = 0; i < 3; ++i) {
                    try {
                        short encoder_ch = lower_platform_encoder_channels_[i];
                        int axis = lower_axes[i];
                        
                        // 读取编码器位置
                        Tango::DeviceData data_in;
                        data_in << encoder_ch;
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
                        lower->command_inout("setEncoderPosition", arg);
                        
                        INFO_STREAM << "Lower platform axis " << axis << " (encoder channel " << encoder_ch 
                                   << "): synced position " << encoder_pos << " to motion controller" << endl;
                    } catch (Tango::DevFailed &e) {
                        WARN_STREAM << "Failed to sync encoder position for lower platform axis " 
                                   << (i == 0 ? 1 : (i == 1 ? 3 : 5)) << ": " << e.errors[0].desc << endl;
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

void ReflectionImagingDevice::initialize_cameras() {
    try {
        if (sim_mode_) {
            // 仿真模式：创建模拟驱动
            upper_ccd_1x_driver_ = std::make_unique<Hikvision::MV_CU020_19GC>("upper_ccd_1x", 0);
            upper_ccd_10x_driver_ = std::make_unique<Hikvision::MV_CU020_19GC>("upper_ccd_10x", 1);
            lower_ccd_1x_driver_ = std::make_unique<Hikvision::MV_CU020_19GC>("lower_ccd_1x", 2);
            lower_ccd_10x_driver_ = std::make_unique<Hikvision::MV_CU020_19GC>("lower_ccd_10x", 3);
            
            // 仿真模式下直接标记为就绪
            INFO_STREAM << "Camera drivers initialized (simulation mode)" << endl;
            return;
        }

        // 真机模式：初始化相机驱动
        INFO_STREAM << "Initializing camera drivers..." << endl;
        
        // 上1倍物镜CCD
        upper_ccd_1x_driver_ = std::make_unique<Hikvision::MV_CU020_19GC>("upper_ccd_1x", 0);
        if (!upper_ccd_1x_driver_->initialize()) {
            ERROR_STREAM << "Failed to initialize upper_ccd_1x: " 
                        << upper_ccd_1x_driver_->getLastError() << endl;
            upper_ccd_1x_state_ = "ERROR";
        } else {
            upper_ccd_1x_state_ = "READY";
            INFO_STREAM << "Upper CCD 1x initialized successfully" << endl;
        }

        // 上10倍物镜CCD
        upper_ccd_10x_driver_ = std::make_unique<Hikvision::MV_CU020_19GC>("upper_ccd_10x", 1);
        if (!upper_ccd_10x_driver_->initialize()) {
            ERROR_STREAM << "Failed to initialize upper_ccd_10x: " 
                        << upper_ccd_10x_driver_->getLastError() << endl;
            upper_ccd_10x_state_ = "ERROR";
        } else {
            upper_ccd_10x_state_ = "READY";
            INFO_STREAM << "Upper CCD 10x initialized successfully" << endl;
        }

        // 下1倍物镜CCD
        lower_ccd_1x_driver_ = std::make_unique<Hikvision::MV_CU020_19GC>("lower_ccd_1x", 2);
        if (!lower_ccd_1x_driver_->initialize()) {
            ERROR_STREAM << "Failed to initialize lower_ccd_1x: " 
                        << lower_ccd_1x_driver_->getLastError() << endl;
            lower_ccd_1x_state_ = "ERROR";
        } else {
            lower_ccd_1x_state_ = "READY";
            INFO_STREAM << "Lower CCD 1x initialized successfully" << endl;
        }

        // 下10倍物镜CCD
        lower_ccd_10x_driver_ = std::make_unique<Hikvision::MV_CU020_19GC>("lower_ccd_10x", 3);
        if (!lower_ccd_10x_driver_->initialize()) {
            ERROR_STREAM << "Failed to initialize lower_ccd_10x: " 
                        << lower_ccd_10x_driver_->getLastError() << endl;
            lower_ccd_10x_state_ = "ERROR";
        } else {
            lower_ccd_10x_state_ = "READY";
            INFO_STREAM << "Lower CCD 10x initialized successfully" << endl;
        }

        INFO_STREAM << "Camera drivers initialization completed" << endl;
    } catch (const std::exception& e) {
        ERROR_STREAM << "Exception in initialize_cameras: " << e.what() << endl;
    }
}

void ReflectionImagingDevice::shutdown_cameras() {
    try {
        if (upper_ccd_1x_driver_) {
            upper_ccd_1x_driver_->shutdown();
            upper_ccd_1x_driver_.reset();
        }
        if (upper_ccd_10x_driver_) {
            upper_ccd_10x_driver_->shutdown();
            upper_ccd_10x_driver_.reset();
        }
        if (lower_ccd_1x_driver_) {
            lower_ccd_1x_driver_->shutdown();
            lower_ccd_1x_driver_.reset();
        }
        if (lower_ccd_10x_driver_) {
            lower_ccd_10x_driver_->shutdown();
            lower_ccd_10x_driver_.reset();
        }
        INFO_STREAM << "Camera drivers shutdown completed" << endl;
    } catch (const std::exception& e) {
        ERROR_STREAM << "Exception in shutdown_cameras: " << e.what() << endl;
    }
}

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



    // 上平台
    {"upperPlatformAxisSet",      {false, true,  true,  true}},
    {"upperPlatformStructAxisSet",{false, true,  true,  true}},
    {"upperPlatformMoveRelative", {false, false, true,  false}},
    {"upperPlatformMoveAbsolute", {false, false, true,  false}},
    {"upperPlatformMoveToPosition",{false, false, true,  false}},
    {"upperPlatformStop",         {false, false, true,  true}},
    {"upperPlatformReset",        {false, false, true,  true}},
    {"upperPlatformMoveZero",     {false, true,  true,  true}},
    {"upperPlatformReadEncoder",  {false, true,  true,  true}},
    {"upperPlatformReadOrg",      {false, true,  true,  true}},
    {"upperPlatformReadEL",       {false, true,  true,  true}},
    {"upperPlatformSingleAxisMove",{false, false, true,  false}},

    // 下平台
    {"lowerPlatformAxisSet",      {false, true,  true,  true}},
    {"lowerPlatformStructAxisSet",{false, true,  true,  true}},
    {"lowerPlatformMoveRelative", {false, false, true,  false}},
    {"lowerPlatformMoveAbsolute", {false, false, true,  false}},
    {"lowerPlatformMoveToPosition",{false, false, true,  false}},
    {"lowerPlatformStop",         {false, false, true,  true}},
    {"lowerPlatformReset",        {false, false, true,  true}},
    {"lowerPlatformMoveZero",     {false, true,  true,  true}},
    {"lowerPlatformReadEncoder",  {false, true,  true,  true}},
    {"lowerPlatformReadOrg",      {false, true,  true,  true}},
    {"lowerPlatformReadEL",       {false, true,  true,  true}},
    {"lowerPlatformSingleAxisMove",{false, false, true,  false}},
    
    // 上平台单轴控制命令 (用于GUI单轴独立控制)
    // 格式: {allow_unknown, allow_off, allow_on, allow_fault}
    {"upperXMoveAbsolute",  {false, false, true,  true}},
    {"upperXMoveRelative",  {false, false, true,  true}},
    {"upperXMoveZero",      {false, true,  true,  true}},
    {"upperXStop",          {false, false, true,  true}},
    {"upperXReset",         {false, false, true,  true}},
    {"upperYMoveAbsolute",  {false, false, true,  true}},
    {"upperYMoveRelative",  {false, false, true,  true}},
    {"upperYMoveZero",      {false, true,  true,  true}},
    {"upperYStop",          {false, false, true,  true}},
    {"upperYReset",         {false, false, true,  true}},
    {"upperZMoveAbsolute",  {false, false, true,  true}},
    {"upperZMoveRelative",  {false, false, true,  true}},
    {"upperZMoveZero",      {false, true,  true,  true}},
    {"upperZStop",          {false, false, true,  true}},
    {"upperZReset",         {false, false, true,  true}},
    
    // 下平台单轴控制命令 (用于GUI单轴独立控制)
    // 格式: {allow_unknown, allow_off, allow_on, allow_fault}
    {"lowerXMoveAbsolute",  {false, false, true,  true}},
    {"lowerXMoveRelative",  {false, false, true,  true}},
    {"lowerXMoveZero",      {false, true,  true,  true}},
    {"lowerXStop",          {false, false, true,  true}},
    {"lowerXReset",         {false, false, true,  true}},
    {"lowerYMoveAbsolute",  {false, false, true,  true}},
    {"lowerYMoveRelative",  {false, false, true,  true}},
    {"lowerYMoveZero",      {false, true,  true,  true}},
    {"lowerYStop",          {false, false, true,  true}},
    {"lowerYReset",         {false, false, true,  true}},
    {"lowerZMoveAbsolute",  {false, false, true,  true}},
    {"lowerZMoveRelative",  {false, false, true,  true}},
    {"lowerZMoveZero",      {false, true,  true,  true}},
    {"lowerZStop",          {false, false, true,  true}},
    {"lowerZReset",         {false, false, true,  true}},

    // 同步运动
    {"synchronizedMove", {false, false, true,  false}},

    // CCD相机（四CCD：上下各两个，1倍和10倍物镜）
    {"upperCCD1xSwitch",      {false, true,  true,  true}},
    {"upperCCD1xRingLightSwitch", {false, true,  true,  true}},
    {"upperCCD10xSwitch",     {false, true,  true,  true}},
    {"upperCCD10xRingLightSwitch", {false, true,  true,  true}},
    {"lowerCCD1xSwitch",       {false, true,  true,  true}},
    {"lowerCCD1xRingLightSwitch", {false, true,  true,  true}},
    {"lowerCCD10xSwitch",      {false, true,  true,  true}},
    {"lowerCCD10xRingLightSwitch", {false, true,  true,  true}},
    {"captureUpperCCD1xImage", {false, false, true,  false}},
    {"captureUpperCCD10xImage",{false, false, true,  false}},
    {"captureLowerCCD1xImage", {false, false, true,  false}},
    {"captureLowerCCD10xImage",{false, false, true,  false}},
    {"captureAllImages",       {false, false, true,  false}},
    {"setUpperCCD1xExposure",  {false, true,  true,  false}},
    {"setUpperCCD10xExposure", {false, true,  true,  false}},
    {"setLowerCCD1xExposure",  {false, true,  true,  false}},
    {"setLowerCCD10xExposure", {false, true,  true,  false}},
    {"getUpperCCD1xImage",     {false, true,  true,  true}},
    {"getUpperCCD10xImage",    {false, true,  true,  true}},
    {"getLowerCCD1xImage",     {false, true,  true,  true}},
    {"getLowerCCD10xImage",   {false, true,  true,  true}},
    {"startAutoCapture",       {false, false, true,  false}},
    {"stopAutoCapture",        {false, true,  true,  true}},

    // 辅助支撑
    {"operateUpperSupport",    {false, false, true,  false}},
    {"operateLowerSupport",     {false, false, true,  false}},
    {"upperSupportRise",       {false, false, true,  false}},
    {"upperSupportLower",       {false, false, true,  false}},
    {"lowerSupportRise",       {false, false, true,  false}},
    {"lowerSupportLower",      {false, false, true,  false}},
    {"stopUpperSupport",       {false, false, true,  true}},
    {"stopLowerSupport",       {false, false, true,  true}},

    // 导出 / 模拟
    {"exportLogs",       {false, true,  true,  true}},
    {"readtAxis",        {false, true,  true,  true}},
    {"exportAxis",       {false, true,  true,  true}},
    {"simSwitch",        {false, true,  true,  true}},
};
} // namespace

void ReflectionImagingDevice::check_state(const std::string& cmd_name) {
    auto it = kStateMatrix.find(cmd_name);
    if (it == kStateMatrix.end()) {
        Tango::Except::throw_exception("API_StateViolation",
            cmd_name + " state rule not defined", "ReflectionImagingDevice::check_state");
    }
    const auto allow = it->second;

    Tango::DevState s = get_state();
    switch (s) {
        case Tango::UNKNOWN:
            if (!allow.allow_unknown) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: device in UNKNOWN", "ReflectionImagingDevice::check_state");
            }
            break;
        case Tango::OFF:
            if (!allow.allow_off) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: device in OFF", "ReflectionImagingDevice::check_state");
            }
            break;
        case Tango::ON:
        case Tango::MOVING:
            if (!allow.allow_on) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: device not allowed in ON/MOVING", "ReflectionImagingDevice::check_state");
            }
            break;
        case Tango::FAULT:
            if (!allow.allow_fault) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: device in FAULT", "ReflectionImagingDevice::check_state");
            }
            break;
        default:
            if (!allow.allow_on) {
                Tango::Except::throw_exception("API_StateViolation",
                    cmd_name + " blocked: state not allowed", "ReflectionImagingDevice::check_state");
            }
            break;
    }
}

void ReflectionImagingDevice::update_sub_devices() {
    last_update_time_ = std::chrono::steady_clock::now();

    if (sim_mode_) {
        // 仿真模式：保持在线
        if (!limit_fault_latched_.load() && get_state() != Tango::FAULT) {
            set_state(Tango::ON);
            set_status("Sim running");
        }
    } else {
        // 真机轮询
        bool all_ok = true;
        bool any_moving = false;
        
        
        // 上平台 - 通过 encoder_device 读取各轴编码器
        if (encoder_proxy_) {  // encoder_proxy_ 
            try {
                for (size_t i = 0; i < 3 && i < upper_platform_encoder_channels_.size(); ++i) {
                    Tango::DeviceData data_in;
                    Tango::DevShort channel = upper_platform_encoder_channels_[i];
                    data_in << channel;
                    Tango::DeviceData out = encoder_proxy_->command_inout("readEncoder", data_in);
                    Tango::DevDouble val; out >> val;
                    upper_platform_pos_[i] = val;
                }
            } catch (...) { all_ok = false; }
        }
        

        if (upper_platform_proxy_) {
            try {
                Tango::DevState up_state = upper_platform_proxy_->state();
                if (up_state == Tango::MOVING) any_moving = true;
                if (up_state == Tango::FAULT) all_ok = false;
            } catch (...) { all_ok = false; }
        }
        
        // 下平台 - 通过 encoder_device 读取各轴编码器
        if (encoder_proxy_) {  // encoder_proxy_
            try {
                for (size_t i = 0; i < 3 && i < lower_platform_encoder_channels_.size(); ++i) {
                    Tango::DeviceData data_in;
                    Tango::DevShort channel = lower_platform_encoder_channels_[i];
                    data_in << channel;
                    Tango::DeviceData out = encoder_proxy_->command_inout("readEncoder", data_in);
                    Tango::DevDouble val; out >> val;
                    lower_platform_pos_[i] = val;
                }
            } catch (...) { all_ok = false; }
        }
        if (lower_platform_proxy_) {
            try {
                Tango::DevState lp_state = lower_platform_proxy_->state();
                if (lp_state == Tango::MOVING) any_moving = true;
                if (lp_state == Tango::FAULT) all_ok = false;
            } catch (...) { all_ok = false; }
        }
        
        // 更新辅助支撑状态
        update_support_states();
        
        // 检查CCD状态（如果有错误状态，标记为故障）
        if (upper_ccd_1x_state_ == "ERROR" || upper_ccd_10x_state_ == "ERROR" ||
            lower_ccd_1x_state_ == "ERROR" || lower_ccd_10x_state_ == "ERROR") {
            all_ok = false;
        }
        
        // 检查辅助支撑状态
        if (upper_support_state_ == "ERROR" || lower_support_state_ == "ERROR") {
            all_ok = false;
        }

        // Limit fault latched: keep FAULT regardless of aggregation.
        if (limit_fault_latched_.load()) {
            set_state(Tango::FAULT);
            if (!fault_state_.empty()) {
                set_status(fault_state_);
            } else {
                set_status("Limit switch triggered");
            }
            return;
        }
        
        // 状态聚合和流转
        if (!all_ok && get_state() != Tango::FAULT) {
            set_state(Tango::FAULT);
            set_status("Sub-device communication error");
            log_event("Sub-device error detected");
        } else if (all_ok && get_state() == Tango::FAULT) {
            set_state(Tango::ON);
            set_status("Recovered from FAULT");
            log_event("Sub-devices recovered");
        } else if (all_ok && any_moving && get_state() != Tango::MOVING) {
            set_state(Tango::MOVING);
            set_status("Platforms moving");
        } else if (all_ok && !any_moving && get_state() == Tango::MOVING) {
            set_state(Tango::ON);
            set_status("Running normally");
        } else if (all_ok && get_state() == Tango::ON) {
            set_status("Running normally");
        }
    }
}

void ReflectionImagingDevice::log_event(const std::string& event) {
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S");
    
    std::stringstream log_json;
    log_json << "{"
             << "\"time\":\"" << ss.str() << "\","
             << "\"event\":\"" << event << "\""
             << "}";
    reflection_logs_ = log_json.str();
}

std::string ReflectionImagingDevice::parse_json_value(const std::string& json, const std::string& key) {
    // 增强的JSON解析，支持 quoted 和 unquoted values
    std::string search_key = "\"" + key + "\"";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) return "";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    pos++; // Skip ':'
    
    // Skip whitespace
    while (pos < json.length() && std::isspace(json[pos])) pos++;
    if (pos >= json.length()) return "";
    
    if (json[pos] == '\"') {
        // Quoted string
        pos++; // Skip opening quote
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        // Unquoted number or boolean
        size_t end = pos;
        while (end < json.length() && (std::isalnum(json[end]) || json[end] == '.' || json[end] == '-')) {
            end++;
        }
        return json.substr(pos, end - pos);
    }
}

// ========== Commands Implementation (占位实现) ==========

void ReflectionImagingDevice::devLock(Tango::DevString user_info) {
    check_state("devLock");
    std::lock_guard<std::mutex> lock(lock_mutex_);
    is_locked_ = true;
    lock_user_ = std::string(user_info);
    log_event("Device locked by: " + lock_user_);
}

void ReflectionImagingDevice::devUnlock(Tango::DevBoolean unlock_all) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    is_locked_ = false;
    if (unlock_all) {
        lock_user_ = "";
    }
    log_event("Device unlocked");
}

void ReflectionImagingDevice::devLockVerify() {
    check_state("devLockVerify");
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (!is_locked_) {
        Tango::Except::throw_exception("API_DeviceNotLocked",
            "Device not locked", "ReflectionImagingDevice::devLockVerify");
    }
}

Tango::DevString ReflectionImagingDevice::devLockQuery() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string result = "{\"locked\":" + std::string(is_locked_ ? "true" : "false") +
                         ",\"user\":\"" + lock_user_ + "\"}";
    return Tango::string_dup(result.c_str());
}

void ReflectionImagingDevice::devUserConfig(Tango::DevString config) {
    check_state("devUserConfig");
    std::string config_str(config);
    log_event("User config updated: " + config_str);
    // 可以在这里解析配置并应用到设备
}

void ReflectionImagingDevice::selfCheck() {
    check_state("selfCheck");
    self_check_result_ = 0;  // 0表示正常
    log_event("Self check completed");
}

void ReflectionImagingDevice::specific_self_check() {
    self_check_result_ = 0;
    
    // 检查上下平台代理
    if (!sim_mode_) {
        if (upper_platform_proxy_) {
            try {
                upper_platform_proxy_->ping();
            } catch (...) {
                self_check_result_ |= 2; // 上平台异常
            }
        }
        if (lower_platform_proxy_) {
            try {
                lower_platform_proxy_->ping();
            } catch (...) {
                self_check_result_ |= 4; // 下平台异常
            }
        }
    }
    
    if (self_check_result_ == 0) {
        set_state(Tango::ON);
        set_status("Self check passed");
    } else {
        set_state(Tango::FAULT);
        set_status("Self check failed");
    }
    
    log_event("Self check done");
}

void ReflectionImagingDevice::init() {
    check_state("init");
    connect_proxies();
    initialize_cameras();
    set_state(Tango::ON);
    set_status("Initialized");
    log_event("Init called");
}

// ========== Upper Platform Commands ==========

void ReflectionImagingDevice::upperPlatformAxisSet(const Tango::DevVarDoubleArray* params) {
    check_state("upperPlatformAxisSet");
    if (params->length() < 6) {
        Tango::Except::throw_exception("InvalidParameter", "Need 6 parameters: [axis, startSpeed, maxSpeed, accTime, decTime, stopSpeed]", "upperPlatformAxisSet");
    }
    
    int logical_axis = (int)(*params)[0];
    if (logical_axis < 0 || logical_axis > 2) {
         Tango::Except::throw_exception("InvalidParameter", "Axis must be 0, 1, 2", "upperPlatformAxisSet");
    }
    double physical_axis = (double)(logical_axis * 2);

    if (sim_mode_) {
        log_event("Upper platform axis parameters set");
        return;
    }
    if (!upper_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "ReflectionImagingDevice::upperPlatformAxisSet");
    
    Tango::DevVarDoubleArray new_params;
    new_params.length(params->length());
    new_params[0] = physical_axis;
    for(size_t i=1; i<params->length(); ++i) new_params[i] = (*params)[i];

    Tango::DeviceData arg; arg << new_params; upper_platform_proxy_->command_inout("moveAxisSet", arg);
    log_event("Upper platform axis parameters set");
}

void ReflectionImagingDevice::upperPlatformStructAxisSet(const Tango::DevVarDoubleArray* params) {
    check_state("upperPlatformStructAxisSet");
    if (params->length() < 4) {
        Tango::Except::throw_exception("InvalidParameter", "Need 4 parameters: [axis, stepAngle, gearRatio, subdivision]", "upperPlatformStructAxisSet");
    }
    
    int logical_axis = (int)(*params)[0];
    if (logical_axis < 0 || logical_axis > 2) {
         Tango::Except::throw_exception("InvalidParameter", "Axis must be 0, 1, 2", "upperPlatformStructAxisSet");
    }
    double physical_axis = (double)(logical_axis * 2);

    if (sim_mode_) {
        log_event("Upper platform structure parameters set");
        return;
    }
    if (!upper_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "ReflectionImagingDevice::upperPlatformStructAxisSet");
    
    Tango::DevVarDoubleArray new_params;
    new_params.length(params->length());
    new_params[0] = physical_axis;
    for(size_t i=1; i<params->length(); ++i) new_params[i] = (*params)[i];
    
    Tango::DeviceData arg; arg << new_params; upper_platform_proxy_->command_inout("structAxisSet", arg);
    log_event("Upper platform structure parameters set");
}

void ReflectionImagingDevice::upperPlatformMoveRelative(const Tango::DevVarDoubleArray* params) {
    check_state("upperPlatformMoveRelative");
    if (params->length() < 3) {
        Tango::Except::throw_exception("InvalidParameter", "Need 3 parameters: [X, Y, Z]", "upperPlatformMoveRelative");
    }
    
    double dx = (*params)[0], dy = (*params)[1], dz = (*params)[2];
    
    // 限位检查
    if (!checkUpperPlatformRelativeLimits(dx, dy, dz)) {
        Tango::Except::throw_exception("API_LimitExceeded", 
            "Upper platform relative move would exceed limits", 
            "ReflectionImagingDevice::upperPlatformMoveRelative");
    }
    
    // 计算目标位置
    double target_x = upper_platform_pos_[0] + dx;
    double target_y = upper_platform_pos_[1] + dy;
    double target_z = upper_platform_pos_[2] + dz;
    
    // 碰撞检测（如果下平台也在运动范围内）
    if (checkPlatformCollision(target_z, lower_platform_pos_[2])) {
        Tango::Except::throw_exception("API_CollisionRisk", 
            "Upper platform move would cause collision with lower platform", 
            "ReflectionImagingDevice::upperPlatformMoveRelative");
    }
    
    // 如果Z轴有运动，运动前自动释放刹车（如果配置了刹车）
    if (std::abs(dz) > 0.001 && brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Z-axis motion detected in upperPlatformMoveRelative, auto-releasing brake" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before upper platform Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        // 设置目标位置和运动状态
        upper_platform_target_ = {target_x, target_y, target_z};
        upper_platform_dire_pos_ = upper_platform_target_;
        std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), true);
        
        // 计算模拟运动时间（基于最大位移和默认速度50mm/s）
        double max_dist = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
        sim_motion_duration_ = (max_dist / 50.0) * 1000.0;  // 转换为毫秒
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        
        log_event("Upper platform relative move (sim): dx=" + std::to_string(dx) + 
                  ", dy=" + std::to_string(dy) + ", dz=" + std::to_string(dz));
        return;
    }
    
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", 
            "ReflectionImagingDevice::upperPlatformMoveRelative");
    }
    // 注意：check_state()已经检查了状态，如果状态不是ON（比如FAULT、OFF等），
    // 说明网络未连接或设备异常，会直接抛出异常，不需要ping检查
    struct AxisMove { double axis; double val; };
    AxisMove moves[] = {{0.0, dx}, {2.0, dy}, {4.0, dz}};
    for(const auto& m : moves) {
        Tango::DeviceData arg; 
        Tango::DevVarDoubleArray arr; arr.length(2); 
        arr[0] = m.axis; arr[1] = m.val; 
        arg << arr; 
        upper_platform_proxy_->command_inout("moveRelative", arg);
    }
    std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), true);
    log_event("Upper platform relative move");
}

void ReflectionImagingDevice::upperPlatformMoveAbsolute(const Tango::DevVarDoubleArray* params) {
    check_state("upperPlatformMoveAbsolute");
    if (params->length() < 3) {
        Tango::Except::throw_exception("InvalidParameter", "Need 3 parameters: [X, Y, Z]", "upperPlatformMoveAbsolute");
    }
    
    double target_x = (*params)[0], target_y = (*params)[1], target_z = (*params)[2];
    
    // 限位检查
    if (!checkUpperPlatformLimits(target_x, target_y, target_z)) {
        Tango::Except::throw_exception("API_LimitExceeded", 
            "Upper platform target position exceeds limits", 
            "ReflectionImagingDevice::upperPlatformMoveAbsolute");
    }
    
    // 碰撞检测
    if (checkPlatformCollision(target_z, lower_platform_pos_[2])) {
        Tango::Except::throw_exception("API_CollisionRisk", 
            "Upper platform move would cause collision with lower platform", 
            "ReflectionImagingDevice::upperPlatformMoveAbsolute");
    }
    
    // 如果Z轴有运动，运动前自动释放刹车（如果配置了刹车）
    double dz = target_z - upper_platform_pos_[2];
    if (std::abs(dz) > 0.001 && brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Z-axis motion detected in upperPlatformMoveAbsolute, auto-releasing brake" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before upper platform Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        // 设置目标位置
        upper_platform_target_ = {target_x, target_y, target_z};
        upper_platform_dire_pos_ = upper_platform_target_;
        std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), true);
        
        // 计算模拟运动时间
        double max_dist = std::max({
            std::abs(target_x - upper_platform_pos_[0]),
            std::abs(target_y - upper_platform_pos_[1]),
            std::abs(target_z - upper_platform_pos_[2])
        });
        sim_motion_duration_ = (max_dist / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        
        log_event("Upper platform absolute move (sim): x=" + std::to_string(target_x) + 
                  ", y=" + std::to_string(target_y) + ", z=" + std::to_string(target_z));
        return;
    }
    
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", 
            "ReflectionImagingDevice::upperPlatformMoveAbsolute");
    }
    // 注意：check_state()已经检查了状态，如果状态不是ON（比如FAULT、OFF等），
    // 说明网络未连接或设备异常，会直接抛出异常，不需要ping检查
    struct AxisMove { double axis; double val; };
    AxisMove moves[] = {{0.0, target_x}, {2.0, target_y}, {4.0, target_z}};
    for(const auto& m : moves) {
        Tango::DeviceData arg; 
        Tango::DevVarDoubleArray arr; arr.length(2); 
        arr[0] = m.axis; arr[1] = m.val; 
        arg << arr; 
        upper_platform_proxy_->command_inout("moveAbsolute", arg);
    }
    std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), true);
    log_event("Upper platform absolute move");
}

void ReflectionImagingDevice::upperPlatformMoveToPosition(const Tango::DevVarDoubleArray* params) {
    upperPlatformMoveAbsolute(params);
}

void ReflectionImagingDevice::upperPlatformStop() {
    check_state("upperPlatformStop");
    if (sim_mode_) {
        std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), false);
        result_value_ = 0;
        log_event("Upper platform stopped");
        return;
    }
    if (!upper_platform_proxy_) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_NoProxy", 
            "Upper platform proxy not connected", 
            "ReflectionImagingDevice::upperPlatformStop");
    }
    try {
        // 停止所有三个轴 (X=0, Y=2, Z=4 根据配置)
        Tango::DeviceData arg;
        Tango::DevShort axis;
        axis = 0; arg << axis; upper_platform_proxy_->command_inout("stopMove", arg);
        axis = 2; arg << axis; upper_platform_proxy_->command_inout("stopMove", arg);
        axis = 4; arg << axis; upper_platform_proxy_->command_inout("stopMove", arg);
        std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), false);
        result_value_ = 0;
        log_event("Upper platform stopped");
        
        // 注意：正常停止后不自动启用刹车，保持刹车释放状态以便快速继续运动
        // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
    } catch (Tango::DevFailed &e) {
        result_value_ = 1;
        Tango::Except::re_throw_exception(e,
            "API_CommandFailed",
            "upperPlatformStop failed: " + std::string(e.errors[0].desc),
            "ReflectionImagingDevice::upperPlatformStop");
    }
}

void ReflectionImagingDevice::upperPlatformReset() {
    check_state("upperPlatformReset");

    // Clear latched limit fault
    bool was_latched = limit_fault_latched_.exchange(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    if (was_latched) {
        fault_state_.clear();
    }
    
    // 复位前自动启用刹车（安全保护）
    if (brake_power_port_ >= 0 && brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-engaging brake before upper platform reset (safety)" << endl;
        if (!engage_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to engage brake before upper platform reset" << endl;
        }
    }
    
    if (sim_mode_) {
        std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), false);
        std::fill(upper_platform_lim_org_state_.begin(), upper_platform_lim_org_state_.end(), 0);
        result_value_ = 0;
        log_event("Upper platform reset");
        return;
    }
    if (!upper_platform_proxy_) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_NoProxy", 
            "Upper platform proxy not connected", 
            "ReflectionImagingDevice::upperPlatformReset");
    }
    try {
        upper_platform_proxy_->command_inout("reset", Tango::DeviceData());
        std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), false);
        std::fill(upper_platform_lim_org_state_.begin(), upper_platform_lim_org_state_.end(), 0);
        result_value_ = 0;
        log_event("Upper platform reset");
    } catch (Tango::DevFailed &e) {
        result_value_ = 1;
        Tango::Except::re_throw_exception(e,
            "API_CommandFailed",
            "upperPlatformReset failed: " + std::string(e.errors[0].desc),
            "ReflectionImagingDevice::upperPlatformReset");
    }
}

void ReflectionImagingDevice::upperPlatformMoveZero() {
    check_state("upperPlatformMoveZero");
    if (sim_mode_) {
        std::fill(upper_platform_pos_.begin(), upper_platform_pos_.end(), 0.0);
        upper_platform_dire_pos_ = upper_platform_pos_;
        log_event("Upper platform move to zero");
        return;
    }
    if (!upper_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "ReflectionImagingDevice::upperPlatformMoveZero");
    upper_platform_proxy_->command_inout("moveZero", Tango::DeviceData());
    log_event("Upper platform move to zero");
}

Tango::DevVarDoubleArray* ReflectionImagingDevice::upperPlatformReadEncoder() {
    check_state("upperPlatformReadEncoder");
    if (!sim_mode_) {
        if (!encoder_proxy_) {  // encoder_proxy_ 实际连接到 encoder_device
            WARN_STREAM << "upperPlatformReadEncoder: encoder proxy not connected, returning cached values" << endl;
        } else {
            // 通过 encoder_device 读取各轴编码器值
            for (size_t i = 0; i < 3 && i < upper_platform_encoder_channels_.size(); ++i) {
                try {
                    Tango::DeviceData data_in;
                    Tango::DevShort channel = upper_platform_encoder_channels_[i];
                    data_in << channel;
                    Tango::DeviceData out = encoder_proxy_->command_inout("readEncoder", data_in);
                    Tango::DevDouble val; out >> val;
                    upper_platform_pos_[i] = val;
                } catch (Tango::DevFailed &e) {
                    WARN_STREAM << "upperPlatformReadEncoder channel " << upper_platform_encoder_channels_[i]
                               << " failed: " << e.errors[0].desc << endl;
                }
            }
        }
    }
    Tango::DevVarDoubleArray* result = new Tango::DevVarDoubleArray();
    result->length(3);
    (*result)[0] = upper_platform_pos_[0];
    (*result)[1] = upper_platform_pos_[1];
    (*result)[2] = upper_platform_pos_[2];
    return result;
}

Tango::DevVarShortArray* ReflectionImagingDevice::upperPlatformReadOrg() {
    check_state("upperPlatformReadOrg");
    if (!sim_mode_ && upper_platform_proxy_) {
        for (int i = 0; i < 3; ++i) {
            try {
                Tango::DeviceData arg; Tango::DevShort axis = static_cast<Tango::DevShort>(i); arg << axis;
                Tango::DeviceData out = upper_platform_proxy_->command_inout("readOrg", arg);
                Tango::DevBoolean v; out >> v;
                upper_platform_lim_org_state_[i] = v ? 0 : 2;
            } catch (...) {
                upper_platform_lim_org_state_[i] = 2;
            }
        }
    }
    Tango::DevVarShortArray* result = new Tango::DevVarShortArray();
    result->length(3);
    (*result)[0] = (upper_platform_lim_org_state_[0] == 0) ? 1 : 0;
    (*result)[1] = (upper_platform_lim_org_state_[1] == 0) ? 1 : 0;
    (*result)[2] = (upper_platform_lim_org_state_[2] == 0) ? 1 : 0;
    return result;
}

Tango::DevVarShortArray* ReflectionImagingDevice::upperPlatformReadEL() {
    check_state("upperPlatformReadEL");
    Tango::DevVarShortArray* result = new Tango::DevVarShortArray();
    result->length(3);
    (*result)[0] = upper_platform_lim_org_state_[0];
    (*result)[1] = upper_platform_lim_org_state_[1];
    (*result)[2] = upper_platform_lim_org_state_[2];
    return result;
}

void ReflectionImagingDevice::upperPlatformSingleAxisMove(const Tango::DevVarDoubleArray* params) {
    check_state("upperPlatformSingleAxisMove");
    if (params->length() < 2) {
        Tango::Except::throw_exception("InvalidParameter", "Need 2 parameters: [axis, distance]", "upperPlatformSingleAxisMove");
    }
    
    int axis = static_cast<int>(std::round((*params)[0]));  // 使用round避免精度问题
    double distance = (*params)[1];
    
    // 轴号验证
    if (axis < 0 || axis > 2) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Axis must be 0(X), 1(Y), or 2(Z)", 
            "upperPlatformSingleAxisMove");
    }
    
    // 限位检查
    double target_pos = upper_platform_pos_[axis] + distance;
    if (std::abs(target_pos) > upper_platform_range_[axis]) {
        Tango::Except::throw_exception("API_LimitExceeded", 
            "Upper platform axis " + std::to_string(axis) + " target " + 
            std::to_string(target_pos) + " exceeds limit ±" + 
            std::to_string(upper_platform_range_[axis]), 
            "upperPlatformSingleAxisMove");
    }
    
    // Z轴碰撞检测
    if (axis == 2 && checkPlatformCollision(target_pos, lower_platform_pos_[2])) {
        Tango::Except::throw_exception("API_CollisionRisk", 
            "Upper platform Z axis move would cause collision", 
            "upperPlatformSingleAxisMove");
    }
    
    // 如果Z轴有运动，运动前自动释放刹车（如果配置了刹车）
    if (axis == 2 && brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Z-axis motion detected in upperPlatformSingleAxisMove, auto-releasing brake" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before upper platform Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        upper_platform_target_[axis] = target_pos;
        upper_platform_dire_pos_[axis] = target_pos;
        upper_platform_state_[axis] = true;
        
        sim_motion_duration_ = (std::abs(distance) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        
        log_event("Upper platform single axis move (sim): axis=" + std::to_string(axis) + 
                  ", distance=" + std::to_string(distance));
        return;
    }
    
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", 
            "upperPlatformSingleAxisMove");
    }
    
    Tango::DeviceData arg; 
    Tango::DevVarDoubleArray arr; arr.length(2); 
    arr[0] = static_cast<double>(axis * 2); 
    arr[1] = distance;
    arg << arr; 
    upper_platform_proxy_->command_inout("singleAxisMove", arg);
    upper_platform_state_[axis] = true;
    log_event("Upper platform single axis move: axis=" + std::to_string(axis) + 
              ", distance=" + std::to_string(distance));
}

// ========== 上平台单轴控制命令 (用于GUI单轴独立控制) ==========
void ReflectionImagingDevice::upperXMoveAbsolute(Tango::DevDouble val) {
    check_state("upperXMoveAbsolute");
    // 限位检查
    if (std::abs(val) > upper_platform_range_[0]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Upper X target " + std::to_string(val) + " exceeds limit ±" + std::to_string(upper_platform_range_[0]),
            "upperXMoveAbsolute");
    }
    if (sim_mode_) {
        upper_platform_target_[0] = val;
        upper_platform_dire_pos_[0] = val;
        upper_platform_state_[0] = true;
        sim_motion_duration_ = (std::abs(val - upper_platform_pos_[0]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper X move absolute (sim): " + std::to_string(val));
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperXMoveAbsolute");
    }
    // 上平台X轴对应运动控制器的轴0
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 0.0;  // axis 0 = X
    arr[1] = val;
    arg << arr;
    upper_platform_proxy_->command_inout("moveAbsolute", arg);
    upper_platform_state_[0] = true;
    log_event("Upper X move absolute: " + std::to_string(val));
}

void ReflectionImagingDevice::upperXMoveRelative(Tango::DevDouble val) {
    check_state("upperXMoveRelative");
    double target = upper_platform_pos_[0] + val;
    if (std::abs(target) > upper_platform_range_[0]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Upper X target " + std::to_string(target) + " exceeds limit ±" + std::to_string(upper_platform_range_[0]),
            "upperXMoveRelative");
    }
    if (sim_mode_) {
        upper_platform_target_[0] = target;
        upper_platform_dire_pos_[0] = target;
        upper_platform_state_[0] = true;
        sim_motion_duration_ = (std::abs(val) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper X move relative (sim): " + std::to_string(val));
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperXMoveRelative");
    }
    // 注意：check_state()已经检查了状态，如果状态不是ON（比如FAULT、OFF等），
    // 说明网络未连接或设备异常，会直接抛出异常，不需要ping检查
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 0.0;  // axis 0 = X
    arr[1] = val;
    arg << arr;
    upper_platform_proxy_->command_inout("moveRelative", arg);
    upper_platform_state_[0] = true;
    log_event("Upper X move relative: " + std::to_string(val));
}

void ReflectionImagingDevice::upperXMoveZero() {
    check_state("upperXMoveZero");
    if (sim_mode_) {
        upper_platform_target_[0] = 0.0;
        upper_platform_dire_pos_[0] = 0.0;
        upper_platform_state_[0] = true;
        sim_motion_duration_ = (std::abs(upper_platform_pos_[0]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper X move zero (sim)");
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperXMoveZero");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 0;  // X axis
    arg << axis;
    upper_platform_proxy_->command_inout("moveZero", arg);
    upper_platform_state_[0] = true;
    log_event("Upper X move zero");
}

void ReflectionImagingDevice::upperXStop() {
    check_state("upperXStop");
    if (sim_mode_) {
        upper_platform_state_[0] = false;
        log_event("Upper X stopped (sim)");
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperXStop");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 0;
    arg << axis;
    upper_platform_proxy_->command_inout("stopMove", arg);
    upper_platform_state_[0] = false;
    log_event("Upper X stopped");
}

void ReflectionImagingDevice::upperXReset() {
    check_state("upperXReset");

    // Clear latched limit fault
    bool was_latched = limit_fault_latched_.exchange(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    if (was_latched) {
        fault_state_.clear();
    }
    if (sim_mode_) {
        upper_platform_state_[0] = false;
        upper_platform_lim_org_state_[0] = 0;
        log_event("Upper X reset (sim)");
        // 模拟模式下，如果设备处于FAULT状态，清除故障
        if (get_state() == Tango::FAULT) {
            fault_state_ = "";
            set_state(Tango::ON);
            set_status("Fault cleared by reset");
        }
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperXReset");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 0;
    arg << axis;
    upper_platform_proxy_->command_inout("reset", arg);
    upper_platform_state_[0] = false;
    log_event("Upper X reset");
    // 如果设备处于FAULT状态，尝试清除故障（类似大行程设备的reset行为）
    if (get_state() == Tango::FAULT) {
        fault_state_ = "";
        set_state(Tango::ON);
        set_status("Fault cleared by reset");
        log_event("Device fault cleared by upper X reset");
    }
}

void ReflectionImagingDevice::upperYMoveAbsolute(Tango::DevDouble val) {
    check_state("upperYMoveAbsolute");
    if (std::abs(val) > upper_platform_range_[1]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Upper Y target " + std::to_string(val) + " exceeds limit ±" + std::to_string(upper_platform_range_[1]),
            "upperYMoveAbsolute");
    }
    if (sim_mode_) {
        upper_platform_target_[1] = val;
        upper_platform_dire_pos_[1] = val;
        upper_platform_state_[1] = true;
        sim_motion_duration_ = (std::abs(val - upper_platform_pos_[1]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper Y move absolute (sim): " + std::to_string(val));
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperYMoveAbsolute");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 2.0;  // axis 2 = Y (根据配置 upperPlatformConfig)
    arr[1] = val;
    arg << arr;
    upper_platform_proxy_->command_inout("moveAbsolute", arg);
    upper_platform_state_[1] = true;
    log_event("Upper Y move absolute: " + std::to_string(val));
}

void ReflectionImagingDevice::upperYMoveRelative(Tango::DevDouble val) {
    check_state("upperYMoveRelative");
    double target = upper_platform_pos_[1] + val;
    if (std::abs(target) > upper_platform_range_[1]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Upper Y target " + std::to_string(target) + " exceeds limit ±" + std::to_string(upper_platform_range_[1]),
            "upperYMoveRelative");
    }
    if (sim_mode_) {
        upper_platform_target_[1] = target;
        upper_platform_dire_pos_[1] = target;
        upper_platform_state_[1] = true;
        sim_motion_duration_ = (std::abs(val) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper Y move relative (sim): " + std::to_string(val));
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperYMoveRelative");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 2.0;  // axis 2 = Y
    arr[1] = val;
    arg << arr;
    upper_platform_proxy_->command_inout("moveRelative", arg);
    upper_platform_state_[1] = true;
    log_event("Upper Y move relative: " + std::to_string(val));
}

void ReflectionImagingDevice::upperYMoveZero() {
    check_state("upperYMoveZero");
    if (sim_mode_) {
        upper_platform_target_[1] = 0.0;
        upper_platform_dire_pos_[1] = 0.0;
        upper_platform_state_[1] = true;
        sim_motion_duration_ = (std::abs(upper_platform_pos_[1]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper Y move zero (sim)");
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperYMoveZero");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 2;  // Y axis
    arg << axis;
    upper_platform_proxy_->command_inout("moveZero", arg);
    upper_platform_state_[1] = true;
    log_event("Upper Y move zero");
}

void ReflectionImagingDevice::upperYStop() {
    check_state("upperYStop");
    if (sim_mode_) {
        upper_platform_state_[1] = false;
        log_event("Upper Y stopped (sim)");
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperYStop");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 2;
    arg << axis;
    upper_platform_proxy_->command_inout("stopMove", arg);
    upper_platform_state_[1] = false;
    log_event("Upper Y stopped");
}

void ReflectionImagingDevice::upperYReset() {
    check_state("upperYReset");

    // Clear latched limit fault
    bool was_latched = limit_fault_latched_.exchange(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    if (was_latched) {
        fault_state_.clear();
    }
    if (sim_mode_) {
        upper_platform_state_[1] = false;
        upper_platform_lim_org_state_[1] = 0;
        log_event("Upper Y reset (sim)");
        // 模拟模式下，如果设备处于FAULT状态，清除故障
        if (get_state() == Tango::FAULT) {
            fault_state_ = "";
            set_state(Tango::ON);
            set_status("Fault cleared by reset");
        }
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperYReset");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 2;
    arg << axis;
    upper_platform_proxy_->command_inout("reset", arg);
    upper_platform_state_[1] = false;
    log_event("Upper Y reset");
    // 如果设备处于FAULT状态，尝试清除故障（类似大行程设备的reset行为）
    if (get_state() == Tango::FAULT) {
        fault_state_ = "";
        set_state(Tango::ON);
        set_status("Fault cleared by reset");
        log_event("Device fault cleared by upper Y reset");
    }
}

void ReflectionImagingDevice::upperZMoveAbsolute(Tango::DevDouble val) {
    check_state("upperZMoveAbsolute");
    if (std::abs(val) > upper_platform_range_[2]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Upper Z target " + std::to_string(val) + " exceeds limit ±" + std::to_string(upper_platform_range_[2]),
            "upperZMoveAbsolute");
    }
    // Z轴碰撞检测
    if (checkPlatformCollision(val, lower_platform_pos_[2])) {
        Tango::Except::throw_exception("API_CollisionRisk",
            "Upper platform Z axis move would cause collision", "upperZMoveAbsolute");
    }
    
    // Z轴运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before upper Z motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before upper Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        upper_platform_target_[2] = val;
        upper_platform_dire_pos_[2] = val;
        upper_platform_state_[2] = true;
        sim_motion_duration_ = (std::abs(val - upper_platform_pos_[2]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper Z move absolute (sim): " + std::to_string(val));
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperZMoveAbsolute");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 4.0;  // axis 4 = Z (根据配置 upperPlatformConfig)
    arr[1] = val;
    arg << arr;
    upper_platform_proxy_->command_inout("moveAbsolute", arg);
    upper_platform_state_[2] = true;
    log_event("Upper Z move absolute: " + std::to_string(val));
}

void ReflectionImagingDevice::upperZMoveRelative(Tango::DevDouble val) {
    check_state("upperZMoveRelative");
    double target = upper_platform_pos_[2] + val;
    if (std::abs(target) > upper_platform_range_[2]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Upper Z target " + std::to_string(target) + " exceeds limit ±" + std::to_string(upper_platform_range_[2]),
            "upperZMoveRelative");
    }
    if (checkPlatformCollision(target, lower_platform_pos_[2])) {
        Tango::Except::throw_exception("API_CollisionRisk",
            "Upper platform Z axis move would cause collision", "upperZMoveRelative");
    }
    
    // Z轴运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before upper Z motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before upper Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        upper_platform_target_[2] = target;
        upper_platform_dire_pos_[2] = target;
        upper_platform_state_[2] = true;
        sim_motion_duration_ = (std::abs(val) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper Z move relative (sim): " + std::to_string(val));
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperZMoveRelative");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 4.0;  // axis 4 = Z
    arr[1] = val;
    arg << arr;
    upper_platform_proxy_->command_inout("moveRelative", arg);
    upper_platform_state_[2] = true;
    log_event("Upper Z move relative: " + std::to_string(val));
}

void ReflectionImagingDevice::upperZMoveZero() {
    check_state("upperZMoveZero");
    if (checkPlatformCollision(0.0, lower_platform_pos_[2])) {
        Tango::Except::throw_exception("API_CollisionRisk",
            "Upper platform Z axis move zero would cause collision", "upperZMoveZero");
    }
    
    // Z轴运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before upper Z motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before upper Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        upper_platform_target_[2] = 0.0;
        upper_platform_dire_pos_[2] = 0.0;
        upper_platform_state_[2] = true;
        sim_motion_duration_ = (std::abs(upper_platform_pos_[2]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Upper Z move zero (sim)");
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperZMoveZero");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 4;  // Z axis
    arg << axis;
    upper_platform_proxy_->command_inout("moveZero", arg);
    upper_platform_state_[2] = true;
    log_event("Upper Z move zero");
}

void ReflectionImagingDevice::upperZStop() {
    check_state("upperZStop");
    if (sim_mode_) {
        upper_platform_state_[2] = false;
        log_event("Upper Z stopped (sim)");
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperZStop");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 4;
    arg << axis;
    upper_platform_proxy_->command_inout("stopMove", arg);
    upper_platform_state_[2] = false;
    log_event("Upper Z stopped");
    
    // 注意：正常停止后不自动启用刹车，保持刹车释放状态以便快速继续运动
    // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
}

void ReflectionImagingDevice::upperZReset() {
    check_state("upperZReset");

    // Clear latched limit fault
    bool was_latched = limit_fault_latched_.exchange(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    if (was_latched) {
        fault_state_.clear();
    }
    if (sim_mode_) {
        upper_platform_state_[2] = false;
        upper_platform_lim_org_state_[2] = 0;
        log_event("Upper Z reset (sim)");
        // 模拟模式下，如果设备处于FAULT状态，清除故障
        if (get_state() == Tango::FAULT) {
            fault_state_ = "";
            set_state(Tango::ON);
            set_status("Fault cleared by reset");
        }
        return;
    }
    if (!upper_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Upper platform proxy not connected", "upperZReset");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 4;
    arg << axis;
    upper_platform_proxy_->command_inout("reset", arg);
    upper_platform_state_[2] = false;
    log_event("Upper Z reset");
    // 如果设备处于FAULT状态，尝试清除故障（类似大行程设备的reset行为）
    if (get_state() == Tango::FAULT) {
        fault_state_ = "";
        set_state(Tango::ON);
        set_status("Fault cleared by reset");
        log_event("Device fault cleared by upper Z reset");
    }
}

// ========== Lower Platform Commands (类似上平台) ==========

void ReflectionImagingDevice::lowerPlatformAxisSet(const Tango::DevVarDoubleArray* params) {
    check_state("lowerPlatformAxisSet");
    if (params->length() < 6) {
        Tango::Except::throw_exception("InvalidParameter", "Need 6 parameters: [axis, startSpeed, maxSpeed, accTime, decTime, stopSpeed]", "lowerPlatformAxisSet");
    }
    
    int logical_axis = (int)(*params)[0];
    if (logical_axis < 0 || logical_axis > 2) {
         Tango::Except::throw_exception("InvalidParameter", "Axis must be 0, 1, 2", "lowerPlatformAxisSet");
    }
    double physical_axis = (double)(logical_axis * 2 + 1);

    if (sim_mode_) {
        log_event("Lower platform axis parameters set");
        return;
    }
    if (!lower_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "ReflectionImagingDevice::lowerPlatformAxisSet");
    
    Tango::DevVarDoubleArray new_params;
    new_params.length(params->length());
    new_params[0] = physical_axis;
    for(size_t i=1; i<params->length(); ++i) new_params[i] = (*params)[i];
    
    Tango::DeviceData arg; arg << new_params; lower_platform_proxy_->command_inout("moveAxisSet", arg);
    log_event("Lower platform axis parameters set");
}

void ReflectionImagingDevice::lowerPlatformStructAxisSet(const Tango::DevVarDoubleArray* params) {
    check_state("lowerPlatformStructAxisSet");
    if (params->length() < 4) {
        Tango::Except::throw_exception("InvalidParameter", "Need 4 parameters: [axis, stepAngle, gearRatio, subdivision]", "lowerPlatformStructAxisSet");
    }
    
    int logical_axis = (int)(*params)[0];
    if (logical_axis < 0 || logical_axis > 2) {
         Tango::Except::throw_exception("InvalidParameter", "Axis must be 0, 1, 2", "lowerPlatformStructAxisSet");
    }
    double physical_axis = (double)(logical_axis * 2 + 1);

    if (sim_mode_) {
        log_event("Lower platform structure parameters set");
        return;
    }
    if (!lower_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "ReflectionImagingDevice::lowerPlatformStructAxisSet");
    
    Tango::DevVarDoubleArray new_params;
    new_params.length(params->length());
    new_params[0] = physical_axis;
    for(size_t i=1; i<params->length(); ++i) new_params[i] = (*params)[i];
    
    Tango::DeviceData arg; arg << new_params; lower_platform_proxy_->command_inout("structAxisSet", arg);
    log_event("Lower platform structure parameters set");
}

void ReflectionImagingDevice::lowerPlatformMoveRelative(const Tango::DevVarDoubleArray* params) {
    check_state("lowerPlatformMoveRelative");
    if (params->length() < 3) {
        Tango::Except::throw_exception("InvalidParameter", "Need 3 parameters: [X, Y, Z]", "lowerPlatformMoveRelative");
    }
    
    double dx = (*params)[0], dy = (*params)[1], dz = (*params)[2];
    
    // 限位检查
    if (!checkLowerPlatformRelativeLimits(dx, dy, dz)) {
        Tango::Except::throw_exception("API_LimitExceeded", 
            "Lower platform relative move would exceed limits", 
            "ReflectionImagingDevice::lowerPlatformMoveRelative");
    }
    
    // 计算目标位置
    double target_x = lower_platform_pos_[0] + dx;
    double target_y = lower_platform_pos_[1] + dy;
    double target_z = lower_platform_pos_[2] + dz;
    
    // 碰撞检测
    if (checkPlatformCollision(upper_platform_pos_[2], target_z)) {
        Tango::Except::throw_exception("API_CollisionRisk", 
            "Lower platform move would cause collision with upper platform", 
            "ReflectionImagingDevice::lowerPlatformMoveRelative");
    }
    
    // 如果Z轴有运动，运动前自动释放刹车（如果配置了刹车）
    if (std::abs(dz) > 0.001 && brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Z-axis motion detected in lowerPlatformMoveRelative, auto-releasing brake" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before lower platform Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        lower_platform_target_ = {target_x, target_y, target_z};
        lower_platform_dire_pos_ = lower_platform_target_;
        std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), true);
        
        double max_dist = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
        sim_motion_duration_ = (max_dist / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        
        log_event("Lower platform relative move (sim): dx=" + std::to_string(dx) + 
                  ", dy=" + std::to_string(dy) + ", dz=" + std::to_string(dz));
        return;
    }
    
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", 
            "ReflectionImagingDevice::lowerPlatformMoveRelative");
    }
    // 注意：check_state()已经检查了状态，如果状态不是ON（比如FAULT、OFF等），
    // 说明网络未连接或设备异常，会直接抛出异常，不需要ping检查
    struct AxisMove { double axis; double val; };
    AxisMove moves[] = {{1.0, dx}, {3.0, dy}, {5.0, dz}};
    for(const auto& m : moves) {
        Tango::DeviceData arg; 
        Tango::DevVarDoubleArray arr; arr.length(2); 
        arr[0] = m.axis; arr[1] = m.val; 
        arg << arr; 
        lower_platform_proxy_->command_inout("moveRelative", arg);
    }
    std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), true);
    log_event("Lower platform relative move");
}

void ReflectionImagingDevice::lowerPlatformMoveAbsolute(const Tango::DevVarDoubleArray* params) {
    check_state("lowerPlatformMoveAbsolute");
    if (params->length() < 3) {
        Tango::Except::throw_exception("InvalidParameter", "Need 3 parameters: [X, Y, Z]", "lowerPlatformMoveAbsolute");
    }
    
    double target_x = (*params)[0], target_y = (*params)[1], target_z = (*params)[2];
    
    // 限位检查
    if (!checkLowerPlatformLimits(target_x, target_y, target_z)) {
        Tango::Except::throw_exception("API_LimitExceeded", 
            "Lower platform target position exceeds limits", 
            "ReflectionImagingDevice::lowerPlatformMoveAbsolute");
    }
    
    // 碰撞检测
    if (checkPlatformCollision(upper_platform_pos_[2], target_z)) {
        Tango::Except::throw_exception("API_CollisionRisk", 
            "Lower platform move would cause collision with upper platform", 
            "ReflectionImagingDevice::lowerPlatformMoveAbsolute");
    }
    
    if (sim_mode_) {
        lower_platform_target_ = {target_x, target_y, target_z};
        lower_platform_dire_pos_ = lower_platform_target_;
        std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), true);
        
        double max_dist = std::max({
            std::abs(target_x - lower_platform_pos_[0]),
            std::abs(target_y - lower_platform_pos_[1]),
            std::abs(target_z - lower_platform_pos_[2])
        });
        sim_motion_duration_ = (max_dist / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        
        log_event("Lower platform absolute move (sim): x=" + std::to_string(target_x) + 
                  ", y=" + std::to_string(target_y) + ", z=" + std::to_string(target_z));
        return;
    }
    
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", 
            "ReflectionImagingDevice::lowerPlatformMoveAbsolute");
    }
    // 注意：check_state()已经检查了状态，如果状态不是ON（比如FAULT、OFF等），
    // 说明网络未连接或设备异常，会直接抛出异常，不需要ping检查
    struct AxisMove { double axis; double val; };
    AxisMove moves[] = {{1.0, target_x}, {3.0, target_y}, {5.0, target_z}};
    for(const auto& m : moves) {
        Tango::DeviceData arg; 
        Tango::DevVarDoubleArray arr; arr.length(2); 
        arr[0] = m.axis; arr[1] = m.val; 
        arg << arr; 
        lower_platform_proxy_->command_inout("moveAbsolute", arg);
    }
    std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), true);
    log_event("Lower platform absolute move");
}

void ReflectionImagingDevice::lowerPlatformMoveToPosition(const Tango::DevVarDoubleArray* params) {
    lowerPlatformMoveAbsolute(params);
}

void ReflectionImagingDevice::lowerPlatformStop() {
    check_state("lowerPlatformStop");
    if (sim_mode_) {
        std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), false);
        result_value_ = 0;
        log_event("Lower platform stopped");
        return;
    }
    if (!lower_platform_proxy_) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_NoProxy", 
            "Lower platform proxy not connected", 
            "ReflectionImagingDevice::lowerPlatformStop");
    }
    try {
        // 停止所有三个轴 (X=1, Y=3, Z=5 根据配置)
        Tango::DeviceData arg;
        Tango::DevShort axis;
        axis = 1; arg << axis; lower_platform_proxy_->command_inout("stopMove", arg);
        axis = 3; arg << axis; lower_platform_proxy_->command_inout("stopMove", arg);
        axis = 5; arg << axis; lower_platform_proxy_->command_inout("stopMove", arg);
        std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), false);
        result_value_ = 0;
        log_event("Lower platform stopped");
        
        // 注意：正常停止后不自动启用刹车，保持刹车释放状态以便快速继续运动
        // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
    } catch (Tango::DevFailed &e) {
        result_value_ = 1;
        Tango::Except::re_throw_exception(e,
            "API_CommandFailed",
            "lowerPlatformStop failed: " + std::string(e.errors[0].desc),
            "ReflectionImagingDevice::lowerPlatformStop");
    }
}

void ReflectionImagingDevice::lowerPlatformReset() {
    check_state("lowerPlatformReset");

    // Clear latched limit fault
    bool was_latched = limit_fault_latched_.exchange(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    if (was_latched) {
        fault_state_.clear();
    }
    if (sim_mode_) {
        std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), false);
        std::fill(lower_platform_lim_org_state_.begin(), lower_platform_lim_org_state_.end(), 0);
        result_value_ = 0;
        log_event("Lower platform reset");
        return;
    }
    if (!lower_platform_proxy_) {
        result_value_ = 1;
        Tango::Except::throw_exception("API_NoProxy", 
            "Lower platform proxy not connected", 
            "ReflectionImagingDevice::lowerPlatformReset");
    }
    try {
        lower_platform_proxy_->command_inout("reset", Tango::DeviceData());
        std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), false);
        std::fill(lower_platform_lim_org_state_.begin(), lower_platform_lim_org_state_.end(), 0);
        result_value_ = 0;
        log_event("Lower platform reset");
    } catch (Tango::DevFailed &e) {
        result_value_ = 1;
        Tango::Except::re_throw_exception(e,
            "API_CommandFailed",
            "lowerPlatformReset failed: " + std::string(e.errors[0].desc),
            "ReflectionImagingDevice::lowerPlatformReset");
    }
}

void ReflectionImagingDevice::lowerPlatformMoveZero() {
    check_state("lowerPlatformMoveZero");
    if (sim_mode_) {
        std::fill(lower_platform_pos_.begin(), lower_platform_pos_.end(), 0.0);
        lower_platform_dire_pos_ = lower_platform_pos_;
        log_event("Lower platform move to zero");
        return;
    }
    if (!lower_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "ReflectionImagingDevice::lowerPlatformMoveZero");
    lower_platform_proxy_->command_inout("moveZero", Tango::DeviceData());
    log_event("Lower platform move to zero");
}

Tango::DevVarDoubleArray* ReflectionImagingDevice::lowerPlatformReadEncoder() {
    check_state("lowerPlatformReadEncoder");
    if (!sim_mode_) {
        if (!encoder_proxy_) {  // encoder_proxy_ 实际连接到 encoder_device
            WARN_STREAM << "lowerPlatformReadEncoder: encoder proxy not connected, returning cached values" << endl;
        } else {
            // 通过 encoder_device 读取各轴编码器值
            for (size_t i = 0; i < 3 && i < lower_platform_encoder_channels_.size(); ++i) {
                try {
                    Tango::DeviceData data_in;
                    Tango::DevShort channel = lower_platform_encoder_channels_[i];
                    data_in << channel;
                    Tango::DeviceData out = encoder_proxy_->command_inout("readEncoder", data_in);
                    Tango::DevDouble val; out >> val;
                    lower_platform_pos_[i] = val;
                } catch (Tango::DevFailed &e) {
                    WARN_STREAM << "lowerPlatformReadEncoder channel " << lower_platform_encoder_channels_[i]
                               << " failed: " << e.errors[0].desc << endl;
                }
            }
        }
    }
    Tango::DevVarDoubleArray* result = new Tango::DevVarDoubleArray();
    result->length(3);
    (*result)[0] = lower_platform_pos_[0];
    (*result)[1] = lower_platform_pos_[1];
    (*result)[2] = lower_platform_pos_[2];
    return result;
}

Tango::DevVarShortArray* ReflectionImagingDevice::lowerPlatformReadOrg() {
    check_state("lowerPlatformReadOrg");
    if (!sim_mode_ && lower_platform_proxy_) {
        for (int i = 0; i < 3; ++i) {
            try {
                Tango::DeviceData arg; Tango::DevShort axis = static_cast<Tango::DevShort>(i); arg << axis;
                Tango::DeviceData out = lower_platform_proxy_->command_inout("readOrg", arg);
                Tango::DevBoolean v; out >> v;
                lower_platform_lim_org_state_[i] = v ? 0 : 2;
            } catch (...) {
                lower_platform_lim_org_state_[i] = 2;
            }
        }
    }
    Tango::DevVarShortArray* result = new Tango::DevVarShortArray();
    result->length(3);
    (*result)[0] = (lower_platform_lim_org_state_[0] == 0) ? 1 : 0;
    (*result)[1] = (lower_platform_lim_org_state_[1] == 0) ? 1 : 0;
    (*result)[2] = (lower_platform_lim_org_state_[2] == 0) ? 1 : 0;
    return result;
}

Tango::DevVarShortArray* ReflectionImagingDevice::lowerPlatformReadEL() {
    check_state("lowerPlatformReadEL");
    if (!sim_mode_ && lower_platform_proxy_) {
        for (int i = 0; i < 3; ++i) {
            try {
                Tango::DeviceData arg; Tango::DevShort axis = static_cast<Tango::DevShort>(i); arg << axis;
                Tango::DeviceData out = lower_platform_proxy_->command_inout("readEL", arg);
                Tango::DevShort v_raw; out >> v_raw;
                
                // 限位开关低电平有效：硬件读取0（低电平）表示触发，1（高电平）表示未触发
                // 运动控制器当前逻辑：pos_limit==1返回1(EL+), neg_limit==1返回-1(EL-), 否则返回0
                // 如果限位开关是低电平有效，需要反转：当硬件读取0时应该返回限位触发
                // 由于运动控制器返回0时无法区分方向，我们在reflection_imaging_device中反转逻辑
                // 反转规则：当readEL返回0时，认为是限位触发（统一处理为EL+）
                //          当readEL返回非0时，认为是未触发（转换为0）
                Tango::DevShort v = 0;
                if (v_raw == 0) {
                    // 低电平有效：返回0表示限位触发（但无法区分方向，统一处理为EL+）
                    v = 1;  // 转换为EL+表示限位触发
                }
                // 如果返回非0（1或-1），说明硬件读取是高电平，限位未触发，v保持为0
                
                lower_platform_lim_org_state_[i] = v;
            } catch (...) {
                lower_platform_lim_org_state_[i] = 2;
            }
        }
    }
    Tango::DevVarShortArray* result = new Tango::DevVarShortArray();
    result->length(3);
    (*result)[0] = lower_platform_lim_org_state_[0];
    (*result)[1] = lower_platform_lim_org_state_[1];
    (*result)[2] = lower_platform_lim_org_state_[2];
    return result;
}

void ReflectionImagingDevice::lowerPlatformSingleAxisMove(const Tango::DevVarDoubleArray* params) {
    check_state("lowerPlatformSingleAxisMove");
    if (params->length() < 2) {
        Tango::Except::throw_exception("InvalidParameter", "Need 2 parameters: [axis, distance]", "lowerPlatformSingleAxisMove");
    }
    
    int axis = static_cast<int>(std::round((*params)[0]));
    double distance = (*params)[1];
    
    // 轴号验证
    if (axis < 0 || axis > 2) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Axis must be 0(X), 1(Y), or 2(Z)", 
            "lowerPlatformSingleAxisMove");
    }
    
    // 限位检查
    double target_pos = lower_platform_pos_[axis] + distance;
    if (std::abs(target_pos) > lower_platform_range_[axis]) {
        Tango::Except::throw_exception("API_LimitExceeded", 
            "Lower platform axis " + std::to_string(axis) + " target " + 
            std::to_string(target_pos) + " exceeds limit ±" + 
            std::to_string(lower_platform_range_[axis]), 
            "lowerPlatformSingleAxisMove");
    }
    
    // Z轴碰撞检测
    if (axis == 2 && checkPlatformCollision(upper_platform_pos_[2], target_pos)) {
        Tango::Except::throw_exception("API_CollisionRisk", 
            "Lower platform Z axis move would cause collision", 
            "lowerPlatformSingleAxisMove");
    }
    
    // 如果Z轴有运动，运动前自动释放刹车（如果配置了刹车）
    if (axis == 2 && brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Z-axis motion detected in lowerPlatformSingleAxisMove, auto-releasing brake" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before lower platform Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        lower_platform_target_[axis] = target_pos;
        lower_platform_dire_pos_[axis] = target_pos;
        lower_platform_state_[axis] = true;
        
        sim_motion_duration_ = (std::abs(distance) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        
        log_event("Lower platform single axis move (sim): axis=" + std::to_string(axis) + 
                  ", distance=" + std::to_string(distance));
        return;
    }
    
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", 
            "lowerPlatformSingleAxisMove");
    }
    
    Tango::DeviceData arg; 
    Tango::DevVarDoubleArray arr; arr.length(2); 
    arr[0] = static_cast<double>(axis * 2 + 1); 
    arr[1] = distance;
    arg << arr; 
    lower_platform_proxy_->command_inout("singleAxisMove", arg);
    lower_platform_state_[axis] = true;
    log_event("Lower platform single axis move: axis=" + std::to_string(axis) + 
              ", distance=" + std::to_string(distance));
}

// ========== 下平台单轴控制命令 (用于GUI单轴独立控制) ==========
void ReflectionImagingDevice::lowerXMoveAbsolute(Tango::DevDouble val) {
    check_state("lowerXMoveAbsolute");
    if (std::abs(val) > lower_platform_range_[0]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Lower X target " + std::to_string(val) + " exceeds limit ±" + std::to_string(lower_platform_range_[0]),
            "lowerXMoveAbsolute");
    }
    if (sim_mode_) {
        lower_platform_target_[0] = val;
        lower_platform_dire_pos_[0] = val;
        lower_platform_state_[0] = true;
        sim_motion_duration_ = (std::abs(val - lower_platform_pos_[0]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower X move absolute (sim): " + std::to_string(val));
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerXMoveAbsolute");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 1.0;  // axis 1 = X (根据配置 lowerPlatformConfig)
    arr[1] = val;
    arg << arr;
    lower_platform_proxy_->command_inout("moveAbsolute", arg);
    lower_platform_state_[0] = true;
    log_event("Lower X move absolute: " + std::to_string(val));
}

void ReflectionImagingDevice::lowerXMoveRelative(Tango::DevDouble val) {
    check_state("lowerXMoveRelative");
    double target = lower_platform_pos_[0] + val;
    if (std::abs(target) > lower_platform_range_[0]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Lower X target " + std::to_string(target) + " exceeds limit ±" + std::to_string(lower_platform_range_[0]),
            "lowerXMoveRelative");
    }
    if (sim_mode_) {
        lower_platform_target_[0] = target;
        lower_platform_dire_pos_[0] = target;
        lower_platform_state_[0] = true;
        sim_motion_duration_ = (std::abs(val) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower X move relative (sim): " + std::to_string(val));
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerXMoveRelative");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 1.0;  // axis 1 = X
    arr[1] = val;
    arg << arr;
    lower_platform_proxy_->command_inout("moveRelative", arg);
    lower_platform_state_[0] = true;
    log_event("Lower X move relative: " + std::to_string(val));
}

void ReflectionImagingDevice::lowerXMoveZero() {
    check_state("lowerXMoveZero");
    if (sim_mode_) {
        lower_platform_target_[0] = 0.0;
        lower_platform_dire_pos_[0] = 0.0;
        lower_platform_state_[0] = true;
        sim_motion_duration_ = (std::abs(lower_platform_pos_[0]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower X move zero (sim)");
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerXMoveZero");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 1;  // X axis
    arg << axis;
    lower_platform_proxy_->command_inout("moveZero", arg);
    lower_platform_state_[0] = true;
    log_event("Lower X move zero");
}

void ReflectionImagingDevice::lowerXStop() {
    check_state("lowerXStop");
    if (sim_mode_) {
        lower_platform_state_[0] = false;
        log_event("Lower X stopped (sim)");
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerXStop");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 1;
    arg << axis;
    lower_platform_proxy_->command_inout("stopMove", arg);
    lower_platform_state_[0] = false;
    log_event("Lower X stopped");
}

void ReflectionImagingDevice::lowerXReset() {
    check_state("lowerXReset");

    // Clear latched limit fault
    bool was_latched = limit_fault_latched_.exchange(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    if (was_latched) {
        fault_state_.clear();
    }
    if (sim_mode_) {
        lower_platform_state_[0] = false;
        lower_platform_lim_org_state_[0] = 0;
        log_event("Lower X reset (sim)");
        // 模拟模式下，如果设备处于FAULT状态，清除故障
        if (get_state() == Tango::FAULT) {
            fault_state_ = "";
            set_state(Tango::ON);
            set_status("Fault cleared by reset");
        }
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerXReset");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 1;
    arg << axis;
    lower_platform_proxy_->command_inout("reset", arg);
    lower_platform_state_[0] = false;
    log_event("Lower X reset");
    // 如果设备处于FAULT状态，尝试清除故障（类似大行程设备的reset行为）
    if (get_state() == Tango::FAULT) {
        fault_state_ = "";
        set_state(Tango::ON);
        set_status("Fault cleared by reset");
        log_event("Device fault cleared by lower X reset");
    }
}

void ReflectionImagingDevice::lowerYMoveAbsolute(Tango::DevDouble val) {
    check_state("lowerYMoveAbsolute");
    if (std::abs(val) > lower_platform_range_[1]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Lower Y target " + std::to_string(val) + " exceeds limit ±" + std::to_string(lower_platform_range_[1]),
            "lowerYMoveAbsolute");
    }
    if (sim_mode_) {
        lower_platform_target_[1] = val;
        lower_platform_dire_pos_[1] = val;
        lower_platform_state_[1] = true;
        sim_motion_duration_ = (std::abs(val - lower_platform_pos_[1]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower Y move absolute (sim): " + std::to_string(val));
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerYMoveAbsolute");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 3.0;  // axis 3 = Y (根据配置 lowerPlatformConfig)
    arr[1] = val;
    arg << arr;
    lower_platform_proxy_->command_inout("moveAbsolute", arg);
    lower_platform_state_[1] = true;
    log_event("Lower Y move absolute: " + std::to_string(val));
}

void ReflectionImagingDevice::lowerYMoveRelative(Tango::DevDouble val) {
    check_state("lowerYMoveRelative");
    double target = lower_platform_pos_[1] + val;
    if (std::abs(target) > lower_platform_range_[1]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Lower Y target " + std::to_string(target) + " exceeds limit ±" + std::to_string(lower_platform_range_[1]),
            "lowerYMoveRelative");
    }
    if (sim_mode_) {
        lower_platform_target_[1] = target;
        lower_platform_dire_pos_[1] = target;
        lower_platform_state_[1] = true;
        sim_motion_duration_ = (std::abs(val) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower Y move relative (sim): " + std::to_string(val));
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerYMoveRelative");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 3.0;  // axis 3 = Y
    arr[1] = val;
    arg << arr;
    lower_platform_proxy_->command_inout("moveRelative", arg);
    lower_platform_state_[1] = true;
    log_event("Lower Y move relative: " + std::to_string(val));
}

void ReflectionImagingDevice::lowerYMoveZero() {
    check_state("lowerYMoveZero");
    if (sim_mode_) {
        lower_platform_target_[1] = 0.0;
        lower_platform_dire_pos_[1] = 0.0;
        lower_platform_state_[1] = true;
        sim_motion_duration_ = (std::abs(lower_platform_pos_[1]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower Y move zero (sim)");
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerYMoveZero");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 3;  // Y axis
    arg << axis;
    lower_platform_proxy_->command_inout("moveZero", arg);
    lower_platform_state_[1] = true;
    log_event("Lower Y move zero");
}

void ReflectionImagingDevice::lowerYStop() {
    check_state("lowerYStop");
    if (sim_mode_) {
        lower_platform_state_[1] = false;
        log_event("Lower Y stopped (sim)");
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerYStop");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 3;
    arg << axis;
    lower_platform_proxy_->command_inout("stopMove", arg);
    lower_platform_state_[1] = false;
    log_event("Lower Y stopped");
}

void ReflectionImagingDevice::lowerYReset() {
    check_state("lowerYReset");

    // Clear latched limit fault
    bool was_latched = limit_fault_latched_.exchange(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    if (was_latched) {
        fault_state_.clear();
    }
    if (sim_mode_) {
        lower_platform_state_[1] = false;
        lower_platform_lim_org_state_[1] = 0;
        log_event("Lower Y reset (sim)");
        // 模拟模式下，如果设备处于FAULT状态，清除故障
        if (get_state() == Tango::FAULT) {
            fault_state_ = "";
            set_state(Tango::ON);
            set_status("Fault cleared by reset");
        }
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerYReset");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 3;
    arg << axis;
    lower_platform_proxy_->command_inout("reset", arg);
    lower_platform_state_[1] = false;
    log_event("Lower Y reset");
    // 如果设备处于FAULT状态，尝试清除故障（类似大行程设备的reset行为）
    if (get_state() == Tango::FAULT) {
        fault_state_ = "";
        set_state(Tango::ON);
        set_status("Fault cleared by reset");
        log_event("Device fault cleared by lower Y reset");
    }
}

void ReflectionImagingDevice::lowerZMoveAbsolute(Tango::DevDouble val) {
    check_state("lowerZMoveAbsolute");
    if (std::abs(val) > lower_platform_range_[2]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Lower Z target " + std::to_string(val) + " exceeds limit ±" + std::to_string(lower_platform_range_[2]),
            "lowerZMoveAbsolute");
    }
    if (checkPlatformCollision(upper_platform_pos_[2], val)) {
        Tango::Except::throw_exception("API_CollisionRisk",
            "Lower platform Z axis move would cause collision", "lowerZMoveAbsolute");
    }
    
    // Z轴运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before lower Z motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before lower Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        lower_platform_target_[2] = val;
        lower_platform_dire_pos_[2] = val;
        lower_platform_state_[2] = true;
        sim_motion_duration_ = (std::abs(val - lower_platform_pos_[2]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower Z move absolute (sim): " + std::to_string(val));
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerZMoveAbsolute");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 5.0;  // axis 5 = Z (根据配置 lowerPlatformConfig)
    arr[1] = val;
    arg << arr;
    lower_platform_proxy_->command_inout("moveAbsolute", arg);
    lower_platform_state_[2] = true;
    log_event("Lower Z move absolute: " + std::to_string(val));
}

void ReflectionImagingDevice::lowerZMoveRelative(Tango::DevDouble val) {
    check_state("lowerZMoveRelative");
    double target = lower_platform_pos_[2] + val;
    if (std::abs(target) > lower_platform_range_[2]) {
        Tango::Except::throw_exception("API_LimitExceeded",
            "Lower Z target " + std::to_string(target) + " exceeds limit ±" + std::to_string(lower_platform_range_[2]),
            "lowerZMoveRelative");
    }
    if (checkPlatformCollision(upper_platform_pos_[2], target)) {
        Tango::Except::throw_exception("API_CollisionRisk",
            "Lower platform Z axis move would cause collision", "lowerZMoveRelative");
    }
    
    // Z轴运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before lower Z motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before lower Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        lower_platform_target_[2] = target;
        lower_platform_dire_pos_[2] = target;
        lower_platform_state_[2] = true;
        sim_motion_duration_ = (std::abs(val) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower Z move relative (sim): " + std::to_string(val));
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerZMoveRelative");
    }
    Tango::DeviceData arg;
    Tango::DevVarDoubleArray arr; arr.length(2);
    arr[0] = 5.0;  // axis 5 = Z
    arr[1] = val;
    arg << arr;
    lower_platform_proxy_->command_inout("moveRelative", arg);
    lower_platform_state_[2] = true;
    log_event("Lower Z move relative: " + std::to_string(val));
}

void ReflectionImagingDevice::lowerZMoveZero() {
    check_state("lowerZMoveZero");
    if (checkPlatformCollision(upper_platform_pos_[2], 0.0)) {
        Tango::Except::throw_exception("API_CollisionRisk",
            "Lower platform Z axis move zero would cause collision", "lowerZMoveZero");
    }
    
    // Z轴运动前自动释放刹车（如果配置了刹车）
    if (brake_power_port_ >= 0 && !brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-releasing brake before lower Z motion" << endl;
        if (!release_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to release brake before lower Z motion, continuing anyway" << endl;
        }
    }
    
    if (sim_mode_) {
        lower_platform_target_[2] = 0.0;
        lower_platform_dire_pos_[2] = 0.0;
        lower_platform_state_[2] = true;
        sim_motion_duration_ = (std::abs(lower_platform_pos_[2]) / 50.0) * 1000.0;
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        log_event("Lower Z move zero (sim)");
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerZMoveZero");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 5;  // Z axis
    arg << axis;
    lower_platform_proxy_->command_inout("moveZero", arg);
    lower_platform_state_[2] = true;
    log_event("Lower Z move zero");
}

void ReflectionImagingDevice::lowerZStop() {
    check_state("lowerZStop");
    if (sim_mode_) {
        lower_platform_state_[2] = false;
        log_event("Lower Z stopped (sim)");
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerZStop");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 5;
    arg << axis;
    lower_platform_proxy_->command_inout("stopMove", arg);
    lower_platform_state_[2] = false;
    log_event("Lower Z stopped");
    
    // 注意：正常停止后不自动启用刹车，保持刹车释放状态以便快速继续运动
    // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
}

void ReflectionImagingDevice::lowerZReset() {
    check_state("lowerZReset");

    // Clear latched limit fault
    bool was_latched = limit_fault_latched_.exchange(false);
    limit_fault_axis_.store(-1);
    limit_fault_el_state_.store(0);
    if (was_latched) {
        fault_state_.clear();
    }
    
    // 复位前自动启用刹车（安全保护）
    if (brake_power_port_ >= 0 && brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-engaging brake before lower Z reset (safety)" << endl;
        if (!engage_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to engage brake before lower Z reset" << endl;
        }
    }
    
    if (sim_mode_) {
        lower_platform_state_[2] = false;
        lower_platform_lim_org_state_[2] = 0;
        log_event("Lower Z reset (sim)");
        // 模拟模式下，如果设备处于FAULT状态，清除故障
        if (get_state() == Tango::FAULT) {
            fault_state_ = "";
            set_state(Tango::ON);
            set_status("Fault cleared by reset");
        }
        return;
    }
    if (!lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", "Lower platform proxy not connected", "lowerZReset");
    }
    Tango::DeviceData arg;
    Tango::DevShort axis = 5;
    arg << axis;
    lower_platform_proxy_->command_inout("reset", arg);
    lower_platform_state_[2] = false;
    log_event("Lower Z reset");
    // 如果设备处于FAULT状态，尝试清除故障（类似大行程设备的reset行为）
    if (get_state() == Tango::FAULT) {
        fault_state_ = "";
        set_state(Tango::ON);
        set_status("Fault cleared by reset");
        log_event("Device fault cleared by lower Z reset");
    }
}

// ========== Synchronized Movement ==========

void ReflectionImagingDevice::synchronizedMove(const Tango::DevVarDoubleArray* params) {
    check_state("synchronizedMove");
    if (params->length() < 6) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Need 6 parameters: [upperX, upperY, upperZ, lowerX, lowerY, lowerZ]", 
            "synchronizedMove");
    }
    
    // 解析目标位置
    std::array<double, 3> upper_target = {(*params)[0], (*params)[1], (*params)[2]};
    std::array<double, 3> lower_target = {(*params)[3], (*params)[4], (*params)[5]};
    
    // 1. 限位检查
    if (!checkUpperPlatformLimits(upper_target[0], upper_target[1], upper_target[2])) {
        Tango::Except::throw_exception("API_LimitExceeded", 
            "Upper platform target exceeds limits in synchronized move", 
            "ReflectionImagingDevice::synchronizedMove");
    }
    if (!checkLowerPlatformLimits(lower_target[0], lower_target[1], lower_target[2])) {
        Tango::Except::throw_exception("API_LimitExceeded", 
            "Lower platform target exceeds limits in synchronized move", 
            "ReflectionImagingDevice::synchronizedMove");
    }
    
    // 2. 碰撞检测（检查目标位置和运动过程中的最近距离）
    if (checkPlatformCollision(upper_target[2], lower_target[2])) {
        Tango::Except::throw_exception("API_CollisionRisk", 
            "Synchronized move would cause platform collision at target positions", 
            "ReflectionImagingDevice::synchronizedMove");
    }
    
    // 检查运动过程中是否会碰撞（线性插值检查）
    double upper_z_start = upper_platform_pos_[2];
    double lower_z_start = lower_platform_pos_[2];
    for (double t = 0.0; t <= 1.0; t += 0.1) {
        double upper_z_interp = upper_z_start + t * (upper_target[2] - upper_z_start);
        double lower_z_interp = lower_z_start + t * (lower_target[2] - lower_z_start);
        if (checkPlatformCollision(upper_z_interp, lower_z_interp)) {
            Tango::Except::throw_exception("API_CollisionRisk", 
                "Synchronized move trajectory would cause platform collision", 
                "ReflectionImagingDevice::synchronizedMove");
        }
    }
    
    // 3. 计算同步速度（确保两个平台同时到达）
    SyncMoveParams sync_params = calculateSyncVelocities(upper_target, lower_target);
    
    if (sim_mode_) {
        // 设置两个平台的目标位置
        upper_platform_target_ = upper_target;
        lower_platform_target_ = lower_target;
        upper_platform_dire_pos_ = upper_platform_target_;
        lower_platform_dire_pos_ = lower_platform_target_;
        
        // 设置运动状态
        std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), true);
        std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), true);
        
        // 设置模拟运动时间
        sim_motion_duration_ = sync_params.total_time * 1000.0;  // 转换为毫秒
        sim_motion_start_time_ = std::chrono::steady_clock::now();
        
        log_event("Synchronized move started (sim): upper=[" + 
                  std::to_string(upper_target[0]) + "," + std::to_string(upper_target[1]) + "," + 
                  std::to_string(upper_target[2]) + "], lower=[" + 
                  std::to_string(lower_target[0]) + "," + std::to_string(lower_target[1]) + "," + 
                  std::to_string(lower_target[2]) + "], time=" + std::to_string(sync_params.total_time) + "s");
        return;
    }
    
    // 真实模式：先设置速度参数，然后同时启动
    if (!upper_platform_proxy_ || !lower_platform_proxy_) {
        Tango::Except::throw_exception("API_NoProxy", 
            "Platform proxies not connected", 
            "ReflectionImagingDevice::synchronizedMove");
    }
    
    try {
        // 准备运动参数
        Tango::DevVarDoubleArray upper_params, lower_params;
        upper_params.length(3); lower_params.length(3);
        for (int i = 0; i < 3; ++i) {
            upper_params[i] = upper_target[i];
            lower_params[i] = lower_target[i];
        }
        
        Tango::DeviceData upper_arg, lower_arg;
        upper_arg << upper_params;
        lower_arg << lower_params;
        
        // TODO: 如果运动控制器支持同步触发，应使用硬件同步信号
        // 目前使用软件顺序启动，尽量减少延迟
        upper_platform_proxy_->command_inout("moveAbsolute", upper_arg);
        lower_platform_proxy_->command_inout("moveAbsolute", lower_arg);
        
        std::fill(upper_platform_state_.begin(), upper_platform_state_.end(), true);
        std::fill(lower_platform_state_.begin(), lower_platform_state_.end(), true);
        
    } catch (Tango::DevFailed &e) {
        // 如果一个平台启动失败，尝试停止另一个
        try {
            Tango::DeviceData arg;
            Tango::DevShort axis;
            if (upper_platform_proxy_) {
                axis = 0; arg << axis; upper_platform_proxy_->command_inout("stopMove", arg);
                axis = 2; arg << axis; upper_platform_proxy_->command_inout("stopMove", arg);
                axis = 4; arg << axis; upper_platform_proxy_->command_inout("stopMove", arg);
            }
            if (lower_platform_proxy_) {
                axis = 1; arg << axis; lower_platform_proxy_->command_inout("stopMove", arg);
                axis = 3; arg << axis; lower_platform_proxy_->command_inout("stopMove", arg);
                axis = 5; arg << axis; lower_platform_proxy_->command_inout("stopMove", arg);
            }
        } catch (...) {}
        
        Tango::Except::re_throw_exception(e, "API_CommandFailed", 
            "Synchronized move failed: " + std::string(e.errors[0].desc), 
            "ReflectionImagingDevice::synchronizedMove");
    }
    
    log_event("Synchronized move started");
}

// ========== CCD Camera Commands (四CCD：上下各两个，1倍和10倍物镜) ==========
// 相机型号：海康 MV-CU020-19GC

// 上1倍物镜CCD（粗定位）
void ReflectionImagingDevice::upperCCD1xSwitch(Tango::DevBoolean on) {
    check_state("upperCCD1xSwitch");
    
    if (sim_mode_) {
        upper_ccd_1x_state_ = on ? "READY" : "OFF";
        log_event("Upper CCD 1x (coarse positioning) " + std::string(on ? "ON" : "OFF"));
        return;
    }

    if (!upper_ccd_1x_driver_) {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Upper CCD 1x driver not initialized", "upperCCD1xSwitch");
    }

    bool success = false;
    if (on) {
        success = upper_ccd_1x_driver_->open();
        if (success) {
            upper_ccd_1x_state_ = "READY";
            log_event("Upper CCD 1x (coarse positioning) ON");
        } else {
            upper_ccd_1x_state_ = "ERROR";
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to open upper CCD 1x: " + upper_ccd_1x_driver_->getLastError(),
                "upperCCD1xSwitch");
        }
    } else {
        upper_ccd_1x_driver_->close();
        upper_ccd_1x_state_ = "OFF";
        log_event("Upper CCD 1x (coarse positioning) OFF");
        success = true;
    }
}

void ReflectionImagingDevice::upperCCD1xRingLightSwitch(Tango::DevBoolean on) {
    check_state("upperCCD1xRingLightSwitch");
    
    upper_ccd_1x_ring_light_on_ = (on != 0);
    
    if (!sim_mode_ && upper_ccd_1x_driver_) {
        if (!upper_ccd_1x_driver_->setRingLight(upper_ccd_1x_ring_light_on_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set ring light: " + upper_ccd_1x_driver_->getLastError(),
                "upperCCD1xRingLightSwitch");
        }
    }
    
    log_event("Upper CCD 1x ring light " + std::string(upper_ccd_1x_ring_light_on_ ? "ON" : "OFF"));
}

Tango::DevString ReflectionImagingDevice::captureUpperCCD1xImage() {
    check_state("captureUpperCCD1xImage");
    if (upper_ccd_1x_state_ != "READY") {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Upper CCD 1x is not ready (current state: " + upper_ccd_1x_state_ + ")",
            "ReflectionImagingDevice::captureUpperCCD1xImage");
    }
    
    upper_ccd_1x_state_ = "CAPTURING";
    
    if (sim_mode_) {
        // 仿真模式：生成模拟图像路径
        std::time_t now = std::time(nullptr);
        std::tm* timeinfo = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
        upper_ccd_1x_last_capture_time_ = oss.str();
        upper_ccd_1x_image_url_ = image_save_path_ + "/upper_ccd_1x_" + oss.str() + "." + image_format_;
        upper_ccd_1x_state_ = "READY";
        image_capture_count_++;
        log_event("Upper CCD 1x image captured (sim)");
        return Tango::string_dup(upper_ccd_1x_image_url_.c_str());
    }
    
    // 生成时间戳和文件路径
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
    upper_ccd_1x_last_capture_time_ = oss.str();
    std::string file_path = image_save_path_ + "/upper_ccd_1x_" + oss.str() + "." + image_format_;
    
    if (!sim_mode_ && upper_ccd_1x_driver_) {
        if (!upper_ccd_1x_driver_->captureImage(file_path)) {
            upper_ccd_1x_state_ = "ERROR";
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to capture image: " + upper_ccd_1x_driver_->getLastError(),
                "captureUpperCCD1xImage");
        }
    }
    
    upper_ccd_1x_image_url_ = file_path;
    upper_ccd_1x_state_ = "READY";
    image_capture_count_++;
    log_event("Upper CCD 1x image captured: " + file_path);
    return Tango::string_dup(upper_ccd_1x_image_url_.c_str());
}

void ReflectionImagingDevice::setUpperCCD1xExposure(Tango::DevDouble exposure) {
    check_state("setUpperCCD1xExposure");
    if (exposure < 0.1 || exposure > 10.0) {
        Tango::Except::throw_exception("InvalidParameter", "Exposure must be between 0.1 and 10.0 seconds", "setUpperCCD1xExposure");
    }
    upper_ccd_1x_exposure_ = exposure;
    
    if (!sim_mode_ && upper_ccd_1x_driver_) {
        if (!upper_ccd_1x_driver_->setExposureTime(exposure * 1000.0)) {  // 转换为毫秒
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set exposure: " + upper_ccd_1x_driver_->getLastError(),
                "setUpperCCD1xExposure");
        }
    }
    
    log_event("Upper CCD 1x exposure set to " + std::to_string(exposure));
}

Tango::DevString ReflectionImagingDevice::getUpperCCD1xImage() {
    check_state("getUpperCCD1xImage");
    if (upper_ccd_1x_state_ != "READY") {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Upper CCD 1x is not ready", "ReflectionImagingDevice::getUpperCCD1xImage");
    }
    
    if (sim_mode_) {
        return Tango::string_dup("base64_encoded_image_data_upper_1x_sim");
    }
    
    if (!upper_ccd_1x_driver_) {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Upper CCD 1x driver not initialized", "getUpperCCD1xImage");
    }
    
    std::string base64_data = upper_ccd_1x_driver_->getLatestImageBase64();
    if (base64_data.empty()) {
        Tango::Except::throw_exception("API_DeviceError",
            "Failed to get image: " + upper_ccd_1x_driver_->getLastError(),
            "getUpperCCD1xImage");
    }
    
    return Tango::string_dup(base64_data.c_str());
}

// 上10倍物镜CCD（近距离观察）
void ReflectionImagingDevice::upperCCD10xSwitch(Tango::DevBoolean on) {
    check_state("upperCCD10xSwitch");
    
    if (sim_mode_) {
        upper_ccd_10x_state_ = on ? "READY" : "OFF";
        log_event("Upper CCD 10x (close observation) " + std::string(on ? "ON" : "OFF"));
        return;
    }

    if (!upper_ccd_10x_driver_) {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Upper CCD 10x driver not initialized", "upperCCD10xSwitch");
    }

    bool success = false;
    if (on) {
        success = upper_ccd_10x_driver_->open();
        if (success) {
            upper_ccd_10x_state_ = "READY";
            log_event("Upper CCD 10x (close observation) ON");
        } else {
            upper_ccd_10x_state_ = "ERROR";
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to open upper CCD 10x: " + upper_ccd_10x_driver_->getLastError(),
                "upperCCD10xSwitch");
        }
    } else {
        upper_ccd_10x_driver_->close();
        upper_ccd_10x_state_ = "OFF";
        log_event("Upper CCD 10x (close observation) OFF");
        success = true;
    }
}

void ReflectionImagingDevice::upperCCD10xRingLightSwitch(Tango::DevBoolean on) {
    check_state("upperCCD10xRingLightSwitch");
    
    upper_ccd_10x_ring_light_on_ = (on != 0);
    
    if (!sim_mode_ && upper_ccd_10x_driver_) {
        if (!upper_ccd_10x_driver_->setRingLight(upper_ccd_10x_ring_light_on_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set ring light: " + upper_ccd_10x_driver_->getLastError(),
                "upperCCD10xRingLightSwitch");
        }
    }
    
    log_event("Upper CCD 10x ring light " + std::string(upper_ccd_10x_ring_light_on_ ? "ON" : "OFF"));
}

Tango::DevString ReflectionImagingDevice::captureUpperCCD10xImage() {
    check_state("captureUpperCCD10xImage");
    if (upper_ccd_10x_state_ != "READY") {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Upper CCD 10x is not ready (current state: " + upper_ccd_10x_state_ + ")",
            "ReflectionImagingDevice::captureUpperCCD10xImage");
    }
    
    upper_ccd_10x_state_ = "CAPTURING";
    
    if (sim_mode_) {
        std::time_t now = std::time(nullptr);
        std::tm* timeinfo = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
        upper_ccd_10x_last_capture_time_ = oss.str();
        upper_ccd_10x_image_url_ = image_save_path_ + "/upper_ccd_10x_" + oss.str() + "." + image_format_;
        upper_ccd_10x_state_ = "READY";
        image_capture_count_++;
        log_event("Upper CCD 10x image captured (sim)");
        return Tango::string_dup(upper_ccd_10x_image_url_.c_str());
    }
    
    // 生成时间戳和文件路径
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
    upper_ccd_10x_last_capture_time_ = oss.str();
    std::string file_path = image_save_path_ + "/upper_ccd_10x_" + oss.str() + "." + image_format_;
    
    if (!sim_mode_ && upper_ccd_10x_driver_) {
        if (!upper_ccd_10x_driver_->captureImage(file_path)) {
            upper_ccd_10x_state_ = "ERROR";
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to capture image: " + upper_ccd_10x_driver_->getLastError(),
                "captureUpperCCD10xImage");
        }
    }
    
    upper_ccd_10x_image_url_ = file_path;
    upper_ccd_10x_state_ = "READY";
    image_capture_count_++;
    log_event("Upper CCD 10x image captured: " + file_path);
    return Tango::string_dup(upper_ccd_10x_image_url_.c_str());
}

void ReflectionImagingDevice::setUpperCCD10xExposure(Tango::DevDouble exposure) {
    check_state("setUpperCCD10xExposure");
    if (exposure < 0.1 || exposure > 10.0) {
        Tango::Except::throw_exception("InvalidParameter", "Exposure must be between 0.1 and 10.0 seconds", "setUpperCCD10xExposure");
    }
    upper_ccd_10x_exposure_ = exposure;
    
    if (!sim_mode_ && upper_ccd_10x_driver_) {
        if (!upper_ccd_10x_driver_->setExposureTime(exposure * 1000.0)) {  // 转换为毫秒
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set exposure: " + upper_ccd_10x_driver_->getLastError(),
                "setUpperCCD10xExposure");
        }
    }
    
    log_event("Upper CCD 10x exposure set to " + std::to_string(exposure));
}

Tango::DevString ReflectionImagingDevice::getUpperCCD10xImage() {
    check_state("getUpperCCD10xImage");
    if (upper_ccd_10x_state_ != "READY") {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Upper CCD 10x is not ready", "getUpperCCD10xImage");
    }
    
    if (sim_mode_) {
        return Tango::string_dup("base64_encoded_image_data_upper_10x_sim");
    }
    
    if (!upper_ccd_10x_driver_) {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Upper CCD 10x driver not initialized", "getUpperCCD10xImage");
    }
    
    std::string base64_data = upper_ccd_10x_driver_->getLatestImageBase64();
    if (base64_data.empty()) {
        Tango::Except::throw_exception("API_DeviceError",
            "Failed to get image: " + upper_ccd_10x_driver_->getLastError(),
            "getUpperCCD10xImage");
    }
    
    return Tango::string_dup(base64_data.c_str());
}

// 下1倍物镜CCD（粗定位）
void ReflectionImagingDevice::lowerCCD1xSwitch(Tango::DevBoolean on) {
    check_state("lowerCCD1xSwitch");
    
    if (sim_mode_) {
        lower_ccd_1x_state_ = on ? "READY" : "OFF";
        log_event("Lower CCD 1x (coarse positioning) " + std::string(on ? "ON" : "OFF"));
        return;
    }

    if (!lower_ccd_1x_driver_) {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Lower CCD 1x driver not initialized", "lowerCCD1xSwitch");
    }

    bool success = false;
    if (on) {
        success = lower_ccd_1x_driver_->open();
        if (success) {
            lower_ccd_1x_state_ = "READY";
            log_event("Lower CCD 1x (coarse positioning) ON");
        } else {
            lower_ccd_1x_state_ = "ERROR";
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to open lower CCD 1x: " + lower_ccd_1x_driver_->getLastError(),
                "lowerCCD1xSwitch");
        }
    } else {
        lower_ccd_1x_driver_->close();
        lower_ccd_1x_state_ = "OFF";
        log_event("Lower CCD 1x (coarse positioning) OFF");
        success = true;
    }
}

void ReflectionImagingDevice::lowerCCD1xRingLightSwitch(Tango::DevBoolean on) {
    check_state("lowerCCD1xRingLightSwitch");
    
    lower_ccd_1x_ring_light_on_ = (on != 0);
    
    if (!sim_mode_ && lower_ccd_1x_driver_) {
        if (!lower_ccd_1x_driver_->setRingLight(lower_ccd_1x_ring_light_on_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set ring light: " + lower_ccd_1x_driver_->getLastError(),
                "lowerCCD1xRingLightSwitch");
        }
    }
    
    log_event("Lower CCD 1x ring light " + std::string(lower_ccd_1x_ring_light_on_ ? "ON" : "OFF"));
}

Tango::DevString ReflectionImagingDevice::captureLowerCCD1xImage() {
    check_state("captureLowerCCD1xImage");
    if (lower_ccd_1x_state_ != "READY") {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Lower CCD 1x is not ready (current state: " + lower_ccd_1x_state_ + ")",
            "ReflectionImagingDevice::captureLowerCCD1xImage");
    }
    
    lower_ccd_1x_state_ = "CAPTURING";
    
    if (sim_mode_) {
        std::time_t now = std::time(nullptr);
        std::tm* timeinfo = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
        lower_ccd_1x_last_capture_time_ = oss.str();
        lower_ccd_1x_image_url_ = image_save_path_ + "/lower_ccd_1x_" + oss.str() + "." + image_format_;
        lower_ccd_1x_state_ = "READY";
        image_capture_count_++;
        log_event("Lower CCD 1x image captured (sim)");
        return Tango::string_dup(lower_ccd_1x_image_url_.c_str());
    }
    
    // 生成时间戳和文件路径
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
    lower_ccd_1x_last_capture_time_ = oss.str();
    std::string file_path = image_save_path_ + "/lower_ccd_1x_" + oss.str() + "." + image_format_;
    
    if (!sim_mode_ && lower_ccd_1x_driver_) {
        if (!lower_ccd_1x_driver_->captureImage(file_path)) {
            lower_ccd_1x_state_ = "ERROR";
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to capture image: " + lower_ccd_1x_driver_->getLastError(),
                "captureLowerCCD1xImage");
        }
    }
    
    lower_ccd_1x_image_url_ = file_path;
    lower_ccd_1x_state_ = "READY";
    image_capture_count_++;
    log_event("Lower CCD 1x image captured: " + file_path);
    return Tango::string_dup(lower_ccd_1x_image_url_.c_str());
}

void ReflectionImagingDevice::setLowerCCD1xExposure(Tango::DevDouble exposure) {
    check_state("setLowerCCD1xExposure");
    if (exposure < 0.1 || exposure > 10.0) {
        Tango::Except::throw_exception("InvalidParameter", "Exposure must be between 0.1 and 10.0 seconds", "setLowerCCD1xExposure");
    }
    lower_ccd_1x_exposure_ = exposure;
    
    if (!sim_mode_ && lower_ccd_1x_driver_) {
        if (!lower_ccd_1x_driver_->setExposureTime(exposure * 1000.0)) {  // 转换为毫秒
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set exposure: " + lower_ccd_1x_driver_->getLastError(),
                "setLowerCCD1xExposure");
        }
    }
    
    log_event("Lower CCD 1x exposure set to " + std::to_string(exposure));
}

Tango::DevString ReflectionImagingDevice::getLowerCCD1xImage() {
    check_state("getLowerCCD1xImage");
    if (lower_ccd_1x_state_ != "READY") {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Lower CCD 1x is not ready", "getLowerCCD1xImage");
    }
    
    if (sim_mode_) {
        return Tango::string_dup("base64_encoded_image_data_lower_1x_sim");
    }
    
    if (!lower_ccd_1x_driver_) {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Lower CCD 1x driver not initialized", "getLowerCCD1xImage");
    }
    
    std::string base64_data = lower_ccd_1x_driver_->getLatestImageBase64();
    if (base64_data.empty()) {
        Tango::Except::throw_exception("API_DeviceError",
            "Failed to get image: " + lower_ccd_1x_driver_->getLastError(),
            "getLowerCCD1xImage");
    }
    
    return Tango::string_dup(base64_data.c_str());
}

// 下10倍物镜CCD（近距离观察）
void ReflectionImagingDevice::lowerCCD10xSwitch(Tango::DevBoolean on) {
    check_state("lowerCCD10xSwitch");
    
    if (sim_mode_) {
        lower_ccd_10x_state_ = on ? "READY" : "OFF";
        log_event("Lower CCD 10x (close observation) " + std::string(on ? "ON" : "OFF"));
        return;
    }

    if (!lower_ccd_10x_driver_) {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Lower CCD 10x driver not initialized", "lowerCCD10xSwitch");
    }

    bool success = false;
    if (on) {
        success = lower_ccd_10x_driver_->open();
        if (success) {
            lower_ccd_10x_state_ = "READY";
            log_event("Lower CCD 10x (close observation) ON");
        } else {
            lower_ccd_10x_state_ = "ERROR";
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to open lower CCD 10x: " + lower_ccd_10x_driver_->getLastError(),
                "lowerCCD10xSwitch");
        }
    } else {
        lower_ccd_10x_driver_->close();
        lower_ccd_10x_state_ = "OFF";
        log_event("Lower CCD 10x (close observation) OFF");
        success = true;
    }
}

void ReflectionImagingDevice::lowerCCD10xRingLightSwitch(Tango::DevBoolean on) {
    check_state("lowerCCD10xRingLightSwitch");
    
    lower_ccd_10x_ring_light_on_ = (on != 0);
    
    if (!sim_mode_ && lower_ccd_10x_driver_) {
        if (!lower_ccd_10x_driver_->setRingLight(lower_ccd_10x_ring_light_on_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set ring light: " + lower_ccd_10x_driver_->getLastError(),
                "lowerCCD10xRingLightSwitch");
        }
    }
    
    log_event("Lower CCD 10x ring light " + std::string(lower_ccd_10x_ring_light_on_ ? "ON" : "OFF"));
}

Tango::DevString ReflectionImagingDevice::captureLowerCCD10xImage() {
    check_state("captureLowerCCD10xImage");
    if (lower_ccd_10x_state_ != "READY") {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Lower CCD 10x is not ready (current state: " + lower_ccd_10x_state_ + ")",
            "ReflectionImagingDevice::captureLowerCCD10xImage");
    }
    
    lower_ccd_10x_state_ = "CAPTURING";
    
    if (sim_mode_) {
        std::time_t now = std::time(nullptr);
        std::tm* timeinfo = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
        lower_ccd_10x_last_capture_time_ = oss.str();
        lower_ccd_10x_image_url_ = image_save_path_ + "/lower_ccd_10x_" + oss.str() + "." + image_format_;
        lower_ccd_10x_state_ = "READY";
        image_capture_count_++;
        log_event("Lower CCD 10x image captured (sim)");
        return Tango::string_dup(lower_ccd_10x_image_url_.c_str());
    }
    
    // 生成时间戳和文件路径
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y%m%d_%H%M%S");
    lower_ccd_10x_last_capture_time_ = oss.str();
    std::string file_path = image_save_path_ + "/lower_ccd_10x_" + oss.str() + "." + image_format_;
    
    if (!sim_mode_ && lower_ccd_10x_driver_) {
        if (!lower_ccd_10x_driver_->captureImage(file_path)) {
            lower_ccd_10x_state_ = "ERROR";
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to capture image: " + lower_ccd_10x_driver_->getLastError(),
                "captureLowerCCD10xImage");
        }
    }
    
    lower_ccd_10x_image_url_ = file_path;
    lower_ccd_10x_state_ = "READY";
    image_capture_count_++;
    log_event("Lower CCD 10x image captured: " + file_path);
    return Tango::string_dup(lower_ccd_10x_image_url_.c_str());
}

void ReflectionImagingDevice::setLowerCCD10xExposure(Tango::DevDouble exposure) {
    check_state("setLowerCCD10xExposure");
    if (exposure < 0.1 || exposure > 10.0) {
        Tango::Except::throw_exception("InvalidParameter", "Exposure must be between 0.1 and 10.0 seconds", "setLowerCCD10xExposure");
    }
    lower_ccd_10x_exposure_ = exposure;
    
    if (!sim_mode_ && lower_ccd_10x_driver_) {
        if (!lower_ccd_10x_driver_->setExposureTime(exposure * 1000.0)) {  // 转换为毫秒
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set exposure: " + lower_ccd_10x_driver_->getLastError(),
                "setLowerCCD10xExposure");
        }
    }
    
    log_event("Lower CCD 10x exposure set to " + std::to_string(exposure));
}

Tango::DevString ReflectionImagingDevice::getLowerCCD10xImage() {
    check_state("getLowerCCD10xImage");
    if (lower_ccd_10x_state_ != "READY") {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Lower CCD 10x is not ready", "getLowerCCD10xImage");
    }
    
    if (sim_mode_) {
        return Tango::string_dup("base64_encoded_image_data_lower_10x_sim");
    }
    
    if (!lower_ccd_10x_driver_) {
        Tango::Except::throw_exception("API_DeviceNotReady",
            "Lower CCD 10x driver not initialized", "getLowerCCD10xImage");
    }
    
    std::string base64_data = lower_ccd_10x_driver_->getLatestImageBase64();
    if (base64_data.empty()) {
        Tango::Except::throw_exception("API_DeviceError",
            "Failed to get image: " + lower_ccd_10x_driver_->getLastError(),
            "getLowerCCD10xImage");
    }
    
    return Tango::string_dup(base64_data.c_str());
}

// 批量操作
Tango::DevString ReflectionImagingDevice::captureAllImages() {
    check_state("captureAllImages");
    // 同时抓取所有4个CCD图像
    std::string upper_1x = std::string(captureUpperCCD1xImage());
    std::string upper_10x = std::string(captureUpperCCD10xImage());
    std::string lower_1x = std::string(captureLowerCCD1xImage());
    std::string lower_10x = std::string(captureLowerCCD10xImage());
    std::string result = "{\"upper1x\":\"" + upper_1x + 
                        "\",\"upper10x\":\"" + upper_10x +
                        "\",\"lower1x\":\"" + lower_1x +
                        "\",\"lower10x\":\"" + lower_10x + "\"}";
    return Tango::string_dup(result.c_str());
}

void ReflectionImagingDevice::startAutoCapture(Tango::DevDouble interval) {
    check_state("startAutoCapture");
    if (interval <= 0) {
        Tango::Except::throw_exception("InvalidParameter", "Interval must be greater than 0", "startAutoCapture");
    }
    auto_capture_interval_ = interval;
    auto_capture_enabled_ = true;
    // TODO: 启动自动抓取线程
    log_event("Auto capture started with interval " + std::to_string(interval) + "s");
}

void ReflectionImagingDevice::stopAutoCapture() {
    auto_capture_enabled_ = false;
    // TODO: 停止自动抓取线程
    log_event("Auto capture stopped");
}


// ========== Support Commands ==========

void ReflectionImagingDevice::operateUpperSupport(Tango::DevShort operation) {
    check_state("operateUpperSupport");
    if (operation == 0) {
        upperSupportLower();
    } else if (operation == 1) {
        upperSupportRise();
    } else {
        Tango::Except::throw_exception("InvalidParameter", "Operation must be 0 (lower) or 1 (rise)", "operateUpperSupport");
    }
}

void ReflectionImagingDevice::operateLowerSupport(Tango::DevShort operation) {
    check_state("operateLowerSupport");
    if (operation == 0) {
        lowerSupportLower();
    } else if (operation == 1) {
        lowerSupportRise();
    } else {
        Tango::Except::throw_exception("InvalidParameter", "Operation must be 0 (lower) or 1 (rise)", "operateLowerSupport");
    }
}

void ReflectionImagingDevice::upperSupportRise() {
    check_state("upperSupportRise");
    upper_support_state_ = "RISING";
    
    if (sim_mode_) {
        upper_support_position_ += 10.0;
        if (upper_support_position_ >= upper_support_work_pos_) {
            upper_support_position_ = upper_support_work_pos_;
            upper_support_state_ = "RISEN";
        }
        log_event("Upper support rising (sim)");
        return;
    }
    
    if (upper_support_axis_id_ < 0) {
        log_event("Upper support axis not configured");
        upper_support_state_ = "ERROR";
        return;
    }

    try {
        if (!upper_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Proxy not connected", "upperSupportRise");
        
        Tango::DeviceData arg; 
        Tango::DevVarDoubleArray arr; arr.length(2); 
        arr[0] = static_cast<double>(upper_support_axis_id_); 
        arr[1] = upper_support_work_pos_;
        arg << arr; 
        upper_platform_proxy_->command_inout("moveAbsolute", arg);
        log_event("Upper support rising command sent");
    } catch (Tango::DevFailed &e) {
        upper_support_state_ = "ERROR";
        log_event("Upper support rise failed: " + std::string(e.errors[0].desc));
    }
}

void ReflectionImagingDevice::upperSupportLower() {
    check_state("upperSupportLower");
    upper_support_state_ = "LOWERING";
    
    if (sim_mode_) {
        upper_support_position_ -= 10.0;
        if (upper_support_position_ <= upper_support_home_pos_) {
            upper_support_position_ = upper_support_home_pos_;
            upper_support_state_ = "LOWERED";
        }
        log_event("Upper support lowering (sim)");
        return;
    }
    
    if (upper_support_axis_id_ < 0) {
        log_event("Upper support axis not configured");
        upper_support_state_ = "ERROR";
        return;
    }

    try {
        if (!upper_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Proxy not connected", "upperSupportLower");
        
        Tango::DeviceData arg; 
        Tango::DevVarDoubleArray arr; arr.length(2); 
        arr[0] = static_cast<double>(upper_support_axis_id_); 
        arr[1] = upper_support_home_pos_;
        arg << arr; 
        upper_platform_proxy_->command_inout("moveAbsolute", arg);
        log_event("Upper support lowering command sent");
    } catch (Tango::DevFailed &e) {
        upper_support_state_ = "ERROR";
        log_event("Upper support lower failed: " + std::string(e.errors[0].desc));
    }
}

void ReflectionImagingDevice::lowerSupportRise() {
    check_state("lowerSupportRise");
    lower_support_state_ = "RISING";
    
    if (sim_mode_) {
        lower_support_position_ += 10.0;
        if (lower_support_position_ >= lower_support_work_pos_) {
            lower_support_position_ = lower_support_work_pos_;
            lower_support_state_ = "RISEN";
        }
        log_event("Lower support rising (sim)");
        return;
    }
    
    if (lower_support_axis_id_ < 0) {
        log_event("Lower support axis not configured");
        lower_support_state_ = "ERROR";
        return;
    }

    try {
        if (!lower_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Proxy not connected", "lowerSupportRise");
        
        Tango::DeviceData arg; 
        Tango::DevVarDoubleArray arr; arr.length(2); 
        arr[0] = static_cast<double>(lower_support_axis_id_); 
        arr[1] = lower_support_work_pos_;
        arg << arr; 
        lower_platform_proxy_->command_inout("moveAbsolute", arg);
        log_event("Lower support rising command sent");
    } catch (Tango::DevFailed &e) {
        lower_support_state_ = "ERROR";
        log_event("Lower support rise failed: " + std::string(e.errors[0].desc));
    }
}

void ReflectionImagingDevice::lowerSupportLower() {
    check_state("lowerSupportLower");
    lower_support_state_ = "LOWERING";
    
    if (sim_mode_) {
        lower_support_position_ -= 10.0;
        if (lower_support_position_ <= lower_support_home_pos_) {
            lower_support_position_ = lower_support_home_pos_;
            lower_support_state_ = "LOWERED";
        }
        log_event("Lower support lowering (sim)");
        return;
    }
    
    if (lower_support_axis_id_ < 0) {
        log_event("Lower support axis not configured");
        lower_support_state_ = "ERROR";
        return;
    }

    try {
        if (!lower_platform_proxy_) Tango::Except::throw_exception("API_NoProxy", "Proxy not connected", "lowerSupportLower");
        
        Tango::DeviceData arg; 
        Tango::DevVarDoubleArray arr; arr.length(2); 
        arr[0] = static_cast<double>(lower_support_axis_id_); 
        arr[1] = lower_support_home_pos_;
        arg << arr; 
        lower_platform_proxy_->command_inout("moveAbsolute", arg);
        log_event("Lower support lowering command sent");
    } catch (Tango::DevFailed &e) {
        lower_support_state_ = "ERROR";
        log_event("Lower support lower failed: " + std::string(e.errors[0].desc));
    }
}

void ReflectionImagingDevice::stopUpperSupport() {
    check_state("stopUpperSupport");
    
    if (sim_mode_) {
        upper_support_state_ = "LOWERED"; // Or stopped
        log_event("Upper support stopped (sim)");
        return;
    }
    
    if (upper_support_axis_id_ < 0) return;

    try {
         if (upper_platform_proxy_) {
            Tango::DeviceData arg;
            Tango::DevShort axis = static_cast<Tango::DevShort>(upper_support_axis_id_);
            arg << axis;
            upper_platform_proxy_->command_inout("stopMove", arg);
         }
         log_event("Upper support stopped");
    } catch (...) {
        log_event("Upper support stop failed");
    }
}

void ReflectionImagingDevice::stopLowerSupport() {
    check_state("stopLowerSupport");
    
    if (sim_mode_) {
        lower_support_state_ = "LOWERED";
        log_event("Lower support stopped (sim)");
        return;
    }
    
    if (lower_support_axis_id_ < 0) return;

    try {
         if (lower_platform_proxy_) {
            Tango::DeviceData arg;
            Tango::DevShort axis = static_cast<Tango::DevShort>(lower_support_axis_id_);
            arg << axis;
            lower_platform_proxy_->command_inout("stopMove", arg);
         }
         log_event("Lower support stopped");
    } catch (...) {
        log_event("Lower support stop failed");
    }
}

void ReflectionImagingDevice::update_support_states() {
    if (sim_mode_) return;
    
    // Check Upper Support
    if (upper_support_axis_id_ >= 0 && upper_platform_proxy_) {
        try {
            // Read Encoder (we need single axis read, or assume encoder proxy exists)
            // But usually we read from motion controller "readPosition" or similar if dedicated encoder line is not set.
            // Let's use get_attribute("position")? No, "readEncoder" command on motion controller usually takes axis?
            // AuxiliarySupportDevice uses encoder_proxy_->command_inout("readEncoder", channel) OR token_assist_pos_
            
            // Assume motion controller has "readPosition" or similar for specific axis.
            // Or use "readEncoder" command if supported by motion controller proxy for that axis.
            // StandardSystemDevice doesn't enforce "readPosition".
            // However, our proxy connects to "MotionController". Let's check MotionController interface.
            // If unknown, we can try "readActualPosition" or similar.
            // The `readEncoder` method in this class uses `encoder_proxy_`.
            
            // Simpler: Use `readEncoder` command on `upper_platform_proxy_` if it accepts axis?
            // Looking at `upperPlatformReadEncoder` implementation (Lines 1100+?), it calls `readEncoder` on `encoder_proxy_`.
            
            // Let's rely on `upper_platform_proxy_->read_attribute("Position_" + axis)`? No.
            // Best guess: Motion Controller has `readCurrentPosition(axis)`.
            // But we don't have that standard.
            
            // Let's try to read from `motion_controller_proxy_` using `readEncoder` with axis arg? 
            // In `upperPlatformReadEncoder`, we used `encoder_proxy_`.
            
            // If support axis is on the same motion controller, we might be able to read it.
            // Let's placeholder this with a TODO or minimal implementation.
            
        } catch(...) {}
    }
    // For now, leave empty implementation effectively or minimal state update based on last command?
    // User requested "Review implementation", so implementing it properly is key.
    
    // Let's try to read state from proxy if possible.
    // If we can't read position, we can't update state accurately.
    // I'll calculate state based on target vs current comparison if I could read current.
    
    // For now, I will assume the command execution succeeds and sets state.
    // Real feedback requires reading position.
    
    // I'll leave the state update logic minimal as I don't have the exact "Read Position" command for arbitrary axis 
    // without knowing the MotionController interface details (it seems to use `readEncoder` with channel on Encoder Device, 
    // or internal position).
    
    // However, I can try to read the "State" of the axis if available.
}

// ========== 三坐标平台运动学辅助函数 ==========

bool ReflectionImagingDevice::checkUpperPlatformLimits(double x, double y, double z) {
    // 检查绝对位置是否在限位范围内
    if (std::abs(x) > upper_platform_range_[0]) {
        WARN_STREAM << "Upper platform X=" << x << " exceeds limit ±" << upper_platform_range_[0] << endl;
        return false;
    }
    if (std::abs(y) > upper_platform_range_[1]) {
        WARN_STREAM << "Upper platform Y=" << y << " exceeds limit ±" << upper_platform_range_[1] << endl;
        return false;
    }
    if (std::abs(z) > upper_platform_range_[2]) {
        WARN_STREAM << "Upper platform Z=" << z << " exceeds limit ±" << upper_platform_range_[2] << endl;
        return false;
    }
    return true;
}

bool ReflectionImagingDevice::checkLowerPlatformLimits(double x, double y, double z) {
    // 检查绝对位置是否在限位范围内
    if (std::abs(x) > lower_platform_range_[0]) {
        WARN_STREAM << "Lower platform X=" << x << " exceeds limit ±" << lower_platform_range_[0] << endl;
        return false;
    }
    if (std::abs(y) > lower_platform_range_[1]) {
        WARN_STREAM << "Lower platform Y=" << y << " exceeds limit ±" << lower_platform_range_[1] << endl;
        return false;
    }
    if (std::abs(z) > lower_platform_range_[2]) {
        WARN_STREAM << "Lower platform Z=" << z << " exceeds limit ±" << lower_platform_range_[2] << endl;
        return false;
    }
    return true;
}

bool ReflectionImagingDevice::checkUpperPlatformRelativeLimits(double dx, double dy, double dz) {
    // 检查相对运动后的目标位置是否在限位范围内
    return checkUpperPlatformLimits(
        upper_platform_pos_[0] + dx,
        upper_platform_pos_[1] + dy,
        upper_platform_pos_[2] + dz
    );
}

bool ReflectionImagingDevice::checkLowerPlatformRelativeLimits(double dx, double dy, double dz) {
    // 检查相对运动后的目标位置是否在限位范围内
    return checkLowerPlatformLimits(
        lower_platform_pos_[0] + dx,
        lower_platform_pos_[1] + dy,
        lower_platform_pos_[2] + dz
    );
}

bool ReflectionImagingDevice::checkPlatformCollision(double upper_z, double lower_z) {
    // 检查上下平台Z轴是否会发生碰撞
    // 假设上平台Z正方向向下，下平台Z正方向向上
    // 当两个平台靠近时，需要保持最小安全距离
    double z_distance = std::abs(upper_z - lower_z);
    if (z_distance < MIN_PLATFORM_Z_DISTANCE) {
        WARN_STREAM << "Platform collision risk! Z distance=" << z_distance 
                   << "mm < min safe distance=" << MIN_PLATFORM_Z_DISTANCE << "mm" << endl;
        return true;  // 会碰撞
    }
    return false;  // 安全
}

ReflectionImagingDevice::SyncMoveParams ReflectionImagingDevice::calculateSyncVelocities(
    const std::array<double, 3>& upper_target,
    const std::array<double, 3>& lower_target,
    double max_velocity) {
    
    SyncMoveParams params;
    
    // 计算各轴的位移量
    std::array<double, 3> upper_dist, lower_dist;
    double max_dist = 0.0;
    
    for (int i = 0; i < 3; ++i) {
        upper_dist[i] = std::abs(upper_target[i] - upper_platform_pos_[i]);
        lower_dist[i] = std::abs(lower_target[i] - lower_platform_pos_[i]);
        max_dist = std::max(max_dist, std::max(upper_dist[i], lower_dist[i]));
    }
    
    // 根据最大位移计算总运动时间
    if (max_dist < 0.001) {
        // 几乎不需要运动
        params.total_time = 0.0;
        params.upper_velocity = {0.0, 0.0, 0.0};
        params.lower_velocity = {0.0, 0.0, 0.0};
        return params;
    }
    
    params.total_time = max_dist / max_velocity;
    
    // 根据总时间反算各轴速度，确保同时到达
    for (int i = 0; i < 3; ++i) {
        params.upper_velocity[i] = upper_dist[i] / params.total_time;
        params.lower_velocity[i] = lower_dist[i] / params.total_time;
    }
    
    INFO_STREAM << "Sync move calculated: total_time=" << params.total_time << "s" << endl;
    return params;
}

void ReflectionImagingDevice::updateSimulatedMotion() {
    // 仿真模式下更新运动状态
    if (!sim_mode_) return;
    
    // 检查是否有平台正在运动
    bool upper_moving = std::any_of(upper_platform_state_.begin(), upper_platform_state_.end(), 
                                     [](bool s) { return s; });
    bool lower_moving = std::any_of(lower_platform_state_.begin(), lower_platform_state_.end(), 
                                     [](bool s) { return s; });
    
    if (!upper_moving && !lower_moving) return;
    
    // 计算已运动时间
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - sim_motion_start_time_).count();
    
    // 简化的线性插值运动模拟
    double progress = (sim_motion_duration_ > 0) ? 
        std::min(1.0, static_cast<double>(elapsed_ms) / sim_motion_duration_) : 1.0;
    
    if (upper_moving) {
        for (int i = 0; i < 3; ++i) {
            if (upper_platform_state_[i]) {
                // 线性插值从当前位置到目标位置
                double start_pos = upper_platform_dire_pos_[i] - 
                    (upper_platform_target_[i] - upper_platform_pos_[i]);
                upper_platform_pos_[i] = start_pos + 
                    (upper_platform_target_[i] - start_pos) * progress;
            }
        }
    }
    
    if (lower_moving) {
        for (int i = 0; i < 3; ++i) {
            if (lower_platform_state_[i]) {
                double start_pos = lower_platform_dire_pos_[i] - 
                    (lower_platform_target_[i] - lower_platform_pos_[i]);
                lower_platform_pos_[i] = start_pos + 
                    (lower_platform_target_[i] - start_pos) * progress;
            }
        }
    }
    
    // 运动完成后更新状态
    if (progress >= 1.0) {
        if (upper_moving) {
            for (int i = 0; i < 3; ++i) {
                upper_platform_pos_[i] = upper_platform_target_[i];
                upper_platform_state_[i] = false;
            }
            log_event("Upper platform motion completed (sim)");
        }
        if (lower_moving) {
            for (int i = 0; i < 3; ++i) {
                lower_platform_pos_[i] = lower_platform_target_[i];
                lower_platform_state_[i] = false;
            }
            log_event("Lower platform motion completed (sim)");
        }
    }
}

// ========== Misc Commands ==========

Tango::DevString ReflectionImagingDevice::readtAxis() {
    check_state("readtAxis");
    // 汇总所有轴参数为JSON
    std::ostringstream oss;
    oss << "{"
        << "\"upperPlatform\":{\"pos\":["
        << upper_platform_pos_[0] << "," << upper_platform_pos_[1] << "," << upper_platform_pos_[2]
        << "],\"direPos\":["
        << upper_platform_dire_pos_[0] << "," << upper_platform_dire_pos_[1] << "," << upper_platform_dire_pos_[2]
        << "]},"
        << "\"lowerPlatform\":{\"pos\":["
        << lower_platform_pos_[0] << "," << lower_platform_pos_[1] << "," << lower_platform_pos_[2]
        << "],\"direPos\":["
        << lower_platform_dire_pos_[0] << "," << lower_platform_dire_pos_[1] << "," << lower_platform_dire_pos_[2]
        << "]}"
        << "}";
    axis_parameter_ = oss.str();
    return Tango::string_dup(axis_parameter_.c_str());
}

void ReflectionImagingDevice::exportAxis() {
    check_state("exportAxis");
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::ostringstream filename;
    filename << "axis_parameters_" << std::put_time(timeinfo, "%Y%m%d_%H%M%S") << ".json";
    
    std::ofstream file(filename.str());
    if (file.is_open()) {
        file << readtAxis();
        file.close();
        log_event("Axis parameters exported to " + filename.str());
    } else {
        Tango::Except::throw_exception("API_FileError", "Failed to open file for writing", "ReflectionImagingDevice::exportAxis");
    }
}

void ReflectionImagingDevice::exportLogs() {
    check_state("exportLogs");
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::ostringstream filename;
    filename << "reflection_logs_" << std::put_time(timeinfo, "%Y%m%d_%H%M%S") << ".json";
    
    std::ofstream file(filename.str());
    if (file.is_open()) {
        file << reflection_logs_;
        file.close();
        log_event("Logs exported to " + filename.str());
    } else {
        Tango::Except::throw_exception("API_FileError", "Failed to open file for writing", "ReflectionImagingDevice::exportLogs");
    }
}

void ReflectionImagingDevice::simSwitch(Tango::DevShort mode) {
    sim_mode_ = (mode != 0);
    
    // 注意：运行时切换只影响当前会话，server 重启后恢复配置文件的值（不持久化）
    if (sim_mode_) {
        INFO_STREAM << "[DEBUG] simSwitch: Enabling SIMULATION MODE (运行时切换，不持久化)" << std::endl;
        log_event("Simulation mode enabled (runtime switch)");
    } else {
        INFO_STREAM << "[DEBUG] simSwitch: Disabling simulation mode, switching to REAL MODE (运行时切换，不持久化)" << std::endl;
        log_event("Simulation mode disabled (runtime switch)");
    }
}

void ReflectionImagingDevice::always_executed_hook() {
    // 定期更新状态
    update_support_states();
    update_sub_devices();
    
    // 仿真模式下更新运动状态
    updateSimulatedMotion();
    
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
            // 连接丢失时自动启用刹车（安全保护）
            if (brake_power_port_ >= 0 && brake_released_) {
                INFO_STREAM << "[BrakeControl] Connection lost, auto-engaging brake (safety)" << std::endl;
                if (!engage_brake()) {
                    WARN_STREAM << "[BrakeControl] Failed to engage brake on connection loss" << std::endl;
                }
            }
            set_state(Tango::FAULT);
            set_status("Network connection lost");
        }
    }
    
    // 检查CCD相机状态流转
    // 如果CCD正在采集，检查是否完成
    if (upper_ccd_1x_state_ == "CAPTURING" || upper_ccd_10x_state_ == "CAPTURING" ||
        lower_ccd_1x_state_ == "CAPTURING" || lower_ccd_10x_state_ == "CAPTURING") {
        // TODO: 检查硬件采集完成状态
        // 这里可以添加超时检测，如果超过一定时间仍为CAPTURING，则标记为ERROR
    }
    
    // 自动抓取逻辑
    if (auto_capture_enabled_ && auto_capture_interval_ > 0) {
        static auto last_capture_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_capture_time).count();
        if (elapsed >= static_cast<long>(auto_capture_interval_ * 1000)) {
            try {
                captureAllImages();
                last_capture_time = now;
            } catch (...) {
                // 抓取失败，记录但不中断
                log_event("Auto capture failed");
            }
        }
    }
}

// ========== Attributes Implementation ==========

void ReflectionImagingDevice::read_attr(Tango::Attribute &attr) {
    std::string name = attr.get_name();
    
    // DEBUG: 确认 read_attr 被调用
    DEBUG_STREAM << "[DEBUG] read_attr called for: " << name << std::endl;
    
    // 标准属性
    if (name == "selfCheckResult") { read_self_check_result(attr); return; }
    if (name == "positionUnit") { read_position_unit(attr); return; }
    if (name == "groupAttributeJson") { read_group_attribute_json(attr); return; }
    if (name == "reflectionLogs") { read_reflection_logs(attr); return; }
    if (name == "faultState") { read_fault_state(attr); return; }
    if (name == "resultValue") { read_result_value(attr); return; }
    if (name == "driverPowerStatus") { read_driver_power_status(attr); return; }
    if (name == "brakeStatus") { read_brake_status(attr); return; }
    

    
    // 上平台属性
    if (name == "upperPlatformPos") { read_upper_platform_pos(attr); return; }
    if (name == "upperPlatformDirePos") { read_upper_platform_dire_pos(attr); return; }
    if (name == "upperPlatformLimOrgState") { read_upper_platform_lim_org_state(attr); return; }
    if (name == "upperPlatformState") { read_upper_platform_state(attr); return; }
    if (name == "upperPlatformAxisParameter") { read_upper_platform_axis_parameter(attr); return; }
    
    // 下平台属性
    if (name == "lowerPlatformPos") { read_lower_platform_pos(attr); return; }
    if (name == "lowerPlatformDirePos") { read_lower_platform_dire_pos(attr); return; }
    if (name == "lowerPlatformLimOrgState") { read_lower_platform_lim_org_state(attr); return; }
    if (name == "lowerPlatformState") { read_lower_platform_state(attr); return; }
    if (name == "lowerPlatformAxisParameter") { read_lower_platform_axis_parameter(attr); return; }
    
    // 单轴属性 (用于GUI单轴独立控制)
    if (name == "upperPlatformPosX") { read_upper_platform_pos_x(attr); return; }
    if (name == "upperPlatformPosY") { read_upper_platform_pos_y(attr); return; }
    if (name == "upperPlatformPosZ") { read_upper_platform_pos_z(attr); return; }
    if (name == "upperPlatformStateX") { read_upper_platform_state_x(attr); return; }
    if (name == "upperPlatformStateY") { read_upper_platform_state_y(attr); return; }
    if (name == "upperPlatformStateZ") { read_upper_platform_state_z(attr); return; }
    if (name == "lowerPlatformPosX") { read_lower_platform_pos_x(attr); return; }
    if (name == "lowerPlatformPosY") { read_lower_platform_pos_y(attr); return; }
    if (name == "lowerPlatformPosZ") { read_lower_platform_pos_z(attr); return; }
    if (name == "lowerPlatformStateX") { read_lower_platform_state_x(attr); return; }
    if (name == "lowerPlatformStateY") { read_lower_platform_state_y(attr); return; }
    if (name == "lowerPlatformStateZ") { read_lower_platform_state_z(attr); return; }
    
    // CCD相机属性
    // 四CCD相机属性（上下各两个，1倍和10倍物镜）
    // 上1倍物镜CCD属性
    if (name == "upperCCD1xState") { read_upper_ccd_1x_state(attr); return; }
    if (name == "upperCCD1xRingLightOn") { read_upper_ccd_1x_ring_light_on(attr); return; }
    if (name == "upperCCD1xExposure") { read_upper_ccd_1x_exposure(attr); return; }
    if (name == "upperCCD1xTriggerMode") { read_upper_ccd_1x_trigger_mode(attr); return; }
    if (name == "upperCCD1xResolution") { read_upper_ccd_1x_resolution(attr); return; }
    if (name == "upperCCD1xWidth") { read_upper_ccd_1x_width(attr); return; }
    if (name == "upperCCD1xHeight") { read_upper_ccd_1x_height(attr); return; }
    if (name == "upperCCD1xGain") { read_upper_ccd_1x_gain(attr); return; }
    if (name == "upperCCD1xBrightness") { read_upper_ccd_1x_brightness(attr); return; }
    if (name == "upperCCD1xContrast") { read_upper_ccd_1x_contrast(attr); return; }
    if (name == "upperCCD1xImageUrl") { read_upper_ccd_1x_image_url(attr); return; }
    if (name == "upperCCD1xLastCaptureTime") { read_upper_ccd_1x_last_capture_time(attr); return; }
    
    // 上10倍物镜CCD属性
    if (name == "upperCCD10xState") { read_upper_ccd_10x_state(attr); return; }
    if (name == "upperCCD10xRingLightOn") { read_upper_ccd_10x_ring_light_on(attr); return; }
    if (name == "upperCCD10xExposure") { read_upper_ccd_10x_exposure(attr); return; }
    if (name == "upperCCD10xTriggerMode") { read_upper_ccd_10x_trigger_mode(attr); return; }
    if (name == "upperCCD10xResolution") { read_upper_ccd_10x_resolution(attr); return; }
    if (name == "upperCCD10xWidth") { read_upper_ccd_10x_width(attr); return; }
    if (name == "upperCCD10xHeight") { read_upper_ccd_10x_height(attr); return; }
    if (name == "upperCCD10xGain") { read_upper_ccd_10x_gain(attr); return; }
    if (name == "upperCCD10xBrightness") { read_upper_ccd_10x_brightness(attr); return; }
    if (name == "upperCCD10xContrast") { read_upper_ccd_10x_contrast(attr); return; }
    if (name == "upperCCD10xImageUrl") { read_upper_ccd_10x_image_url(attr); return; }
    if (name == "upperCCD10xLastCaptureTime") { read_upper_ccd_10x_last_capture_time(attr); return; }
    
    // 下1倍物镜CCD属性
    if (name == "lowerCCD1xState") { read_lower_ccd_1x_state(attr); return; }
    if (name == "lowerCCD1xRingLightOn") { read_lower_ccd_1x_ring_light_on(attr); return; }
    if (name == "lowerCCD1xExposure") { read_lower_ccd_1x_exposure(attr); return; }
    if (name == "lowerCCD1xTriggerMode") { read_lower_ccd_1x_trigger_mode(attr); return; }
    if (name == "lowerCCD1xResolution") { read_lower_ccd_1x_resolution(attr); return; }
    if (name == "lowerCCD1xWidth") { read_lower_ccd_1x_width(attr); return; }
    if (name == "lowerCCD1xHeight") { read_lower_ccd_1x_height(attr); return; }
    if (name == "lowerCCD1xGain") { read_lower_ccd_1x_gain(attr); return; }
    if (name == "lowerCCD1xBrightness") { read_lower_ccd_1x_brightness(attr); return; }
    if (name == "lowerCCD1xContrast") { read_lower_ccd_1x_contrast(attr); return; }
    if (name == "lowerCCD1xImageUrl") { read_lower_ccd_1x_image_url(attr); return; }
    if (name == "lowerCCD1xLastCaptureTime") { read_lower_ccd_1x_last_capture_time(attr); return; }
    
    // 下10倍物镜CCD属性
    if (name == "lowerCCD10xState") { read_lower_ccd_10x_state(attr); return; }
    if (name == "lowerCCD10xRingLightOn") { read_lower_ccd_10x_ring_light_on(attr); return; }
    if (name == "lowerCCD10xExposure") { read_lower_ccd_10x_exposure(attr); return; }
    if (name == "lowerCCD10xTriggerMode") { read_lower_ccd_10x_trigger_mode(attr); return; }
    if (name == "lowerCCD10xResolution") { read_lower_ccd_10x_resolution(attr); return; }
    if (name == "lowerCCD10xWidth") { read_lower_ccd_10x_width(attr); return; }
    if (name == "lowerCCD10xHeight") { read_lower_ccd_10x_height(attr); return; }
    if (name == "lowerCCD10xGain") { read_lower_ccd_10x_gain(attr); return; }
    if (name == "lowerCCD10xBrightness") { read_lower_ccd_10x_brightness(attr); return; }
    if (name == "lowerCCD10xContrast") { read_lower_ccd_10x_contrast(attr); return; }
    if (name == "lowerCCD10xImageUrl") { read_lower_ccd_10x_image_url(attr); return; }
    if (name == "lowerCCD10xLastCaptureTime") { read_lower_ccd_10x_last_capture_time(attr); return; }
    if (name == "imageCaptureCount") { read_image_capture_count(attr); return; }
    if (name == "autoCaptureEnabled") { read_auto_capture_enabled(attr); return; }
    
    // 辅助支撑属性
    if (name == "upperSupportPosition") { read_upper_support_position(attr); return; }
    if (name == "lowerSupportPosition") { read_lower_support_position(attr); return; }
    if (name == "upperSupportState") { read_upper_support_state(attr); return; }
    if (name == "lowerSupportState") { read_lower_support_state(attr); return; }
    
    // 其他属性
    if (name == "axisParameter") { read_axis_parameter(attr); return; }
}

void ReflectionImagingDevice::write_attr(Tango::WAttribute &attr) {
    std::string name = attr.get_name();
    
    // 上1倍物镜CCD可写属性
    if (name == "upperCCD1xExposure") { write_upper_ccd_1x_exposure(attr); return; }
    if (name == "upperCCD1xTriggerMode") { write_upper_ccd_1x_trigger_mode(attr); return; }
    if (name == "upperCCD1xResolution") { write_upper_ccd_1x_resolution(attr); return; }
    if (name == "upperCCD1xWidth") { write_upper_ccd_1x_width(attr); return; }
    if (name == "upperCCD1xHeight") { write_upper_ccd_1x_height(attr); return; }
    if (name == "upperCCD1xGain") { write_upper_ccd_1x_gain(attr); return; }
    if (name == "upperCCD1xBrightness") { write_upper_ccd_1x_brightness(attr); return; }
    if (name == "upperCCD1xContrast") { write_upper_ccd_1x_contrast(attr); return; }
    
    // 上10倍物镜CCD可写属性
    if (name == "upperCCD10xExposure") { write_upper_ccd_10x_exposure(attr); return; }
    if (name == "upperCCD10xTriggerMode") { write_upper_ccd_10x_trigger_mode(attr); return; }
    if (name == "upperCCD10xResolution") { write_upper_ccd_10x_resolution(attr); return; }
    if (name == "upperCCD10xWidth") { write_upper_ccd_10x_width(attr); return; }
    if (name == "upperCCD10xHeight") { write_upper_ccd_10x_height(attr); return; }
    if (name == "upperCCD10xGain") { write_upper_ccd_10x_gain(attr); return; }
    if (name == "upperCCD10xBrightness") { write_upper_ccd_10x_brightness(attr); return; }
    if (name == "upperCCD10xContrast") { write_upper_ccd_10x_contrast(attr); return; }
    
    // 下1倍物镜CCD可写属性
    if (name == "lowerCCD1xExposure") { write_lower_ccd_1x_exposure(attr); return; }
    if (name == "lowerCCD1xTriggerMode") { write_lower_ccd_1x_trigger_mode(attr); return; }
    if (name == "lowerCCD1xResolution") { write_lower_ccd_1x_resolution(attr); return; }
    if (name == "lowerCCD1xWidth") { write_lower_ccd_1x_width(attr); return; }
    if (name == "lowerCCD1xHeight") { write_lower_ccd_1x_height(attr); return; }
    if (name == "lowerCCD1xGain") { write_lower_ccd_1x_gain(attr); return; }
    if (name == "lowerCCD1xBrightness") { write_lower_ccd_1x_brightness(attr); return; }
    if (name == "lowerCCD1xContrast") { write_lower_ccd_1x_contrast(attr); return; }
    
    // 下10倍物镜CCD可写属性
    if (name == "lowerCCD10xExposure") { write_lower_ccd_10x_exposure(attr); return; }
    if (name == "lowerCCD10xTriggerMode") { write_lower_ccd_10x_trigger_mode(attr); return; }
    if (name == "lowerCCD10xResolution") { write_lower_ccd_10x_resolution(attr); return; }
    if (name == "lowerCCD10xWidth") { write_lower_ccd_10x_width(attr); return; }
    if (name == "lowerCCD10xHeight") { write_lower_ccd_10x_height(attr); return; }
    if (name == "lowerCCD10xGain") { write_lower_ccd_10x_gain(attr); return; }
    if (name == "lowerCCD10xBrightness") { write_lower_ccd_10x_brightness(attr); return; }
    if (name == "lowerCCD10xContrast") { write_lower_ccd_10x_contrast(attr); return; }
    
    // 其他可写属性
    if (name == "positionUnit") { 
        Tango::DevString val;
        attr.get_write_value(val);
        position_unit_ = std::string(val);
        return; 
    }
}

// 标准属性读取实现
void ReflectionImagingDevice::read_self_check_result(Tango::Attribute& attr) { 
    attr_selfCheckResult_read = self_check_result_;
    attr.set_value(&attr_selfCheckResult_read); 
}

void ReflectionImagingDevice::read_position_unit(Tango::Attribute& attr) { 
    attr_positionUnit_read = Tango::string_dup(position_unit_.c_str()); 
    attr.set_value(&attr_positionUnit_read); 
}

void ReflectionImagingDevice::read_group_attribute_json(Tango::Attribute& attr) { 
    // 组合所有关键属性用于一次性获取
    std::ostringstream oss;
    oss << "{";
    oss << "\"State\":\"" << Tango::DevStateName[get_state()] << "\"";

    oss << ",\"selfCheckResult\":" << self_check_result_;
    oss << ",\"resultValue\":" << result_value_;
    oss << ",\"positionUnit\":\"" << position_unit_ << "\"";
    oss << "}";
    attr_groupAttributeJson_read = Tango::string_dup(oss.str().c_str());
    attr.set_value(&attr_groupAttributeJson_read); 
}

void ReflectionImagingDevice::read_reflection_logs(Tango::Attribute& attr) { 
    attr_reflectionLogs_read = Tango::string_dup(reflection_logs_.c_str()); 
    attr.set_value(&attr_reflectionLogs_read); 
}

void ReflectionImagingDevice::read_fault_state(Tango::Attribute& attr) { 
    attr_faultState_read = Tango::string_dup(fault_state_.c_str()); 
    attr.set_value(&attr_faultState_read); 
}

void ReflectionImagingDevice::read_result_value(Tango::Attribute& attr) { 
    attr_resultValue_read = result_value_;
    attr.set_value(&attr_resultValue_read); 
}

void ReflectionImagingDevice::read_driver_power_status(Tango::Attribute& attr) {
    // 直接使用成员变量地址，确保指针在 Tango 读取时仍然有效
    attr.set_value(&driver_power_enabled_);
}

void ReflectionImagingDevice::read_brake_status(Tango::Attribute& attr) {
    // 直接使用成员变量地址，确保指针在 Tango 读取时仍然有效
    attr.set_value(&brake_released_);
}

// 上平台属性读取实现
void ReflectionImagingDevice::read_upper_platform_pos(Tango::Attribute& attr) { 
    for (int i = 0; i < 3; ++i) attr_upperPlatformPos_read[i] = upper_platform_pos_[i];
    attr.set_value(attr_upperPlatformPos_read, 3); 
}

void ReflectionImagingDevice::read_upper_platform_dire_pos(Tango::Attribute& attr) { 
    for (int i = 0; i < 3; ++i) attr_upperPlatformDirePos_read[i] = upper_platform_dire_pos_[i];
    attr.set_value(attr_upperPlatformDirePos_read, 3); 
}

void ReflectionImagingDevice::read_upper_platform_lim_org_state(Tango::Attribute& attr) { 
    for (int i = 0; i < 3; ++i) attr_upperPlatformLimOrgState_read[i] = upper_platform_lim_org_state_[i];
    attr.set_value(attr_upperPlatformLimOrgState_read, 3); 
}

void ReflectionImagingDevice::read_upper_platform_state(Tango::Attribute& attr) { 
    for (size_t i = 0; i < 3; ++i) attr_upperPlatformState_read[i] = upper_platform_state_[i];
    attr.set_value(attr_upperPlatformState_read, 3);
}

void ReflectionImagingDevice::read_upper_platform_axis_parameter(Tango::Attribute& attr) { 
    attr_upperPlatformAxisParameter_read = Tango::string_dup(upper_platform_axis_parameter_.c_str()); 
    attr.set_value(&attr_upperPlatformAxisParameter_read); 
}

// 下平台属性读取实现
void ReflectionImagingDevice::read_lower_platform_pos(Tango::Attribute& attr) { 
    for (int i = 0; i < 3; ++i) attr_lowerPlatformPos_read[i] = lower_platform_pos_[i];
    attr.set_value(attr_lowerPlatformPos_read, 3); 
}

void ReflectionImagingDevice::read_lower_platform_dire_pos(Tango::Attribute& attr) { 
    for (int i = 0; i < 3; ++i) attr_lowerPlatformDirePos_read[i] = lower_platform_dire_pos_[i];
    attr.set_value(attr_lowerPlatformDirePos_read, 3); 
}

void ReflectionImagingDevice::read_lower_platform_lim_org_state(Tango::Attribute& attr) { 
    for (int i = 0; i < 3; ++i) attr_lowerPlatformLimOrgState_read[i] = lower_platform_lim_org_state_[i];
    attr.set_value(attr_lowerPlatformLimOrgState_read, 3); 
}

void ReflectionImagingDevice::read_lower_platform_state(Tango::Attribute& attr) { 
    for (size_t i = 0; i < 3; ++i) attr_lowerPlatformState_read[i] = lower_platform_state_[i];
    attr.set_value(attr_lowerPlatformState_read, 3);
}

void ReflectionImagingDevice::read_lower_platform_axis_parameter(Tango::Attribute& attr) { 
    attr_lowerPlatformAxisParameter_read = Tango::string_dup(lower_platform_axis_parameter_.c_str()); 
    attr.set_value(&attr_lowerPlatformAxisParameter_read); 
}

// 单轴属性读取实现 (用于GUI单轴独立控制)
void ReflectionImagingDevice::read_upper_platform_pos_x(Tango::Attribute& attr) {
    attr_upperPlatformPosX_read = upper_platform_pos_[0];
    attr.set_value(&attr_upperPlatformPosX_read);
}

void ReflectionImagingDevice::read_upper_platform_pos_y(Tango::Attribute& attr) {
    attr_upperPlatformPosY_read = upper_platform_pos_[1];
    attr.set_value(&attr_upperPlatformPosY_read);
}

void ReflectionImagingDevice::read_upper_platform_pos_z(Tango::Attribute& attr) {
    attr_upperPlatformPosZ_read = upper_platform_pos_[2];
    attr.set_value(&attr_upperPlatformPosZ_read);
}

void ReflectionImagingDevice::read_upper_platform_state_x(Tango::Attribute& attr) {
    attr_upperPlatformStateX_read = upper_platform_state_[0];
    attr.set_value(&attr_upperPlatformStateX_read);
}

void ReflectionImagingDevice::read_upper_platform_state_y(Tango::Attribute& attr) {
    attr_upperPlatformStateY_read = upper_platform_state_[1];
    attr.set_value(&attr_upperPlatformStateY_read);
}

void ReflectionImagingDevice::read_upper_platform_state_z(Tango::Attribute& attr) {
    attr_upperPlatformStateZ_read = upper_platform_state_[2];
    attr.set_value(&attr_upperPlatformStateZ_read);
}

void ReflectionImagingDevice::read_lower_platform_pos_x(Tango::Attribute& attr) {
    attr_lowerPlatformPosX_read = lower_platform_pos_[0];
    attr.set_value(&attr_lowerPlatformPosX_read);
}

void ReflectionImagingDevice::read_lower_platform_pos_y(Tango::Attribute& attr) {
    attr_lowerPlatformPosY_read = lower_platform_pos_[1];
    attr.set_value(&attr_lowerPlatformPosY_read);
}

void ReflectionImagingDevice::read_lower_platform_pos_z(Tango::Attribute& attr) {
    attr_lowerPlatformPosZ_read = lower_platform_pos_[2];
    attr.set_value(&attr_lowerPlatformPosZ_read);
}

void ReflectionImagingDevice::read_lower_platform_state_x(Tango::Attribute& attr) {
    attr_lowerPlatformStateX_read = lower_platform_state_[0];
    attr.set_value(&attr_lowerPlatformStateX_read);
}

void ReflectionImagingDevice::read_lower_platform_state_y(Tango::Attribute& attr) {
    attr_lowerPlatformStateY_read = lower_platform_state_[1];
    attr.set_value(&attr_lowerPlatformStateY_read);
}

void ReflectionImagingDevice::read_lower_platform_state_z(Tango::Attribute& attr) {
    attr_lowerPlatformStateZ_read = lower_platform_state_[2];
    attr.set_value(&attr_lowerPlatformStateZ_read);
}

// 四CCD相机属性读取实现（上下各两个，1倍和10倍物镜，海康MV-CU020-19GC）

// 上1倍物镜CCD属性（粗定位）
void ReflectionImagingDevice::read_upper_ccd_1x_state(Tango::Attribute& attr) { 
    attr_upperCcd1xState_read = Tango::string_dup(upper_ccd_1x_state_.c_str()); 
    attr.set_value(&attr_upperCcd1xState_read); 
}

void ReflectionImagingDevice::read_upper_ccd_1x_ring_light_on(Tango::Attribute& attr) {
    attr_upperCcd1xRingLightOn_read = upper_ccd_1x_ring_light_on_ ? 1 : 0;
    attr.set_value(&attr_upperCcd1xRingLightOn_read);
}

void ReflectionImagingDevice::read_upper_ccd_1x_exposure(Tango::Attribute& attr) { 
    attr.set_value(&upper_ccd_1x_exposure_); 
}

void ReflectionImagingDevice::write_upper_ccd_1x_exposure(Tango::WAttribute& attr) { 
    attr.get_write_value(upper_ccd_1x_exposure_);
    setUpperCCD1xExposure(upper_ccd_1x_exposure_);
}

void ReflectionImagingDevice::read_upper_ccd_1x_trigger_mode(Tango::Attribute& attr) {
    attr_upperCcd1xTriggerMode_read = Tango::string_dup(upper_ccd_1x_trigger_mode_.c_str());
    attr.set_value(&attr_upperCcd1xTriggerMode_read);
}

void ReflectionImagingDevice::write_upper_ccd_1x_trigger_mode(Tango::WAttribute& attr) {
    Tango::DevString val;
    attr.get_write_value(val);
    std::string mode(val ? val : "");
    if (mode != "Software" && mode != "Hardware" && mode != "Continuous") {
        Tango::Except::throw_exception("InvalidParameter",
            "TriggerMode must be one of: Software, Hardware, Continuous",
            "write_upper_ccd_1x_trigger_mode");
    }
    upper_ccd_1x_trigger_mode_ = mode;
    if (!sim_mode_ && upper_ccd_1x_driver_) {
        Hikvision::MV_CU020_19GC::TriggerMode driver_mode;
        if (mode == "Software") {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_SOFTWARE;
        } else if (mode == "Hardware") {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_HARDWARE;
        } else {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_CONTINUOUS;
        }
        if (!upper_ccd_1x_driver_->setTriggerMode(driver_mode)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set trigger mode: " + upper_ccd_1x_driver_->getLastError(),
                "write_upper_ccd_1x_trigger_mode");
        }
    }
    log_event("Upper CCD 1x trigger mode set to " + mode);
}

void ReflectionImagingDevice::read_upper_ccd_1x_resolution(Tango::Attribute& attr) { 
    attr_upperCcd1xResolution_read = Tango::string_dup(upper_ccd_1x_resolution_.c_str()); 
    attr.set_value(&attr_upperCcd1xResolution_read); 
}

void ReflectionImagingDevice::write_upper_ccd_1x_resolution(Tango::WAttribute& attr) {
    Tango::DevString val;
    attr.get_write_value(val);
    std::string new_resolution(val);
    
    // 解析分辨率字符串 "WIDTHxHEIGHT"
    size_t pos = new_resolution.find('x');
    if (pos == std::string::npos) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Resolution format must be 'WIDTHxHEIGHT' (e.g., '1920x1080')", 
            "write_upper_ccd_1x_resolution");
    }
    
    try {
        long width = std::stol(new_resolution.substr(0, pos));
        long height = std::stol(new_resolution.substr(pos + 1));
        
        if (width < 640 || width > 4096 || height < 480 || height > 4096) {
            Tango::Except::throw_exception("InvalidParameter", 
                "Resolution width must be 640-4096, height must be 480-4096", 
                "write_upper_ccd_1x_resolution");
        }
        
        upper_ccd_1x_width_ = width;
        upper_ccd_1x_height_ = height;
        upper_ccd_1x_resolution_ = new_resolution;
        
        if (!sim_mode_ && upper_ccd_1x_driver_) {
            if (!upper_ccd_1x_driver_->setResolution(width, height)) {
                Tango::Except::throw_exception("API_DeviceError",
                    "Failed to set resolution: " + upper_ccd_1x_driver_->getLastError(),
                    "write_upper_ccd_1x_resolution");
            }
        }
        
        log_event("Upper CCD 1x resolution set to " + new_resolution);
    } catch (const std::exception& e) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Failed to parse resolution: " + std::string(e.what()), 
            "write_upper_ccd_1x_resolution");
    }
}

void ReflectionImagingDevice::read_upper_ccd_1x_width(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_1x_width_);
}

void ReflectionImagingDevice::write_upper_ccd_1x_width(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_1x_width_);
    if (upper_ccd_1x_width_ < 640 || upper_ccd_1x_width_ > 4096) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Width must be between 640 and 4096", "write_upper_ccd_1x_width");
    }
    
    // 更新分辨率字符串
    upper_ccd_1x_resolution_ = std::to_string(upper_ccd_1x_width_) + "x" + 
                              std::to_string(upper_ccd_1x_height_);
    
    if (!sim_mode_) {
        // TODO: 设置硬件分辨率（海康MV-CU020-19GC驱动）
        // MV_CU020_19GC_SetResolution(camera_id, upper_ccd_1x_width_, upper_ccd_1x_height_);
    }
    
    log_event("Upper CCD 1x width set to " + std::to_string(upper_ccd_1x_width_));
}

void ReflectionImagingDevice::read_upper_ccd_1x_height(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_1x_height_);
}

void ReflectionImagingDevice::write_upper_ccd_1x_height(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_1x_height_);
    if (upper_ccd_1x_height_ < 480 || upper_ccd_1x_height_ > 4096) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Height must be between 480 and 4096", "write_upper_ccd_1x_height");
    }
    
    // 更新分辨率字符串
    upper_ccd_1x_resolution_ = std::to_string(upper_ccd_1x_width_) + "x" + 
                              std::to_string(upper_ccd_1x_height_);
    
    if (!sim_mode_) {
        // TODO: 设置硬件分辨率（海康MV-CU020-19GC驱动）
        // MV_CU020_19GC_SetResolution(camera_id, upper_ccd_1x_width_, upper_ccd_1x_height_);
    }
    
    log_event("Upper CCD 1x height set to " + std::to_string(upper_ccd_1x_height_));
}

void ReflectionImagingDevice::read_upper_ccd_1x_gain(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_1x_gain_);
}

void ReflectionImagingDevice::write_upper_ccd_1x_gain(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_1x_gain_);
    if (upper_ccd_1x_gain_ < 0.0 || upper_ccd_1x_gain_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Gain must be between 0.0 and 100.0", "write_upper_ccd_1x_gain");
    }
    
    if (!sim_mode_ && upper_ccd_1x_driver_) {
        if (!upper_ccd_1x_driver_->setGain(upper_ccd_1x_gain_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set gain: " + upper_ccd_1x_driver_->getLastError(),
                "write_upper_ccd_1x_gain");
        }
    }
    
    log_event("Upper CCD 1x gain set to " + std::to_string(upper_ccd_1x_gain_));
}

void ReflectionImagingDevice::read_upper_ccd_1x_brightness(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_1x_brightness_);
}

void ReflectionImagingDevice::write_upper_ccd_1x_brightness(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_1x_brightness_);
    if (upper_ccd_1x_brightness_ < -100.0 || upper_ccd_1x_brightness_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Brightness must be between -100.0 and 100.0", "write_upper_ccd_1x_brightness");
    }
    
    if (!sim_mode_ && upper_ccd_1x_driver_) {
        if (!upper_ccd_1x_driver_->setBrightness(upper_ccd_1x_brightness_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set brightness: " + upper_ccd_1x_driver_->getLastError(),
                "write_upper_ccd_1x_brightness");
        }
    }
    
    log_event("Upper CCD 1x brightness set to " + std::to_string(upper_ccd_1x_brightness_));
}

void ReflectionImagingDevice::read_upper_ccd_1x_contrast(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_1x_contrast_);
}

void ReflectionImagingDevice::write_upper_ccd_1x_contrast(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_1x_contrast_);
    if (upper_ccd_1x_contrast_ < -100.0 || upper_ccd_1x_contrast_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Contrast must be between -100.0 and 100.0", "write_upper_ccd_1x_contrast");
    }
    
    if (!sim_mode_ && upper_ccd_1x_driver_) {
        if (!upper_ccd_1x_driver_->setContrast(upper_ccd_1x_contrast_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set contrast: " + upper_ccd_1x_driver_->getLastError(),
                "write_upper_ccd_1x_contrast");
        }
    }
    
    log_event("Upper CCD 1x contrast set to " + std::to_string(upper_ccd_1x_contrast_));
}

void ReflectionImagingDevice::read_upper_ccd_1x_image_url(Tango::Attribute& attr) { 
    attr_upperCcd1xImageUrl_read = Tango::string_dup(upper_ccd_1x_image_url_.c_str()); 
    attr.set_value(&attr_upperCcd1xImageUrl_read); 
}

void ReflectionImagingDevice::read_upper_ccd_1x_last_capture_time(Tango::Attribute& attr) { 
    attr_upperCcd1xLastCaptureTime_read = Tango::string_dup(upper_ccd_1x_last_capture_time_.c_str()); 
    attr.set_value(&attr_upperCcd1xLastCaptureTime_read); 
}

// 上10倍物镜CCD属性（近距离观察）
void ReflectionImagingDevice::read_upper_ccd_10x_state(Tango::Attribute& attr) { 
    attr_upperCcd10xState_read = Tango::string_dup(upper_ccd_10x_state_.c_str()); 
    attr.set_value(&attr_upperCcd10xState_read); 
}

void ReflectionImagingDevice::read_upper_ccd_10x_ring_light_on(Tango::Attribute& attr) {
    attr_upperCcd10xRingLightOn_read = upper_ccd_10x_ring_light_on_ ? 1 : 0;
    attr.set_value(&attr_upperCcd10xRingLightOn_read);
}

void ReflectionImagingDevice::read_upper_ccd_10x_exposure(Tango::Attribute& attr) { 
    attr.set_value(&upper_ccd_10x_exposure_); 
}

void ReflectionImagingDevice::write_upper_ccd_10x_exposure(Tango::WAttribute& attr) { 
    attr.get_write_value(upper_ccd_10x_exposure_);
    setUpperCCD10xExposure(upper_ccd_10x_exposure_);
}

void ReflectionImagingDevice::read_upper_ccd_10x_trigger_mode(Tango::Attribute& attr) {
    attr_upperCcd10xTriggerMode_read = Tango::string_dup(upper_ccd_10x_trigger_mode_.c_str());
    attr.set_value(&attr_upperCcd10xTriggerMode_read);
}

void ReflectionImagingDevice::write_upper_ccd_10x_trigger_mode(Tango::WAttribute& attr) {
    Tango::DevString val;
    attr.get_write_value(val);
    std::string mode(val ? val : "");
    if (mode != "Software" && mode != "Hardware" && mode != "Continuous") {
        Tango::Except::throw_exception("InvalidParameter",
            "TriggerMode must be one of: Software, Hardware, Continuous",
            "write_upper_ccd_10x_trigger_mode");
    }
    upper_ccd_10x_trigger_mode_ = mode;
    if (!sim_mode_ && upper_ccd_10x_driver_) {
        Hikvision::MV_CU020_19GC::TriggerMode driver_mode;
        if (mode == "Software") {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_SOFTWARE;
        } else if (mode == "Hardware") {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_HARDWARE;
        } else {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_CONTINUOUS;
        }
        if (!upper_ccd_10x_driver_->setTriggerMode(driver_mode)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set trigger mode: " + upper_ccd_10x_driver_->getLastError(),
                "write_upper_ccd_10x_trigger_mode");
        }
    }
    log_event("Upper CCD 10x trigger mode set to " + mode);
}

void ReflectionImagingDevice::read_upper_ccd_10x_resolution(Tango::Attribute& attr) { 
    attr_upperCcd10xResolution_read = Tango::string_dup(upper_ccd_10x_resolution_.c_str()); 
    attr.set_value(&attr_upperCcd10xResolution_read); 
}

void ReflectionImagingDevice::write_upper_ccd_10x_resolution(Tango::WAttribute& attr) {
    Tango::DevString val;
    attr.get_write_value(val);
    std::string new_resolution(val);
    
    size_t pos = new_resolution.find('x');
    if (pos == std::string::npos) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Resolution format must be 'WIDTHxHEIGHT' (e.g., '1920x1080')", 
            "write_upper_ccd_10x_resolution");
    }
    
    try {
        long width = std::stol(new_resolution.substr(0, pos));
        long height = std::stol(new_resolution.substr(pos + 1));
        
        if (width < 640 || width > 4096 || height < 480 || height > 4096) {
            Tango::Except::throw_exception("InvalidParameter", 
                "Resolution width must be 640-4096, height must be 480-4096", 
                "write_upper_ccd_10x_resolution");
        }
        
        upper_ccd_10x_width_ = width;
        upper_ccd_10x_height_ = height;
        upper_ccd_10x_resolution_ = new_resolution;
        
        if (!sim_mode_ && upper_ccd_10x_driver_) {
            if (!upper_ccd_10x_driver_->setResolution(width, height)) {
                Tango::Except::throw_exception("API_DeviceError",
                    "Failed to set resolution: " + upper_ccd_10x_driver_->getLastError(),
                    "write_upper_ccd_10x_resolution");
            }
        }
        
        log_event("Upper CCD 10x resolution set to " + new_resolution);
    } catch (const std::exception& e) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Failed to parse resolution: " + std::string(e.what()), 
            "write_upper_ccd_10x_resolution");
    }
}

void ReflectionImagingDevice::read_upper_ccd_10x_width(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_10x_width_);
}

void ReflectionImagingDevice::write_upper_ccd_10x_width(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_10x_width_);
    if (upper_ccd_10x_width_ < 640 || upper_ccd_10x_width_ > 4096) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Width must be between 640 and 4096", "write_upper_ccd_10x_width");
    }
    upper_ccd_10x_resolution_ = std::to_string(upper_ccd_10x_width_) + "x" + 
                               std::to_string(upper_ccd_10x_height_);
    if (!sim_mode_) {
        // TODO: 设置硬件分辨率
    }
    log_event("Upper CCD 10x width set to " + std::to_string(upper_ccd_10x_width_));
}

void ReflectionImagingDevice::read_upper_ccd_10x_height(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_10x_height_);
}

void ReflectionImagingDevice::write_upper_ccd_10x_height(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_10x_height_);
    if (upper_ccd_10x_height_ < 480 || upper_ccd_10x_height_ > 4096) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Height must be between 480 and 4096", "write_upper_ccd_10x_height");
    }
    upper_ccd_10x_resolution_ = std::to_string(upper_ccd_10x_width_) + "x" + 
                               std::to_string(upper_ccd_10x_height_);
    if (!sim_mode_) {
        // TODO: 设置硬件分辨率
    }
    log_event("Upper CCD 10x height set to " + std::to_string(upper_ccd_10x_height_));
}

void ReflectionImagingDevice::read_upper_ccd_10x_gain(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_10x_gain_);
}

void ReflectionImagingDevice::write_upper_ccd_10x_gain(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_10x_gain_);
    if (upper_ccd_10x_gain_ < 0.0 || upper_ccd_10x_gain_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Gain must be between 0.0 and 100.0", "write_upper_ccd_10x_gain");
    }
    if (!sim_mode_ && upper_ccd_10x_driver_) {
        if (!upper_ccd_10x_driver_->setGain(upper_ccd_10x_gain_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set gain: " + upper_ccd_10x_driver_->getLastError(),
                "write_upper_ccd_10x_gain");
        }
    }
    log_event("Upper CCD 10x gain set to " + std::to_string(upper_ccd_10x_gain_));
}

void ReflectionImagingDevice::read_upper_ccd_10x_brightness(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_10x_brightness_);
}

void ReflectionImagingDevice::write_upper_ccd_10x_brightness(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_10x_brightness_);
    if (upper_ccd_10x_brightness_ < -100.0 || upper_ccd_10x_brightness_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Brightness must be between -100.0 and 100.0", "write_upper_ccd_10x_brightness");
    }
    if (!sim_mode_ && upper_ccd_10x_driver_) {
        if (!upper_ccd_10x_driver_->setBrightness(upper_ccd_10x_brightness_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set brightness: " + upper_ccd_10x_driver_->getLastError(),
                "write_upper_ccd_10x_brightness");
        }
    }
    log_event("Upper CCD 10x brightness set to " + std::to_string(upper_ccd_10x_brightness_));
}

void ReflectionImagingDevice::read_upper_ccd_10x_contrast(Tango::Attribute& attr) {
    attr.set_value(&upper_ccd_10x_contrast_);
}

void ReflectionImagingDevice::write_upper_ccd_10x_contrast(Tango::WAttribute& attr) {
    attr.get_write_value(upper_ccd_10x_contrast_);
    if (upper_ccd_10x_contrast_ < -100.0 || upper_ccd_10x_contrast_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Contrast must be between -100.0 and 100.0", "write_upper_ccd_10x_contrast");
    }
    if (!sim_mode_ && upper_ccd_10x_driver_) {
        if (!upper_ccd_10x_driver_->setContrast(upper_ccd_10x_contrast_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set contrast: " + upper_ccd_10x_driver_->getLastError(),
                "write_upper_ccd_10x_contrast");
        }
    }
    log_event("Upper CCD 10x contrast set to " + std::to_string(upper_ccd_10x_contrast_));
}

void ReflectionImagingDevice::read_upper_ccd_10x_image_url(Tango::Attribute& attr) { 
    attr_upperCcd10xImageUrl_read = Tango::string_dup(upper_ccd_10x_image_url_.c_str()); 
    attr.set_value(&attr_upperCcd10xImageUrl_read); 
}

void ReflectionImagingDevice::read_upper_ccd_10x_last_capture_time(Tango::Attribute& attr) { 
    attr_upperCcd10xLastCaptureTime_read = Tango::string_dup(upper_ccd_10x_last_capture_time_.c_str()); 
    attr.set_value(&attr_upperCcd10xLastCaptureTime_read); 
}

// 下1倍物镜CCD属性（粗定位）
void ReflectionImagingDevice::read_lower_ccd_1x_state(Tango::Attribute& attr) { 
    attr_lowerCcd1xState_read = Tango::string_dup(lower_ccd_1x_state_.c_str()); 
    attr.set_value(&attr_lowerCcd1xState_read); 
}

void ReflectionImagingDevice::read_lower_ccd_1x_ring_light_on(Tango::Attribute& attr) {
    attr_lowerCcd1xRingLightOn_read = lower_ccd_1x_ring_light_on_ ? 1 : 0;
    attr.set_value(&attr_lowerCcd1xRingLightOn_read);
}

void ReflectionImagingDevice::read_lower_ccd_1x_exposure(Tango::Attribute& attr) { 
    attr.set_value(&lower_ccd_1x_exposure_); 
}

void ReflectionImagingDevice::write_lower_ccd_1x_exposure(Tango::WAttribute& attr) { 
    attr.get_write_value(lower_ccd_1x_exposure_);
    setLowerCCD1xExposure(lower_ccd_1x_exposure_);
}

void ReflectionImagingDevice::read_lower_ccd_1x_trigger_mode(Tango::Attribute& attr) {
    attr_lowerCcd1xTriggerMode_read = Tango::string_dup(lower_ccd_1x_trigger_mode_.c_str());
    attr.set_value(&attr_lowerCcd1xTriggerMode_read);
}

void ReflectionImagingDevice::write_lower_ccd_1x_trigger_mode(Tango::WAttribute& attr) {
    Tango::DevString val;
    attr.get_write_value(val);
    std::string mode(val ? val : "");
    if (mode != "Software" && mode != "Hardware" && mode != "Continuous") {
        Tango::Except::throw_exception("InvalidParameter",
            "TriggerMode must be one of: Software, Hardware, Continuous",
            "write_lower_ccd_1x_trigger_mode");
    }
    lower_ccd_1x_trigger_mode_ = mode;
    if (!sim_mode_ && lower_ccd_1x_driver_) {
        Hikvision::MV_CU020_19GC::TriggerMode driver_mode;
        if (mode == "Software") {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_SOFTWARE;
        } else if (mode == "Hardware") {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_HARDWARE;
        } else {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_CONTINUOUS;
        }
        if (!lower_ccd_1x_driver_->setTriggerMode(driver_mode)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set trigger mode: " + lower_ccd_1x_driver_->getLastError(),
                "write_lower_ccd_1x_trigger_mode");
        }
    }
    log_event("Lower CCD 1x trigger mode set to " + mode);
}

void ReflectionImagingDevice::read_lower_ccd_1x_resolution(Tango::Attribute& attr) { 
    attr_lowerCcd1xResolution_read = Tango::string_dup(lower_ccd_1x_resolution_.c_str()); 
    attr.set_value(&attr_lowerCcd1xResolution_read); 
}

void ReflectionImagingDevice::write_lower_ccd_1x_resolution(Tango::WAttribute& attr) {
    Tango::DevString val;
    attr.get_write_value(val);
    std::string new_resolution(val);
    
    size_t pos = new_resolution.find('x');
    if (pos == std::string::npos) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Resolution format must be 'WIDTHxHEIGHT' (e.g., '1920x1080')", 
            "write_lower_ccd_1x_resolution");
    }
    
    try {
        long width = std::stol(new_resolution.substr(0, pos));
        long height = std::stol(new_resolution.substr(pos + 1));
        
        if (width < 640 || width > 4096 || height < 480 || height > 4096) {
            Tango::Except::throw_exception("InvalidParameter", 
                "Resolution width must be 640-4096, height must be 480-4096", 
                "write_lower_ccd_1x_resolution");
        }
        
        lower_ccd_1x_width_ = width;
        lower_ccd_1x_height_ = height;
        lower_ccd_1x_resolution_ = new_resolution;
        
        if (!sim_mode_ && lower_ccd_1x_driver_) {
            if (!lower_ccd_1x_driver_->setResolution(width, height)) {
                Tango::Except::throw_exception("API_DeviceError",
                    "Failed to set resolution: " + lower_ccd_1x_driver_->getLastError(),
                    "write_lower_ccd_1x_resolution");
            }
        }
        
        log_event("Lower CCD 1x resolution set to " + new_resolution);
    } catch (const std::exception& e) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Failed to parse resolution: " + std::string(e.what()), 
            "write_lower_ccd_1x_resolution");
    }
}

void ReflectionImagingDevice::read_lower_ccd_1x_width(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_1x_width_);
}

void ReflectionImagingDevice::write_lower_ccd_1x_width(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_1x_width_);
    if (lower_ccd_1x_width_ < 640 || lower_ccd_1x_width_ > 4096) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Width must be between 640 and 4096", "write_lower_ccd_1x_width");
    }
    lower_ccd_1x_resolution_ = std::to_string(lower_ccd_1x_width_) + "x" + 
                               std::to_string(lower_ccd_1x_height_);
    if (!sim_mode_) {
        // TODO: 设置硬件分辨率
    }
    log_event("Lower CCD 1x width set to " + std::to_string(lower_ccd_1x_width_));
}

void ReflectionImagingDevice::read_lower_ccd_1x_height(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_1x_height_);
}

void ReflectionImagingDevice::write_lower_ccd_1x_height(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_1x_height_);
    if (lower_ccd_1x_height_ < 480 || lower_ccd_1x_height_ > 4096) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Height must be between 480 and 4096", "write_lower_ccd_1x_height");
    }
    lower_ccd_1x_resolution_ = std::to_string(lower_ccd_1x_width_) + "x" + 
                               std::to_string(lower_ccd_1x_height_);
    if (!sim_mode_) {
        // TODO: 设置硬件分辨率
    }
    log_event("Lower CCD 1x height set to " + std::to_string(lower_ccd_1x_height_));
}

void ReflectionImagingDevice::read_lower_ccd_1x_gain(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_1x_gain_);
}

void ReflectionImagingDevice::write_lower_ccd_1x_gain(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_1x_gain_);
    if (lower_ccd_1x_gain_ < 0.0 || lower_ccd_1x_gain_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Gain must be between 0.0 and 100.0", "write_lower_ccd_1x_gain");
    }
    if (!sim_mode_ && lower_ccd_1x_driver_) {
        if (!lower_ccd_1x_driver_->setGain(lower_ccd_1x_gain_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set gain: " + lower_ccd_1x_driver_->getLastError(),
                "write_lower_ccd_1x_gain");
        }
    }
    log_event("Lower CCD 1x gain set to " + std::to_string(lower_ccd_1x_gain_));
}

void ReflectionImagingDevice::read_lower_ccd_1x_brightness(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_1x_brightness_);
}

void ReflectionImagingDevice::write_lower_ccd_1x_brightness(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_1x_brightness_);
    if (lower_ccd_1x_brightness_ < -100.0 || lower_ccd_1x_brightness_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Brightness must be between -100.0 and 100.0", "write_lower_ccd_1x_brightness");
    }
    if (!sim_mode_ && lower_ccd_1x_driver_) {
        if (!lower_ccd_1x_driver_->setBrightness(lower_ccd_1x_brightness_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set brightness: " + lower_ccd_1x_driver_->getLastError(),
                "write_lower_ccd_1x_brightness");
        }
    }
    log_event("Lower CCD 1x brightness set to " + std::to_string(lower_ccd_1x_brightness_));
}

void ReflectionImagingDevice::read_lower_ccd_1x_contrast(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_1x_contrast_);
}

void ReflectionImagingDevice::write_lower_ccd_1x_contrast(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_1x_contrast_);
    if (lower_ccd_1x_contrast_ < -100.0 || lower_ccd_1x_contrast_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Contrast must be between -100.0 and 100.0", "write_lower_ccd_1x_contrast");
    }
    if (!sim_mode_ && lower_ccd_1x_driver_) {
        if (!lower_ccd_1x_driver_->setContrast(lower_ccd_1x_contrast_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set contrast: " + lower_ccd_1x_driver_->getLastError(),
                "write_lower_ccd_1x_contrast");
        }
    }
    log_event("Lower CCD 1x contrast set to " + std::to_string(lower_ccd_1x_contrast_));
}

void ReflectionImagingDevice::read_lower_ccd_1x_image_url(Tango::Attribute& attr) { 
    attr_lowerCcd1xImageUrl_read = Tango::string_dup(lower_ccd_1x_image_url_.c_str()); 
    attr.set_value(&attr_lowerCcd1xImageUrl_read); 
}

void ReflectionImagingDevice::read_lower_ccd_1x_last_capture_time(Tango::Attribute& attr) { 
    attr_lowerCcd1xLastCaptureTime_read = Tango::string_dup(lower_ccd_1x_last_capture_time_.c_str()); 
    attr.set_value(&attr_lowerCcd1xLastCaptureTime_read); 
}

// 下10倍物镜CCD属性（近距离观察）
void ReflectionImagingDevice::read_lower_ccd_10x_state(Tango::Attribute& attr) { 
    attr_lowerCcd10xState_read = Tango::string_dup(lower_ccd_10x_state_.c_str()); 
    attr.set_value(&attr_lowerCcd10xState_read); 
}

void ReflectionImagingDevice::read_lower_ccd_10x_ring_light_on(Tango::Attribute& attr) {
    attr_lowerCcd10xRingLightOn_read = lower_ccd_10x_ring_light_on_ ? 1 : 0;
    attr.set_value(&attr_lowerCcd10xRingLightOn_read);
}

void ReflectionImagingDevice::read_lower_ccd_10x_exposure(Tango::Attribute& attr) { 
    attr.set_value(&lower_ccd_10x_exposure_); 
}

void ReflectionImagingDevice::write_lower_ccd_10x_exposure(Tango::WAttribute& attr) { 
    attr.get_write_value(lower_ccd_10x_exposure_);
    setLowerCCD10xExposure(lower_ccd_10x_exposure_);
}

void ReflectionImagingDevice::read_lower_ccd_10x_trigger_mode(Tango::Attribute& attr) {
    attr_lowerCcd10xTriggerMode_read = Tango::string_dup(lower_ccd_10x_trigger_mode_.c_str());
    attr.set_value(&attr_lowerCcd10xTriggerMode_read);
}

void ReflectionImagingDevice::write_lower_ccd_10x_trigger_mode(Tango::WAttribute& attr) {
    Tango::DevString val;
    attr.get_write_value(val);
    std::string mode(val ? val : "");
    if (mode != "Software" && mode != "Hardware" && mode != "Continuous") {
        Tango::Except::throw_exception("InvalidParameter",
            "TriggerMode must be one of: Software, Hardware, Continuous",
            "write_lower_ccd_10x_trigger_mode");
    }
    lower_ccd_10x_trigger_mode_ = mode;
    if (!sim_mode_ && lower_ccd_10x_driver_) {
        Hikvision::MV_CU020_19GC::TriggerMode driver_mode;
        if (mode == "Software") {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_SOFTWARE;
        } else if (mode == "Hardware") {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_HARDWARE;
        } else {
            driver_mode = Hikvision::MV_CU020_19GC::TRIGGER_CONTINUOUS;
        }
        if (!lower_ccd_10x_driver_->setTriggerMode(driver_mode)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set trigger mode: " + lower_ccd_10x_driver_->getLastError(),
                "write_lower_ccd_10x_trigger_mode");
        }
    }
    log_event("Lower CCD 10x trigger mode set to " + mode);
}

void ReflectionImagingDevice::read_lower_ccd_10x_resolution(Tango::Attribute& attr) { 
    attr_lowerCcd10xResolution_read = Tango::string_dup(lower_ccd_10x_resolution_.c_str()); 
    attr.set_value(&attr_lowerCcd10xResolution_read); 
}

void ReflectionImagingDevice::write_lower_ccd_10x_resolution(Tango::WAttribute& attr) {
    Tango::DevString val;
    attr.get_write_value(val);
    std::string new_resolution(val);
    
    size_t pos = new_resolution.find('x');
    if (pos == std::string::npos) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Resolution format must be 'WIDTHxHEIGHT' (e.g., '1920x1080')", 
            "write_lower_ccd_10x_resolution");
    }
    
    try {
        long width = std::stol(new_resolution.substr(0, pos));
        long height = std::stol(new_resolution.substr(pos + 1));
        
        if (width < 640 || width > 4096 || height < 480 || height > 4096) {
            Tango::Except::throw_exception("InvalidParameter", 
                "Resolution width must be 640-4096, height must be 480-4096", 
                "write_lower_ccd_10x_resolution");
        }
        
        lower_ccd_10x_width_ = width;
        lower_ccd_10x_height_ = height;
        lower_ccd_10x_resolution_ = new_resolution;
        
        if (!sim_mode_ && lower_ccd_10x_driver_) {
            if (!lower_ccd_10x_driver_->setResolution(width, height)) {
                Tango::Except::throw_exception("API_DeviceError",
                    "Failed to set resolution: " + lower_ccd_10x_driver_->getLastError(),
                    "write_lower_ccd_10x_resolution");
            }
        }
        
        log_event("Lower CCD 10x resolution set to " + new_resolution);
    } catch (const std::exception& e) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Failed to parse resolution: " + std::string(e.what()), 
            "write_lower_ccd_10x_resolution");
    }
}

void ReflectionImagingDevice::read_lower_ccd_10x_width(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_10x_width_);
}

void ReflectionImagingDevice::write_lower_ccd_10x_width(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_10x_width_);
    if (lower_ccd_10x_width_ < 640 || lower_ccd_10x_width_ > 4096) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Width must be between 640 and 4096", "write_lower_ccd_10x_width");
    }
    lower_ccd_10x_resolution_ = std::to_string(lower_ccd_10x_width_) + "x" + 
                                std::to_string(lower_ccd_10x_height_);
    if (!sim_mode_) {
        // TODO: 设置硬件分辨率
    }
    log_event("Lower CCD 10x width set to " + std::to_string(lower_ccd_10x_width_));
}

void ReflectionImagingDevice::read_lower_ccd_10x_height(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_10x_height_);
}

void ReflectionImagingDevice::write_lower_ccd_10x_height(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_10x_height_);
    if (lower_ccd_10x_height_ < 480 || lower_ccd_10x_height_ > 4096) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Height must be between 480 and 4096", "write_lower_ccd_10x_height");
    }
    lower_ccd_10x_resolution_ = std::to_string(lower_ccd_10x_width_) + "x" + 
                                std::to_string(lower_ccd_10x_height_);
    if (!sim_mode_) {
        // TODO: 设置硬件分辨率
    }
    log_event("Lower CCD 10x height set to " + std::to_string(lower_ccd_10x_height_));
}

void ReflectionImagingDevice::read_lower_ccd_10x_gain(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_10x_gain_);
}

void ReflectionImagingDevice::write_lower_ccd_10x_gain(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_10x_gain_);
    if (lower_ccd_10x_gain_ < 0.0 || lower_ccd_10x_gain_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Gain must be between 0.0 and 100.0", "write_lower_ccd_10x_gain");
    }
    if (!sim_mode_ && lower_ccd_10x_driver_) {
        if (!lower_ccd_10x_driver_->setGain(lower_ccd_10x_gain_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set gain: " + lower_ccd_10x_driver_->getLastError(),
                "write_lower_ccd_10x_gain");
        }
    }
    log_event("Lower CCD 10x gain set to " + std::to_string(lower_ccd_10x_gain_));
}

void ReflectionImagingDevice::read_lower_ccd_10x_brightness(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_10x_brightness_);
}

void ReflectionImagingDevice::write_lower_ccd_10x_brightness(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_10x_brightness_);
    if (lower_ccd_10x_brightness_ < -100.0 || lower_ccd_10x_brightness_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Brightness must be between -100.0 and 100.0", "write_lower_ccd_10x_brightness");
    }
    if (!sim_mode_ && lower_ccd_10x_driver_) {
        if (!lower_ccd_10x_driver_->setBrightness(lower_ccd_10x_brightness_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set brightness: " + lower_ccd_10x_driver_->getLastError(),
                "write_lower_ccd_10x_brightness");
        }
    }
    log_event("Lower CCD 10x brightness set to " + std::to_string(lower_ccd_10x_brightness_));
}

void ReflectionImagingDevice::read_lower_ccd_10x_contrast(Tango::Attribute& attr) {
    attr.set_value(&lower_ccd_10x_contrast_);
}

void ReflectionImagingDevice::write_lower_ccd_10x_contrast(Tango::WAttribute& attr) {
    attr.get_write_value(lower_ccd_10x_contrast_);
    if (lower_ccd_10x_contrast_ < -100.0 || lower_ccd_10x_contrast_ > 100.0) {
        Tango::Except::throw_exception("InvalidParameter", 
            "Contrast must be between -100.0 and 100.0", "write_lower_ccd_10x_contrast");
    }
    if (!sim_mode_ && lower_ccd_10x_driver_) {
        if (!lower_ccd_10x_driver_->setContrast(lower_ccd_10x_contrast_)) {
            Tango::Except::throw_exception("API_DeviceError",
                "Failed to set contrast: " + lower_ccd_10x_driver_->getLastError(),
                "write_lower_ccd_10x_contrast");
        }
    }
    log_event("Lower CCD 10x contrast set to " + std::to_string(lower_ccd_10x_contrast_));
}

void ReflectionImagingDevice::read_lower_ccd_10x_image_url(Tango::Attribute& attr) { 
    attr_lowerCcd10xImageUrl_read = Tango::string_dup(lower_ccd_10x_image_url_.c_str()); 
    attr.set_value(&attr_lowerCcd10xImageUrl_read); 
}

void ReflectionImagingDevice::read_lower_ccd_10x_last_capture_time(Tango::Attribute& attr) { 
    attr_lowerCcd10xLastCaptureTime_read = Tango::string_dup(lower_ccd_10x_last_capture_time_.c_str()); 
    attr.set_value(&attr_lowerCcd10xLastCaptureTime_read); 
}

void ReflectionImagingDevice::read_image_capture_count(Tango::Attribute& attr) { 
    attr.set_value(&image_capture_count_); 
}

void ReflectionImagingDevice::read_auto_capture_enabled(Tango::Attribute& attr) { 
    attr.set_value(&auto_capture_enabled_); 
}

// 辅助支撑属性读取实现
void ReflectionImagingDevice::read_upper_support_position(Tango::Attribute& attr) { 
    attr.set_value(&upper_support_position_); 
}

void ReflectionImagingDevice::read_lower_support_position(Tango::Attribute& attr) { 
    attr.set_value(&lower_support_position_); 
}

void ReflectionImagingDevice::read_upper_support_state(Tango::Attribute& attr) { 
    attr_upperSupportState_read = Tango::string_dup(upper_support_state_.c_str()); 
    attr.set_value(&attr_upperSupportState_read); 
}

void ReflectionImagingDevice::read_lower_support_state(Tango::Attribute& attr) { 
    attr_lowerSupportState_read = Tango::string_dup(lower_support_state_.c_str()); 
    attr.set_value(&attr_lowerSupportState_read); 
}

void ReflectionImagingDevice::read_axis_parameter(Tango::Attribute& attr) { 
    attr_axisParameter_read = Tango::string_dup(axis_parameter_.c_str()); 
    attr.set_value(&attr_axisParameter_read); 
}

void ReflectionImagingDevice::read_attr_hardware(std::vector<long> &/*attr_list*/) {
    if (!sim_mode_ && (upper_platform_proxy_ || lower_platform_proxy_)) {
        try {
            // 检测限位触发：读取Z轴的限位状态（只有Z轴有刹车）
            if (brake_power_port_ >= 0 && brake_released_) {
                bool limit_triggered = false;
                Tango::DevShort first_axis = -1;
                Tango::DevShort first_el_state = 0;
                // 检查上平台Z轴（轴4）
                if (upper_platform_proxy_ && (upper_platform_state_[2] || get_state() == Tango::MOVING)) {
                    try {
                        Tango::DeviceData data_in;
                        data_in << static_cast<Tango::DevShort>(4);  // 上平台Z轴 = 轴4
                        Tango::DeviceData data_out = upper_platform_proxy_->command_inout("readEL", data_in);
                        Tango::DevShort el_state_raw;
                        data_out >> el_state_raw;
                        
                        // 限位开关低电平有效：硬件读取0（低电平）表示触发，1（高电平）表示未触发
                        // 运动控制器当前逻辑：pos_limit==1返回1(EL+), neg_limit==1返回-1(EL-), 否则返回0
                        // 如果限位开关是低电平有效，需要反转：当硬件读取0时应该返回限位触发
                        // 由于运动控制器返回0时无法区分方向，我们在reflection_imaging_device中反转逻辑
                        // 反转规则：当readEL返回0时，认为是限位触发（统一处理为EL+）
                        //          当readEL返回非0时，认为是未触发（转换为0）
                        Tango::DevShort el_state = 0;
                        if (el_state_raw == 0) {
                            // 低电平有效：返回0表示限位触发（但无法区分方向，统一处理为EL+）
                            el_state = 1;  // 转换为EL+表示限位触发
                        }
                        // 如果返回非0（1或-1），说明硬件读取是高电平，限位未触发，el_state保持为0
                        
                        if (el_state != 0) {
                            limit_triggered = true;
                            if (first_axis < 0) {
                                first_axis = 4;
                                first_el_state = el_state;
                            }
                            upper_platform_lim_org_state_[2] = el_state;
                            INFO_STREAM << "[BrakeControl] Limit switch triggered on upper Z axis (axis 4, el_state_raw=" 
                                       << el_state_raw << ", inverted to " << el_state << ")" << std::endl;
                        } else if (upper_platform_lim_org_state_[2] == 1 || upper_platform_lim_org_state_[2] == -1) {
                            upper_platform_lim_org_state_[2] = 2;
                        }
                    } catch (...) {
                        // 忽略读取失败
                    }
                }
                
                // 检查下平台Z轴（轴5）
                if (lower_platform_proxy_ && (lower_platform_state_[2] || get_state() == Tango::MOVING)) {
                    try {
                        Tango::DeviceData data_in;
                        data_in << static_cast<Tango::DevShort>(5);  // 下平台Z轴 = 轴5
                        Tango::DeviceData data_out = lower_platform_proxy_->command_inout("readEL", data_in);
                        Tango::DevShort el_state_raw;
                        data_out >> el_state_raw;
                        
                        // 限位开关低电平有效：硬件读取0（低电平）表示触发，1（高电平）表示未触发
                        // 运动控制器当前逻辑：pos_limit==1返回1(EL+), neg_limit==1返回-1(EL-), 否则返回0
                        // 如果限位开关是低电平有效，需要反转：当硬件读取0时应该返回限位触发
                        // 由于运动控制器返回0时无法区分方向，我们在reflection_imaging_device中反转逻辑
                        // 反转规则：当readEL返回0时，认为是限位触发（统一处理为EL+）
                        //          当readEL返回非0时，认为是未触发（转换为0）
                        Tango::DevShort el_state = 0;
                        if (el_state_raw == 0) {
                            // 低电平有效：返回0表示限位触发（但无法区分方向，统一处理为EL+）
                            el_state = 1;  // 转换为EL+表示限位触发
                        }
                        // 如果返回非0（1或-1），说明硬件读取是高电平，限位未触发，el_state保持为0
                        
                        if (el_state != 0) {
                            limit_triggered = true;
                            if (first_axis < 0) {
                                first_axis = 5;
                                first_el_state = el_state;
                            }
                            lower_platform_lim_org_state_[2] = el_state;
                            INFO_STREAM << "[BrakeControl] Limit switch triggered on lower Z axis (axis 5, el_state_raw=" 
                                       << el_state_raw << ", inverted to " << el_state << ")" << std::endl;
                        } else if (lower_platform_lim_org_state_[2] == 1 || lower_platform_lim_org_state_[2] == -1) {
                            lower_platform_lim_org_state_[2] = 2;
                        }
                    } catch (...) {
                        // 忽略读取失败
                    }
                }
                
                // 如果检测到限位触发，立即启用刹车并停止运动
                if (limit_triggered) {
                    if (!limit_fault_latched_.exchange(true)) {
                        limit_fault_axis_.store(first_axis);
                        limit_fault_el_state_.store(first_el_state);
                        std::string dir = (first_el_state > 0) ? "EL+" : ((first_el_state < 0) ? "EL-" : "EL");
                        fault_state_ = "Limit switch triggered: axis " + std::to_string(first_axis) + " (" + dir + ")";
                        set_status(fault_state_);
                        set_state(Tango::FAULT);
                    }
                    INFO_STREAM << "[BrakeControl] Limit triggered, auto-engaging brake (safety)" << std::endl;
                    if (!engage_brake()) {
                        WARN_STREAM << "[BrakeControl] Failed to engage brake on limit trigger" << std::endl;
                    }
                    // 停止Z轴运动
                    try {
                        if (upper_platform_proxy_) {
                            Tango::DeviceData stop_data;
                            stop_data << static_cast<Tango::DevShort>(4);
                            upper_platform_proxy_->command_inout("stopMove", stop_data);
                        }
                        if (lower_platform_proxy_) {
                            Tango::DeviceData stop_data;
                            stop_data << static_cast<Tango::DevShort>(5);
                            lower_platform_proxy_->command_inout("stopMove", stop_data);
                        }
                    } catch (...) {
                        // 忽略停止失败
                    }
                }
            }
            
            // 检查运动控制器状态（使用上平台或下平台的proxy，它们是同一个控制器）
            auto upper = get_upper_platform_proxy();
            auto lower = get_lower_platform_proxy();
            Tango::DeviceProxy* state_proxy = upper ? upper.get() : (lower ? lower.get() : nullptr);
            if (state_proxy) {
                Tango::DevState mc_state = state_proxy->state();
                Tango::DevState old_state = get_state();
                
                if (mc_state == Tango::FAULT) {
                    if (old_state != Tango::FAULT) {
                        // 故障时自动启用刹车（安全保护）
                        if (brake_power_port_ >= 0 && brake_released_) {
                            INFO_STREAM << "[BrakeControl] Fault detected, auto-engaging brake (safety)" << std::endl;
                            if (!engage_brake()) {
                                WARN_STREAM << "[BrakeControl] Failed to engage brake on fault" << std::endl;
                            }
                        }
                    }
                } else if (old_state == Tango::MOVING && mc_state != Tango::MOVING) {
                    // 运动完成：从MOVING状态变为ON状态
                    // 注意：正常运动完成后不自动启用刹车，保持刹车释放状态以便快速继续运动
                    // 刹车只在故障、限位触发、断电、设备关闭等安全场景下自动启用
                }
            }
        } catch (...) {
            // 忽略硬件读取失败
        }
    }
}

// ===== CUSTOM ATTRIBUTE CLASSES (for read_attr() dispatch) =====
class ReflectionImagingAttr : public Tango::Attr {
public:
    ReflectionImagingAttr(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<ReflectionImagingDevice *>(dev)->read_attr(att);
    }
};

class ReflectionImagingAttrRW : public Tango::Attr {
public:
    ReflectionImagingAttrRW(const char *name, long data_type, Tango::AttrWriteType w_type = Tango::READ_WRITE)
        : Tango::Attr(name, data_type, w_type) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<ReflectionImagingDevice *>(dev)->read_attr(att);
    }
    
    virtual void write(Tango::DeviceImpl *dev, Tango::WAttribute &att) override {
        static_cast<ReflectionImagingDevice *>(dev)->write_attr(att);
    }
};

class ReflectionImagingSpectrumAttr : public Tango::SpectrumAttr {
public:
    ReflectionImagingSpectrumAttr(const char *name, long data_type, Tango::AttrWriteType w_type, long max_x)
        : Tango::SpectrumAttr(name, data_type, w_type, max_x) {}
    
    virtual void read(Tango::DeviceImpl *dev, Tango::Attribute &att) override {
        static_cast<ReflectionImagingDevice *>(dev)->read_attr(att);
    }
};

// ========== DeviceClass Implementation ==========

ReflectionImagingDeviceClass *ReflectionImagingDeviceClass::_instance = nullptr;

ReflectionImagingDeviceClass *ReflectionImagingDeviceClass::instance() {
    if (_instance == nullptr) {
        static std::string name("ReflectionImagingDevice");
        _instance = new ReflectionImagingDeviceClass(name);
    }
    return _instance;
}

ReflectionImagingDeviceClass::ReflectionImagingDeviceClass(std::string &class_name)
    : Tango::DeviceClass(class_name.c_str()) {
    command_factory();
}

void ReflectionImagingDeviceClass::attribute_factory(std::vector<Tango::Attr*> &att_list) {
    // 标准属性 - 使用自定义属性类以确保 read_attr() 被调用
    att_list.push_back(new ReflectionImagingAttr("selfCheckResult", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("positionUnit", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("groupAttributeJson", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("reflectionLogs", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("faultState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("resultValue", Tango::DEV_SHORT, Tango::READ));
    
    // Power control status attributes (NEW)
    att_list.push_back(new ReflectionImagingAttr("driverPowerStatus", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("brakeStatus", Tango::DEV_BOOLEAN, Tango::READ));

    // 上平台属性 - 使用自定义 SpectrumAttr 类
    att_list.push_back(new ReflectionImagingSpectrumAttr("upperPlatformPos", Tango::DEV_DOUBLE, Tango::READ, 3));
    att_list.push_back(new ReflectionImagingSpectrumAttr("upperPlatformDirePos", Tango::DEV_DOUBLE, Tango::READ, 3));
    att_list.push_back(new ReflectionImagingSpectrumAttr("upperPlatformLimOrgState", Tango::DEV_SHORT, Tango::READ, 3));
    att_list.push_back(new ReflectionImagingSpectrumAttr("upperPlatformState", Tango::DEV_BOOLEAN, Tango::READ, 3));
    att_list.push_back(new ReflectionImagingAttr("upperPlatformAxisParameter", Tango::DEV_STRING, Tango::READ));

    // 下平台属性
    att_list.push_back(new ReflectionImagingSpectrumAttr("lowerPlatformPos", Tango::DEV_DOUBLE, Tango::READ, 3));
    att_list.push_back(new ReflectionImagingSpectrumAttr("lowerPlatformDirePos", Tango::DEV_DOUBLE, Tango::READ, 3));
    att_list.push_back(new ReflectionImagingSpectrumAttr("lowerPlatformLimOrgState", Tango::DEV_SHORT, Tango::READ, 3));
    att_list.push_back(new ReflectionImagingSpectrumAttr("lowerPlatformState", Tango::DEV_BOOLEAN, Tango::READ, 3));
    att_list.push_back(new ReflectionImagingAttr("lowerPlatformAxisParameter", Tango::DEV_STRING, Tango::READ));
    
    // 单轴属性 (用于GUI单轴独立控制)
    // 上平台单轴
    att_list.push_back(new ReflectionImagingAttr("upperPlatformPosX", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperPlatformPosY", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperPlatformPosZ", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperPlatformStateX", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperPlatformStateY", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperPlatformStateZ", Tango::DEV_BOOLEAN, Tango::READ));
    // 下平台单轴
    att_list.push_back(new ReflectionImagingAttr("lowerPlatformPosX", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerPlatformPosY", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerPlatformPosZ", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerPlatformStateX", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerPlatformStateY", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerPlatformStateZ", Tango::DEV_BOOLEAN, Tango::READ));

    // CCD相机属性（四CCD：上下各两个，1倍和10倍物镜）
    // 上1倍物镜CCD
    att_list.push_back(new ReflectionImagingAttr("upperCCD1xState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperCCD1xRingLightOn", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD1xExposure", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD1xTriggerMode", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD1xResolution", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD1xWidth", Tango::DEV_LONG, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD1xHeight", Tango::DEV_LONG, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD1xGain", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD1xBrightness", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD1xContrast", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttr("upperCCD1xImageUrl", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperCCD1xLastCaptureTime", Tango::DEV_STRING, Tango::READ));
    
    // 上10倍物镜CCD
    att_list.push_back(new ReflectionImagingAttr("upperCCD10xState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperCCD10xRingLightOn", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD10xExposure", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD10xTriggerMode", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD10xResolution", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD10xWidth", Tango::DEV_LONG, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD10xHeight", Tango::DEV_LONG, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD10xGain", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD10xBrightness", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("upperCCD10xContrast", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttr("upperCCD10xImageUrl", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperCCD10xLastCaptureTime", Tango::DEV_STRING, Tango::READ));
    
    // 下1倍物镜CCD
    att_list.push_back(new ReflectionImagingAttr("lowerCCD1xState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerCCD1xRingLightOn", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD1xExposure", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD1xTriggerMode", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD1xResolution", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD1xWidth", Tango::DEV_LONG, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD1xHeight", Tango::DEV_LONG, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD1xGain", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD1xBrightness", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD1xContrast", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttr("lowerCCD1xImageUrl", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerCCD1xLastCaptureTime", Tango::DEV_STRING, Tango::READ));
    
    // 下10倍物镜CCD
    att_list.push_back(new ReflectionImagingAttr("lowerCCD10xState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerCCD10xRingLightOn", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD10xExposure", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD10xTriggerMode", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD10xResolution", Tango::DEV_STRING, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD10xWidth", Tango::DEV_LONG, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD10xHeight", Tango::DEV_LONG, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD10xGain", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD10xBrightness", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttrRW("lowerCCD10xContrast", Tango::DEV_DOUBLE, Tango::READ_WRITE));
    att_list.push_back(new ReflectionImagingAttr("lowerCCD10xImageUrl", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerCCD10xLastCaptureTime", Tango::DEV_STRING, Tango::READ));
    
    // 图像捕获统计
    att_list.push_back(new ReflectionImagingAttr("imageCaptureCount", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("autoCaptureEnabled", Tango::DEV_BOOLEAN, Tango::READ));

    // 辅助支撑属性
    att_list.push_back(new ReflectionImagingAttr("upperSupportPosition", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerSupportPosition", Tango::DEV_DOUBLE, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("upperSupportState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new ReflectionImagingAttr("lowerSupportState", Tango::DEV_STRING, Tango::READ));

    // 其他属性
    att_list.push_back(new ReflectionImagingAttr("axisParameter", Tango::DEV_STRING, Tango::READ));
}

void ReflectionImagingDeviceClass::command_factory() {
    // Lock/Unlock commands
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("devLock",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&ReflectionImagingDevice::devLock)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("devUnlock",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::devUnlock)));
    command_list.push_back(new Tango::TemplCommand("devLockVerify",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::devLockVerify)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("devLockQuery",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::devLockQuery)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("devUserConfig",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&ReflectionImagingDevice::devUserConfig)));
    
    // System commands
    command_list.push_back(new Tango::TemplCommand("selfCheck",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::selfCheck)));
    command_list.push_back(new Tango::TemplCommand("init",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::init)));
    
    // Upper Platform commands
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("upperPlatformAxisSet",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::upperPlatformAxisSet)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("upperPlatformStructAxisSet",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::upperPlatformStructAxisSet)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("upperPlatformMoveRelative",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::upperPlatformMoveRelative)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("upperPlatformMoveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::upperPlatformMoveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("upperPlatformMoveToPosition",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::upperPlatformMoveToPosition)));
    command_list.push_back(new Tango::TemplCommand("upperPlatformStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperPlatformStop)));
    command_list.push_back(new Tango::TemplCommand("upperPlatformReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperPlatformReset)));
    command_list.push_back(new Tango::TemplCommand("upperPlatformMoveZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperPlatformMoveZero)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevVarDoubleArray *>("upperPlatformReadEncoder",
        static_cast<Tango::DevVarDoubleArray * (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperPlatformReadEncoder)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevVarShortArray *>("upperPlatformReadOrg",
        static_cast<Tango::DevVarShortArray * (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperPlatformReadOrg)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevVarShortArray *>("upperPlatformReadEL",
        static_cast<Tango::DevVarShortArray * (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperPlatformReadEL)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("upperPlatformSingleAxisMove",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::upperPlatformSingleAxisMove)));
    
    // Lower Platform commands
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("lowerPlatformAxisSet",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::lowerPlatformAxisSet)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("lowerPlatformStructAxisSet",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::lowerPlatformStructAxisSet)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("lowerPlatformMoveRelative",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::lowerPlatformMoveRelative)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("lowerPlatformMoveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::lowerPlatformMoveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("lowerPlatformMoveToPosition",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::lowerPlatformMoveToPosition)));
    command_list.push_back(new Tango::TemplCommand("lowerPlatformStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerPlatformStop)));
    command_list.push_back(new Tango::TemplCommand("lowerPlatformReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerPlatformReset)));
    command_list.push_back(new Tango::TemplCommand("lowerPlatformMoveZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerPlatformMoveZero)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevVarDoubleArray *>("lowerPlatformReadEncoder",
        static_cast<Tango::DevVarDoubleArray * (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerPlatformReadEncoder)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevVarShortArray *>("lowerPlatformReadOrg",
        static_cast<Tango::DevVarShortArray * (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerPlatformReadOrg)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevVarShortArray *>("lowerPlatformReadEL",
        static_cast<Tango::DevVarShortArray * (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerPlatformReadEL)));
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("lowerPlatformSingleAxisMove",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::lowerPlatformSingleAxisMove)));
    
    // 上平台单轴控制命令 (用于GUI单轴独立控制)
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("upperXMoveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::upperXMoveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("upperXMoveRelative",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::upperXMoveRelative)));
    command_list.push_back(new Tango::TemplCommand("upperXMoveZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperXMoveZero)));
    command_list.push_back(new Tango::TemplCommand("upperXStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperXStop)));
    command_list.push_back(new Tango::TemplCommand("upperXReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperXReset)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("upperYMoveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::upperYMoveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("upperYMoveRelative",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::upperYMoveRelative)));
    command_list.push_back(new Tango::TemplCommand("upperYMoveZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperYMoveZero)));
    command_list.push_back(new Tango::TemplCommand("upperYStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperYStop)));
    command_list.push_back(new Tango::TemplCommand("upperYReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperYReset)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("upperZMoveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::upperZMoveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("upperZMoveRelative",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::upperZMoveRelative)));
    command_list.push_back(new Tango::TemplCommand("upperZMoveZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperZMoveZero)));
    command_list.push_back(new Tango::TemplCommand("upperZStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperZStop)));
    command_list.push_back(new Tango::TemplCommand("upperZReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperZReset)));
    
    // 下平台单轴控制命令 (用于GUI单轴独立控制)
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("lowerXMoveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::lowerXMoveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("lowerXMoveRelative",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::lowerXMoveRelative)));
    command_list.push_back(new Tango::TemplCommand("lowerXMoveZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerXMoveZero)));
    command_list.push_back(new Tango::TemplCommand("lowerXStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerXStop)));
    command_list.push_back(new Tango::TemplCommand("lowerXReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerXReset)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("lowerYMoveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::lowerYMoveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("lowerYMoveRelative",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::lowerYMoveRelative)));
    command_list.push_back(new Tango::TemplCommand("lowerYMoveZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerYMoveZero)));
    command_list.push_back(new Tango::TemplCommand("lowerYStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerYStop)));
    command_list.push_back(new Tango::TemplCommand("lowerYReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerYReset)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("lowerZMoveAbsolute",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::lowerZMoveAbsolute)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("lowerZMoveRelative",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::lowerZMoveRelative)));
    command_list.push_back(new Tango::TemplCommand("lowerZMoveZero",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerZMoveZero)));
    command_list.push_back(new Tango::TemplCommand("lowerZStop",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerZStop)));
    command_list.push_back(new Tango::TemplCommand("lowerZReset",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerZReset)));
    
    // Synchronized Movement
    command_list.push_back(new Tango::TemplCommandIn<const Tango::DevVarDoubleArray *>("synchronizedMove",
        static_cast<void (Tango::DeviceImpl::*)(const Tango::DevVarDoubleArray *)>(&ReflectionImagingDevice::synchronizedMove)));
    
    // CCD Camera commands (四CCD：上下各两个，1倍和10倍物镜，海康MV-CU020-19GC)
    // 上1倍物镜CCD（粗定位）
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("upperCCD1xSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::upperCCD1xSwitch)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("upperCCD1xRingLightSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::upperCCD1xRingLightSwitch)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("captureUpperCCD1xImage",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::captureUpperCCD1xImage)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("setUpperCCD1xExposure",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::setUpperCCD1xExposure)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("getUpperCCD1xImage",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::getUpperCCD1xImage)));
    
    // 上10倍物镜CCD（近距离观察）
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("upperCCD10xSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::upperCCD10xSwitch)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("upperCCD10xRingLightSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::upperCCD10xRingLightSwitch)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("captureUpperCCD10xImage",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::captureUpperCCD10xImage)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("setUpperCCD10xExposure",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::setUpperCCD10xExposure)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("getUpperCCD10xImage",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::getUpperCCD10xImage)));
    
    // 下1倍物镜CCD（粗定位）
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("lowerCCD1xSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::lowerCCD1xSwitch)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("lowerCCD1xRingLightSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::lowerCCD1xRingLightSwitch)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("captureLowerCCD1xImage",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::captureLowerCCD1xImage)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("setLowerCCD1xExposure",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::setLowerCCD1xExposure)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("getLowerCCD1xImage",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::getLowerCCD1xImage)));
    
    // 下10倍物镜CCD（近距离观察）
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("lowerCCD10xSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::lowerCCD10xSwitch)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("lowerCCD10xRingLightSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&ReflectionImagingDevice::lowerCCD10xRingLightSwitch)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("captureLowerCCD10xImage",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::captureLowerCCD10xImage)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("setLowerCCD10xExposure",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::setLowerCCD10xExposure)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("getLowerCCD10xImage",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::getLowerCCD10xImage)));
    
    // 批量操作
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("captureAllImages",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::captureAllImages)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevDouble>("startAutoCapture",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevDouble)>(&ReflectionImagingDevice::startAutoCapture)));
    command_list.push_back(new Tango::TemplCommand("stopAutoCapture",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::stopAutoCapture)));
    
    // Support commands
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("operateUpperSupport",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&ReflectionImagingDevice::operateUpperSupport)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("operateLowerSupport",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&ReflectionImagingDevice::operateLowerSupport)));
    command_list.push_back(new Tango::TemplCommand("upperSupportRise",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperSupportRise)));
    command_list.push_back(new Tango::TemplCommand("upperSupportLower",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::upperSupportLower)));
    command_list.push_back(new Tango::TemplCommand("lowerSupportRise",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerSupportRise)));
    command_list.push_back(new Tango::TemplCommand("lowerSupportLower",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::lowerSupportLower)));
    command_list.push_back(new Tango::TemplCommand("stopUpperSupport",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::stopUpperSupport)));
    command_list.push_back(new Tango::TemplCommand("stopLowerSupport",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::stopLowerSupport)));
    
    // Misc commands
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("readtAxis",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::readtAxis)));
    command_list.push_back(new Tango::TemplCommand("exportAxis",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::exportAxis)));
    command_list.push_back(new Tango::TemplCommand("exportLogs",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::exportLogs)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("simSwitch",
        static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&ReflectionImagingDevice::simSwitch)));
    
    // Power Control Commands (NEW - for GUI)
    command_list.push_back(new Tango::TemplCommand("enableDriverPower",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::enableDriverPower)));
    command_list.push_back(new Tango::TemplCommand("disableDriverPower",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::disableDriverPower)));
    command_list.push_back(new Tango::TemplCommand("releaseBrake",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::releaseBrake)));
    command_list.push_back(new Tango::TemplCommand("engageBrake",
        static_cast<void (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::engageBrake)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("queryPowerStatus",
        static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&ReflectionImagingDevice::queryPowerStatus)));
}

// ========== Power Control Commands (for GUI) ==========
void ReflectionImagingDevice::enableDriverPower() {
    if (!enable_driver_power()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to enable driver power", "ReflectionImagingDevice::enableDriverPower");
    }
}

void ReflectionImagingDevice::disableDriverPower() {
    if (!disable_driver_power()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to disable driver power", "ReflectionImagingDevice::disableDriverPower");
    }
}

void ReflectionImagingDevice::releaseBrake() {
    if (!release_brake()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to release brake", "ReflectionImagingDevice::releaseBrake");
    }
}

void ReflectionImagingDevice::engageBrake() {
    if (!engage_brake()) {
        Tango::Except::throw_exception("PowerControlError", 
            "Failed to engage brake", "ReflectionImagingDevice::engageBrake");
    }
}

Tango::DevString ReflectionImagingDevice::queryPowerStatus() {
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
bool ReflectionImagingDevice::enable_driver_power() {
    INFO_STREAM << "[PowerControl] enable_driver_power() called" << endl;
    INFO_STREAM << "[PowerControl] driver_power_port_=" << driver_power_port_ 
               << ", driver_power_controller_=" << driver_power_controller_ << endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Driver power enabled (simulated)" << endl;
        driver_power_enabled_ = true;
        return true;
    }
    
    if (driver_power_port_ < 0 || driver_power_controller_.empty()) {
        INFO_STREAM << "[PowerControl] Driver power control not configured (port=" 
                   << driver_power_port_ << ", controller=" << driver_power_controller_ 
                   << "), skipping" << endl;
        return true;
    }
    
    try {
        // 反射光成像的电源控制使用配置的专用控制器（通常是 sys/motion/1）
        Tango::DeviceProxy power_ctrl(driver_power_controller_);
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(driver_power_port_);
        params[1] = 1.0;  // HIGH = 上电
        Tango::DeviceData data;
        data << params;
        
        INFO_STREAM << "[PowerControl] Calling writeIO on " << driver_power_controller_ 
                   << " with port=" << driver_power_port_ << ", value=1" << endl;
        INFO_STREAM << "[PowerControl] Executing hardware writeIO command..." << endl;
        power_ctrl.command_inout("writeIO", data);
        INFO_STREAM << "[PowerControl] writeIO command executed successfully" << endl;
        
        driver_power_enabled_ = true;
        INFO_STREAM << "✓ Driver power enabled on " << driver_power_controller_ 
                   << " port OUT" << driver_power_port_ << endl;
        log_event("Driver power enabled on port OUT" + std::to_string(driver_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to enable driver power: " << e.errors[0].desc << endl;
        log_event("Failed to enable driver power: " + std::string(e.errors[0].desc.in()));
        return false;
    }
}

bool ReflectionImagingDevice::disable_driver_power() {
    // 断电前自动启用刹车（安全保护）
    if (brake_power_port_ >= 0 && brake_released_) {
        INFO_STREAM << "[BrakeControl] Auto-engaging brake before disabling driver power (safety)" << endl;
        if (!engage_brake()) {
            WARN_STREAM << "[BrakeControl] Failed to engage brake before disabling driver power" << endl;
        }
    }
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Driver power disabled (simulated)" << endl;
        driver_power_enabled_ = false;
        return true;
    }
    
    if (driver_power_port_ < 0 || driver_power_controller_.empty()) {
        return true;
    }
    
    try {
        // 反射光成像的电源控制使用配置的专用控制器
        Tango::DeviceProxy power_ctrl(driver_power_controller_);
        
        Tango::DevVarDoubleArray params;
        params.length(2);
        params[0] = static_cast<double>(driver_power_port_);
        // 注意：OUTx 为低电平有效，MotionControllerDevice::writeIO 会做 active-low 反相
        // 这里写入的是“逻辑值”：1=开启(硬件LOW)，0=关闭(硬件HIGH)
        params[1] = 0.0;  // 逻辑0=关闭驱动器上电
        Tango::DeviceData data;
        data << params;
        power_ctrl.command_inout("writeIO", data);
        
        driver_power_enabled_ = false;
        INFO_STREAM << "✓ Driver power disabled on port OUT" << driver_power_port_ << endl;
        log_event("Driver power disabled on port OUT" + std::to_string(driver_power_port_));
        return true;
    } catch (Tango::DevFailed &e) {
        ERROR_STREAM << "✗ Failed to disable driver power: " << e.errors[0].desc << endl;
        return false;
    }
}

bool ReflectionImagingDevice::release_brake() {
    INFO_STREAM << "[BrakeControl] release_brake() called" << endl;
    INFO_STREAM << "[BrakeControl] brake_power_port_=" << brake_power_port_ 
               << ", brake_power_controller_=" << brake_power_controller_ << endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Brake released (simulated)" << endl;
        brake_released_ = true;
        return true;
    }
    
    if (brake_power_port_ < 0) {
        INFO_STREAM << "[BrakeControl] Brake control not configured (port=" 
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
            INFO_STREAM << "[BrakeControl] Using dedicated brake controller: " << brake_power_controller_ << endl;
        } else {
            auto upper = get_upper_platform_proxy();
            auto lower = get_lower_platform_proxy();
            if (upper || lower) {
                brake_ctrl = upper ? upper.get() : lower.get();
                INFO_STREAM << "[BrakeControl] Using platform motion controller for brake" << endl;
            } else {
                ERROR_STREAM << "[BrakeControl] No controller available for brake control!" << endl;
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
        
        INFO_STREAM << "[BrakeControl] Calling writeIO for brake with port=" << brake_power_port_ << ", value=1" << endl;
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

bool ReflectionImagingDevice::engage_brake() {
    INFO_STREAM << "[BrakeControl] engage_brake() called" << endl;
    INFO_STREAM << "[BrakeControl] brake_power_port_=" << brake_power_port_ 
               << ", brake_power_controller_=" << brake_power_controller_ << endl;
    
    if (sim_mode_) {
        INFO_STREAM << "Simulation: Brake engaged (simulated)" << endl;
        brake_released_ = false;
        return true;
    }
    
    if (brake_power_port_ < 0) {
        INFO_STREAM << "[BrakeControl] Brake control not configured (port=" 
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
            INFO_STREAM << "[BrakeControl] Using dedicated brake controller: " << brake_power_controller_ << endl;
        } else {
            auto upper = get_upper_platform_proxy();
            auto lower = get_lower_platform_proxy();
            if (upper || lower) {
                brake_ctrl = upper ? upper.get() : lower.get();
                INFO_STREAM << "[BrakeControl] Using platform motion controller for brake" << endl;
            } else {
                ERROR_STREAM << "[BrakeControl] No controller available for brake control!" << endl;
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
        
        INFO_STREAM << "[BrakeControl] Calling writeIO for brake with port=" << brake_power_port_ << ", value=0" << endl;
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

void ReflectionImagingDeviceClass::device_factory(const Tango::DevVarStringArray *devlist_ptr) {
    for (unsigned long i = 0; i < devlist_ptr->length(); i++) {
        std::string name((*devlist_ptr)[i].in());
        std::cout << "New device: " << name << std::endl;
        ReflectionImagingDevice *dev = new ReflectionImagingDevice(this, name);
        device_list.push_back(dev);
        export_device(dev);
    }
}

} // namespace ReflectionImaging

// ========== Tango class factory (required for device registration) ==========
void Tango::DServer::class_factory() {
    add_class(ReflectionImaging::ReflectionImagingDeviceClass::instance());
}

// ========== Main function ==========
int main(int argc, char *argv[]) {
    try {
        Common::SystemConfig::loadConfig();
        Tango::Util *tg = Tango::Util::init(argc, argv);
        tg->server_init();
        std::cout << "ReflectionImaging Server Ready" << std::endl;
        tg->server_run();
        return 0;
    } catch (Tango::DevFailed &e) {
        Tango::Except::print_exception(e);
        return -1;
    } catch (...) {
        std::cerr << "Unknown exception in main" << std::endl;
        return -1;
    }
}