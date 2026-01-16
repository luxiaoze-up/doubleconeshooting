#ifndef INTERLOCK_SERVICE_H
#define INTERLOCK_SERVICE_H

#include "common/standard_system_device.h"
#include <tango.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <map>

namespace Interlock {

// 联锁条件类型
enum class InterlockType {
    POSITION_LIMIT,         // 位置限制：A位置超限时B不能运动
    POSITION_MUTUAL,        // 双向位置联锁：A超限限制B，B非零位限制A
    RANGE_CONDITIONAL,      // 条件范围限制：A在某范围时B限制不同
    VALVE_OPERATION,        // 阀门操作联锁：位置超限时不能关闭阀门
    SHIELD_OPERATION,       // 屏蔽罩操作联锁：位置超限时不能操作屏蔽罩
    VACUUM_PRESSURE,        // 真空压力联锁
    DEVICE_STATE            // 设备状态联锁
};

// 联锁动作类型
enum class InterlockAction {
    STOP,                   // 停止运动
    ALARM,                  // 报警
    WARNING,                // 警告
    BLOCK_MOTION,           // 阻止运动
    BLOCK_VALVE_CLOSE,      // 阻止关闭阀门
    BLOCK_SHIELD_OPERATE    // 阻止操作屏蔽罩
};

// 设备标识符常量
namespace DeviceId {
    // 哈工大设备
    constexpr const char* SONGBA_LARGE_STROKE = "songba_large_stroke";           // 送靶大行程
    constexpr const char* BACKLIGHT_LARGE_STROKE = "backlight_large_stroke";     // 背光大行程
    constexpr const char* TARGET_TRANSFER = "target_transfer";                    // 靶件转移大行程
    constexpr const char* XY_DETECTION = "xy_detection";                          // X/γ探测大行程
    constexpr const char* LIFTING_MECHANISM = "lifting_mechanism";                // 升降机构
    constexpr const char* UPPER_CHAR_SUPPORT = "upper_char_support";              // 上表征辅助支撑
    constexpr const char* LOWER_CHAR_SUPPORT = "lower_char_support";              // 下表征辅助支撑
    constexpr const char* SHOOTING_SUPPORT = "shooting_support";                  // 打靶辅助支撑
    constexpr const char* DN630_VALVE = "dn630_valve";                            // DN630插板阀
    constexpr const char* SHIELD = "shield";                                      // 屏蔽罩
    // 三英设备
    constexpr const char* OBLIQUE1_SOURCE = "oblique1_source";                    // 斜向1相衬射线源
    constexpr const char* OBLIQUE1_DETECTOR = "oblique1_detector";                // 斜向1相衬探测器
    constexpr const char* OBLIQUE2_SOURCE = "oblique2_source";                    // 斜向2相衬射线源
    constexpr const char* OBLIQUE2_DETECTOR = "oblique2_detector";                // 斜向2相衬探测器
    constexpr const char* OBLIQUE3_SOURCE = "oblique3_source";                    // 斜向3相衬射线源
    constexpr const char* OBLIQUE3_DETECTOR = "oblique3_detector";                // 斜向3相衬探测器
    constexpr const char* HORIZONTAL_SOURCE = "horizontal_source";                // 水平相衬射线源
    constexpr const char* HORIZONTAL_DETECTOR = "horizontal_detector";            // 水平相衬探测器
    constexpr const char* VERTICAL_SOURCE = "vertical_source";                    // 竖直相衬射线源
}

// 联锁条件定义（扩展版）
struct InterlockCondition {
    std::string id;                 // 条件ID
    std::string description;        // 描述
    InterlockType type;             // 类型
    
    // 源设备（触发条件的设备）
    std::string source_device;      // 源设备标识
    std::string source_attribute;   // 源属性名
    
    // 目标设备（被限制的设备）
    std::string target_device;      // 目标设备标识
    
    // 阈值配置
    double source_threshold;        // 源设备触发阈值（超过此值触发）
    double target_max_position;     // 目标设备最大允许位置
    double range_min;               // 条件范围最小值（用于区间判断）
    double range_max;               // 条件范围最大值
    
    // 非零位判断
    bool check_source_zero;         // 是否检查源设备非零位
    bool check_target_zero;         // 是否检查目标设备非零位
    double zero_threshold;          // 零位判断阈值（小于此值视为零位，默认1.0mm）
    
    InterlockAction action;         // 触发动作
    bool enabled;                   // 是否启用
    
    // 默认构造
    InterlockCondition() : type(InterlockType::POSITION_LIMIT),
        source_threshold(0), target_max_position(0),
        range_min(0), range_max(0), check_source_zero(false), 
        check_target_zero(false), zero_threshold(1.0), 
        action(InterlockAction::BLOCK_MOTION), enabled(true) {}
};

// 联锁状态记录
struct InterlockStatus {
    std::string condition_id;
    bool triggered;
    double source_value;
    double target_value;
    std::string message;
    time_t timestamp;
};

// 设备位置缓存
struct DevicePositionCache {
    double position;
    bool valid;
    time_t last_update;
    
    DevicePositionCache() : position(0), valid(false), last_update(0) {}
};

class InterlockService : public Common::StandardSystemDevice {
private:
    // Lock system
    bool is_locked_;
    std::string lock_user_;
    std::mutex lock_mutex_;
    
    // Interlock state
    bool interlock_enabled_;       // 联锁功能总开关
    bool emergency_stopped_;       // 急停状态
    short interlock_level_;        // 联锁级别: 0=off, 1=warning, 2=block, 3=emergency
    long self_check_result_;
    short result_value_;
    
    // Device Tango names (from properties) - 设备ID到Tango设备名的映射
    std::map<std::string, std::string> device_tango_names_;
    std::string vacuum_device_name_;
    
    // Runtime data
    std::vector<InterlockCondition> conditions_;
    std::vector<InterlockStatus> status_history_;
    std::string interlock_logs_;
    std::string alarm_state_;
    std::string active_interlocks_;  // JSON of currently active interlocks
    
    // Device proxies
    std::map<std::string, std::unique_ptr<Tango::DeviceProxy>> device_proxies_;
    
    // Position cache for all devices
    std::map<std::string, DevicePositionCache> position_cache_;
    double vacuum_pressure_;
    bool valve_state_;
    bool shield_state_;  // 屏蔽罩状态
    
public:
    InterlockService(Tango::DeviceClass *device_class, std::string &device_name);
    virtual ~InterlockService();
    
    virtual void init_device() override;
    virtual void delete_device() override;
    
    // ===== COMMANDS =====
    // Lock/Unlock
    void devLock(Tango::DevString user_info);
    void devUnlock(Tango::DevBoolean unlock_all);
    void devLockVerify();
    Tango::DevString devLockQuery();
    void devUserConfig(Tango::DevString config);
    
    // System
    void selfCheck();
    void init();
    void reset();
    
    // Interlock control
    void enableInterlock();         // 启用联锁
    void disableInterlock();        // 禁用联锁(需要密码)
    void emergencyStop();           // 急停所有设备
    void releaseEmergencyStop();    // 解除急停
    Tango::DevBoolean checkInterlocks();  // 检查所有联锁条件
    Tango::DevBoolean checkMotionAllowed(Tango::DevString device_name);  // 检查设备是否允许运动
    Tango::DevBoolean checkValveCloseAllowed();   // 检查是否允许关闭阀门
    Tango::DevBoolean checkShieldOperateAllowed(); // 检查是否允许操作屏蔽罩
    Tango::DevDouble getMaxAllowedPosition(Tango::DevString device_name); // 获取设备当前允许的最大位置
    
    // Condition management
    void addCondition(Tango::DevString json_condition);
    void removeCondition(Tango::DevString condition_id);
    void clearAllConditions();
    Tango::DevString getConditions();
    
    // Status query
    Tango::DevString getActiveInterlocks();
    Tango::DevString getInterlockHistory();
    void setInterlockLevel(Tango::DevShort level);
    
    // ===== ATTRIBUTES =====
    virtual void read_attr(Tango::Attribute &attr) override;
    void read_self_check_result(Tango::Attribute &attr);
    void read_interlock_enabled(Tango::Attribute &attr);
    void read_emergency_stopped(Tango::Attribute &attr);
    void read_interlock_level(Tango::Attribute &attr);
    void read_interlock_logs(Tango::Attribute &attr);
    void read_alarm_state(Tango::Attribute &attr);
    void read_active_interlocks(Tango::Attribute &attr);
    void read_result_value(Tango::Attribute &attr);
    void read_group_attribute_json(Tango::Attribute &attr);
    
    // Hooks
    virtual void specific_self_check() override;
    virtual void always_executed_hook() override;
    virtual void read_attr_hardware(std::vector<long> &attr_list) override;
    
private:
    void connect_to_devices();
    void setup_interlock_rules();         // 根据文档配置所有联锁规则
    void setup_hit_internal_rules();      // 哈工大内部联锁
    void setup_hit_sanying_rules();       // 哈工大与三英联锁
    void setup_hit_wanrui_rules();        // 哈工大与万瑞联锁
    void setup_sanying_wanrui_rules();    // 三英与万瑞联锁
    
    void update_device_positions();
    bool evaluate_condition(const InterlockCondition& cond);
    void execute_action(const InterlockCondition& cond);
    void stop_device(const std::string& device_id);
    void stop_all_devices();
    void log_event(const std::string& event);
    void update_active_interlocks();
    
    // 设备位置读取
    double read_device_position(const std::string& device_id);
    bool read_device_position_safe(const std::string& device_id, double& value);
    double read_device_attribute(const std::string& tango_name, const std::string& attr);
    double read_device_value(const std::string& device_id, const std::string& attr, bool& ok);
    
    // 零位判断
    bool isAtZeroPosition(const std::string& device_id, double threshold = 1.0);
    bool isNotAtZeroPosition(const std::string& device_id, double threshold = 1.0);
    
    // 辅助方法
    std::string getDeviceTangoName(const std::string& device_id);
    void addConditionInternal(const InterlockCondition& cond);
};

class InterlockServiceClass : public Tango::DeviceClass {
public:
    static InterlockServiceClass *instance();
    
protected:
    InterlockServiceClass(std::string &class_name);
    
public:
    virtual void attribute_factory(std::vector<Tango::Attr *> &att_list) override;
    virtual void command_factory() override;
    virtual void device_factory(const Tango::DevVarStringArray *devlist_ptr) override;
    
private:
    static InterlockServiceClass *_instance;
};

} // namespace Interlock

#endif // INTERLOCK_SERVICE_H
