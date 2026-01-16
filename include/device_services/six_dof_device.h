#ifndef SIX_DOF_DEVICE_H
#define SIX_DOF_DEVICE_H

#include "common/standard_system_device.h"
#include "common/kinematics.h"
#include <tango.h>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <atomic>

namespace SixDof {

const int NUM_AXES = 6;

class SixDofDevice : public Common::StandardSystemDevice {
private:
    // Lock system
    bool is_locked_;
    std::string lock_user_;
    std::mutex lock_mutex_;
    
    // Configuration properties (from Property sheet)
    std::string bundle_no_;           // bundleNo
    std::string laser_no_;            // laserNo
    std::string system_no_;           // systemNo
    std::string sub_dev_list_;        // subDevList
    std::vector<std::string> model_list_;  // modelList
    std::string current_model_;       // currentModel
    std::string connect_string_;      // connectString
    std::string error_dict_;          // errorDict
    std::string device_name_;         // deviceName
    std::string device_id_;           // deviceID
    std::string device_position_;     // devicePosition
    std::string device_product_date_; // deviceProductDate
    std::string device_install_date_; // deviceInstallDate
    std::vector<short> move_range_;   // moveRange
    short limit_number_;              // limitNumber
    std::string sdof_config_;         // sdofConfig (Stewart platform configuration)
    std::string motion_controller_name_; // motionControllerName
    std::string encoder_name_;        // encoderName
    std::vector<short> encoder_channels_;  // encoderChannels - 每个轴对应的编码器通道号
    
    // Motor parameters
    double motor_step_angle_;         // motorStepAngle - 电机步距角 (默认1.8度)
    double motor_gear_ratio_;         // motorGearRatio - 齿轮比 (默认1.0)
    double motor_subdivision_;        // motorSubdivision - 细分数 (六自由度默认12800)
    
    // Driver and brake power control (NEW)
    short driver_power_port_;         // driverPowerPort - 驱动器上电端口号
    std::string driver_power_controller_; // driverPowerController - 控制驱动器上电的运动控制器名称
    short brake_power_port_;          // brakePowerPort - 刹车供电端口号
    std::string brake_power_controller_;  // brakePowerController - 控制刹车供电的运动控制器名称
    bool driver_power_enabled_;       // 驱动器电源状态
    bool brake_released_;             // 刹车释放状态
    
    // Runtime state
    std::array<double, NUM_AXES> axis_pos_;        // axisPos - encoder positions
    std::array<double, NUM_AXES> dire_pos_;        // direPos - command positions
    std::array<double, NUM_AXES> six_freedom_pose_; // sixFreedomPose [X,Y,Z,ThetaX,ThetaY,ThetaZ]
    std::array<double, NUM_AXES> current_leg_lengths_; // 当前leg长度（用运动学normalleg初始化）
    std::array<double, NUM_AXES> normal_leg_lengths_; // 初始leg长度
    bool open_brake_state_;                         // openBrakeState
    std::array<short, NUM_AXES> lim_org_state_;    // limOrgState
    std::array<bool, NUM_AXES> sdof_state_;        // sdofState - axis busy/idle

    // Limit fault latch: once limit is detected, hold FAULT until reset
    std::atomic<bool> limit_fault_latched_{false};
    std::atomic<short> limit_fault_axis_{-1};      // 0-5
    std::atomic<short> limit_fault_el_state_{0};   // 1=EL+, -1=EL-, 0=none
    std::string six_logs_;                          // sixLogs
    std::string alarm_state_;                       // alarmState
    std::string axis_parameter_;                    // axisParameter JSON
    short result_value_;                            // resultValue (0=success, 1=fail)
    std::string position_unit_;                     // positionUnit
    long self_check_result_;                        // selfCheckResult
    bool sim_mode_;
    
    // Shadow variables for attribute reading to prevent pointer escaping
    Tango::DevLong attr_selfCheckResult_read;
    Tango::DevString attr_positionUnit_read;
    Tango::DevString attr_groupAttributeJson_read;
    Tango::DevString attr_sixLogs_read;
    Tango::DevString attr_alarmState_read;
    Tango::DevString attr_axisParameter_read;
    Tango::DevBoolean attr_sdofState_read[NUM_AXES];
    Tango::DevShort attr_resultValue_read;
    Tango::DevBoolean attr_openBrakeState_read;
    
    // Proxies
    std::shared_ptr<Tango::DeviceProxy> motion_controller_proxy_;
    std::shared_ptr<Tango::DeviceProxy> encoder_proxy_;
    mutable std::mutex proxy_mutex_;  // 保护 proxy 访问的互斥锁
    
    // Kinematics
    std::unique_ptr<Common::StewartPlatformKinematics> kinematics_;
    
    static const double POS_LIMIT;
    static const double ROT_LIMIT;
    
public:
    SixDofDevice(Tango::DeviceClass *device_class, std::string &device_name);
    virtual ~SixDofDevice();
    
    virtual void init_device() override;
    virtual void delete_device() override;
    
    // ===== COMMANDS (from Command sheet) =====
    // Lock/Unlock commands (1-5)
    void devLock();
    void devUnlock(Tango::DevBoolean unlock_all);
    void devLockVerify();
    Tango::DevString devLockQuery();
    void devUserConfig();
    
    // System commands (6-7)
    void selfCheck();
    void init();
    
    // Motion parameter commands (8-9)
    void moveAxisSet(const Tango::DevVarDoubleArray *params);      // Start/Max/Acc/Dec/Stop speed
    void structAxisSet(const Tango::DevVarDoubleArray *params);    // StepAngle/GearRatio/Subdivision
    
    // Pose motion commands (10, 12)
    void movePoseRelative(const Tango::DevVarDoubleArray *pose);   // 6DOF pose relative (铰点面pose，非靶点)
    void movePoseAbsolute(const Tango::DevVarDoubleArray *pose);   // 6DOF pose absolute (铰点面pose，非靶点)
    void movePosePvt(Tango::DevString argin);                      // 6DOF PVT trajectory motion
    
    // Reset/Zero commands (13-14)
    void reset();                    // Override from base
    void sixMoveZero();              // Home all axes
    void singleReset(Tango::DevShort axis);  // Reset single axis
    
    // Single axis motion commands (16-17)
    void singleMoveRelative(const Tango::DevVarDoubleArray *params); // [axis, distance]
    void singleMoveAbsolute(const Tango::DevVarDoubleArray *params); // [axis, position]
    
    // Read commands (18-20)
    Tango::DevVarDoubleArray* readEncoder();   // Read all encoder positions
    Tango::DevBoolean readOrg(Tango::DevShort axis);  // Check if axis at origin
    Tango::DevShort readEL(Tango::DevShort axis);     // Read limit switch state
    
    // Control commands (21-22)
    void stop();                     // Stop all axes
    void openBrake(Tango::DevBoolean open);  // Open/Close brake
    
    // Parameter commands (24-25)
    Tango::DevString readtAxis();    // Read axis parameters as JSON
    void exportAxis();               // Export parameters to file

    // Simulation
    void simSwitch(Tango::DevShort mode);
    
    // Database commands (NEW)
    void saveEncoderPositions();     // 保存当前编码器值到数据库axis_pos属性
    
    // Power Control Commands (NEW - for GUI)
    void enableDriverPower();         // 手动启动驱动器电源
    void disableDriverPower();        // 手动关闭驱动器电源
    void releaseBrake();              // 手动释放刹车
    void engageBrake();               // 手动启用刹车
    Tango::DevString queryPowerStatus();  // 查询电源状态（返回JSON）
    
    // ===== ATTRIBUTE READS (from Attribute sheet) =====
    virtual void read_attr(Tango::Attribute &attr) override;
    virtual void write_attr(Tango::WAttribute &attr);
    void read_self_check_result(Tango::Attribute &attr);
    void read_position_unit(Tango::Attribute &attr);
    void write_position_unit(Tango::WAttribute &attr);
    void read_group_attribute_json(Tango::Attribute &attr);
    void read_axis_pos(Tango::Attribute &attr);
    void read_dire_pos(Tango::Attribute &attr);
    void read_open_brake_state(Tango::Attribute &attr);
    void read_six_freedom_pose(Tango::Attribute &attr);
    void read_six_logs(Tango::Attribute &attr);
    void read_alarm_state(Tango::Attribute &attr);
    void read_axis_parameter(Tango::Attribute &attr);
    void read_lim_org_state(Tango::Attribute &attr);
    void read_sdof_state(Tango::Attribute &attr);
    void read_result_value(Tango::Attribute &attr);
    void read_driver_power_status(Tango::Attribute &attr);   // 驱动器电源状态
    void read_brake_status(Tango::Attribute &attr);          // 刹车状态
    
    // Inherited hooks
    virtual void specific_self_check() override;
    virtual void always_executed_hook() override;
    virtual void read_attr_hardware(std::vector<long> &attr_list) override;
    
private:
    void connect_proxies();
    // Background connection monitor (LargeStroke-style)
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

    bool validate_pose(const std::array<double, NUM_AXES> &pose);
    void update_axis_positions();
    void update_pose_from_encoders();
    void log_event(const std::string &event);
    void send_move_command(int axis, int position, bool relative);
    void configure_kinematics();
    void check_state(const std::string& cmd_name);
    
    // Power control methods (NEW)
    bool enable_driver_power();       // 启动驱动器电源
    bool disable_driver_power();      // 关闭驱动器电源
    bool release_brake();             // 释放刹车
    bool engage_brake();              // 启用刹车

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
};

class SixDofDeviceClass : public Tango::DeviceClass {
public:
    static SixDofDeviceClass *instance();
    
protected:
    SixDofDeviceClass(std::string &class_name);
    
public:
    virtual void attribute_factory(std::vector<Tango::Attr *> &att_list) override;
    virtual void command_factory() override;
    virtual void device_factory(const Tango::DevVarStringArray *devlist_ptr) override;
    
private:
    static SixDofDeviceClass *_instance;
};

} // namespace SixDof

#endif // SIX_DOF_DEVICE_H
