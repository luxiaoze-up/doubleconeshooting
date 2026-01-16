#include "common/standard_system_device.h"
#include <iostream>
#include <stdexcept>

namespace Common {

StandardSystemDevice::StandardSystemDevice(Tango::DeviceClass *cl, std::string &name)
    : Tango::Device_4Impl(cl, name.c_str()), is_locked_(false) {
    init_device();
}

StandardSystemDevice::StandardSystemDevice(Tango::DeviceClass *cl, const char *name)
    : Tango::Device_4Impl(cl, name), is_locked_(false) {
    init_device();
}

StandardSystemDevice::StandardSystemDevice(Tango::DeviceClass *cl, const char *name, const char *description)
    : Tango::Device_4Impl(cl, name, description), is_locked_(false) {
    init_device();
}

StandardSystemDevice::~StandardSystemDevice() {
    delete_device();
}

void StandardSystemDevice::init_device() {
    // Base initialization
    is_locked_ = false;
    locker_info_ = "";
    self_check_result_ = -1;  // Not checked yet
    
    // Read standard properties
    Tango::DbData db_data;
    db_data.push_back(Tango::DbDatum("bundleNo"));
    db_data.push_back(Tango::DbDatum("laserNo"));
    db_data.push_back(Tango::DbDatum("systemNo"));
    db_data.push_back(Tango::DbDatum("subDevList"));
    db_data.push_back(Tango::DbDatum("errorDict"));
    get_db_device()->get_property(db_data);
    
    if (!db_data[0].is_empty()) db_data[0] >> bundle_no_;
    if (!db_data[1].is_empty()) db_data[1] >> laser_no_;
    if (!db_data[2].is_empty()) db_data[2] >> system_no_;
    if (!db_data[3].is_empty()) db_data[3] >> sub_dev_list_;
    if (!db_data[4].is_empty()) db_data[4] >> error_dict_;
}

void StandardSystemDevice::delete_device() {
    // Base cleanup
}

void StandardSystemDevice::dev_lock(Tango::DevString argin) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string client_info = argin;
    
    if (is_locked_) {
        if (locker_info_ == client_info) {
            return; // Already locked by same user
        }
        Tango::Except::throw_exception("Locked", "Device is already locked by another user: " + locker_info_, "StandardSystemDevice::dev_lock");
    }
    
    is_locked_ = true;
    locker_info_ = client_info;
    INFO_STREAM << "Device locked by: " << locker_info_ << std::endl;
}

void StandardSystemDevice::dev_unlock(Tango::DevBoolean argin) {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    bool force_unlock_all = argin;
    (void)force_unlock_all; // Silence unused warning
    
    // In a real implementation, we should check if the caller is the locker or an admin.
    // For now, we trust the logic or assume force_unlock_all is admin privilege.
    
    if (is_locked_) {
        is_locked_ = false;
        locker_info_ = "";
        INFO_STREAM << "Device unlocked" << std::endl;
    }
}

void StandardSystemDevice::dev_lock_verify() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    if (!is_locked_) {
        Tango::Except::throw_exception("NotLocked", "Device is not locked", "StandardSystemDevice::dev_lock_verify");
    }
    // Here we could verify if the caller matches the locker, but Tango doesn't easily give caller identity in this context without extra setup.
}

Tango::DevString StandardSystemDevice::dev_lock_query() {
    std::lock_guard<std::mutex> lock(lock_mutex_);
    std::string result;
    if (is_locked_) {
        result = "{\"locked\": true, \"locker\": \"" + locker_info_ + "\"}";
    } else {
        result = "{\"locked\": false}";
    }
    
    // Allocate string for Tango
    return Tango::string_dup(result.c_str());
}

void StandardSystemDevice::dev_user_config(Tango::DevString argin) {
    // Placeholder for user config logic
    INFO_STREAM << "User config received: " << argin << std::endl;
}

void StandardSystemDevice::self_check() {
    INFO_STREAM << "Starting self check..." << std::endl;
    self_check_result_ = -1;  // Reset to "in progress"
    
    try {
        specific_self_check();
        self_check_result_ = 0;  // Success
        INFO_STREAM << "Self check completed successfully." << std::endl;
    } catch (const Tango::DevFailed &e) {
        self_check_result_ = 1;  // Generic error
        ERROR_STREAM << "Self check failed: " << e.errors[0].desc << std::endl;
        throw;
    } catch (...) {
        self_check_result_ = 1;  // Generic error
        ERROR_STREAM << "Self check failed with unknown error" << std::endl;
        throw;
    }
}

void StandardSystemDevice::read_self_check_result(Tango::Attribute &attr) {
    attr.set_value(&self_check_result_);
}

void StandardSystemDevice::init_cmd() {
    // Re-initialize device
    init_device();
}

void StandardSystemDevice::reset() {
    INFO_STREAM << "StandardSystemDevice::reset() called" << std::endl;
    // Default implementation: clear errors, maybe re-init
    // For now, just log
}

void StandardSystemDevice::check_lock_access() {
    // Helper to be called by other commands to ensure lock
    // For now, we might just check if it IS locked, or if we implement user checking, check that too.
    // If the system requires locking before operation:
    /*
    if (!is_locked_) {
         Tango::Except::throw_exception("AccessDenied", "Device must be locked before operation", "StandardSystemDevice::check_lock_access");
    }
    */
    // But often read-only is allowed. This depends on the command.
}

} // namespace Common
