#ifndef DEVICE_INTERFACE_H
#define DEVICE_INTERFACE_H

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

// Note: Tango forward declarations removed - include full headers where needed

namespace Common {

// 设备状态枚举
enum class DeviceStatus {
    INIT,
    ON,
    OFF,
    MOVING,
    ALARM,
    FAULT,
    UNKNOWN
};

// 设备接口基类
class IDevice {
public:
    virtual ~IDevice() = default;
    
    virtual void initialize() = 0;
    virtual void shutdown() = 0;
    virtual DeviceStatus getStatus() const = 0;
    virtual std::string getStatusString() const = 0;
    virtual bool isReady() const = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
};

// 运动设备接口
class IMotionDevice : public virtual IDevice {
public:
    virtual void moveToPosition(double position) = 0;
    virtual double getCurrentPosition() const = 0;
    virtual bool isMoving() const = 0;
    virtual void setVelocity(double velocity) = 0;
    virtual double getVelocity() const = 0;
};

// 多轴运动设备接口
class IMultiAxisDevice : public virtual IDevice {
public:
    virtual void moveToPosition(const std::vector<double>& positions) = 0;
    virtual std::vector<double> getCurrentPosition() const = 0;
    virtual bool isMoving() const = 0;
    virtual int getAxisCount() const = 0;
};

// 真空设备接口
class IVacuumDevice : public virtual IDevice {
public:
    virtual void startPumping() = 0;
    virtual void stopPumping() = 0;
    virtual void vent() = 0;
    virtual double getPressure() const = 0;
    virtual bool isPumping() const = 0;
    virtual void setTargetPressure(double pressure) = 0;
    virtual double getTargetPressure() const = 0;
    
    // 闸板阀控制
    virtual void openGateValve() = 0;
    virtual void closeGateValve() = 0;
    virtual bool isGateValveOpen() const = 0;
};

// 联锁服务接口
class IInterlockService : public virtual IDevice {
public:
    virtual bool checkInterlocks() = 0;
    virtual void emergencyStop() = 0;
    virtual bool isInterlockActive() const = 0;
    virtual void setInterlockActive(bool active) = 0;
};

// 设备代理工厂 (简化版本，不依赖Tango)
class DeviceProxyFactory {
public:
    static bool testConnection(const std::string& device_name);
    static std::vector<std::string> getAvailableDevices();
    static std::string getDeviceInfo(const std::string& device_name);
};

// 错误处理类
class DeviceError : public std::runtime_error {
public:
    enum ErrorType {
        CONNECTION_ERROR,
        COMMAND_ERROR,
        ATTRIBUTE_ERROR,
        TIMEOUT_ERROR,
        INTERLOCK_ERROR
    };
    
    DeviceError(ErrorType type, const std::string& message, const std::string& device = "");
    
    ErrorType getType() const { return type_; }
    std::string getDevice() const { return device_; }
    
private:
    ErrorType type_;
    std::string device_;
};

// 模拟设备类（用于测试）
class MockDevice : public IMotionDevice {
private:
    double position_;
    double velocity_;
    bool is_moving_;
    DeviceStatus status_;
    std::string name_;
    
public:
    explicit MockDevice(const std::string& name);
    
    // IDevice interface
    void initialize() override;
    void shutdown() override;
    DeviceStatus getStatus() const override;
    std::string getStatusString() const override;
    bool isReady() const override;
    void stop() override;
    void reset() override;
    
    // IMotionDevice interface
    void moveToPosition(double position) override;
    double getCurrentPosition() const override;
    bool isMoving() const override;
    void setVelocity(double velocity) override;
    double getVelocity() const override;
};

class MockMultiAxisDevice : public IMultiAxisDevice {
private:
    std::vector<double> positions_;
    bool is_moving_;
    DeviceStatus status_;
    std::string name_;
    int axis_count_;

public:
    MockMultiAxisDevice(const std::string& name, int axis_count);

    // IDevice interface
    void initialize() override;
    void shutdown() override;
    DeviceStatus getStatus() const override;
    std::string getStatusString() const override;
    bool isReady() const override;
    void stop() override;
    void reset() override;

    // IMultiAxisDevice interface
    void moveToPosition(const std::vector<double>& positions) override;
    std::vector<double> getCurrentPosition() const override;
    bool isMoving() const override;
    int getAxisCount() const override;
};

class MockVacuumDevice : public IVacuumDevice {
private:
    double pressure_;
    double target_pressure_;
    bool is_pumping_;
    bool gate_valve_open_;
    DeviceStatus status_;
    std::string name_;

public:
    explicit MockVacuumDevice(const std::string& name);

    // IDevice interface
    void initialize() override;
    void shutdown() override;
    DeviceStatus getStatus() const override;
    std::string getStatusString() const override;
    bool isReady() const override;
    void stop() override;
    void reset() override;

    // IVacuumDevice interface
    void startPumping() override;
    void stopPumping() override;
    void vent() override;
    double getPressure() const override;
    bool isPumping() const override;
    void setTargetPressure(double pressure) override;
    double getTargetPressure() const override;
    
    // Gate valve control
    void openGateValve() override;
    void closeGateValve() override;
    bool isGateValveOpen() const override;
};

class MockInterlockService : public IInterlockService {
private:
    bool interlock_active_;
    DeviceStatus status_;
    std::string name_;

public:
    explicit MockInterlockService(const std::string& name);

    // IDevice interface
    void initialize() override;
    void shutdown() override;
    DeviceStatus getStatus() const override;
    std::string getStatusString() const override;
    bool isReady() const override;
    void stop() override;
    void reset() override;

    // IInterlockService interface
    bool checkInterlocks() override;
    void emergencyStop() override;
    bool isInterlockActive() const override;
    void setInterlockActive(bool active) override;
};

} // namespace Common

#endif // DEVICE_INTERFACE_H
