#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <string>
#include <vector>

namespace Common {
namespace SystemConfig {

// Load configuration from JSON file
void loadConfig(const std::string& config_path = "../config/system_config.json");

// 修改命令行参数，添加 -ORBendPoint 避免 CORBA 网络超时
// 必须在 Tango::Util::init() 之前调用
// 返回修改后的 argc/argv（使用静态存储，生命周期与程序相同）
void fixOrbEndpoint(int& argc, char**& argv);

// 默认控制器IP地址(用于运动控制器和六自由度平台)
extern std::string DEFAULT_CONTROLLER_IP;

// 默认PLC IP地址(用于真空系统等)
extern std::string DEFAULT_PLC_IP;

// Tango数据库配置
extern std::string DEFAULT_TANGO_HOST;

// 模拟模式 (true=不连接真实 PLC，使用内部模拟)
extern bool SIM_MODE;

// 统一 Proxy 断线重连间隔（秒）
// 用于依赖 Tango::DeviceProxy 的设备（如六自由度/大行程/辅助支撑等）在 hook 中做节流重连。
// 可通过 system_config.json 的 proxy_reconnect_interval_sec 覆盖。
extern int PROXY_RECONNECT_INTERVAL_SEC;

// 运行时配置管理（用于保存用户通过GUI/命令切换的模拟模式选择）
// 优先级：运行时配置 > 主配置文件 > 默认值
// 
// 读取运行时配置中的模拟模式设置
// 如果运行时配置文件不存在或读取失败，返回 false（表示使用主配置）
// 返回值：true=成功读取到运行时配置，false=使用主配置
bool loadRuntimeSimMode(bool& sim_mode);

// 保存模拟模式到运行时配置文件
// 返回值：true=保存成功，false=保存失败（但不影响当前运行）
bool saveRuntimeSimMode(bool sim_mode);

} // namespace SystemConfig
} // namespace Common

#endif // SYSTEM_CONFIG_H
