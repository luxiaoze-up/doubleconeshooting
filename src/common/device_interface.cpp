#include "common/device_interface.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace Common {

// DeviceProxyFactory implementation (simplified version)
bool DeviceProxyFactory::testConnection(const std::string& device_name) {
    // Simulate connection test
    std::cout << "Testing connection to device: " << device_name << std::endl;
    return true; // Always return true for simulation
}

std::vector<std::string> DeviceProxyFactory::getAvailableDevices() {
    return {
        "sys/target_transport/1",
        "sys/six_dof/1",
        "sys/vacuum/1",
        "sys/interlock/1"
    };
}

std::string DeviceProxyFactory::getDeviceInfo(const std::string& device_name) {
    return "Mock device info for: " + device_name;
}

// DeviceError implementation
DeviceError::DeviceError(ErrorType type, const std::string& message, const std::string& device)
    : std::runtime_error(message), type_(type), device_(device) {
}

// MockDevice implementation
MockDevice::MockDevice(const std::string& name) 
    : position_(0.0), velocity_(1.0), is_moving_(false), 
      status_(DeviceStatus::INIT), name_(name) {
}

void MockDevice::initialize() {
    std::cout << "Initializing mock device: " << name_ << std::endl;
    status_ = DeviceStatus::ON;
}

void MockDevice::shutdown() {
    std::cout << "Shutting down mock device: " << name_ << std::endl;
    status_ = DeviceStatus::OFF;
}

DeviceStatus MockDevice::getStatus() const {
    return status_;
}

std::string MockDevice::getStatusString() const {
    switch (status_) {
        case DeviceStatus::INIT: return "INIT";
        case DeviceStatus::ON: return "ON";
        case DeviceStatus::OFF: return "OFF";
        case DeviceStatus::MOVING: return "MOVING";
        case DeviceStatus::ALARM: return "ALARM";
        case DeviceStatus::FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}

bool MockDevice::isReady() const {
    return status_ == DeviceStatus::ON && !is_moving_;
}

void MockDevice::stop() {
    std::cout << "Stopping mock device: " << name_ << std::endl;
    is_moving_ = false;
    status_ = DeviceStatus::ON;
}

void MockDevice::reset() {
    std::cout << "Resetting mock device: " << name_ << std::endl;
    position_ = 0.0;
    is_moving_ = false;
    status_ = DeviceStatus::ON;
}

void MockDevice::moveToPosition(double position) {
    std::cout << "Mock device " << name_ << " moving to position: " << position << std::endl;
    status_ = DeviceStatus::MOVING;
    is_moving_ = true;
    
    // Simulate movement time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    position_ = position;
    is_moving_ = false;
    status_ = DeviceStatus::ON;
}

double MockDevice::getCurrentPosition() const {
    return position_;
}

bool MockDevice::isMoving() const {
    return is_moving_;
}

void MockDevice::setVelocity(double velocity) {
    velocity_ = velocity;
    std::cout << "Mock device " << name_ << " velocity set to: " << velocity << std::endl;
}

double MockDevice::getVelocity() const {
    return velocity_;
}

// MockMultiAxisDevice implementation
MockMultiAxisDevice::MockMultiAxisDevice(const std::string& name, int axis_count)
    : is_moving_(false), status_(DeviceStatus::INIT), name_(name), axis_count_(axis_count) {
    positions_.resize(axis_count, 0.0);
}

void MockMultiAxisDevice::initialize() {
    std::cout << "Initializing mock multi-axis device: " << name_ << std::endl;
    status_ = DeviceStatus::ON;
}

void MockMultiAxisDevice::shutdown() {
    std::cout << "Shutting down mock multi-axis device: " << name_ << std::endl;
    status_ = DeviceStatus::OFF;
}

DeviceStatus MockMultiAxisDevice::getStatus() const { return status_; }

std::string MockMultiAxisDevice::getStatusString() const {
    return (status_ == DeviceStatus::ON) ? "ON" : "OFF";
}

bool MockMultiAxisDevice::isReady() const {
    return status_ == DeviceStatus::ON && !is_moving_;
}

void MockMultiAxisDevice::stop() {
    std::cout << "Stopping mock multi-axis device: " << name_ << std::endl;
    is_moving_ = false;
}

void MockMultiAxisDevice::reset() {
    std::cout << "Resetting mock multi-axis device: " << name_ << std::endl;
    std::fill(positions_.begin(), positions_.end(), 0.0);
    is_moving_ = false;
}

void MockMultiAxisDevice::moveToPosition(const std::vector<double>& positions) {
    if (positions.size() != positions_.size()) {
        std::cerr << "Dimension mismatch in mock move" << std::endl;
        return;
    }
    std::cout << "Mock multi-axis device " << name_ << " moving..." << std::endl;
    is_moving_ = true;
    // Simulate movement
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    positions_ = positions;
    is_moving_ = false;
}

std::vector<double> MockMultiAxisDevice::getCurrentPosition() const {
    return positions_;
}

bool MockMultiAxisDevice::isMoving() const { return is_moving_; }

int MockMultiAxisDevice::getAxisCount() const { return axis_count_; }


// MockVacuumDevice implementation
MockVacuumDevice::MockVacuumDevice(const std::string& name)
    : pressure_(101325.0), target_pressure_(1e-5), is_pumping_(false), 
      gate_valve_open_(false), status_(DeviceStatus::INIT), name_(name) {
}

void MockVacuumDevice::initialize() {
    std::cout << "Initializing mock vacuum device: " << name_ << std::endl;
    status_ = DeviceStatus::ON;
}

void MockVacuumDevice::shutdown() {
    std::cout << "Shutting down mock vacuum device: " << name_ << std::endl;
    status_ = DeviceStatus::OFF;
}

DeviceStatus MockVacuumDevice::getStatus() const { return status_; }

std::string MockVacuumDevice::getStatusString() const {
    return (status_ == DeviceStatus::ON) ? "ON" : "OFF";
}

bool MockVacuumDevice::isReady() const { return status_ == DeviceStatus::ON; }

void MockVacuumDevice::stop() { stopPumping(); }

void MockVacuumDevice::reset() { vent(); }

void MockVacuumDevice::startPumping() {
    std::cout << "Mock vacuum " << name_ << " start pumping" << std::endl;
    is_pumping_ = true;
    // Simulate pressure drop
    pressure_ = target_pressure_; 
}

void MockVacuumDevice::stopPumping() {
    std::cout << "Mock vacuum " << name_ << " stop pumping" << std::endl;
    is_pumping_ = false;
}

void MockVacuumDevice::vent() {
    std::cout << "Mock vacuum " << name_ << " venting" << std::endl;
    is_pumping_ = false;
    pressure_ = 101325.0;
}

double MockVacuumDevice::getPressure() const { return pressure_; }

bool MockVacuumDevice::isPumping() const { return is_pumping_; }

void MockVacuumDevice::setTargetPressure(double pressure) { target_pressure_ = pressure; }

double MockVacuumDevice::getTargetPressure() const { return target_pressure_; }

void MockVacuumDevice::openGateValve() {
    std::cout << "[DEBUG] Mock vacuum " << name_ << " opening gate valve" << std::endl;
    gate_valve_open_ = true;
    std::cout << "[OK] Gate valve opened" << std::endl;
}

void MockVacuumDevice::closeGateValve() {
    std::cout << "[DEBUG] Mock vacuum " << name_ << " closing gate valve" << std::endl;
    gate_valve_open_ = false;
    std::cout << "[OK] Gate valve closed" << std::endl;
}

bool MockVacuumDevice::isGateValveOpen() const { return gate_valve_open_; }


// MockInterlockService implementation
MockInterlockService::MockInterlockService(const std::string& name)
    : interlock_active_(true), status_(DeviceStatus::INIT), name_(name) {
}

void MockInterlockService::initialize() {
    std::cout << "Initializing mock interlock service: " << name_ << std::endl;
    status_ = DeviceStatus::ON;
}

void MockInterlockService::shutdown() {
    std::cout << "Shutting down mock interlock service: " << name_ << std::endl;
    status_ = DeviceStatus::OFF;
}

DeviceStatus MockInterlockService::getStatus() const { return status_; }

std::string MockInterlockService::getStatusString() const {
    return (status_ == DeviceStatus::ON) ? "ON" : "OFF";
}

bool MockInterlockService::isReady() const { return status_ == DeviceStatus::ON; }

void MockInterlockService::stop() { }

void MockInterlockService::reset() { interlock_active_ = true; }

bool MockInterlockService::checkInterlocks() { return interlock_active_; }

void MockInterlockService::emergencyStop() {
    std::cout << "Mock interlock " << name_ << " EMERGENCY STOP!" << std::endl;
    interlock_active_ = false; // Or true depending on logic, usually stop means interlock broken/active
}

bool MockInterlockService::isInterlockActive() const { return interlock_active_; }

void MockInterlockService::setInterlockActive(bool active) {
    interlock_active_ = active;
    std::cout << "Mock interlock " << name_ << " set to " << (active ? "ACTIVE" : "INACTIVE") << std::endl;
}

} // namespace Common
