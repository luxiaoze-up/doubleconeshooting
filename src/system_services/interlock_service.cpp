#include "system_services/interlock_service.h"
#include "common/system_config.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace {

// 默认位置属性，根据设备类别挑选，减少条件配置重复
std::string default_position_attr(const std::string &device_id) {
    using namespace Interlock::DeviceId;
    if (device_id == LIFTING_MECHANISM || device_id == UPPER_CHAR_SUPPORT ||
        device_id == LOWER_CHAR_SUPPORT || device_id == SHOOTING_SUPPORT) {
        return "pos";
    }
    if (device_id == DN630_VALVE) {
        return "valveState";
    }
    if (device_id == SHIELD) {
        return "shieldState";
    }
    return "largeRangePos";
}

} // namespace

namespace Interlock {

InterlockService::InterlockService(Tango::DeviceClass *device_class, std::string &device_name)
    : Common::StandardSystemDevice(device_class, device_name),
      is_locked_(false),
      lock_user_(""),
      interlock_enabled_(true),
      emergency_stopped_(false),
      interlock_level_(2),
      self_check_result_(0),
      result_value_(0),
      interlock_logs_("{}"),
      alarm_state_("NORMAL"),
      active_interlocks_("[]"),
      vacuum_pressure_(0.0),
      valve_state_(false),
      shield_state_(false) {
    init_device();
}

InterlockService::~InterlockService() {
    delete_device();
}

void InterlockService::init_device() {
    Common::StandardSystemDevice::init_device();

    // 读取设备Tango名属性（属性名与DeviceId一致），附带真空设备名
    std::vector<std::string> prop_keys = {
        DeviceId::SONGBA_LARGE_STROKE, DeviceId::BACKLIGHT_LARGE_STROKE,
        DeviceId::TARGET_TRANSFER, DeviceId::XY_DETECTION,
        DeviceId::LIFTING_MECHANISM, DeviceId::UPPER_CHAR_SUPPORT,
        DeviceId::LOWER_CHAR_SUPPORT, DeviceId::SHOOTING_SUPPORT,
        DeviceId::DN630_VALVE, DeviceId::SHIELD,
        DeviceId::OBLIQUE1_SOURCE, DeviceId::OBLIQUE1_DETECTOR,
        DeviceId::OBLIQUE2_SOURCE, DeviceId::OBLIQUE2_DETECTOR,
        DeviceId::OBLIQUE3_SOURCE, DeviceId::OBLIQUE3_DETECTOR,
        DeviceId::HORIZONTAL_SOURCE, DeviceId::HORIZONTAL_DETECTOR,
        DeviceId::VERTICAL_SOURCE, "vacuum_device"};

    Tango::DbData db_data;
    for (const auto &k : prop_keys) {
        db_data.push_back(Tango::DbDatum(k));
    }
    get_db_device()->get_property(db_data);

    for (size_t i = 0; i + 1 < prop_keys.size(); ++i) { // last one is vacuum
        if (!db_data[i].is_empty()) {
            std::string name;
            db_data[i] >> name;
            device_tango_names_[prop_keys[i]] = name;
        }
    }
    if (!db_data.back().is_empty()) {
        db_data.back() >> vacuum_device_name_;
    }

    interlock_enabled_ = true;
    emergency_stopped_ = false;
    interlock_level_ = 2;
    alarm_state_ = "NORMAL";
    active_interlocks_ = "[]";
    status_history_.clear();
    conditions_.clear();

    connect_to_devices();
    setup_interlock_rules();

    set_state(Tango::ON);
    set_status("Interlock service ready");
    log_event("InterlockService initialized with " + std::to_string(conditions_.size()) + " rules");
}

void InterlockService::delete_device() {
    device_proxies_.clear();
    Common::StandardSystemDevice::delete_device();
}

void InterlockService::connect_to_devices() {
#ifdef HAS_TANGO
    for (const auto &kv : device_tango_names_) {
        if (kv.second.empty()) continue;
        try {
            auto proxy = std::make_unique<Tango::DeviceProxy>(kv.second);
            proxy->ping();
            device_proxies_[kv.first] = std::move(proxy);
        } catch (...) {
            WARN_STREAM << "Failed to connect to device " << kv.first << " (" << kv.second << ")" << std::endl;
        }
    }
    if (!vacuum_device_name_.empty()) {
        try {
            auto proxy = std::make_unique<Tango::DeviceProxy>(vacuum_device_name_);
            proxy->ping();
            device_proxies_[vacuum_device_name_] = std::move(proxy);
        } catch (...) {
            WARN_STREAM << "Failed to connect to vacuum device " << vacuum_device_name_ << std::endl;
        }
    }
#endif
}

void InterlockService::setup_interlock_rules() {
    conditions_.clear();
    setup_hit_internal_rules();
    setup_hit_sanying_rules();
    setup_hit_wanrui_rules();
    setup_sanying_wanrui_rules();
}

// 哈工大内部联锁规则
void InterlockService::setup_hit_internal_rules() {
    // 送靶大行程与表征辅助支撑
    InterlockCondition c1; c1.id = "HIT_01"; c1.description = "送靶>740禁上表征前进";
    c1.type = InterlockType::POSITION_LIMIT; c1.source_device = DeviceId::SONGBA_LARGE_STROKE; c1.source_attribute = "largeRangePos"; c1.source_threshold = 740.0; c1.target_device = DeviceId::UPPER_CHAR_SUPPORT; c1.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c1);
    InterlockCondition c1b = c1; c1b.id = "HIT_01B"; c1b.target_device = DeviceId::LOWER_CHAR_SUPPORT; addConditionInternal(c1b);

    // 表征超位禁止送靶
    InterlockCondition c2; c2.id = "HIT_02"; c2.description = "上表征>260禁送靶";
    c2.type = InterlockType::POSITION_LIMIT; c2.source_device = DeviceId::UPPER_CHAR_SUPPORT; c2.source_attribute = "pos"; c2.source_threshold = 260.0; c2.target_device = DeviceId::SONGBA_LARGE_STROKE; c2.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c2);
    InterlockCondition c2b = c2; c2b.id = "HIT_03"; c2b.description = "下表征>230禁送靶"; c2b.source_device = DeviceId::LOWER_CHAR_SUPPORT; c2b.source_threshold = 230.0; addConditionInternal(c2b);

    // 表征非零位限制送靶<=740
    InterlockCondition c3; c3.id = "HIT_04"; c3.description = "表征非零位送靶<=740";
    c3.type = InterlockType::POSITION_MUTUAL; c3.source_device = DeviceId::UPPER_CHAR_SUPPORT; c3.source_attribute = "pos"; c3.check_source_zero = true; c3.target_device = DeviceId::SONGBA_LARGE_STROKE; c3.target_max_position = 740.0; addConditionInternal(c3);
    InterlockCondition c3b = c3; c3b.id = "HIT_04B"; c3b.source_device = DeviceId::LOWER_CHAR_SUPPORT; addConditionInternal(c3b);

    // 送靶与背光
    InterlockCondition c4; c4.id = "HIT_05"; c4.description = "送靶>650禁背光";
    c4.type = InterlockType::POSITION_LIMIT; c4.source_device = DeviceId::SONGBA_LARGE_STROKE; c4.source_attribute = "largeRangePos"; c4.source_threshold = 650.0; c4.target_device = DeviceId::BACKLIGHT_LARGE_STROKE; c4.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c4);

    InterlockCondition c5; c5.id = "HIT_06"; c5.description = "背光430-580送靶<=650";
    c5.type = InterlockType::RANGE_CONDITIONAL; c5.source_device = DeviceId::BACKLIGHT_LARGE_STROKE; c5.source_attribute = "largeRangePos"; c5.range_min = 430.0; c5.range_max = 580.0; c5.target_device = DeviceId::SONGBA_LARGE_STROKE; c5.target_max_position = 650.0; addConditionInternal(c5);

    InterlockCondition c6; c6.id = "HIT_07"; c6.description = "背光任意位置送靶<=750";
    c6.type = InterlockType::RANGE_CONDITIONAL; c6.source_device = DeviceId::BACKLIGHT_LARGE_STROKE; c6.source_attribute = "largeRangePos"; c6.range_min = 0.0; c6.range_max = 640.0; c6.target_device = DeviceId::SONGBA_LARGE_STROKE; c6.target_max_position = 750.0; addConditionInternal(c6);

    InterlockCondition c7; c7.id = "HIT_08"; c7.description = "背光非零位送靶<=750";
    c7.type = InterlockType::POSITION_MUTUAL; c7.source_device = DeviceId::BACKLIGHT_LARGE_STROKE; c7.source_attribute = "largeRangePos"; c7.check_source_zero = true; c7.target_device = DeviceId::SONGBA_LARGE_STROKE; c7.target_max_position = 750.0; addConditionInternal(c7);

    InterlockCondition c8; c8.id = "HIT_09"; c8.description = "送靶700-750背光<=640";
    c8.type = InterlockType::RANGE_CONDITIONAL; c8.source_device = DeviceId::SONGBA_LARGE_STROKE; c8.source_attribute = "largeRangePos"; c8.range_min = 700.0; c8.range_max = 750.0; c8.target_device = DeviceId::BACKLIGHT_LARGE_STROKE; c8.target_max_position = 640.0; addConditionInternal(c8);

    // 背光在600-640时自限（配合送靶700-750场景，确保不超上限）
    InterlockCondition c8b; c8b.id = "HIT_09B"; c8b.description = "背光600-640自限<=640";
    c8b.type = InterlockType::RANGE_CONDITIONAL; c8b.source_device = DeviceId::BACKLIGHT_LARGE_STROKE; c8b.source_attribute = "largeRangePos"; c8b.range_min = 600.0; c8b.range_max = 640.0; c8b.target_device = DeviceId::BACKLIGHT_LARGE_STROKE; c8b.target_max_position = 640.0; addConditionInternal(c8b);

    // 送靶与靶件转移
    InterlockCondition c9; c9.id = "HIT_10"; c9.description = "送靶>1500禁转移";
    c9.type = InterlockType::POSITION_LIMIT; c9.source_device = DeviceId::SONGBA_LARGE_STROKE; c9.source_attribute = "largeRangePos"; c9.source_threshold = 1500.0; c9.target_device = DeviceId::TARGET_TRANSFER; c9.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c9);

    InterlockCondition c10; c10.id = "HIT_11"; c10.description = "转移非零位送靶<=1500";
    c10.type = InterlockType::POSITION_MUTUAL; c10.source_device = DeviceId::TARGET_TRANSFER; c10.source_attribute = "largeRangePos"; c10.check_source_zero = true; c10.target_device = DeviceId::SONGBA_LARGE_STROKE; c10.target_max_position = 1500.0; addConditionInternal(c10);

    // 送靶与X/γ
    InterlockCondition c11; c11.id = "HIT_12"; c11.description = "送靶>1700禁Xγ";
    c11.type = InterlockType::POSITION_LIMIT; c11.source_device = DeviceId::SONGBA_LARGE_STROKE; c11.source_attribute = "largeRangePos"; c11.source_threshold = 1700.0; c11.target_device = DeviceId::XY_DETECTION; c11.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c11);

    InterlockCondition c12; c12.id = "HIT_13"; c12.description = "Xγ非零位送靶<=1700";
    c12.type = InterlockType::POSITION_MUTUAL; c12.source_device = DeviceId::XY_DETECTION; c12.source_attribute = "largeRangePos"; c12.check_source_zero = true; c12.target_device = DeviceId::SONGBA_LARGE_STROKE; c12.target_max_position = 1700.0; addConditionInternal(c12);

    // 送靶与升降
    InterlockCondition c13; c13.id = "HIT_14"; c13.description = "送靶>1800禁升降";
    c13.type = InterlockType::POSITION_LIMIT; c13.source_device = DeviceId::SONGBA_LARGE_STROKE; c13.source_attribute = "largeRangePos"; c13.source_threshold = 1800.0; c13.target_device = DeviceId::LIFTING_MECHANISM; c13.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c13);

    InterlockCondition c14; c14.id = "HIT_15"; c14.description = "升降非零位送靶<=1800";
    c14.type = InterlockType::POSITION_MUTUAL; c14.source_device = DeviceId::LIFTING_MECHANISM; c14.source_attribute = "pos"; c14.check_source_zero = true; c14.target_device = DeviceId::SONGBA_LARGE_STROKE; c14.target_max_position = 1800.0; addConditionInternal(c14);

    // 送靶与插板阀
    InterlockCondition c15; c15.id = "HIT_16"; c15.description = "送靶>2500禁关阀";
    c15.type = InterlockType::VALVE_OPERATION; c15.source_device = DeviceId::SONGBA_LARGE_STROKE; c15.source_attribute = "largeRangePos"; c15.source_threshold = 2500.0; c15.target_device = DeviceId::DN630_VALVE; c15.action = InterlockAction::BLOCK_VALVE_CLOSE; addConditionInternal(c15);

    InterlockCondition c16; c16.id = "HIT_17"; c16.description = "阀关闭送靶<=2500";
    c16.type = InterlockType::RANGE_CONDITIONAL; c16.source_device = DeviceId::DN630_VALVE; c16.source_attribute = "valveState"; c16.range_min = 0.0; c16.range_max = 0.5; c16.target_device = DeviceId::SONGBA_LARGE_STROKE; c16.target_max_position = 2500.0; addConditionInternal(c16);

    // 送靶与打靶辅助支撑
    InterlockCondition c17; c17.id = "HIT_18"; c17.description = "送靶>8000禁打靶辅助前进";
    c17.type = InterlockType::POSITION_LIMIT; c17.source_device = DeviceId::SONGBA_LARGE_STROKE; c17.source_attribute = "largeRangePos"; c17.source_threshold = 8000.0; c17.target_device = DeviceId::SHOOTING_SUPPORT; c17.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c17);

    InterlockCondition c18; c18.id = "HIT_19"; c18.description = "打靶辅助>230禁送靶";
    c18.type = InterlockType::POSITION_LIMIT; c18.source_device = DeviceId::SHOOTING_SUPPORT; c18.source_attribute = "pos"; c18.source_threshold = 230.0; c18.target_device = DeviceId::SONGBA_LARGE_STROKE; c18.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c18);

    InterlockCondition c19; c19.id = "HIT_20"; c19.description = "打靶辅助非零位送靶<=750";
    c19.type = InterlockType::POSITION_MUTUAL; c19.source_device = DeviceId::SHOOTING_SUPPORT; c19.source_attribute = "pos"; c19.check_source_zero = true; c19.target_device = DeviceId::SONGBA_LARGE_STROKE; c19.target_max_position = 750.0; addConditionInternal(c19);

    // 转移与阀
    InterlockCondition c20; c20.id = "HIT_21"; c20.description = "转移>50禁关阀";
    c20.type = InterlockType::VALVE_OPERATION; c20.source_device = DeviceId::TARGET_TRANSFER; c20.source_attribute = "largeRangePos"; c20.source_threshold = 50.0; c20.target_device = DeviceId::DN630_VALVE; c20.action = InterlockAction::BLOCK_VALVE_CLOSE; addConditionInternal(c20);

    InterlockCondition c21; c21.id = "HIT_22"; c21.description = "阀关闭转移<=50";
    c21.type = InterlockType::RANGE_CONDITIONAL; c21.source_device = DeviceId::DN630_VALVE; c21.source_attribute = "valveState"; c21.range_min = 0.0; c21.range_max = 0.5; c21.target_device = DeviceId::TARGET_TRANSFER; c21.target_max_position = 50.0; addConditionInternal(c21);

    // X/γ与阀
    InterlockCondition c22; c22.id = "HIT_23"; c22.description = "Xγ>20禁关阀";
    c22.type = InterlockType::VALVE_OPERATION; c22.source_device = DeviceId::XY_DETECTION; c22.source_attribute = "largeRangePos"; c22.source_threshold = 20.0; c22.target_device = DeviceId::DN630_VALVE; c22.action = InterlockAction::BLOCK_VALVE_CLOSE; addConditionInternal(c22);

    InterlockCondition c23; c23.id = "HIT_24"; c23.description = "阀关闭Xγ<=20";
    c23.type = InterlockType::RANGE_CONDITIONAL; c23.source_device = DeviceId::DN630_VALVE; c23.source_attribute = "valveState"; c23.range_min = 0.0; c23.range_max = 0.5; c23.target_device = DeviceId::XY_DETECTION; c23.target_max_position = 20.0; addConditionInternal(c23);
}

// 哈工大与三英联锁
void InterlockService::setup_hit_sanying_rules() {
    std::vector<std::string> xiangchen = {
        DeviceId::OBLIQUE1_SOURCE, DeviceId::OBLIQUE1_DETECTOR,
        DeviceId::OBLIQUE2_SOURCE, DeviceId::OBLIQUE2_DETECTOR,
        DeviceId::OBLIQUE3_SOURCE, DeviceId::OBLIQUE3_DETECTOR,
        DeviceId::HORIZONTAL_SOURCE, DeviceId::HORIZONTAL_DETECTOR,
        DeviceId::VERTICAL_SOURCE};

    for (size_t i = 0; i < xiangchen.size(); ++i) {
        InterlockCondition c; c.id = "HS_" + std::to_string(i + 1) + "A"; c.description = "送靶>50禁相衬前进";
        c.type = InterlockType::POSITION_LIMIT; c.source_device = DeviceId::SONGBA_LARGE_STROKE; c.source_attribute = "largeRangePos"; c.source_threshold = 50.0; c.target_device = xiangchen[i]; c.action = InterlockAction::BLOCK_MOTION; addConditionInternal(c);

        InterlockCondition cb; cb.id = "HS_" + std::to_string(i + 1) + "B"; cb.description = "相衬非零位送靶<=50";
        cb.type = InterlockType::POSITION_MUTUAL; cb.source_device = xiangchen[i]; cb.source_attribute = "largeRangePos"; cb.check_source_zero = true; cb.target_device = DeviceId::SONGBA_LARGE_STROKE; cb.target_max_position = 50.0; addConditionInternal(cb);
    }
}

// 哈工大与万瑞屏蔽罩联锁
void InterlockService::setup_hit_wanrui_rules() {
    InterlockCondition c1; c1.id = "HW_01"; c1.description = "送靶3500-5000禁屏蔽罩";
    c1.type = InterlockType::SHIELD_OPERATION; c1.source_device = DeviceId::SONGBA_LARGE_STROKE; c1.source_attribute = "largeRangePos"; c1.range_min = 3500.0; c1.range_max = 5000.0; c1.action = InterlockAction::BLOCK_SHIELD_OPERATE; addConditionInternal(c1);

    InterlockCondition c2; c2.id = "HW_02"; c2.description = "背光>500禁屏蔽罩";
    c2.type = InterlockType::SHIELD_OPERATION; c2.source_device = DeviceId::BACKLIGHT_LARGE_STROKE; c2.source_attribute = "largeRangePos"; c2.source_threshold = 500.0; c2.action = InterlockAction::BLOCK_SHIELD_OPERATE; addConditionInternal(c2);

    InterlockCondition c3; c3.id = "HW_03"; c3.description = "转移>500禁屏蔽罩";
    c3.type = InterlockType::SHIELD_OPERATION; c3.source_device = DeviceId::TARGET_TRANSFER; c3.source_attribute = "largeRangePos"; c3.source_threshold = 500.0; c3.action = InterlockAction::BLOCK_SHIELD_OPERATE; addConditionInternal(c3);

    InterlockCondition c4; c4.id = "HW_04"; c4.description = "Xγ>500禁屏蔽罩";
    c4.type = InterlockType::SHIELD_OPERATION; c4.source_device = DeviceId::XY_DETECTION; c4.source_attribute = "largeRangePos"; c4.source_threshold = 500.0; c4.action = InterlockAction::BLOCK_SHIELD_OPERATE; addConditionInternal(c4);

    InterlockCondition c5; c5.id = "HW_05"; c5.description = "升降>100禁屏蔽罩";
    c5.type = InterlockType::SHIELD_OPERATION; c5.source_device = DeviceId::LIFTING_MECHANISM; c5.source_attribute = "pos"; c5.source_threshold = 100.0; c5.action = InterlockAction::BLOCK_SHIELD_OPERATE; addConditionInternal(c5);
}

// 三英与万瑞屏蔽罩联锁
void InterlockService::setup_sanying_wanrui_rules() {
    std::vector<std::string> devs = {
        DeviceId::OBLIQUE1_SOURCE, DeviceId::OBLIQUE1_DETECTOR,
        DeviceId::OBLIQUE2_SOURCE, DeviceId::OBLIQUE2_DETECTOR,
        DeviceId::OBLIQUE3_SOURCE, DeviceId::OBLIQUE3_DETECTOR,
        DeviceId::HORIZONTAL_SOURCE, DeviceId::HORIZONTAL_DETECTOR,
        DeviceId::VERTICAL_SOURCE};
    for (size_t i = 0; i < devs.size(); ++i) {
        InterlockCondition c; c.id = "SW_" + std::to_string(i + 1); c.description = "相衬>300禁屏蔽罩";
        c.type = InterlockType::SHIELD_OPERATION; c.source_device = devs[i]; c.source_attribute = "largeRangePos"; c.source_threshold = 300.0; c.action = InterlockAction::BLOCK_SHIELD_OPERATE; addConditionInternal(c);
    }
}

// ===== LOCK/UNLOCK COMMANDS =====
void InterlockService::devLock(Tango::DevString user_info) {
    std::lock_guard<std::mutex> g(lock_mutex_);
    if (is_locked_ && lock_user_ != std::string(user_info)) {
        Tango::Except::throw_exception("DEVICE_LOCKED", "Device locked by " + lock_user_, "InterlockService::devLock");
    }
    is_locked_ = true;
    lock_user_ = std::string(user_info);
    log_event("Device locked by " + lock_user_);
    result_value_ = 0;
}

void InterlockService::devUnlock(Tango::DevBoolean unlock_all) {
    std::lock_guard<std::mutex> g(lock_mutex_);
    if (unlock_all || is_locked_) {
        is_locked_ = false;
        lock_user_.clear();
        log_event("Device unlocked");
    }
    result_value_ = 0;
}

void InterlockService::devLockVerify() {
    std::lock_guard<std::mutex> g(lock_mutex_);
    if (!is_locked_) {
        Tango::Except::throw_exception("DEVICE_NOT_LOCKED", "Device not locked", "InterlockService::devLockVerify");
    }
    result_value_ = 0;
}

Tango::DevString InterlockService::devLockQuery() {
    std::lock_guard<std::mutex> g(lock_mutex_);
    std::string result = is_locked_ ? lock_user_ : "UNLOCKED";
    return Tango::string_dup(result.c_str());
}

void InterlockService::devUserConfig(Tango::DevString config) {
    log_event("User config: " + std::string(config));
    result_value_ = 0;
}

// ===== SYSTEM COMMANDS =====
void InterlockService::selfCheck() {
    log_event("Self check started");
    self_check_result_ = 0;
#ifdef HAS_TANGO
    for (auto &p : device_proxies_) {
        try {
            if (p.second) p.second->ping();
        } catch (...) {
            self_check_result_ |= 1;
        }
    }
#endif
    result_value_ = (self_check_result_ == 0) ? 0 : 1;
    log_event("Self check completed: " + std::to_string(self_check_result_));
}

void InterlockService::init() {
    log_event("Init command");
    emergency_stopped_ = false;
    alarm_state_ = "NORMAL";
    set_state(Tango::ON);
    result_value_ = 0;
}

void InterlockService::reset() {
    log_event("Reset command");
    Common::StandardSystemDevice::reset();
    emergency_stopped_ = false;
    alarm_state_ = "NORMAL";
    active_interlocks_ = "[]";
    set_state(Tango::ON);
    result_value_ = 0;
}

// ===== INTERLOCK CONTROL COMMANDS =====
void InterlockService::enableInterlock() {
    interlock_enabled_ = true;
    log_event("Interlock enabled");
    result_value_ = 0;
}

void InterlockService::disableInterlock() {
    interlock_enabled_ = false;
    alarm_state_ = "INTERLOCK_DISABLED";
    log_event("Interlock disabled");
    result_value_ = 0;
}

void InterlockService::emergencyStop() {
    emergency_stopped_ = true;
    alarm_state_ = "EMERGENCY_STOP";
    set_state(Tango::ALARM);
    stop_all_devices();
    log_event("Emergency stop triggered");
    result_value_ = 0;
}

void InterlockService::releaseEmergencyStop() {
    if (!emergency_stopped_) return;
    emergency_stopped_ = false;
    alarm_state_ = "NORMAL";
    set_state(Tango::ON);
    log_event("Emergency stop released");
    result_value_ = 0;
}

Tango::DevBoolean InterlockService::checkInterlocks() {
    if (!interlock_enabled_) return true;
    if (emergency_stopped_) return false;

    update_device_positions();
    bool all_ok = true;
    for (const auto &cond : conditions_) {
        if (!cond.enabled) continue;
        if (!evaluate_condition(cond)) {
            all_ok = false;
            if (interlock_level_ >= 2) {
                execute_action(cond);
            } else if (interlock_level_ == 1) {
                log_event("Warning only: " + cond.id + " - " + cond.description);
            }
        }
    }
    update_active_interlocks();
    return all_ok;
}

Tango::DevBoolean InterlockService::checkMotionAllowed(Tango::DevString device_name) {
    if (!interlock_enabled_) return true;
    if (emergency_stopped_) return false;
    std::string dev(device_name);
    update_device_positions();
    for (const auto &cond : conditions_) {
        if (!cond.enabled) continue;
        if (cond.target_device == dev) {
            if (!evaluate_condition(cond)) {
                log_event("Motion blocked for " + dev + " by " + cond.id);
                return false;
            }
        }
    }
    return true;
}

Tango::DevBoolean InterlockService::checkValveCloseAllowed() {
    if (!interlock_enabled_ || emergency_stopped_) return false;
    update_device_positions();
    for (const auto &cond : conditions_) {
        if (!cond.enabled) continue;
        if (cond.type == InterlockType::VALVE_OPERATION) {
            if (!evaluate_condition(cond)) return false;
        }
    }
    return true;
}

Tango::DevBoolean InterlockService::checkShieldOperateAllowed() {
    if (!interlock_enabled_ || emergency_stopped_) return false;
    update_device_positions();
    for (const auto &cond : conditions_) {
        if (!cond.enabled) continue;
        if (cond.type == InterlockType::SHIELD_OPERATION) {
            if (!evaluate_condition(cond)) return false;
        }
    }
    return true;
}

Tango::DevDouble InterlockService::getMaxAllowedPosition(Tango::DevString device_name) {
    std::string dev(device_name);
    double max_pos = std::numeric_limits<double>::infinity();
    update_device_positions();
    for (const auto &cond : conditions_) {
        if (!cond.enabled) continue;
        if (cond.target_device != dev) continue;

        bool ok_src = false;
        double src = read_device_value(cond.source_device, cond.source_attribute, ok_src);
        if (!ok_src) continue;

        switch (cond.type) {
            case InterlockType::POSITION_MUTUAL:
                if (cond.check_source_zero && src > cond.zero_threshold && cond.target_max_position > 0) {
                    max_pos = std::min(max_pos, cond.target_max_position);
                }
                break;
            case InterlockType::RANGE_CONDITIONAL:
                if (src >= cond.range_min && src <= cond.range_max && cond.target_max_position > 0) {
                    max_pos = std::min(max_pos, cond.target_max_position);
                }
                break;
            default:
                if (cond.target_max_position > 0) {
                    max_pos = std::min(max_pos, cond.target_max_position);
                }
                break;
        }
    }
    if (!std::isfinite(max_pos)) max_pos = 1e9;
    return max_pos;
}

// ===== CONDITION MANAGEMENT =====
void InterlockService::addCondition(Tango::DevString json_condition) {
    // 简化：仅记录日志，不解析JSON
    log_event("AddCondition not implemented: " + std::string(json_condition));
    result_value_ = 0;
}

void InterlockService::removeCondition(Tango::DevString condition_id) {
    std::string id(condition_id);
    conditions_.erase(std::remove_if(conditions_.begin(), conditions_.end(), [&id](const InterlockCondition &c) { return c.id == id; }), conditions_.end());
    log_event("Condition removed: " + id);
    result_value_ = 0;
}

void InterlockService::clearAllConditions() {
    conditions_.clear();
    log_event("All conditions cleared");
    result_value_ = 0;
}

Tango::DevString InterlockService::getConditions() {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < conditions_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"id\":\"" << conditions_[i].id << "\",\"desc\":\"" << conditions_[i].description << "\",\"enabled\":" << (conditions_[i].enabled ? "true" : "false") << "}";
    }
    oss << "]";
    return Tango::string_dup(oss.str().c_str());
}

Tango::DevString InterlockService::getActiveInterlocks() {
    return Tango::string_dup(active_interlocks_.c_str());
}

Tango::DevString InterlockService::getInterlockHistory() {
    return Tango::string_dup(interlock_logs_.c_str());
}

void InterlockService::setInterlockLevel(Tango::DevShort level) {
    interlock_level_ = level;
    log_event("Interlock level set to " + std::to_string(level));
    result_value_ = 0;
}

// ===== ATTRIBUTE READERS =====
void InterlockService::read_attr(Tango::Attribute &attr) {
    std::string name = attr.get_name();
    if (name == "selfCheckResult") {
        read_self_check_result(attr);
    } else if (name == "interlockEnabled") {
        read_interlock_enabled(attr);
    } else if (name == "emergencyStopped") {
        read_emergency_stopped(attr);
    } else if (name == "interlockLevel") {
        read_interlock_level(attr);
    } else if (name == "interlockLogs") {
        read_interlock_logs(attr);
    } else if (name == "alarmState") {
        read_alarm_state(attr);
    } else if (name == "activeInterlocks") {
        read_active_interlocks(attr);
    } else if (name == "resultValue") {
        read_result_value(attr);
    } else if (name == "groupAttributeJson") {
        read_group_attribute_json(attr);
    }
}

void InterlockService::read_self_check_result(Tango::Attribute &attr) { attr.set_value(&self_check_result_); }
void InterlockService::read_interlock_enabled(Tango::Attribute &attr) { attr.set_value(&interlock_enabled_); }
void InterlockService::read_emergency_stopped(Tango::Attribute &attr) { attr.set_value(&emergency_stopped_); }
void InterlockService::read_interlock_level(Tango::Attribute &attr) { attr.set_value(&interlock_level_); }

void InterlockService::read_interlock_logs(Tango::Attribute &attr) {
    Tango::DevString val = Tango::string_dup(interlock_logs_.c_str());
    attr.set_value(&val);
}

void InterlockService::read_alarm_state(Tango::Attribute &attr) {
    Tango::DevString val = Tango::string_dup(alarm_state_.c_str());
    attr.set_value(&val);
}

void InterlockService::read_active_interlocks(Tango::Attribute &attr) {
    Tango::DevString val = Tango::string_dup(active_interlocks_.c_str());
    attr.set_value(&val);
}

void InterlockService::read_result_value(Tango::Attribute &attr) { attr.set_value(&result_value_); }

void InterlockService::read_group_attribute_json(Tango::Attribute &attr) {
    std::ostringstream oss;
    oss << "{\"interlockEnabled\":" << (interlock_enabled_ ? "true" : "false")
        << ",\"emergencyStopped\":" << (emergency_stopped_ ? "true" : "false")
        << ",\"interlockLevel\":" << interlock_level_
        << ",\"alarmState\":\"" << alarm_state_ << "\""
        << ",\"conditionCount\":" << conditions_.size() << "}";
    Tango::DevString val = Tango::string_dup(oss.str().c_str());
    attr.set_value(&val);
}

// ===== HOOKS =====
void InterlockService::specific_self_check() {
#ifdef HAS_TANGO
    for (auto &pair : device_proxies_) {
        if (pair.second) pair.second->ping();
    }
#endif
}

void InterlockService::always_executed_hook() {
    Common::StandardSystemDevice::always_executed_hook();
    if (interlock_enabled_ && !emergency_stopped_) {
        // 可在此开启周期性联锁检查
    }
}

void InterlockService::read_attr_hardware(std::vector<long> &/*attr_list*/) {
    update_device_positions();
}

// ===== PRIVATE METHODS =====
void InterlockService::update_device_positions() {
#ifdef HAS_TANGO
    for (const auto &kv : device_tango_names_) {
        double val = 0.0;
        if (read_device_position_safe(kv.first, val)) {
            position_cache_[kv.first].position = val;
            position_cache_[kv.first].valid = true;
            position_cache_[kv.first].last_update = time(nullptr);
        }
    }

    if (device_proxies_.count(vacuum_device_name_)) {
        auto &proxy = device_proxies_[vacuum_device_name_];
        if (proxy) {
            try {
                Tango::DeviceAttribute da = proxy->read_attribute("pressure");
                da >> vacuum_pressure_;
            } catch (...) {}
            try {
                Tango::DeviceAttribute da = proxy->read_attribute("valveState");
                da >> valve_state_;
            } catch (...) {}
        }
    }
#endif
}

bool InterlockService::evaluate_condition(const InterlockCondition &cond) {
    bool ok_src = false, ok_tgt = false;
    double src = read_device_value(cond.source_device, cond.source_attribute, ok_src);
    double tgt = read_device_value(cond.target_device, default_position_attr(cond.target_device), ok_tgt);

    switch (cond.type) {
        case InterlockType::POSITION_LIMIT:
            if (!ok_src) return false; // fail-safe:读不到即阻断
            return !(src > cond.source_threshold);

        case InterlockType::POSITION_MUTUAL:
            if (!ok_src) return false;
            if (cond.check_source_zero && src > cond.zero_threshold) {
                if (!ok_tgt) return false;
                return tgt <= cond.target_max_position;
            }
            if (cond.check_target_zero && ok_tgt && tgt > cond.zero_threshold && cond.target_max_position > 0) {
                return src <= cond.target_max_position;
            }
            return true;

        case InterlockType::RANGE_CONDITIONAL:
            if (!ok_src) return false;
            if (src >= cond.range_min && src <= cond.range_max) {
                if (cond.target_max_position > 0) {
                    if (!ok_tgt) return false;
                    return tgt <= cond.target_max_position;
                }
                return false; // range命中且无上限即视为不允许
            }
            return true;

        case InterlockType::VALVE_OPERATION:
            if (!ok_src) return false;
            return !(src > cond.source_threshold);

        case InterlockType::SHIELD_OPERATION:
            if (!ok_src) return false;
            if (cond.range_max > cond.range_min) {
                return !(src >= cond.range_min && src <= cond.range_max);
            }
            return !(src > cond.source_threshold);

        case InterlockType::VACUUM_PRESSURE:
            return vacuum_pressure_ <= cond.source_threshold;

        case InterlockType::DEVICE_STATE:
        default:
            return true;
    }
}

void InterlockService::execute_action(const InterlockCondition &cond) {
    switch (cond.action) {
        case InterlockAction::STOP:
            stop_device(cond.target_device);
            break;
        case InterlockAction::ALARM:
            alarm_state_ = "INTERLOCK:" + cond.id;
            set_state(Tango::ALARM);
            break;
        case InterlockAction::BLOCK_MOTION:
            log_event("Motion blocked: " + cond.id + " - " + cond.description);
            break;
        case InterlockAction::BLOCK_VALVE_CLOSE:
            log_event("Valve close blocked: " + cond.id);
            break;
        case InterlockAction::BLOCK_SHIELD_OPERATE:
            log_event("Shield operate blocked: " + cond.id);
            break;
        case InterlockAction::WARNING:
            log_event("Warning: " + cond.description);
            break;
    }
}

void InterlockService::stop_device(const std::string &device_id) {
#ifdef HAS_TANGO
    auto it = device_proxies_.find(device_id);
    if (it != device_proxies_.end() && it->second) {
        try {
            it->second->command_inout("stop");
            log_event("Stopped device: " + device_id);
        } catch (...) {
            log_event("Stop failed: " + device_id);
        }
    }
#else
    log_event("(MOCK) stop device: " + device_id);
#endif
}

void InterlockService::stop_all_devices() {
#ifdef HAS_TANGO
    for (auto &p : device_proxies_) {
        if (p.second) {
            try { p.second->command_inout("stop"); } catch (...) {}
        }
    }
#else
    log_event("(MOCK) stop all devices");
#endif
}

void InterlockService::log_event(const std::string &event) {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    INFO_STREAM << "[" << buf << "] " << event << std::endl;

    std::ostringstream oss;
    oss << "\"" << buf << "\":\"" << event << "\"";
    if (interlock_logs_.empty()) interlock_logs_ = "{" + oss.str() + "}";
    else if (interlock_logs_ == "{}") interlock_logs_ = "{" + oss.str() + "}";
    else interlock_logs_.insert(interlock_logs_.size() - 1, "," + oss.str());
}

void InterlockService::update_active_interlocks() {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto &cond : conditions_) {
        if (!cond.enabled) continue;
        if (!evaluate_condition(cond)) {
            if (!first) oss << ",";
            oss << "{\"id\":\"" << cond.id << "\",\"desc\":\"" << cond.description << "\"}";
            first = false;
        }
    }
    oss << "]";
    active_interlocks_ = oss.str();
}

double InterlockService::read_device_position(const std::string &device_id) {
    auto it = position_cache_.find(device_id);
    if (it != position_cache_.end() && it->second.valid) return it->second.position;
    double val = 0.0;
    if (read_device_position_safe(device_id, val)) {
        position_cache_[device_id].position = val;
        position_cache_[device_id].valid = true;
        position_cache_[device_id].last_update = time(nullptr);
        return val;
    }
    return 0.0;
}

bool InterlockService::read_device_position_safe(const std::string &device_id, double &value) {
    bool ok = false;
    value = read_device_value(device_id, default_position_attr(device_id), ok);
    return ok;
}

double InterlockService::read_device_attribute(const std::string &tango_name, const std::string &attr) {
#ifdef HAS_TANGO
    try {
        Tango::DeviceAttribute da = Tango::DeviceProxy(tango_name).read_attribute(attr);
        double v = 0.0; da >> v; return v;
    } catch (...) {
        return 0.0;
    }
#else
    (void)tango_name; (void)attr; return 0.0;
#endif
}

double InterlockService::read_device_value(const std::string &device_id, const std::string &attr, bool &ok) {
#ifdef HAS_TANGO
    auto it_proxy = device_proxies_.find(device_id);
    std::string tango_name = (it_proxy != device_proxies_.end() && it_proxy->second) ? it_proxy->second->dev_name() : getDeviceTangoName(device_id);
    if (tango_name.empty()) { ok = false; return 0.0; }
    try {
        Tango::DeviceAttribute da = Tango::DeviceProxy(tango_name).read_attribute(attr.empty() ? default_position_attr(device_id) : attr);
        double v = 0.0; da >> v; ok = true; return v;
    } catch (...) {
        ok = false; return 0.0;
    }
#else
    (void)device_id; (void)attr; ok = true; return 0.0;
#endif
}

bool InterlockService::isAtZeroPosition(const std::string &device_id, double threshold) {
    bool ok = false;
    double v = read_device_value(device_id, default_position_attr(device_id), ok);
    return ok && std::abs(v) <= threshold;
}

bool InterlockService::isNotAtZeroPosition(const std::string &device_id, double threshold) { return !isAtZeroPosition(device_id, threshold); }

std::string InterlockService::getDeviceTangoName(const std::string &device_id) {
    auto it = device_tango_names_.find(device_id);
    if (it != device_tango_names_.end()) return it->second;
    return "";
}

void InterlockService::addConditionInternal(const InterlockCondition &cond) { conditions_.push_back(cond); }

// ===== CLASS FACTORY =====
InterlockServiceClass *InterlockServiceClass::_instance = nullptr;

InterlockServiceClass *InterlockServiceClass::instance() {
    if (_instance == nullptr) {
        std::string class_name = "InterlockService";
        _instance = new InterlockServiceClass(class_name);
    }
    return _instance;
}

InterlockServiceClass::InterlockServiceClass(std::string &class_name) : Tango::DeviceClass(class_name) {}

void InterlockServiceClass::attribute_factory(std::vector<Tango::Attr *> &att_list) {
    att_list.push_back(new Tango::Attr("selfCheckResult", Tango::DEV_LONG, Tango::READ));
    att_list.push_back(new Tango::Attr("groupAttributeJson", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::Attr("interlockEnabled", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new Tango::Attr("emergencyStopped", Tango::DEV_BOOLEAN, Tango::READ));
    att_list.push_back(new Tango::Attr("interlockLevel", Tango::DEV_SHORT, Tango::READ));
    att_list.push_back(new Tango::Attr("interlockLogs", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::Attr("alarmState", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::Attr("activeInterlocks", Tango::DEV_STRING, Tango::READ));
    att_list.push_back(new Tango::Attr("resultValue", Tango::DEV_SHORT, Tango::READ));
}

void InterlockServiceClass::command_factory() {
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("devLock", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&InterlockService::devLock)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevBoolean>("devUnlock", static_cast<void (Tango::DeviceImpl::*)(Tango::DevBoolean)>(&InterlockService::devUnlock)));
    command_list.push_back(new Tango::TemplCommand("devLockVerify", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::devLockVerify)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("devLockQuery", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&InterlockService::devLockQuery)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("devUserConfig", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&InterlockService::devUserConfig)));

    command_list.push_back(new Tango::TemplCommand("selfCheck", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::selfCheck)));
    command_list.push_back(new Tango::TemplCommand("init", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::init)));
    command_list.push_back(new Tango::TemplCommand("reset", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::reset)));

    command_list.push_back(new Tango::TemplCommand("enableInterlock", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::enableInterlock)));
    command_list.push_back(new Tango::TemplCommand("disableInterlock", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::disableInterlock)));
    command_list.push_back(new Tango::TemplCommand("emergencyStop", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::emergencyStop)));
    command_list.push_back(new Tango::TemplCommand("releaseEmergencyStop", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::releaseEmergencyStop)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevBoolean>("checkInterlocks", static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)()>(&InterlockService::checkInterlocks)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevString, Tango::DevBoolean>("checkMotionAllowed", static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)(Tango::DevString)>(&InterlockService::checkMotionAllowed)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevBoolean>("checkValveCloseAllowed", static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)()>(&InterlockService::checkValveCloseAllowed)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevBoolean>("checkShieldOperateAllowed", static_cast<Tango::DevBoolean (Tango::DeviceImpl::*)()>(&InterlockService::checkShieldOperateAllowed)));
    command_list.push_back(new Tango::TemplCommandInOut<Tango::DevString, Tango::DevDouble>("getMaxAllowedPosition", static_cast<Tango::DevDouble (Tango::DeviceImpl::*)(Tango::DevString)>(&InterlockService::getMaxAllowedPosition)));

    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("addCondition", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&InterlockService::addCondition)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevString>("removeCondition", static_cast<void (Tango::DeviceImpl::*)(Tango::DevString)>(&InterlockService::removeCondition)));
    command_list.push_back(new Tango::TemplCommand("clearAllConditions", static_cast<void (Tango::DeviceImpl::*)()>(&InterlockService::clearAllConditions)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("getConditions", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&InterlockService::getConditions)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("getActiveInterlocks", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&InterlockService::getActiveInterlocks)));
    command_list.push_back(new Tango::TemplCommandOut<Tango::DevString>("getInterlockHistory", static_cast<Tango::DevString (Tango::DeviceImpl::*)()>(&InterlockService::getInterlockHistory)));
    command_list.push_back(new Tango::TemplCommandIn<Tango::DevShort>("setInterlockLevel", static_cast<void (Tango::DeviceImpl::*)(Tango::DevShort)>(&InterlockService::setInterlockLevel)));
}

void InterlockServiceClass::device_factory(const Tango::DevVarStringArray *devlist_ptr) {
    for (unsigned long i = 0; i < devlist_ptr->length(); i++) {
        std::string dev_name = (*devlist_ptr)[i].in();
        InterlockService *dev = new InterlockService(this, dev_name);
        device_list.push_back(dev);
        export_device(dev);
    }
}

} // namespace Interlock

// Main function
void Tango::DServer::class_factory() { add_class(Interlock::InterlockServiceClass::instance()); }

int main(int argc, char *argv[]) {
    try {
        Common::SystemConfig::loadConfig();
        Tango::Util *tg = Tango::Util::init(argc, argv);
        tg->server_init();
        std::cout << "Interlock Server Ready" << std::endl;
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
