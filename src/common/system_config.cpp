#include "common/system_config.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
// Windows 兼容的 setenv
inline int setenv(const char* name, const char* value, int overwrite) {
    if (!overwrite && std::getenv(name) != nullptr) return 0;
    return _putenv_s(name, value);
}
#endif

#ifdef HAS_QT5
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <QTextCodec>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#endif

namespace Common {
namespace SystemConfig {

// Initialize with defaults
std::string DEFAULT_CONTROLLER_IP = "192.168.1.177";
std::string DEFAULT_PLC_IP = "192.168.1.177";
std::string DEFAULT_TANGO_HOST = "192.168.80.98:10000";
bool SIM_MODE = false;  // 默认关闭模拟模式
int PROXY_RECONNECT_INTERVAL_SEC = 5;

void loadConfig(const std::string& config_path) {
#ifdef HAS_QT5
    QString qPath = QString::fromStdString(config_path);
    QFile file(qPath);
    
    // Simple path check logic
    if (!file.exists()) {
        // If we are in build directory, try ../config/system_config.json
        // If we are in root, try config/system_config.json
        // The default passed is "../config/system_config.json"
        
        // Check if "config/system_config.json" exists (running from root)
        if (QFile::exists("config/system_config.json")) {
            qPath = "config/system_config.json";
            file.setFileName(qPath);
        }
    }

    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Warning: Could not open config file " << qPath.toStdString() << ". Using defaults." << std::endl;
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isNull()) {
        std::cerr << "Warning: Failed to parse config file " << qPath.toStdString() << ". Using defaults." << std::endl;
        return;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("controller_ip")) {
        DEFAULT_CONTROLLER_IP = obj["controller_ip"].toString().toStdString();
    }
    if (obj.contains("plc_ip")) {
        DEFAULT_PLC_IP = obj["plc_ip"].toString().toStdString();
    }
    if (obj.contains("tango_host")) {
        DEFAULT_TANGO_HOST = obj["tango_host"].toString().toStdString();
    }
    if (obj.contains("sim_mode")) {
        SIM_MODE = obj["sim_mode"].toBool();
    }
    if (obj.contains("proxy_reconnect_interval_sec")) {
        int v = obj["proxy_reconnect_interval_sec"].toInt(PROXY_RECONNECT_INTERVAL_SEC);
        // 过小会导致频繁重连刷日志；小于 1 统一按 1 秒处理
        PROXY_RECONNECT_INTERVAL_SEC = (v < 1) ? 1 : v;
    }
    
    std::cout << "Loaded configuration from " << qPath.toStdString() << std::endl;
    std::cout << "  Controller IP: " << DEFAULT_CONTROLLER_IP << std::endl;
    std::cout << "  PLC IP: " << DEFAULT_PLC_IP << std::endl;
    std::cout << "  Tango Host: " << DEFAULT_TANGO_HOST << std::endl;
    std::cout << "  Sim Mode: " << (SIM_MODE ? "ENABLED" : "disabled") << std::endl;
    std::cout << "  Proxy Reconnect Interval: " << PROXY_RECONNECT_INTERVAL_SEC << "s" << std::endl;

    // Set TANGO_HOST environment variable (use setenv for C library compatibility)
    // qputenv only works within Qt, but Tango uses getenv() directly
    setenv("TANGO_HOST", DEFAULT_TANGO_HOST.c_str(), 1);
    
    // 设置 omniORB 超时参数，避免 Tango 数据库不可达时长时间等待
    // 这些参数单位是毫秒，必须在 Tango::Util::init() 之前设置
    setenv("ORBclientCallTimeOutPeriod", "5000", 0);      // 客户端调用超时 5 秒
    setenv("ORBclientConnectTimeOutPeriod", "3000", 0);   // 客户端连接超时 3 秒
    std::cout << "  ORB Timeouts: call=5000ms, connect=3000ms" << std::endl;
#else
    // Qt5 not available - use default values
    std::cerr << "Warning: Qt5 not available, using default configuration values." << std::endl;
    std::cerr << "  Controller IP: " << DEFAULT_CONTROLLER_IP << std::endl;
    std::cerr << "  PLC IP: " << DEFAULT_PLC_IP << std::endl;
    std::cerr << "  Tango Host: " << DEFAULT_TANGO_HOST << std::endl;
    
    // Set TANGO_HOST environment variable
    setenv("TANGO_HOST", DEFAULT_TANGO_HOST.c_str(), 1);
    
    // 设置 omniORB 超时参数，避免 Tango 数据库不可达时长时间等待
    setenv("ORBclientCallTimeOutPeriod", "5000", 0);      // 客户端调用超时 5 秒
    setenv("ORBclientConnectTimeOutPeriod", "3000", 0);   // 客户端连接超时 3 秒
    std::cerr << "  ORB Timeouts: call=5000ms, connect=3000ms" << std::endl;
#endif
}

// 静态存储，保存修改后的 argv（生命周期与程序相同）
static std::vector<char*> s_fixed_argv;
static char s_orb_param[] = "-ORBendPoint";
static char s_orb_value[] = "giop:tcp:127.0.0.1:";

void fixOrbEndpoint(int& argc, char**& argv) {
    // 检查是否已有 -ORBendPoint 参数
    for (int i = 0; i < argc; ++i) {
        if (strstr(argv[i], "-ORBendPoint") != nullptr) {
            return;  // 已有参数，不需要添加
        }
    }
    
    // 复制原有参数并添加 -ORBendPoint
    s_fixed_argv.clear();
    for (int i = 0; i < argc; ++i) {
        s_fixed_argv.push_back(argv[i]);
    }
    s_fixed_argv.push_back(s_orb_param);
    s_fixed_argv.push_back(s_orb_value);
    s_fixed_argv.push_back(nullptr);
    
    argc = static_cast<int>(s_fixed_argv.size()) - 1;
    argv = s_fixed_argv.data();
}

// 获取运行时配置文件路径（尝试多个可能的位置）
static std::string getRuntimeConfigPath() {
#ifdef HAS_QT5
    // 尝试多个路径（与 loadConfig 的路径逻辑一致）
    QStringList possiblePaths = {
        "config/runtime_config.json",
        "../config/runtime_config.json",
        "./runtime_config.json"
    };
    
    for (const QString& path : possiblePaths) {
        if (QFile::exists(path)) {
            return path.toStdString();
        }
    }
    
    // 如果都不存在，返回最可能的路径（与 system_config.json 同目录）
    if (QFile::exists("config/system_config.json")) {
        return "config/runtime_config.json";
    } else if (QFile::exists("../config/system_config.json")) {
        return "../config/runtime_config.json";
    }
    
    return "config/runtime_config.json";  // 默认路径
#else
    // 没有 Qt5，使用简单路径
    return "config/runtime_config.json";
#endif
}

bool loadRuntimeSimMode(bool& sim_mode) {
#ifdef HAS_QT5
    std::string config_path = getRuntimeConfigPath();
    QString qPath = QString::fromStdString(config_path);
    QFile file(qPath);
    
    if (!file.exists()) {
        // 运行时配置文件不存在是正常的（首次运行或用户未切换过模式）
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Warning: Could not open runtime config file " << config_path << ". Using main config." << std::endl;
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isNull() || !doc.isObject()) {
        std::cerr << "Warning: Failed to parse runtime config file " << config_path << ". Using main config." << std::endl;
        return false;
    }
    
    QJsonObject obj = doc.object();
    
    if (obj.contains("sim_mode")) {
        sim_mode = obj["sim_mode"].toBool();
        std::cout << "Loaded runtime sim_mode from " << config_path << ": " << (sim_mode ? "ENABLED" : "disabled") << std::endl;
        return true;
    }
    
    return false;  // 配置文件中没有 sim_mode 字段
#else
    // Qt5 not available - cannot read runtime config
    return false;
#endif
}

bool saveRuntimeSimMode(bool sim_mode) {
#ifdef HAS_QT5
    std::string config_path = getRuntimeConfigPath();
    QString qPath = QString::fromStdString(config_path);
    
    // 确保目录存在
    QDir dir = QFileInfo(qPath).absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            std::cerr << "Warning: Could not create directory for runtime config: " << dir.absolutePath().toStdString() << std::endl;
            return false;
        }
    }
    
    QFile file(qPath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "Warning: Could not write runtime config file " << config_path << ": " 
                  << file.errorString().toStdString() << std::endl;
        return false;
    }
    
    // 读取现有配置（如果存在），保留其他字段
    QJsonObject obj;
    if (QFile::exists(qPath)) {
        QFile readFile(qPath);
        if (readFile.open(QIODevice::ReadOnly)) {
            QByteArray data = readFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.isObject()) {
                obj = doc.object();
            }
        }
    }
    
    // 更新 sim_mode
    obj["sim_mode"] = sim_mode;
    obj["_comment"] = "Runtime configuration - user's choice of simulation mode";
    QDateTime now = QDateTime::currentDateTime();
    obj["_last_updated"] = now.toString(Qt::ISODate);
    
    QJsonDocument doc(obj);
    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec(QTextCodec::codecForName("UTF-8"));
#endif
    out << doc.toJson(QJsonDocument::Indented);
    
    file.close();
    
    std::cout << "Saved runtime sim_mode to " << config_path << ": " << (sim_mode ? "ENABLED" : "disabled") << std::endl;
    return true;
#else
    // Qt5 not available - cannot save runtime config
    std::cerr << "Warning: Cannot save runtime config (Qt5 not available)" << std::endl;
    return false;
#endif
}

} // namespace SystemConfig
} // namespace Common
