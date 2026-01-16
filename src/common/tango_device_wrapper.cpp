#include "common/tango_device_wrapper.h"
#include <iostream>

// 辅助宏：从 DevFailed 获取错误描述（兼容不同 Tango 版本）
#define TANGO_ERROR_DESC(e) (std::string(e.errors[0].desc))

#ifdef HAS_TANGO

namespace Common {

// --- TangoDeviceBase ---

TangoDeviceBase::TangoDeviceBase(const std::string& device_name) 
    : device_name_(device_name) {
}

void TangoDeviceBase::initialize() {
    try {
        proxy_ = std::make_unique<Tango::DeviceProxy>(device_name_);
        proxy_->ping();
    } catch (Tango::DevFailed& e) {
        throw DeviceError(DeviceError::CONNECTION_ERROR, 
            "Failed to connect to " + device_name_ + ": " + TANGO_ERROR_DESC(e), device_name_);
    }
}

void TangoDeviceBase::shutdown() {
    proxy_.reset();
}

DeviceStatus TangoDeviceBase::getStatus() const {
    if (!proxy_) return DeviceStatus::UNKNOWN;
    try {
        Tango::DevState state = proxy_->state();
        switch (state) {
            case Tango::ON: return DeviceStatus::ON;
            case Tango::OFF: return DeviceStatus::OFF;
            case Tango::MOVING: return DeviceStatus::MOVING;
            case Tango::ALARM: return DeviceStatus::ALARM;
            case Tango::FAULT: return DeviceStatus::FAULT;
            case Tango::INIT: return DeviceStatus::INIT;
            default: return DeviceStatus::UNKNOWN;
        }
    } catch (...) {
        std::cerr << "Error in getStatus for " << device_name_ << std::endl;
        return DeviceStatus::UNKNOWN;
    }
}

std::string TangoDeviceBase::getStatusString() const {
    if (!proxy_) return "DISCONNECTED";
    try {
        // Tango::DeviceProxy::status() 返回 std::string (Tango 9+)
        // 对于旧版本可能返回 char* 或 CORBA::String_var
        std::string status_str = proxy_->status();
        return status_str;
    } catch (...) {
        std::cerr << "Error in getStatusString for " << device_name_ << std::endl;
        return "ERROR";
    }
}

bool TangoDeviceBase::isReady() const {
    return getStatus() == DeviceStatus::ON;
}

void TangoDeviceBase::stop() {
    if (!proxy_) return;
    try {
        proxy_->command_inout("stop");
    } catch (Tango::DevFailed& e) {
        throw DeviceError(DeviceError::COMMAND_ERROR, "Stop failed: " + TANGO_ERROR_DESC(e), device_name_);
    }
}

void TangoDeviceBase::reset() {
    if (!proxy_) return;
    try {
        proxy_->command_inout("reset");
    } catch (Tango::DevFailed& e) {
        throw DeviceError(DeviceError::COMMAND_ERROR, "Reset failed: " + TANGO_ERROR_DESC(e), device_name_);
    }
}

// --- TangoMotionDevice ---

TangoMotionDevice::TangoMotionDevice(const std::string& device_name)
    : TangoDeviceBase(device_name) {}

void TangoMotionDevice::moveToPosition(double position) {
    std::cout << "[TangoMotion] moveToPosition: " << position << " on " << device_name_ << std::endl;
    if (!proxy_) throw DeviceError(DeviceError::CONNECTION_ERROR, "Not connected", device_name_);
    try {
        Tango::DeviceData data;
        data << position;
        // Try standard command names used by our devices
        try {
            proxy_->command_inout("moveAbsolute", data);  // LargeStrokeDevice, MotionControllerDevice
        } catch (Tango::DevFailed&) {
            try {
                proxy_->command_inout("move_absolute", data);  // Legacy format
            } catch (Tango::DevFailed&) {
                proxy_->command_inout("move_to", data);  // Fallback
            }
        }
    } catch (Tango::DevFailed& e) {
        throw DeviceError(DeviceError::COMMAND_ERROR, "Move failed: " + TANGO_ERROR_DESC(e), device_name_);
    }
}

double TangoMotionDevice::getCurrentPosition() const {
    if (!proxy_) return 0.0;
    try {
        // Try different attribute names used by our devices
        Tango::DeviceAttribute attr;
        double pos = 0.0;
        try {
            attr = proxy_->read_attribute("largeRangePos");  // LargeStrokeDevice
            attr >> pos;
        } catch (Tango::DevFailed&) {
            try {
                attr = proxy_->read_attribute("position");  // Generic
                attr >> pos;
            } catch (Tango::DevFailed&) {
                attr = proxy_->read_attribute("currentPosition");  // Alternative
                attr >> pos;
            }
        }
        std::cout << "[TangoMotion] getCurrentPosition: " << pos << " from " << device_name_ << std::endl;
        return pos;
    } catch (Tango::DevFailed &e) {
        std::cerr << "Error in getCurrentPosition for " << device_name_ << ": " << TANGO_ERROR_DESC(e) << std::endl;
        return 0.0;
    } catch (...) { 
        std::cerr << "Error in getCurrentPosition for " << device_name_ << ": Unknown exception" << std::endl;
        return 0.0; 
    }
}

bool TangoMotionDevice::isMoving() const {
    if (!proxy_) return false;
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("is_moving");
        bool moving;
        attr >> moving;
        return moving;
    } catch (...) { 
        std::cerr << "Error in isMoving for " << device_name_ << std::endl;
        return false; 
    }
}

void TangoMotionDevice::setVelocity(double velocity) {
    if (!proxy_) return;
    try {
        Tango::DeviceAttribute attr("velocity", velocity);
        proxy_->write_attribute(attr);
    } catch (...) {
        std::cerr << "Error in setVelocity for " << device_name_ << std::endl;
    }
}

double TangoMotionDevice::getVelocity() const {
    if (!proxy_) return 0.0;
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("velocity");
        double vel;
        attr >> vel;
        return vel;
    } catch (...) { 
        std::cerr << "Error in getVelocity for " << device_name_ << std::endl;
        return 0.0; 
    }
}

// --- TangoMultiAxisDevice ---

TangoMultiAxisDevice::TangoMultiAxisDevice(const std::string& device_name)
    : TangoDeviceBase(device_name) {}

void TangoMultiAxisDevice::moveToPosition(const std::vector<double>& positions) {
    std::cout << "[TangoMultiAxis] moveToPosition size=" << positions.size() << " on " << device_name_ << std::endl;
    if (!proxy_) throw DeviceError(DeviceError::CONNECTION_ERROR, "Not connected", device_name_);
    try {
        Tango::DeviceData data;
        data << positions;
        // Try specific commands based on device type or just try-catch chain
        try {
            proxy_->command_inout("move_pose_absolute", data); // For SixDof
        } catch (Tango::DevFailed&) {
            try {
                proxy_->command_inout("move_all", data); // For MultiAxis
            } catch (Tango::DevFailed&) {
                proxy_->command_inout("move_to", data); // Legacy
            }
        }
    } catch (Tango::DevFailed& e) {
        throw DeviceError(DeviceError::COMMAND_ERROR, "Move failed: " + TANGO_ERROR_DESC(e), device_name_);
    }
}

std::vector<double> TangoMultiAxisDevice::getCurrentPosition() const {
    if (!proxy_) return {};
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("position");
        Tango::DevVarDoubleArray* pos_array;
        attr >> pos_array;
        std::vector<double> res;
        for(unsigned int i=0; i<pos_array->length(); ++i) res.push_back((*pos_array)[i]);
        std::cout << "[TangoMultiAxis] getCurrentPosition size=" << res.size() << " from " << device_name_ << std::endl;
        return res;
    } catch (Tango::DevFailed &e) {
        std::cerr << "Error in getCurrentPosition (MultiAxis) for " << device_name_ << ": " << TANGO_ERROR_DESC(e) << std::endl;
        return {};
    } catch (...) { 
        std::cerr << "Error in getCurrentPosition (MultiAxis) for " << device_name_ << ": Unknown exception" << std::endl;
        return {}; 
    }
}

bool TangoMultiAxisDevice::isMoving() const {
    if (!proxy_) return false;
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("is_moving");
        bool moving;
        attr >> moving;
        return moving;
    } catch (...) { 
        std::cerr << "Error in isMoving (MultiAxis) for " << device_name_ << std::endl;
        return false; 
    }
}

int TangoMultiAxisDevice::getAxisCount() const {
    if (!proxy_) return 0;
    try {
        // Determine axis count by reading position attribute dimension
        Tango::DeviceAttribute attr = proxy_->read_attribute("position");
        if (attr.get_quality() == Tango::ATTR_INVALID) return 0;
        return attr.get_dim_x();
    } catch (...) {
        return 0;
    }
}

// --- TangoVacuumDevice ---

TangoVacuumDevice::TangoVacuumDevice(const std::string& device_name)
    : TangoDeviceBase(device_name) {}

void TangoVacuumDevice::startPumping() {
    std::cout << "[TangoVacuum] startPumping on " << device_name_ << std::endl;
    if (!proxy_) return;
    try { proxy_->command_inout("start_pumping"); } catch(...) {}
}

void TangoVacuumDevice::stopPumping() {
    if (!proxy_) return;
    try { proxy_->command_inout("stop_pumping"); } catch(...) {}
}

void TangoVacuumDevice::vent() {
    if (!proxy_) return;
    try { proxy_->command_inout("vent"); } catch(...) {}
}

double TangoVacuumDevice::getPressure() const {
    if (!proxy_) return 0.0;
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("pressure");
        double val;
        attr >> val;
        std::cout << "[TangoVacuum] getPressure: " << val << " from " << device_name_ << std::endl;
        return val;
    } catch (...) { return 0.0; }
}

bool TangoVacuumDevice::isPumping() const {
    if (!proxy_) return false;
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("is_pumping");
        bool val;
        attr >> val;
        return val;
    } catch (...) { return false; }
}

void TangoVacuumDevice::setTargetPressure(double pressure) {
    if (!proxy_) return;
    try {
        Tango::DeviceAttribute attr("target_pressure", pressure);
        proxy_->write_attribute(attr);
    } catch (...) {}
}

double TangoVacuumDevice::getTargetPressure() const {
    if (!proxy_) return 0.0;
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("target_pressure");
        double val;
        attr >> val;
        return val;
    } catch (...) { return 0.0; }
}

void TangoVacuumDevice::openGateValve() {
    std::cout << "[DEBUG] TangoVacuumDevice::openGateValve on " << device_name_ << std::endl;
    if (!proxy_) return;
    try {
        proxy_->command_inout("open_gate_valve");
        std::cout << "[OK] Gate valve open command sent" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] openGateValve failed: " << e.what() << std::endl;
    }
}

void TangoVacuumDevice::closeGateValve() {
    std::cout << "[DEBUG] TangoVacuumDevice::closeGateValve on " << device_name_ << std::endl;
    if (!proxy_) return;
    try {
        proxy_->command_inout("close_gate_valve");
        std::cout << "[OK] Gate valve close command sent" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] closeGateValve failed: " << e.what() << std::endl;
    }
}

bool TangoVacuumDevice::isGateValveOpen() const {
    if (!proxy_) return false;
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("gate_valve_open");
        bool val;
        attr >> val;
        return val;
    } catch (...) { return false; }
}

// --- TangoInterlockService ---

TangoInterlockService::TangoInterlockService(const std::string& device_name)
    : TangoDeviceBase(device_name) {}

bool TangoInterlockService::checkInterlocks() {
    std::cout << "[TangoInterlock] checkInterlocks on " << device_name_ << std::endl;
    if (!proxy_) return false;
    try {
        Tango::DeviceData result = proxy_->command_inout("check_interlocks");
        bool val;
        result >> val;
        return val;
    } catch (...) { return false; }
}

void TangoInterlockService::emergencyStop() {
    std::cout << "[TangoInterlock] emergencyStop on " << device_name_ << std::endl;
    if (!proxy_) return;
    try { proxy_->command_inout("emergency_stop"); } catch(...) {}
}

bool TangoInterlockService::isInterlockActive() const {
    if (!proxy_) return false;
    try {
        Tango::DeviceAttribute attr = proxy_->read_attribute("interlock_active");
        bool val;
        attr >> val;
        std::cout << "[TangoInterlock] isInterlockActive: " << (val ? "true" : "false") << " from " << device_name_ << std::endl;
        return val;
    } catch (...) { return false; }
}

void TangoInterlockService::setInterlockActive(bool active) {
    if (!proxy_) return;
    try {
        Tango::DeviceAttribute attr("interlock_active", active);
        proxy_->write_attribute(attr);
    } catch (...) {}
}

} // namespace Common

#endif // HAS_TANGO
