#ifndef AUXILIARY_SUPPORT_DEVICE_H
#define AUXILIARY_SUPPORT_DEVICE_H

#include "common/standard_system_device.h"
#include <tango.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <atomic>
#include <chrono>

namespace AuxiliarySupport {

// AuxiliarySupportDevice implements 辅助支撑类 (Auxiliary Support) interface
class AuxiliarySupportDevice : public Common::StandardSystemDevice {
private:
    // Lock system
    bool is_locked_;
    std::string lock_user_;
    std::mutex lock_mutex_;
    
    // ===== 固有状态属性 (Property - Sheet: Property) =====
    std::string bundle_no_;           // bundleNo - 束组编码
    std::string laser_no_;            // laserNo - 子束编码
    std::string system_no_;           // systemNo - 系统/分系统编码
    std::string sub_dev_list_;        // subDevList - 关联设备名 (JSON数组)
    std::vector<std::string> model_list_;  // modelList - 支持型号列表
    std::string current_model_;       // currentModel - 当前型号
    std::string connect_string_;      // connectString - 设备连接信息 (JSON)
    std::string error_dict_;          // errorDict - 错误编码信息 (JSON)
    std::string device_name_;         // deviceName - 设备名称
    std::string device_id_;           // deviceID - 设备编号
    std::string device_position_;     // devicePosition - 设备安装位置描述
    std::string device_product_date_; // deviceProductDate - 设备出厂日期
    std::string device_install_date_; // deviceInstallDate - 设备上线日期
    
    // Configuration
    std::string motion_controller_name_;  // motionControllerName - 下层运动控制器服务名
    std::string encoder_name_;            // encoderName - 下层绝对编码器服务名
    short encoder_channel_;               // encoderChannel - 编码器采集器通道号
    std::vector<long> axis_ids_;
    short move_range_;            // moveRange - 行程
    short limit_number_;          // limitNumber - 限位开关数量
    short force_range_;           // forceRange - 力范围
    double hold_pos_;             // holdPos - 加持位置
    
    // Motor parameters
    double motor_step_angle_;         // motorStepAngle - 电机步距角 (默认1.8度)
    double motor_gear_ratio_;         // motorGearRatio - 齿轮比 (默认1.0)
    double motor_subdivision_;        // motorSubdivision - 细分数 (默认12800)
    
    // Driver power control (NEW)
    short driver_power_port_;         // driverPowerPort - 驱动器上电端口号
    std::string driver_power_controller_; // driverPowerController - 控制驱动器上电的运动控制器名称
    bool driver_power_enabled_;       // 驱动器电源状态
    
    // Support type configuration (新增)
    std::string support_type_;            // supportType - 支撑类型: "ray", "reflection", "targeting"
    std::string support_position_;        // supportPosition - 支撑位置: "upper", "lower", "single"
    std::string support_orientation_;     // supportOrientation - 支撑方向: "vertical", "oblique"
    short force_sensor_channel_;          // forceSensorChannel - 力传感器通道号
    std::string force_sensor_controller_; // forceSensorController - 力传感器控制器名称（可选，为空则使用motionControllerName）
    double force_sensor_scale_;           // forceSensorScale - 力传感器转换系数（电压→力的乘数，默认1000）
    double force_sensor_offset_;          // forceSensorOffset - 力传感器零点偏移（默认0）
    
    // Runtime state
    double token_assist_pos_;     // tokenAssistPos - encoder position
    double dire_pos_;             // direPos - command position
    double target_force_;         // targetForce - force value
    short assist_lim_org_state_;  // AssistLimOrgState
    bool assist_state_;           // AssistState - busy/idle
    std::string support_logs_;    // supportLogs
    std::string fault_state_;     // faultState
    std::string axis_parameter_;  // axisParameter JSON
    short result_value_;          // resultValue
    std::string position_unit_;   // positionUnit
    long self_check_result_;      // selfCheckResult
    bool sim_mode_;

    // Limit fault latch: once limit is detected, hold FAULT until reset
    std::atomic<bool> limit_fault_latched_{false};
    std::atomic<short> limit_fault_el_state_{0};   // 1=EL+, -1=EL-, 0=none
    
    // Proxies
    std::shared_ptr<Tango::DeviceProxy> motion_controller_proxy_;
    std::shared_ptr<Tango::DeviceProxy> encoder_proxy_;
    mutable std::mutex proxy_mutex_;  // 保护 proxy 访问的互斥锁
    
public:
    AuxiliarySupportDevice(Tango::DeviceClass *device_class, std::string &device_name);
    virtual ~AuxiliarySupportDevice();
    
    virtual void init_device() override;
    virtual void delete_device() override;
    
    // ===== COMMANDS =====
    // Lock/Unlock (1-5)
    void devLock(Tango::DevString user_info);
    void devUnlock(Tango::DevBoolean unlock_all);
    void devLockVerify();
    Tango::DevString devLockQuery();
    void devUserConfig(Tango::DevString config);
    
    // System (6-8)
    void selfCheck();
    void init();
    void reset();
    
    // Parameter (9-10)
    void moveAxisSet(const Tango::DevVarDoubleArray *params);
    void structAxisSet(const Tango::DevVarDoubleArray *params);

    // Simulation
    void simSwitch(Tango::DevShort mode);
    
    // Power Control Commands (for GUI)
    void enableDriverPower();         // 手动启动驱动器电源
    void disableDriverPower();        // 手动关闭驱动器电源
    Tango::DevString queryPowerStatus();  // 查询电源状态（返回JSON）
    
    // Motion (11-13)
    void moveRelative(Tango::DevDouble distance);
    void moveAbsolute(Tango::DevDouble position);
    void stop();
    
    // Read (14, 17, 20-21)
    Tango::DevDouble readEncoder();
    Tango::DevDouble readForce();
    Tango::DevBoolean readOrg();
    Tango::DevShort readEL();
    
    // Auto/Force (15-16, 18)
    void assistAuto(Tango::DevDouble position);
    void setHoldPos(Tango::DevDouble position);
    void setForceZero();
    
    // Export (22-23)
    Tango::DevString readtAxis();
    void exportAxis();
    
    // ===== ATTRIBUTES =====
    virtual void read_attr(Tango::Attribute &attr) override;
    virtual void write_attr(Tango::WAttribute &attr);
    void read_self_check_result(Tango::Attribute &attr);
    void read_position_unit(Tango::Attribute &attr);
    void write_position_unit(Tango::WAttribute &attr);
    void read_group_attribute_json(Tango::Attribute &attr);
    void read_token_assist_pos(Tango::Attribute &attr);
    void read_dire_pos(Tango::Attribute &attr);
    void read_target_force(Tango::Attribute &attr);
    void read_force_value(Tango::Attribute &attr);  // 实际力传感器值（通过readForce读取）
    void read_support_logs(Tango::Attribute &attr);
    void read_fault_state(Tango::Attribute &attr);
    void read_axis_parameter(Tango::Attribute &attr);
    void read_assist_lim_org_state(Tango::Attribute &attr);
    void read_assist_state(Tango::Attribute &attr);
    void read_result_value(Tango::Attribute &attr);
    void read_driver_power_status(Tango::Attribute &attr);
    
    // ===== 固有状态属性读取 =====
    void read_bundle_no(Tango::Attribute &attr);
    void read_laser_no(Tango::Attribute &attr);
    void read_system_no(Tango::Attribute &attr);
    void read_sub_dev_list(Tango::Attribute &attr);
    void read_model_list(Tango::Attribute &attr);
    void read_current_model(Tango::Attribute &attr);
    void read_connect_string(Tango::Attribute &attr);
    void read_error_dict(Tango::Attribute &attr);
    void read_device_name_attr(Tango::Attribute &attr);
    void read_device_id(Tango::Attribute &attr);
    void read_device_position(Tango::Attribute &attr);
    void read_device_product_date(Tango::Attribute &attr);
    void read_device_install_date(Tango::Attribute &attr);
    
    // Support type attributes (新增)
    void read_support_type(Tango::Attribute &attr);
    void read_support_position(Tango::Attribute &attr);
    void read_support_orientation(Tango::Attribute &attr);
    void read_force_sensor_channel(Tango::Attribute &attr);
    
    // Export logs command (状态机24)
    void exportLogs();
    
    // Hooks
    virtual void specific_self_check() override;
    virtual void always_executed_hook() override;
    virtual void read_attr_hardware(std::vector<long> &attr_list) override;
    
private:
    void connect_proxies();
    // Background connection monitor (LargeStroke-style: command path reads connection_healthy_ only)
    void start_connection_monitor();
    void stop_connection_monitor();
    void connection_monitor_loop();
    
    // Proxy access helpers (lifetime-safe, LargeStroke-style)
    std::shared_ptr<Tango::DeviceProxy> get_motion_controller_proxy();
    std::shared_ptr<Tango::DeviceProxy> get_encoder_proxy();
    void reset_motion_controller_proxy();
    void reset_encoder_proxy();
    void rebuild_motion_controller_proxy(int timeout_ms);
    void rebuild_encoder_proxy(int timeout_ms);
    void perform_post_motion_reconnect_restore();  // 重连后恢复配置和状态

    void log_event(const std::string &event);
    void update_position();
    void update_device_name_and_id();  // 根据supportType和supportPosition更新deviceName和deviceID
    
    // Power control methods (NEW)
    bool enable_driver_power();       // 启动驱动器电源
    bool disable_driver_power();      // 关闭驱动器电源

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
    
    // ===== 状态机检查辅助函数 =====
    // 检查命令在当前状态下是否允许执行
    void check_state_for_lock_commands();      // devLock, devUnlock: UNKNOWN, OFF, FAULT
    void check_state_for_all_states();         // devLockVerify, devLockQuery, devUserConfig: 所有状态
    void check_state_for_init_commands();      // selfCheck, init: UNKNOWN, OFF, FAULT
    void check_state_for_reset();              // reset: ON, FAULT
    void check_state_for_param_commands();     // moveAxisSet, structAxisSet: OFF, ON, FAULT
    void check_state_for_motion_commands();    // moveRelative, moveAbsolute, assistAuto, setHoldPos: ON only
    void check_state_for_operational_commands();  // Stop, readEncoder, readForce, readOrg, readEL, setForceZero, etc.: OFF, ON, MOVING, FAULT (not UNKNOWN)

    // Tango 影子变量 (确保属性读取时的指针寿命)
    Tango::DevString attr_position_unit_read;
    Tango::DevString attr_support_logs_read;
    Tango::DevString attr_fault_state_read;
    Tango::DevString attr_bundle_no_read;
    Tango::DevString attr_laser_no_read;
    Tango::DevString attr_system_no_read;
    Tango::DevString attr_sub_dev_list_read;
    Tango::DevString attr_model_list_read;
    Tango::DevString attr_current_model_read;
    Tango::DevString attr_connect_string_read;
    Tango::DevString attr_error_dict_read;
    Tango::DevString attr_device_name_read;
    Tango::DevString attr_device_id_read;
    Tango::DevString attr_device_position_read;
    Tango::DevString attr_device_product_date_read;
    Tango::DevString attr_device_install_date_read;
    Tango::DevString attr_support_type_read;
    Tango::DevString attr_support_position_read;
    Tango::DevString attr_support_orientation_read;
    Tango::DevString attr_group_attribute_json_read;
    Tango::DevString attr_axis_parameter_read;
};

class AuxiliarySupportDeviceClass : public Tango::DeviceClass {
public:
    static AuxiliarySupportDeviceClass *instance();
    
protected:
    AuxiliarySupportDeviceClass(std::string &class_name);
    
public:
    virtual void attribute_factory(std::vector<Tango::Attr *> &att_list) override;
    virtual void command_factory() override;
    virtual void device_factory(const Tango::DevVarStringArray *devlist_ptr) override;
    
private:
    static AuxiliarySupportDeviceClass *_instance;
};

} // namespace AuxiliarySupport

#endif // AUXILIARY_SUPPORT_DEVICE_H
