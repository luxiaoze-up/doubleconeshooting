#ifndef MOTION_CONTROLLER_DEVICE_H
#define MOTION_CONTROLLER_DEVICE_H

#include <tango.h>
#include <string>
#include <vector>
#include <array>
#include <mutex>
#include <set>
#include <chrono>

namespace MotionController {

// Maximum number of axes supported
const int MAX_AXES = 8;
const int MAX_AD_CHANNELS = 3;
const int MAX_IO_CHANNELS = 16;

class MotionControllerDevice : public Tango::Device_4Impl {
public:
    MotionControllerDevice(Tango::DeviceClass *cl, std::string &name);
    MotionControllerDevice(Tango::DeviceClass *cl, const char *name);
    MotionControllerDevice(Tango::DeviceClass *cl, const char *name, const char *description);
    ~MotionControllerDevice();

    void init_device();
    void delete_device();

    // ========== Standard Commands (规范名称) ==========
    // Lock/Unlock commands
    void devLock(Tango::DevString argin);
    void devUnlock(Tango::DevBoolean argin);
    void devLockVerify();
    Tango::DevString devLockQuery();
    void devUserConfig(Tango::DevString argin);
    
    // System commands
    void selfCheck();
    void init();
    void connect();
    void disconnect();
    void reset(Tango::DevShort axis_id);
    
    // Motion commands (规范名称)
    void moveZero(Tango::DevShort axis_id);                          // 回零
    void moveRelative(const Tango::DevVarDoubleArray *argin);        // 相对运动 [axis_id, distance]
    void moveAbsolute(const Tango::DevVarDoubleArray *argin);        // 绝对运动 [axis_id, position]
    void stopMove(Tango::DevShort axis_id);                          // 停止
    
    // PVTS motion commands
    void setPvts(Tango::DevString argin);                            // PVTS参数设置
    void movePvts(Tango::DevString argin);                           // PVTS运动
    
    // Parameter configuration commands (规范名称)
    void setMoveParameter(const Tango::DevVarDoubleArray *argin);    // 电机运动属性设置
    void setStructParameter(const Tango::DevVarDoubleArray *argin);  // 电机结构属性设置
    void setAnalog(const Tango::DevVarDoubleArray *argin);           // 模拟量输出值设置
    
    // Status query commands (规范名称)
    Tango::DevShort checkMoveState(Tango::DevShort axis_id);         // 检测轴运动状态
    Tango::DevBoolean readOrg(Tango::DevShort axis_id);              // 原点状态读取
    Tango::DevShort readEL(Tango::DevShort axis_id);                 // 限位状态读取
    Tango::DevDouble readPos(Tango::DevShort axis_id);               // 读取当前位置
    
    // Encoder commands
    void setEncoderPosition(const Tango::DevVarDoubleArray *argin);  // 设置编码器位置 [axis_id, position]
    
    // IO commands
    Tango::DevShort readIO(Tango::DevShort port);                    // 通用IO读
    void writeIO(const Tango::DevVarDoubleArray *argin);             // 通用IO写
    Tango::DevDouble readAD(Tango::DevShort channel);                // 模拟量读入
    void writeAD(const Tango::DevVarDoubleArray *argin);             // 模拟量输出
    
    // Utility commands
    void exportAxis();                                                // 轴参数导出
    void simSwitch(Tango::DevShort mode);                            // 模拟运行开关
    Tango::DevString errorParse(Tango::DevShort error_code);         // 错误码解析
    
    // Interlock interface - 联锁接口
    void setDisabled(bool disabled);                                  // 设置禁用状态（由联锁服务调用）
    bool isDisabled() const { return is_disabled_; }                  // 查询禁用状态

    // ========== Attribute read/write methods ==========
    void read_selfCheckResult(Tango::Attribute &attr);
    void read_positionUnit(Tango::Attribute &attr);
    void write_positionUnit(Tango::WAttribute &attr);
    void read_groupAttributeJson(Tango::Attribute &attr);
    void read_controllerLogs(Tango::Attribute &attr);
    void read_faultState(Tango::Attribute &attr);
    void read_motorPos(Tango::Attribute &attr);
    void read_structParameter(Tango::Attribute &attr);
    void read_moveParameter(Tango::Attribute &attr);
    void read_analogOutValue(Tango::Attribute &attr);
    void read_analogInValue(Tango::Attribute &attr);
    void read_genericIoInputValue(Tango::Attribute &attr);
    void read_specialLocationValue(Tango::Attribute &attr);
    void read_axisStatus(Tango::Attribute &attr);
    void read_resultValue(Tango::Attribute &attr);

    virtual void always_executed_hook();
    virtual void read_attr_hardware(std::vector<long> &attr_list);
    virtual void read_attr(Tango::Attribute &attr);
    virtual void write_attr(Tango::WAttribute &attr);

private:
    // ========== Properties (规范Property) ==========
    // Standard properties
    std::string bundle_no_;           // bundleNo
    std::string laser_no_;            // laserNo
    std::string system_no_;           // systemNo
    std::string sub_dev_list_;        // subDevList (JSON array)
    std::vector<std::string> model_list_;  // modelList
    std::string current_model_;       // currentModel
    std::string connect_string_;      // connectString (JSON)
    std::string error_dict_;          // errorDict (JSON)
    
    // Device info properties
    std::string device_name_;         // deviceName
    std::string device_id_;           // deviceID
    std::string device_model_;        // deviceModel
    std::string device_position_;     // devicePosition
    std::string device_product_date_; // deviceProductDate
    std::string device_install_date_; // deviceInstallDate
    std::string controller_property_; // controllerProperty (JSON)
    std::string analog_output_value_prop_; // analogOutputValue (JSON)
    std::string struct_parameter_prop_;    // structParameter (JSON)
    std::string is_brake_;            // isBrake (JSON)
    std::string move_parameter_prop_; // moveParameter (JSON)
    
    // ========== Attributes (规范Attribute) ==========
    Tango::DevLong self_check_result_;       // selfCheckResult
    std::string position_unit_;              // positionUnit
    std::string group_attribute_json_;       // groupAttributeJson
    std::string controller_logs_;            // controllerLogs
    std::string fault_state_;                // faultState
    std::array<double, MAX_AXES> motor_pos_; // motorPos
    std::string struct_parameter_attr_;      // structParameter (Attribute)
    std::string move_parameter_attr_;        // moveParameter (Attribute)
    std::array<double, MAX_AD_CHANNELS> analog_out_value_;  // analogOutValue
    std::array<double, MAX_AD_CHANNELS> analog_in_value_;   // analogInValue
    std::array<double, MAX_IO_CHANNELS> generic_io_value_;  // genericIoInputValue
    std::array<double, MAX_AXES> special_location_value_;   // specialLocationValue
    std::array<bool, MAX_AXES> axis_status_;                // axisStaus
    Tango::DevShort result_value_;           // resultValue
    bool is_disabled_;                       // 是否被联锁禁用（移至此处以匹配初始化顺序）
    
    // Shadow variables for attribute reading to prevent pointer escaping
    Tango::DevLong attr_selfCheckResult_read;
    Tango::DevString attr_positionUnit_read;
    Tango::DevString attr_groupAttributeJson_read;
    Tango::DevString attr_controllerLogs_read;
    Tango::DevString attr_faultState_read;
    Tango::DevString attr_structParameter_read;
    Tango::DevString attr_moveParameter_read;
    Tango::DevBoolean attr_axisStatus_read[MAX_AXES];
    Tango::DevShort attr_resultValue_read;
    
    // ========== Hardware connection ==========
    std::string controller_ip_;
    int card_id_;
    bool is_connected_;
    bool sim_mode_;                          // 模拟运行模式
    
    // Lock management
    bool is_locked_;
    std::string locker_info_;
    std::mutex lock_mutex_;
    
    // State machine - track moving axes
    std::set<short> moving_axes_;           // 当前正在运动的轴集合
    std::mutex state_mutex_;                 // 状态机互斥锁
    
    // ========== Reconnection mechanism ==========
    int reconnect_attempts_;                 // 当前重连尝试次数
    int max_reconnect_attempts_;             // 最大重连尝试次数
    int reconnect_interval_ms_;              // 重连间隔(毫秒)
    std::chrono::steady_clock::time_point last_reconnect_time_;  // 上次重连时间
    bool reconnect_in_progress_;             // 是否正在重连中
    int health_check_interval_count_;        // 健康检查计数器
    static const int HEALTH_CHECK_INTERVAL = 100;  // 每100次hook调用检查一次连接健康
    
    // Internal helpers
    void check_connection();
    void check_error(short error_code, const std::string &context);
    void update_motor_positions();
    void update_axis_status();
    void log_event(const std::string &event);
    
    // Reconnection helpers
    bool try_reconnect();                    // 尝试重连，返回是否成功
    bool check_connection_health();          // 检查连接健康状态
};

class MotionControllerDeviceClass : public Tango::DeviceClass {
public:
    static MotionControllerDeviceClass *instance();
    MotionControllerDeviceClass(std::string &name);
    ~MotionControllerDeviceClass();

    void command_factory();
    void attribute_factory(std::vector<Tango::Attr *> &att_list);
    void device_factory(const Tango::DevVarStringArray *dev_list);

private:
    static MotionControllerDeviceClass *_instance;
};

} // namespace MotionController

#endif // MOTION_CONTROLLER_DEVICE_H
