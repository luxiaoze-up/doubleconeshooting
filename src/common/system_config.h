#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <string>

namespace SystemConfig {
    // Unified configuration for server IPs
    // User requested to change Tango server IP to 192.168.20.32
    // This configuration is used as the default value if not specified in the Tango Database properties.
    
    extern std::string DEFAULT_CONTROLLER_IP;
    extern std::string DEFAULT_PLC_IP;
    extern std::string DEFAULT_TANGO_HOST;

    void loadConfig(const std::string& config_path = "../config/system_config.json");
}

#endif // SYSTEM_CONFIG_H
