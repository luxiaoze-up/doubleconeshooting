#ifndef STANDARD_SYSTEM_DEVICE_H
#define STANDARD_SYSTEM_DEVICE_H

#include <tango.h>
#include <string>
#include <mutex>

namespace Common {

class StandardSystemDevice : public Tango::Device_4Impl {
public:
    StandardSystemDevice(Tango::DeviceClass *cl, std::string &name);
    StandardSystemDevice(Tango::DeviceClass *cl, const char *name);
    StandardSystemDevice(Tango::DeviceClass *cl, const char *name, const char *description);
    virtual ~StandardSystemDevice();

    virtual void init_device();
    virtual void delete_device();

    // Standard Commands
    virtual void dev_lock(Tango::DevString argin);
    virtual void dev_unlock(Tango::DevBoolean argin);
    virtual void dev_lock_verify();
    virtual Tango::DevString dev_lock_query();
    virtual void dev_user_config(Tango::DevString argin);
    virtual void self_check();
    virtual void init_cmd(); // Renamed from init to avoid conflict with init_device
    virtual void reset();    // Added reset method

    // Attribute read methods
    virtual void read_self_check_result(Tango::Attribute &attr);

    // Helper for subclasses to check lock
    void check_lock_access();

protected:
    std::string locker_info_;
    bool is_locked_;
    std::mutex lock_mutex_;
    
    // Self-check result: <0=not checked, 0=OK, >0=error code
    Tango::DevLong self_check_result_;
    
    // Standard properties
    std::string bundle_no_;
    std::string laser_no_;
    std::string system_no_;
    std::string sub_dev_list_;   // JSON array
    std::string error_dict_;     // JSON object

    // Subclasses should override this for specific self-check logic
    virtual void specific_self_check() = 0;
};

} // namespace Common

#endif // STANDARD_SYSTEM_DEVICE_H
