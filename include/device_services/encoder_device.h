#ifndef ENCODER_DEVICE_H
#define ENCODER_DEVICE_H

#include <tango.h>
#include <string>
#include <vector>
#include <array>
#include <mutex>
#include <memory>

#include "common/encoder_acquisition.h"

namespace Encoder {

const int MAX_ENCODER_CHANNELS = 20;  // 支持2个AELAN10编码器，每个10通道，共20通道

// 状态机规则枚举
enum class StateMachineRule {
    ALL_STATES,     // 所有状态都允许
    NOT_ON,         // UNKNOWN/OFF/FAULT可用，ON不可用
    NOT_UNKNOWN,    // OFF/ON/FAULT可用，UNKNOWN不可用
    ONLY_ON         // 仅ON状态可用
};

class EncoderDevice : public Tango::Device_4Impl {
public:
    EncoderDevice(Tango::DeviceClass *cl, std::string &name);
    EncoderDevice(Tango::DeviceClass *cl, const char *name);
    EncoderDevice(Tango::DeviceClass *cl, const char *name, const char *description);
    ~EncoderDevice();

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
    
    // Encoder commands (规范名称)
    Tango::DevDouble readEncoder(Tango::DevShort channel);              // 读位置
    void setEncoderResolution(const Tango::DevVarDoubleArray *argin);   // 设置分辨率
    void makeZero(Tango::DevShort channel);                             // 设置零点
    void reset(Tango::DevShort channel);                                // 复位
    void exportLogs();                                                  // 日志导出
    void exportResolution();                                            // 分辨率导出
    void simSwitch(Tango::DevShort mode);                               // 模拟运行开关

    // ========== Attribute read/write methods ==========
    void read_selfCheckResult(Tango::Attribute &attr);
    void read_positionUnit(Tango::Attribute &attr);
    void write_positionUnit(Tango::WAttribute &attr);
    void read_groupAttributeJson(Tango::Attribute &attr);
    void read_motorPos(Tango::Attribute &attr);
    void read_encoderLogs(Tango::Attribute &attr);
    void read_faultState(Tango::Attribute &attr);
    void read_encoderResolution(Tango::Attribute &attr);

    virtual void always_executed_hook();
    virtual void read_attr_hardware(std::vector<long> &attr_list);
    virtual void read_attr(Tango::Attribute &attr);
    virtual void write_attr(Tango::WAttribute &attr);

private:
    // ========== Properties (规范Property) ==========
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
    std::string device_model_;
    std::string device_position_;
    std::string device_product_date_;
    std::string device_install_date_;
    std::string encoder_resolution_prop_;  // Property JSON
    
    // 编码器采集器配置 Property
    std::vector<std::string> encoder_collector_ips_;    // encoderCollectorIPs
    std::vector<long> encoder_collector_ports_;         // encoderCollectorPorts
    long channels_per_collector_;                       // channelsPerCollector
    
    // ========== Attributes ==========
    Tango::DevLong self_check_result_;
    std::string position_unit_;
    std::string group_attribute_json_;
    std::array<double, MAX_ENCODER_CHANNELS> motor_pos_;
    std::string encoder_logs_;
    std::string fault_state_;
    std::array<double, MAX_ENCODER_CHANNELS> encoder_resolution_arr_;
    
    // Shadow variables for attribute reading to prevent pointer escaping
    Tango::DevLong attr_selfCheckResult_read;
    Tango::DevString attr_positionUnit_read;
    Tango::DevString attr_groupAttributeJson_read;
    Tango::DevString attr_encoderLogs_read;
    Tango::DevString attr_faultState_read;
    
    // Hardware connection
    bool is_connected_;
    bool sim_mode_;
    std::vector<short> axis_ids_;
    
    // Zero offset for each channel
    std::array<double, MAX_ENCODER_CHANNELS> zero_offset_;
    
    // Lock management
    bool is_locked_;
    std::string locker_info_;
    std::mutex lock_mutex_;

    // Acquisition manager
    std::unique_ptr<Common::EncoderAcquisitionManager> encoder_manager_;
    static constexpr int DATA_TIMEOUT_MS = 500; // ms
    
    void check_connection();
    void log_event(const std::string &event);
    void update_motor_positions();
    double read_position_from_cache(short channel, const std::string &context);
    double read_raw_from_cache(short channel, const std::string &context);
    bool is_valid_channel(short channel) const;  // 验证通道是否在配置的axis_ids中
    
    // JSON parsing helpers (for encoderResolution Property)
    std::string parse_json_string(const std::string& json, const std::string& key);
    double parse_json_double(const std::string& json, const std::string& key, double default_val = 1.0);
    void parse_encoder_resolution_prop();  // 解析encoderResolution Property
    
    // State machine helpers - 状态机检查
    void check_state_for_command(const std::string &cmd_name, StateMachineRule rule);
    bool is_state_allowed(StateMachineRule rule) const;
};

class EncoderDeviceClass : public Tango::DeviceClass {
public:
    static EncoderDeviceClass *instance();
    EncoderDeviceClass(std::string &name);
    ~EncoderDeviceClass();

    void command_factory();
    void attribute_factory(std::vector<Tango::Attr *> &att_list);
    void device_factory(const Tango::DevVarStringArray *dev_list);

private:
    static EncoderDeviceClass *_instance;
};

} // namespace Encoder

#endif // ENCODER_DEVICE_H
