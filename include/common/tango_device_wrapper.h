#ifndef TANGO_DEVICE_WRAPPER_H
#define TANGO_DEVICE_WRAPPER_H

// 定义 HAS_TANGO 宏（如果未定义）
#ifndef HAS_TANGO
#define HAS_TANGO 1
#endif

#ifdef HAS_TANGO
// Tango 完整头文件 - 必须在 device_interface.h 之前包含
// 以避免前向声明冲突
#include <bitset>  // 某些 Tango 头文件需要
#include <tango/tango.h>
// #include <tango/client/devapi.h>
#endif

#include "common/device_interface.h"
#include <memory>
#include <string>
#include <vector>

#ifdef HAS_TANGO

namespace Common {

class TangoDeviceBase : public virtual IDevice {
protected:
    std::unique_ptr<Tango::DeviceProxy> proxy_;
    std::string device_name_;

public:
    explicit TangoDeviceBase(const std::string& device_name);
    virtual ~TangoDeviceBase() = default;

    void initialize() override;
    void shutdown() override;
    DeviceStatus getStatus() const override;
    std::string getStatusString() const override;
    bool isReady() const override;
    void stop() override;
    void reset() override;
};

class TangoMotionDevice : public TangoDeviceBase, public IMotionDevice {
public:
    explicit TangoMotionDevice(const std::string& device_name);
    
    void moveToPosition(double position) override;
    double getCurrentPosition() const override;
    bool isMoving() const override;
    void setVelocity(double velocity) override;
    double getVelocity() const override;
};

class TangoMultiAxisDevice : public TangoDeviceBase, public IMultiAxisDevice {
public:
    explicit TangoMultiAxisDevice(const std::string& device_name);

    void moveToPosition(const std::vector<double>& positions) override;
    std::vector<double> getCurrentPosition() const override;
    bool isMoving() const override;
    int getAxisCount() const override;
};

class TangoVacuumDevice : public TangoDeviceBase, public IVacuumDevice {
public:
    explicit TangoVacuumDevice(const std::string& device_name);

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

class TangoInterlockService : public TangoDeviceBase, public IInterlockService {
public:
    explicit TangoInterlockService(const std::string& device_name);

    bool checkInterlocks() override;
    void emergencyStop() override;
    bool isInterlockActive() const override;
    void setInterlockActive(bool active) override;
};

} // namespace Common
#endif // HAS_TANGO

#endif // TANGO_DEVICE_WRAPPER_H
