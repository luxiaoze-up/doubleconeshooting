#ifndef LARGE_STROKE_DEVICE_H
#define LARGE_STROKE_DEVICE_H

#include "common/standard_system_device.h"
#include <tango.h>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace LargeStroke {

class LargeStrokeDevice : public Common::StandardSystemDevice {
private:
    // Lock system
    bool is_locked_;
    std::string lock_user_;         // 锁定用户名
    std::string lock_role_;         // 锁定角色名
    int lock_role_level_;           // 锁定角色等级
    std::mutex lock_mutex_;
    
    // Configuration properties
    std::string bundle_no_;
    std::string laser_no_;
    std::string system_no_;
    std::string sub_dev_list_;
    std::vector<std::string> model_list_;
    std::string current_model_;
    std::string motion_controller_connect_string_;  // MotionControllerConnectString
    std::string encoder_connect_string_;             // EncoderConnectString
    std::string error_dict_;
    std::string device_name_;
    std::string device_id_;
    std::string device_position_;
    std::string device_product_date_;
    std::string device_install_date_;
    short move_range_;
    short limit_number_;
    std::string motion_controller_name_;
    std::string encoder_name_;
    int axis_id_;
    short encoder_channel_;  // 编码器通道号（与运动控制器轴号可能不同）
    
    // Motor parameters
    double motor_step_angle_;         // motorStepAngle - 电机步距角 (默认1.8度)
    double motor_gear_ratio_;         // motorGearRatio - 齿轮比 (默认1.0)
    double motor_subdivision_;        // motorSubdivision - 细分数 (默认12800)
    
    // Driver and brake power control (NEW)
    short driver_power_port_;         // driverPowerPort - 驱动器上电端口号
    std::string driver_power_controller_; // driverPowerController - 控制驱动器上电的运动控制器名称
    short brake_power_port_;          // brakePowerPort - 刹车供电端口号
    std::string brake_power_controller_;  // brakePowerController - 控制刹车供电的运动控制器名称
    bool driver_power_enabled_;       // 驱动器电源状态
    bool brake_released_;             // 刹车释放状态
    
    // Runtime state
    double large_range_pos_;      // largeRangePos - encoder position
    double dire_pos_;             // direPos - command position
    std::string host_plug_state_; // hostPlugState - valve state
    short large_lim_org_state_;   // LargeLimOrgState
    bool large_range_state_;      // LargeRangeState - busy/idle
    // Limit fault latch: once limit is detected, hold FAULT until reset()
    std::atomic<bool> limit_fault_latched_;
    std::atomic<short> limit_fault_el_state_;  // 1=EL+, -1=EL-, 0=none
    std::string linear_logs_;     // LinearLogs
    std::string alarm_state_;     // alarmState
    std::string axis_parameter_;  // axisParameter JSON
    short result_value_;          // resultValue
    std::string position_unit_;   // positionUnit
    long self_check_result_;      // selfCheckResult: -1未自检, 0正常, 1电机异常, 2相机异常, 3光源异常, 4其他异常
    bool sim_mode_;
    
    // Unit conversion factors (可配置)
    double steps_per_mm_;         // 每毫米步数 (默认100.0)
    double steps_per_rad_;        // 每弧度步数 (默认1000.0)
    
    // Shadow variables for attribute reading to prevent pointer escaping
    Tango::DevLong attr_self_check_result_read;
    Tango::DevString attr_position_unit_read;
    Tango::DevString attr_group_attribute_json_read;
    Tango::DevString attr_host_plug_state_read;
    Tango::DevDouble attr_large_range_pos_read;
    Tango::DevDouble attr_dire_pos_read;
    Tango::DevString attr_linear_logs_read;
    Tango::DevString attr_alarm_state_read;
    Tango::DevString attr_axis_parameter_read;
    Tango::DevShort attr_large_lim_org_state_read;
    Tango::DevBoolean attr_large_range_state_read;
    Tango::DevShort attr_result_value_read;
    
    // Polling/Timing state (for periodic updates)
    std::chrono::steady_clock::time_point last_alarm_check_;
    std::chrono::steady_clock::time_point last_state_check_;
    
    // Proxies
    std::shared_ptr<Tango::DeviceProxy> motion_controller_proxy_;
    std::shared_ptr<Tango::DeviceProxy> encoder_proxy_;
    mutable std::mutex proxy_mutex_;
    
    // 连接状态标志（由后台定期更新，命令执行时只读取，不做网络操作）
    std::atomic<bool> connection_healthy_;                 // 连接是否健康

    // 当后台线程完成 motion proxy 重建后，标记需要在主线程执行"恢复动作"（上电/刹车/参数/同步）
    std::atomic<bool> motion_restore_pending_{false};
    std::atomic<int> restore_retry_count_{0};  // 恢复操作重试计数
    static constexpr int MAX_RESTORE_RETRIES = 3;  // 最大重试次数

    // 独立后台连接维护线程（避免在命令/属性请求路径里触发重连）
    std::atomic<bool> stop_connection_monitor_;
    std::thread connection_monitor_thread_;
    std::chrono::steady_clock::time_point last_reconnect_attempt_;
    
    // JSON parsing helpers
    std::string parse_json_string(const std::string& json, const std::string& key);
    int parse_json_int(const std::string& json, const std::string& key, int default_val = 0);
    
    // Unit conversion helpers
    double convert_to_steps(double value);   // Convert from positionUnit to steps
    double convert_from_steps(double steps); // Convert from steps to positionUnit
    
public:
    LargeStrokeDevice(Tango::DeviceClass *device_class, std::string &device_name);
    virtual ~LargeStrokeDevice();
    
    virtual void init_device() override;
    virtual void delete_device() override;
    
    // ===== COMMANDS =====
    // Lock/Unlock (1-5)
    void devLock();
    void devUnlock(Tango::DevBoolean unlock_all);
    void devLockVerify();
    Tango::DevString devLockQuery();
    void devUserConfig();
    
    // System (6-7)
    void selfCheck();
    void init();
    
    // Parameter (8-9)
    void moveAxisSet(const Tango::DevVarDoubleArray *params);
    void structAxisSet(const Tango::DevVarDoubleArray *params);
    
    // Motion (10-13)
    void moveRelative(Tango::DevDouble distance);
    void moveAbsolute(Tango::DevDouble position);
    void stop();
    void reset();
    
    // Read (14, 18-19)
    Tango::DevDouble readEncoder();
    Tango::DevBoolean readOrg();
    Tango::DevShort readEL();
    
    // Auto/Valve (15-17, 20)
    void largeMoveAuto();
    void openValue(Tango::DevShort state);
    Tango::DevBoolean plugInRead();
    void runAction(Tango::DevShort action);
    
    // Export (21-23)
    void exportLogs();           // 日志导出
    Tango::DevString readtAxis();
    void exportAxis();
    
    // Simulation
    void simSwitch(Tango::DevShort mode);
    
    // Power Control Commands (for GUI)
    void enableDriverPower();         // 手动启动驱动器电源
    void disableDriverPower();        // 手动关闭驱动器电源
    void releaseBrake();              // 手动释放刹车
    void engageBrake();               // 手动启用刹车
    Tango::DevString queryPowerStatus();  // 查询电源状态（返回JSON）
    
    // State machine check
    void check_state_for_command(const std::string& cmd_name, 
                                 bool require_on = false,
                                 bool allow_unknown = false);
    void check_state_not_on(const std::string& cmd_name);  // NOT_ON规则: UNKNOWN/OFF/FAULT可用
    
    // Quick connection check (for fast failure detection)
    bool quick_check_connection();  // 快速检查连接状态，避免等待超时
    
    // ===== ATTRIBUTES =====
    virtual void read_attr(Tango::Attribute &attr) override;
    virtual void write_attr(Tango::WAttribute &attr);  // 处理可写属性
    void read_self_check_result(Tango::Attribute &attr);
    void read_position_unit(Tango::Attribute &attr);
    void write_position_unit(Tango::WAttribute &attr);
    void read_group_attribute_json(Tango::Attribute &attr);
    void read_host_plug_state(Tango::Attribute &attr);
    void read_large_range_pos(Tango::Attribute &attr);
    void read_dire_pos(Tango::Attribute &attr);
    void read_linear_logs(Tango::Attribute &attr);
    void read_alarm_state(Tango::Attribute &attr);
    void read_axis_parameter(Tango::Attribute &attr);
    void read_large_lim_org_state(Tango::Attribute &attr);
    void read_large_range_state(Tango::Attribute &attr);
    void read_result_value(Tango::Attribute &attr);
    void read_driver_power_status(Tango::Attribute &attr);
    void read_brake_status(Tango::Attribute &attr);
    
    // Hooks
    virtual void specific_self_check() override;
    virtual void always_executed_hook() override;
    virtual void read_attr_hardware(std::vector<long> &attr_list) override;
    
private:
    void connect_proxies();
    void log_event(const std::string &event);
    void update_position();

    // Aggressive refactor: proxy rebuild + post-reconnect restore are centralized.
    void rebuild_motion_proxy(int timeout_ms);
    void rebuild_encoder_proxy(int timeout_ms);
    void perform_post_motion_reconnect_restore();

    // Proxy access helpers (lifetime-safe)
    std::shared_ptr<Tango::DeviceProxy> get_motion_controller_proxy();
    std::shared_ptr<Tango::DeviceProxy> get_encoder_proxy();
    void reset_motion_controller_proxy();
    void reset_encoder_proxy();

    // Background connection monitor
    void start_connection_monitor();
    void stop_connection_monitor();
    void connection_monitor_loop();
    
    // Power control methods (NEW)
    bool enable_driver_power();       // 启动驱动器电源
    bool disable_driver_power();      // 关闭驱动器电源
    bool release_brake();             // 释放刹车
    bool engage_brake();              // 启用刹车
};

class LargeStrokeDeviceClass : public Tango::DeviceClass {
public:
    static LargeStrokeDeviceClass *instance();
    
protected:
    LargeStrokeDeviceClass(std::string &class_name);
    
public:
    virtual void attribute_factory(std::vector<Tango::Attr *> &att_list) override;
    virtual void command_factory() override;
    virtual void device_factory(const Tango::DevVarStringArray *devlist_ptr) override;
    
private:
    static LargeStrokeDeviceClass *_instance;
};

} // namespace LargeStroke

#endif // LARGE_STROKE_DEVICE_H
