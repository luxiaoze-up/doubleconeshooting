#ifndef REFLECTION_IMAGING_DEVICE_H
#define REFLECTION_IMAGING_DEVICE_H

#include "common/standard_system_device.h"
#include "drivers/mv_cu020_19gc.h"
#include <tango.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <array>
#include <atomic>
#include <thread>
#include <atomic>

namespace ReflectionImaging {

// 子设备类型标识
enum SubDeviceType {
    DEV_UPPER_PLATFORM = 0,  // 上三坐标平台
    DEV_LOWER_PLATFORM,  // 下三坐标平台
    DEV_COUNT
};

// 三坐标平台轴索引
enum PlatformAxis {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2
};

class ReflectionImagingDevice : public Common::StandardSystemDevice {
private:
    // 锁定
    bool is_locked_;
    std::string lock_user_;
    std::mutex lock_mutex_;

    // ===== Properties =====
    std::string bundle_no_;
    std::string laser_no_;
    std::string system_no_;
    std::string sub_dev_list_;
    std::vector<std::string> model_list_;
    std::string current_model_;
    std::string connect_string_;
    std::string error_dict_;
    std::string device_name_;
    std::string device_id_;
    std::string device_position_;
    std::string device_product_date_;
    std::string device_install_date_;
    
    // 上下双三坐标平台配置
    std::string upper_platform_config_;  // JSON格式配置
    std::string lower_platform_config_;  // JSON格式配置
    std::array<short, 3> upper_platform_range_;  // [X, Y, Z]行程
    std::array<short, 3> lower_platform_range_;  // [X, Y, Z]行程
    short upper_platform_limit_number_;
    short lower_platform_limit_number_;
    
    // 四CCD相机配置（上下各两个：1倍和10倍物镜）
    // 相机型号：海康 MV-CU020-19GC
    std::string upper_ccd_1x_config_;   // 上1倍物镜CCD配置（粗定位）
    std::string upper_ccd_10x_config_;  // 上10倍物镜CCD配置（近距离观察）
    std::string lower_ccd_1x_config_;   // 下1倍物镜CCD配置（粗定位）
    std::string lower_ccd_10x_config_;  // 下10倍物镜CCD配置（近距离观察）
    std::string image_save_path_;
    std::string image_format_;
    double auto_capture_interval_;
    
    // 辅助支撑配置
    std::string upper_support_config_;  // JSON格式配置
    std::string lower_support_config_;  // JSON格式配置
    
    // 运动控制器和编码器
    std::string motion_controller_name_;
    std::string encoder_name_;
    
    // 编码器通道配置（用于通过 encoder_device 读取外部编码器）
    std::vector<short> upper_platform_encoder_channels_;  // 上平台3轴对应的编码器通道
    std::vector<short> lower_platform_encoder_channels_;  // 下平台3轴对应的编码器通道
    
    // Motor parameters
    double motor_step_angle_;         // motorStepAngle - 电机步距角 (默认1.8度)
    double motor_gear_ratio_;         // motorGearRatio - 齿轮比 (默认1.0)
    double motor_subdivision_;        // motorSubdivision - 细分数 (默认12800)
    
    // Driver power control (NEW)
    short driver_power_port_;         // driverPowerPort - 驱动器上电端口号
    std::string driver_power_controller_; // driverPowerController - 控制驱动器上电的运动控制器名称
    bool driver_power_enabled_;       // 驱动器电源状态
    
    // Brake power control (NEW - 部分轴有刹车：设备1-z和设备2-z，OUT5)
    short brake_power_port_;          // brakePowerPort - 刹车供电端口号
    std::string brake_power_controller_;  // brakePowerController - 控制刹车供电的运动控制器名称
    bool brake_released_;             // 刹车释放状态
    
    // Cached Support Configuration
    int upper_support_axis_id_;
    double upper_support_work_pos_;
    double upper_support_home_pos_;
    int lower_support_axis_id_;
    double lower_support_work_pos_;
    double lower_support_home_pos_;

    // ===== Runtime State / Attributes =====
    long self_check_result_;
    std::string position_unit_;
    std::string fault_state_;
    std::string reflection_logs_;  // 改为reflection_logs
    short result_value_;

    // Limit fault latch: once limit is detected, hold FAULT until reset
    std::atomic<bool> limit_fault_latched_{false};
    std::atomic<short> limit_fault_axis_{-1};      // 4=upper Z, 5=lower Z
    std::atomic<short> limit_fault_el_state_{0};   // 1=EL+, -1=EL-, 0=none

    // 上三坐标平台状态
    std::array<double, 3> upper_platform_pos_;      // [X, Y, Z]位置
    std::array<double, 3> upper_platform_dire_pos_; // [X, Y, Z]指令位置
    std::array<short, 3> upper_platform_lim_org_state_;  // [X, Y, Z]限位/原点状态
    std::array<bool, 3> upper_platform_state_;      // [X, Y, Z]运行状态
    std::string upper_platform_axis_parameter_;     // JSON格式轴参数
    
    // 下三坐标平台状态
    std::array<double, 3> lower_platform_pos_;      // [X, Y, Z]位置
    std::array<double, 3> lower_platform_dire_pos_; // [X, Y, Z]指令位置
    std::array<short, 3> lower_platform_lim_org_state_;  // [X, Y, Z]限位/原点状态
    std::array<bool, 3> lower_platform_state_;      // [X, Y, Z]运行状态
    std::string lower_platform_axis_parameter_;     // JSON格式轴参数

    // 四CCD相机状态（上下各两个：1倍和10倍物镜）
    // 上1倍物镜CCD（粗定位）
    std::string upper_ccd_1x_state_;      // READY, CAPTURING, ERROR
    double upper_ccd_1x_exposure_;        // 曝光时间（秒）
    std::string upper_ccd_1x_trigger_mode_;  // Software, Hardware, Continuous
    std::string upper_ccd_1x_resolution_;  // "1920x1080"
    long upper_ccd_1x_width_;             // 分辨率宽度
    long upper_ccd_1x_height_;            // 分辨率高度
    double upper_ccd_1x_gain_;            // 增益（0.0-100.0）
    double upper_ccd_1x_brightness_;       // 亮度（-100.0-100.0）
    double upper_ccd_1x_contrast_;         // 对比度（-100.0-100.0）
    bool upper_ccd_1x_ring_light_on_;
    std::string upper_ccd_1x_image_url_;   // 最新图像URL
    std::string upper_ccd_1x_last_capture_time_;
    
    // 上10倍物镜CCD（近距离观察）
    std::string upper_ccd_10x_state_;
    double upper_ccd_10x_exposure_;
    std::string upper_ccd_10x_trigger_mode_;
    std::string upper_ccd_10x_resolution_;
    long upper_ccd_10x_width_;            // 分辨率宽度
    long upper_ccd_10x_height_;           // 分辨率高度
    double upper_ccd_10x_gain_;           // 增益（0.0-100.0）
    double upper_ccd_10x_brightness_;      // 亮度（-100.0-100.0）
    double upper_ccd_10x_contrast_;        // 对比度（-100.0-100.0）
    bool upper_ccd_10x_ring_light_on_;
    std::string upper_ccd_10x_image_url_;
    std::string upper_ccd_10x_last_capture_time_;
    
    // 下1倍物镜CCD（粗定位）
    std::string lower_ccd_1x_state_;
    double lower_ccd_1x_exposure_;
    std::string lower_ccd_1x_trigger_mode_;
    std::string lower_ccd_1x_resolution_;
    long lower_ccd_1x_width_;             // 分辨率宽度
    long lower_ccd_1x_height_;            // 分辨率高度
    double lower_ccd_1x_gain_;            // 增益（0.0-100.0）
    double lower_ccd_1x_brightness_;      // 亮度（-100.0-100.0）
    double lower_ccd_1x_contrast_;        // 对比度（-100.0-100.0）
    bool lower_ccd_1x_ring_light_on_;
    std::string lower_ccd_1x_image_url_;
    std::string lower_ccd_1x_last_capture_time_;
    
    // 下10倍物镜CCD（近距离观察）
    std::string lower_ccd_10x_state_;
    double lower_ccd_10x_exposure_;
    std::string lower_ccd_10x_trigger_mode_;
    std::string lower_ccd_10x_resolution_;
    long lower_ccd_10x_width_;            // 分辨率宽度
    long lower_ccd_10x_height_;           // 分辨率高度
    double lower_ccd_10x_gain_;           // 增益（0.0-100.0）
    double lower_ccd_10x_brightness_;     // 亮度（-100.0-100.0）
    double lower_ccd_10x_contrast_;       // 对比度（-100.0-100.0）
    bool lower_ccd_10x_ring_light_on_;
    std::string lower_ccd_10x_image_url_;
    std::string lower_ccd_10x_last_capture_time_;
    
    long image_capture_count_;
    bool auto_capture_enabled_;

    // 辅助支撑状态
    double upper_support_position_;
    double lower_support_position_;
    std::string upper_support_state_;  // LOWERED, RISING, RISEN, LOWERING, ERROR
    std::string lower_support_state_;

    std::string axis_parameter_;
    bool sim_mode_;
    std::chrono::steady_clock::time_point last_update_time_;

    // Shadow variables for attribute reading to prevent pointer escaping
    Tango::DevLong attr_selfCheckResult_read;
    Tango::DevString attr_positionUnit_read;
    Tango::DevString attr_faultState_read;
    Tango::DevString attr_reflectionLogs_read;
    Tango::DevShort attr_resultValue_read;
    Tango::DevString attr_groupAttributeJson_read;
    
    // CCD Shadow variables
    Tango::DevString attr_upperCcd1xState_read;
    Tango::DevString attr_upperCcd1xTriggerMode_read;
    Tango::DevString attr_upperCcd1xResolution_read;
    Tango::DevString attr_upperCcd1xImageUrl_read;
    Tango::DevString attr_upperCcd1xLastCaptureTime_read;
    Tango::DevBoolean attr_upperCcd1xRingLightOn_read;
    Tango::DevString attr_upperCcd10xState_read;
    Tango::DevString attr_upperCcd10xTriggerMode_read;
    Tango::DevString attr_upperCcd10xResolution_read;
    Tango::DevString attr_upperCcd10xImageUrl_read;
    Tango::DevString attr_upperCcd10xLastCaptureTime_read;
    Tango::DevBoolean attr_upperCcd10xRingLightOn_read;
    Tango::DevString attr_lowerCcd1xState_read;
    Tango::DevString attr_lowerCcd1xTriggerMode_read;
    Tango::DevString attr_lowerCcd1xResolution_read;
    Tango::DevString attr_lowerCcd1xImageUrl_read;
    Tango::DevString attr_lowerCcd1xLastCaptureTime_read;
    Tango::DevBoolean attr_lowerCcd1xRingLightOn_read;
    Tango::DevString attr_lowerCcd10xState_read;
    Tango::DevString attr_lowerCcd10xTriggerMode_read;
    Tango::DevString attr_lowerCcd10xResolution_read;
    Tango::DevString attr_lowerCcd10xImageUrl_read;
    Tango::DevString attr_lowerCcd10xLastCaptureTime_read;
    Tango::DevBoolean attr_lowerCcd10xRingLightOn_read;
    
    // Support Shadow variables
    Tango::DevString attr_upperSupportState_read;
    Tango::DevString attr_lowerSupportState_read;
    Tango::DevString attr_upperPlatformAxisParameter_read;
    Tango::DevString attr_lowerPlatformAxisParameter_read;
    Tango::DevString attr_axisParameter_read;
    Tango::DevString attr_alarmState_read;
    
    // Spectrum shadow variables
    Tango::DevDouble attr_upperPlatformPos_read[3];
    Tango::DevDouble attr_upperPlatformDirePos_read[3];
    Tango::DevShort attr_upperPlatformLimOrgState_read[3];
    Tango::DevBoolean attr_upperPlatformState_read[3];
    Tango::DevDouble attr_lowerPlatformPos_read[3];
    Tango::DevDouble attr_lowerPlatformDirePos_read[3];
    Tango::DevShort attr_lowerPlatformLimOrgState_read[3];
    Tango::DevBoolean attr_lowerPlatformState_read[3];
    
    // 单轴属性shadow变量 (用于GUI单轴独立控制)
    Tango::DevDouble attr_upperPlatformPosX_read;
    Tango::DevDouble attr_upperPlatformPosY_read;
    Tango::DevDouble attr_upperPlatformPosZ_read;
    Tango::DevBoolean attr_upperPlatformStateX_read;
    Tango::DevBoolean attr_upperPlatformStateY_read;
    Tango::DevBoolean attr_upperPlatformStateZ_read;
    Tango::DevDouble attr_lowerPlatformPosX_read;
    Tango::DevDouble attr_lowerPlatformPosY_read;
    Tango::DevDouble attr_lowerPlatformPosZ_read;
    Tango::DevBoolean attr_lowerPlatformStateX_read;
    Tango::DevBoolean attr_lowerPlatformStateY_read;
    Tango::DevBoolean attr_lowerPlatformStateZ_read;

    // ===== Proxies =====
    std::shared_ptr<Tango::DeviceProxy> encoder_proxy_;    // 编码器设备代理
    std::shared_ptr<Tango::DeviceProxy> upper_platform_proxy_;  // 上平台运动控制器代理
    std::shared_ptr<Tango::DeviceProxy> lower_platform_proxy_;  // 下平台运动控制器代理
    mutable std::mutex proxy_mutex_;  // 保护 proxy 访问的互斥锁
    
    // ===== CCD相机驱动 =====
    std::unique_ptr<Hikvision::MV_CU020_19GC> upper_ccd_1x_driver_;   // 上1倍物镜CCD驱动
    std::unique_ptr<Hikvision::MV_CU020_19GC> upper_ccd_10x_driver_; // 上10倍物镜CCD驱动
    std::unique_ptr<Hikvision::MV_CU020_19GC> lower_ccd_1x_driver_;  // 下1倍物镜CCD驱动
    std::unique_ptr<Hikvision::MV_CU020_19GC> lower_ccd_10x_driver_; // 下10倍物镜CCD驱动

    // Helpers
    void check_state(const std::string& cmd_name);
    void connect_proxies();
    // Background connection monitor (LargeStroke-style)
    void start_connection_monitor();
    void stop_connection_monitor();
    void connection_monitor_loop();
    
    // Proxy access helpers (lifetime-safe, LargeStroke-style)
    std::shared_ptr<Tango::DeviceProxy> get_encoder_proxy();
    std::shared_ptr<Tango::DeviceProxy> get_upper_platform_proxy();
    std::shared_ptr<Tango::DeviceProxy> get_lower_platform_proxy();
    void reset_encoder_proxy();
    void reset_upper_platform_proxy();
    void reset_lower_platform_proxy();
    void rebuild_encoder_proxy(int timeout_ms);
    void rebuild_upper_platform_proxy(int timeout_ms);
    void rebuild_lower_platform_proxy(int timeout_ms);
    void perform_post_motion_reconnect_restore();  // 重连后恢复配置和状态

    void update_sub_devices();
    void log_event(const std::string& event);
    std::string parse_json_value(const std::string& json, const std::string& key);
    
    // Camera driver helpers
    void initialize_cameras();  // 初始化所有CCD相机驱动
    void shutdown_cameras();   // 关闭所有CCD相机驱动
    
    // Power control methods (NEW)
    bool enable_driver_power();       // 启动驱动器电源
    bool disable_driver_power();      // 关闭驱动器电源
    bool release_brake();             // 释放刹车（用于Z轴）
    bool engage_brake();              // 启用刹车（用于Z轴）
    
    // 辅助支撑辅助函数
    void update_support_states();
    
    // ===== 三坐标平台运动学辅助函数 =====
    // 限位检查
    bool checkUpperPlatformLimits(double x, double y, double z);
    bool checkLowerPlatformLimits(double x, double y, double z);
    bool checkUpperPlatformRelativeLimits(double dx, double dy, double dz);
    bool checkLowerPlatformRelativeLimits(double dx, double dy, double dz);
    
    // 碰撞检测（上下平台Z轴安全距离检查）
    bool checkPlatformCollision(double upper_z, double lower_z);
    // 暂时禁用碰撞检测（设为0），等确认正确的物理布局后再重新实现
    // 原值: 50.0mm
    static constexpr double MIN_PLATFORM_Z_DISTANCE = 0.0;  // 最小安全距离(mm)
    
    // 同步运动速度匹配计算
    struct SyncMoveParams {
        std::array<double, 3> upper_velocity;  // 上平台各轴速度
        std::array<double, 3> lower_velocity;  // 下平台各轴速度
        double total_time;                      // 总运动时间
    };
    SyncMoveParams calculateSyncVelocities(
        const std::array<double, 3>& upper_target,
        const std::array<double, 3>& lower_target,
        double max_velocity = 100.0);  // 默认最大速度 mm/s
    
    // 仿真模式运动模拟
    void updateSimulatedMotion();
    std::chrono::steady_clock::time_point sim_motion_start_time_;
    std::array<double, 3> upper_platform_target_;  // 上平台目标位置
    std::array<double, 3> lower_platform_target_;  // 下平台目标位置
    double sim_motion_duration_;                    // 模拟运动持续时间(ms)

    // Connection health (updated by background thread)
    std::atomic<bool> connection_healthy_{false};
    std::atomic<bool> reconnect_pending_{false};
    std::atomic<bool> stop_connection_monitor_{false};
    std::thread connection_monitor_thread_;
    std::chrono::steady_clock::time_point last_reconnect_attempt_{std::chrono::steady_clock::now()};
    
    // 当后台线程完成 motion proxy 重建后，标记需要在主线程执行"恢复动作"（上电/刹车/参数/同步）
    std::atomic<bool> motion_restore_pending_{false};
    std::atomic<int> restore_retry_count_{0};  // 恢复操作重试计数
    static constexpr int MAX_RESTORE_RETRIES = 3;  // 最大重试次数

public:
    ReflectionImagingDevice(Tango::DeviceClass *device_class, std::string &device_name);
    virtual ~ReflectionImagingDevice();

    virtual void init_device() override;
    virtual void delete_device() override;
    virtual void always_executed_hook() override;

    // ===== Commands =====
    // Lock/Unlock
    void devLock(Tango::DevString user_info);
    void devUnlock(Tango::DevBoolean unlock_all);
    void devLockVerify();
    Tango::DevString devLockQuery();
    void devUserConfig(Tango::DevString config);

    // System
    void selfCheck();
    void specific_self_check() override;
    void init();

    // Upper Platform (上三坐标平台)
    void upperPlatformAxisSet(const Tango::DevVarDoubleArray* params);  // [轴号, 起始速度, 最大速度, 加速时间, 减速时间, 停止速度]
    void upperPlatformStructAxisSet(const Tango::DevVarDoubleArray* params);  // [轴号, 步距角, 传动比, 细分]
    void upperPlatformMoveRelative(const Tango::DevVarDoubleArray* params);  // [X, Y, Z]
    void upperPlatformMoveAbsolute(const Tango::DevVarDoubleArray* params);  // [X, Y, Z]
    void upperPlatformMoveToPosition(const Tango::DevVarDoubleArray* params);  // [X, Y, Z]
    void upperPlatformStop();
    void upperPlatformReset();
    void upperPlatformMoveZero();
    Tango::DevVarDoubleArray* upperPlatformReadEncoder();  // 返回 [X, Y, Z]
    Tango::DevVarShortArray* upperPlatformReadOrg();  // 返回 [X, Y, Z] (0-否, 1-是)
    Tango::DevVarShortArray* upperPlatformReadEL();  // 返回 [X, Y, Z]
    void upperPlatformSingleAxisMove(const Tango::DevVarDoubleArray* params);  // [轴号, 位移]
    
    // 上平台单轴控制命令 (用于GUI单轴独立控制)
    void upperXMoveAbsolute(Tango::DevDouble val);
    void upperXMoveRelative(Tango::DevDouble val);
    void upperXMoveZero();
    void upperXStop();
    void upperXReset();
    void upperYMoveAbsolute(Tango::DevDouble val);
    void upperYMoveRelative(Tango::DevDouble val);
    void upperYMoveZero();
    void upperYStop();
    void upperYReset();
    void upperZMoveAbsolute(Tango::DevDouble val);
    void upperZMoveRelative(Tango::DevDouble val);
    void upperZMoveZero();
    void upperZStop();
    void upperZReset();

    // Lower Platform (下三坐标平台)
    void lowerPlatformAxisSet(const Tango::DevVarDoubleArray* params);
    void lowerPlatformStructAxisSet(const Tango::DevVarDoubleArray* params);
    void lowerPlatformMoveRelative(const Tango::DevVarDoubleArray* params);
    void lowerPlatformMoveAbsolute(const Tango::DevVarDoubleArray* params);
    void lowerPlatformMoveToPosition(const Tango::DevVarDoubleArray* params);
    void lowerPlatformStop();
    void lowerPlatformReset();
    void lowerPlatformMoveZero();
    Tango::DevVarDoubleArray* lowerPlatformReadEncoder();
    Tango::DevVarShortArray* lowerPlatformReadOrg();  // 返回 [X, Y, Z] (0-否, 1-是)
    Tango::DevVarShortArray* lowerPlatformReadEL();
    void lowerPlatformSingleAxisMove(const Tango::DevVarDoubleArray* params);
    
    // 下平台单轴控制命令 (用于GUI单轴独立控制)
    void lowerXMoveAbsolute(Tango::DevDouble val);
    void lowerXMoveRelative(Tango::DevDouble val);
    void lowerXMoveZero();
    void lowerXStop();
    void lowerXReset();
    void lowerYMoveAbsolute(Tango::DevDouble val);
    void lowerYMoveRelative(Tango::DevDouble val);
    void lowerYMoveZero();
    void lowerYStop();
    void lowerYReset();
    void lowerZMoveAbsolute(Tango::DevDouble val);
    void lowerZMoveRelative(Tango::DevDouble val);
    void lowerZMoveZero();
    void lowerZStop();
    void lowerZReset();

    // Synchronized Movement (同步运动)
    void synchronizedMove(const Tango::DevVarDoubleArray* params);  // [上X, 上Y, 上Z, 下X, 下Y, 下Z]

    // CCD Camera (四CCD相机：上下各两个，1倍和10倍物镜)
    // 上1倍物镜CCD（粗定位）
    void upperCCD1xSwitch(Tango::DevBoolean on);
    void upperCCD1xRingLightSwitch(Tango::DevBoolean on);
    Tango::DevString captureUpperCCD1xImage();
    void setUpperCCD1xExposure(Tango::DevDouble exposure);
    Tango::DevString getUpperCCD1xImage();  // 返回Base64编码
    
    // 上10倍物镜CCD（近距离观察）
    void upperCCD10xSwitch(Tango::DevBoolean on);
    void upperCCD10xRingLightSwitch(Tango::DevBoolean on);
    Tango::DevString captureUpperCCD10xImage();
    void setUpperCCD10xExposure(Tango::DevDouble exposure);
    Tango::DevString getUpperCCD10xImage();  // 返回Base64编码
    
    // 下1倍物镜CCD（粗定位）
    void lowerCCD1xSwitch(Tango::DevBoolean on);
    void lowerCCD1xRingLightSwitch(Tango::DevBoolean on);
    Tango::DevString captureLowerCCD1xImage();
    void setLowerCCD1xExposure(Tango::DevDouble exposure);
    Tango::DevString getLowerCCD1xImage();  // 返回Base64编码
    
    // 下10倍物镜CCD（近距离观察）
    void lowerCCD10xSwitch(Tango::DevBoolean on);
    void lowerCCD10xRingLightSwitch(Tango::DevBoolean on);
    Tango::DevString captureLowerCCD10xImage();
    void setLowerCCD10xExposure(Tango::DevDouble exposure);
    Tango::DevString getLowerCCD10xImage();  // 返回Base64编码
    
    // 批量操作
    Tango::DevString captureAllImages();  // 同时抓取所有4个CCD图像
    void startAutoCapture(Tango::DevDouble interval);
    void stopAutoCapture();

    // Support (辅助支撑)
    void operateUpperSupport(Tango::DevShort operation);  // 0-降下, 1-升起
    void operateLowerSupport(Tango::DevShort operation);
    void upperSupportRise();
    void upperSupportLower();
    void lowerSupportRise();
    void lowerSupportLower();
    void stopUpperSupport();
    void stopLowerSupport();

    // Misc
    Tango::DevString readtAxis();
    void exportAxis();
    void exportLogs();
    void simSwitch(Tango::DevShort mode);
    
    // Power Control Commands (for GUI)
    void enableDriverPower();         // 手动启动驱动器电源
    void disableDriverPower();        // 手动关闭驱动器电源
    void releaseBrake();              // 手动释放刹车
    void engageBrake();               // 手动启用刹车
    Tango::DevString queryPowerStatus();  // 查询电源状态（返回JSON）

    // ===== Attributes =====
    virtual void read_attr(Tango::Attribute &attr) override;
    virtual void write_attr(Tango::WAttribute &attr);  // 处理可写属性
    virtual void read_attr_hardware(std::vector<long> &attr_list) override;

    // 标准属性
    void read_self_check_result(Tango::Attribute& attr);
    void read_position_unit(Tango::Attribute& attr);
    void read_group_attribute_json(Tango::Attribute& attr);
    void read_reflection_logs(Tango::Attribute& attr);
    void read_fault_state(Tango::Attribute& attr);
    void read_result_value(Tango::Attribute& attr);
    void read_driver_power_status(Tango::Attribute& attr);
    void read_brake_status(Tango::Attribute& attr);

    // 上平台属性
    void read_upper_platform_pos(Tango::Attribute& attr);
    void read_upper_platform_dire_pos(Tango::Attribute& attr);
    void read_upper_platform_lim_org_state(Tango::Attribute& attr);
    void read_upper_platform_state(Tango::Attribute& attr);
    void read_upper_platform_axis_parameter(Tango::Attribute& attr);

    // 下平台属性
    void read_lower_platform_pos(Tango::Attribute& attr);
    void read_lower_platform_dire_pos(Tango::Attribute& attr);
    void read_lower_platform_lim_org_state(Tango::Attribute& attr);
    void read_lower_platform_state(Tango::Attribute& attr);
    void read_lower_platform_axis_parameter(Tango::Attribute& attr);
    
    // 单轴属性读取方法 (用于GUI单轴独立控制)
    void read_upper_platform_pos_x(Tango::Attribute& attr);
    void read_upper_platform_pos_y(Tango::Attribute& attr);
    void read_upper_platform_pos_z(Tango::Attribute& attr);
    void read_upper_platform_state_x(Tango::Attribute& attr);
    void read_upper_platform_state_y(Tango::Attribute& attr);
    void read_upper_platform_state_z(Tango::Attribute& attr);
    void read_lower_platform_pos_x(Tango::Attribute& attr);
    void read_lower_platform_pos_y(Tango::Attribute& attr);
    void read_lower_platform_pos_z(Tango::Attribute& attr);
    void read_lower_platform_state_x(Tango::Attribute& attr);
    void read_lower_platform_state_y(Tango::Attribute& attr);
    void read_lower_platform_state_z(Tango::Attribute& attr);

    // CCD相机属性（四CCD：上下各两个，1倍和10倍物镜）
    // 上1倍物镜CCD属性
    void read_upper_ccd_1x_state(Tango::Attribute& attr);
    void read_upper_ccd_1x_ring_light_on(Tango::Attribute& attr);
    void read_upper_ccd_1x_exposure(Tango::Attribute& attr);
    void write_upper_ccd_1x_exposure(Tango::WAttribute& attr);
    void read_upper_ccd_1x_trigger_mode(Tango::Attribute& attr);
    void write_upper_ccd_1x_trigger_mode(Tango::WAttribute& attr);
    void read_upper_ccd_1x_resolution(Tango::Attribute& attr);
    void write_upper_ccd_1x_resolution(Tango::WAttribute& attr);
    void read_upper_ccd_1x_width(Tango::Attribute& attr);
    void write_upper_ccd_1x_width(Tango::WAttribute& attr);
    void read_upper_ccd_1x_height(Tango::Attribute& attr);
    void write_upper_ccd_1x_height(Tango::WAttribute& attr);
    void read_upper_ccd_1x_gain(Tango::Attribute& attr);
    void write_upper_ccd_1x_gain(Tango::WAttribute& attr);
    void read_upper_ccd_1x_brightness(Tango::Attribute& attr);
    void write_upper_ccd_1x_brightness(Tango::WAttribute& attr);
    void read_upper_ccd_1x_contrast(Tango::Attribute& attr);
    void write_upper_ccd_1x_contrast(Tango::WAttribute& attr);
    void read_upper_ccd_1x_image_url(Tango::Attribute& attr);
    void read_upper_ccd_1x_last_capture_time(Tango::Attribute& attr);
    
    // 上10倍物镜CCD属性
    void read_upper_ccd_10x_state(Tango::Attribute& attr);
    void read_upper_ccd_10x_ring_light_on(Tango::Attribute& attr);
    void read_upper_ccd_10x_exposure(Tango::Attribute& attr);
    void write_upper_ccd_10x_exposure(Tango::WAttribute& attr);
    void read_upper_ccd_10x_trigger_mode(Tango::Attribute& attr);
    void write_upper_ccd_10x_trigger_mode(Tango::WAttribute& attr);
    void read_upper_ccd_10x_resolution(Tango::Attribute& attr);
    void write_upper_ccd_10x_resolution(Tango::WAttribute& attr);
    void read_upper_ccd_10x_width(Tango::Attribute& attr);
    void write_upper_ccd_10x_width(Tango::WAttribute& attr);
    void read_upper_ccd_10x_height(Tango::Attribute& attr);
    void write_upper_ccd_10x_height(Tango::WAttribute& attr);
    void read_upper_ccd_10x_gain(Tango::Attribute& attr);
    void write_upper_ccd_10x_gain(Tango::WAttribute& attr);
    void read_upper_ccd_10x_brightness(Tango::Attribute& attr);
    void write_upper_ccd_10x_brightness(Tango::WAttribute& attr);
    void read_upper_ccd_10x_contrast(Tango::Attribute& attr);
    void write_upper_ccd_10x_contrast(Tango::WAttribute& attr);
    void read_upper_ccd_10x_image_url(Tango::Attribute& attr);
    void read_upper_ccd_10x_last_capture_time(Tango::Attribute& attr);
    
    // 下1倍物镜CCD属性
    void read_lower_ccd_1x_state(Tango::Attribute& attr);
    void read_lower_ccd_1x_ring_light_on(Tango::Attribute& attr);
    void read_lower_ccd_1x_exposure(Tango::Attribute& attr);
    void write_lower_ccd_1x_exposure(Tango::WAttribute& attr);
    void read_lower_ccd_1x_trigger_mode(Tango::Attribute& attr);
    void write_lower_ccd_1x_trigger_mode(Tango::WAttribute& attr);
    void read_lower_ccd_1x_resolution(Tango::Attribute& attr);
    void write_lower_ccd_1x_resolution(Tango::WAttribute& attr);
    void read_lower_ccd_1x_width(Tango::Attribute& attr);
    void write_lower_ccd_1x_width(Tango::WAttribute& attr);
    void read_lower_ccd_1x_height(Tango::Attribute& attr);
    void write_lower_ccd_1x_height(Tango::WAttribute& attr);
    void read_lower_ccd_1x_gain(Tango::Attribute& attr);
    void write_lower_ccd_1x_gain(Tango::WAttribute& attr);
    void read_lower_ccd_1x_brightness(Tango::Attribute& attr);
    void write_lower_ccd_1x_brightness(Tango::WAttribute& attr);
    void read_lower_ccd_1x_contrast(Tango::Attribute& attr);
    void write_lower_ccd_1x_contrast(Tango::WAttribute& attr);
    void read_lower_ccd_1x_image_url(Tango::Attribute& attr);
    void read_lower_ccd_1x_last_capture_time(Tango::Attribute& attr);
    
    // 下10倍物镜CCD属性
    void read_lower_ccd_10x_state(Tango::Attribute& attr);
    void read_lower_ccd_10x_ring_light_on(Tango::Attribute& attr);
    void read_lower_ccd_10x_exposure(Tango::Attribute& attr);
    void write_lower_ccd_10x_exposure(Tango::WAttribute& attr);
    void read_lower_ccd_10x_trigger_mode(Tango::Attribute& attr);
    void write_lower_ccd_10x_trigger_mode(Tango::WAttribute& attr);
    void read_lower_ccd_10x_resolution(Tango::Attribute& attr);
    void write_lower_ccd_10x_resolution(Tango::WAttribute& attr);
    void read_lower_ccd_10x_width(Tango::Attribute& attr);
    void write_lower_ccd_10x_width(Tango::WAttribute& attr);
    void read_lower_ccd_10x_height(Tango::Attribute& attr);
    void write_lower_ccd_10x_height(Tango::WAttribute& attr);
    void read_lower_ccd_10x_gain(Tango::Attribute& attr);
    void write_lower_ccd_10x_gain(Tango::WAttribute& attr);
    void read_lower_ccd_10x_brightness(Tango::Attribute& attr);
    void write_lower_ccd_10x_brightness(Tango::WAttribute& attr);
    void read_lower_ccd_10x_contrast(Tango::Attribute& attr);
    void write_lower_ccd_10x_contrast(Tango::WAttribute& attr);
    void read_lower_ccd_10x_image_url(Tango::Attribute& attr);
    void read_lower_ccd_10x_last_capture_time(Tango::Attribute& attr);
    
    void read_image_capture_count(Tango::Attribute& attr);
    void read_auto_capture_enabled(Tango::Attribute& attr);

    // 辅助支撑属性
    void read_upper_support_position(Tango::Attribute& attr);
    void read_lower_support_position(Tango::Attribute& attr);
    void read_upper_support_state(Tango::Attribute& attr);
    void read_lower_support_state(Tango::Attribute& attr);

    // 其他属性
    void read_axis_parameter(Tango::Attribute& attr);
};

class ReflectionImagingDeviceClass : public Tango::DeviceClass {
public:
    static ReflectionImagingDeviceClass *instance();
    static ReflectionImagingDeviceClass *_instance;

    ReflectionImagingDeviceClass(std::string &class_name);
    ~ReflectionImagingDeviceClass() {};

    void attribute_factory(std::vector<Tango::Attr *> &att_list);
    void command_factory();
    void device_factory(const Tango::DevVarStringArray *devlist_ptr);
};

} // namespace ReflectionImaging

#endif // REFLECTION_IMAGING_DEVICE_H

