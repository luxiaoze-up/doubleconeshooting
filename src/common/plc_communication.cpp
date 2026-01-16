#include "common/plc_communication.h"
#include <cstring>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace Common {
namespace PLC {

// ========== OPCUACommunication (open62541 implementation) ==========

OPCUACommunication::OPCUACommunication()
    : server_url_(""), connected_(false), client_(nullptr), reconnect_attempts_(0) {
#ifdef USE_OPEN62541
    client_ = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client_));
    std::cout << "[OPC-UA] Client created with open62541 library" << std::endl;
#else
    std::cout << "[OPC-UA] WARNING: open62541 not enabled, OPC UA communication disabled" << std::endl;
#endif
}

OPCUACommunication::~OPCUACommunication() {
    disconnect();
#ifdef USE_OPEN62541
    if (client_) {
        UA_Client_delete(client_);
        client_ = nullptr;
    }
#endif
}

bool OPCUACommunication::connect(const std::string& ip, int port) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    std::cout << "[OPC-UA] ========== OPC UA CONNECT ==========" << std::endl;
    std::cout << "[OPC-UA] Connecting to: " << ip << ":" << port << std::endl;
    
#ifdef USE_OPEN62541
    if (connected_) {
        std::cout << "[OPC-UA] Already connected, disconnecting first..." << std::endl;
        UA_Client_disconnect(client_);
        connected_ = false;
    }
    
    // Build OPC UA server URL
    std::ostringstream url_stream;
    url_stream << "opc.tcp://" << ip << ":" << port;
    server_url_ = url_stream.str();
    
    std::cout << "[OPC-UA] Server URL: " << server_url_ << std::endl;
    
    // Connect to the OPC UA server
    UA_StatusCode status = UA_Client_connect(client_, server_url_.c_str());
    
    if (status == UA_STATUSCODE_GOOD) {
        connected_ = true;
        reconnect_attempts_ = 0;
        std::cout << "[OPC-UA] SUCCESS: Connected to OPC UA server" << std::endl;
        std::cout << "[OPC-UA] ========================================" << std::endl;
        return true;
    } else {
        std::cerr << "[OPC-UA] FAILED: Connection failed with status: 0x" 
                  << std::hex << status << std::dec << std::endl;
        std::cerr << "[OPC-UA] Status message: " << UA_StatusCode_name(status) << std::endl;
        std::cout << "[OPC-UA] ========================================" << std::endl;
        return false;
    }
#else
    std::cerr << "[OPC-UA] ERROR: open62541 library not enabled" << std::endl;
    return false;
#endif
}

void OPCUACommunication::disconnect() {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    std::cout << "[OPC-UA] Disconnecting from OPC UA server..." << std::endl;
    
#ifdef USE_OPEN62541
    if (client_ && connected_) {
        UA_Client_disconnect(client_);
    }
#endif
    
    connected_ = false;
    std::cout << "[OPC-UA] Disconnected." << std::endl;
}

bool OPCUACommunication::isConnected() const {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    return connected_;
}

bool OPCUACommunication::attemptReconnect() {
    // 快速检查：如果已达到最大重试次数，立即返回（不打印日志，避免刷屏）
    if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        // 每 5 秒重置一次计数器，允许再次尝试（测试用，生产环境可改为 30 秒）
        static auto last_reset = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_reset).count();
        if (elapsed >= 5) {
            last_reset = now;
            reconnect_attempts_ = 0;
            std::cout << "[OPC-UA] Resetting reconnect counter after cooldown" << std::endl;
        } else {
            return false;  // 静默失败，不尝试重连
        }
    }
    
    std::cout << "[OPC-UA] Attempting reconnect... (attempt " << (reconnect_attempts_ + 1) 
              << "/" << MAX_RECONNECT_ATTEMPTS << ")" << std::endl;
    
    reconnect_attempts_++;
    
#ifdef _WIN32
    Sleep(RECONNECT_DELAY_MS);
#else
    usleep(RECONNECT_DELAY_MS * 1000);
#endif
    
#ifdef USE_OPEN62541
    // 重要：重连前先断开，清理残留 socket 和内部状态
    UA_Client_disconnect(client_);
    connected_ = false;
    
    UA_StatusCode status = UA_Client_connect(client_, server_url_.c_str());
    if (status == UA_STATUSCODE_GOOD) {
        connected_ = true;
        reconnect_attempts_ = 0;
        std::cout << "[OPC-UA] Reconnect SUCCESS" << std::endl;
        return true;
    }
#endif
    
    std::cout << "[OPC-UA] Reconnect FAILED" << std::endl;
    return false;
}

std::string OPCUACommunication::buildNodeId(const PLCAddress& address) {
    // 检查缓存
    auto it = node_id_cache_.find(address.address_string);
    if (it != node_id_cache_.end()) {
        return it->second;
    }
    
    // 使用不带引号的标准 NodeId 格式
    // 模拟器创建：ua.NodeId('%I0.0', 3) -> ns=3;s=%I0.0
    // UA_NODEID_STRING_ALLOC(3, identifier) 的第二个参数应该是 identifier 字符串（不带引号）
    // 标准格式：ns=3;s=%I0.0（而不是 ns=3;s="%I0.0"）
    std::string identifier = address.address_string;
    
    // std::cout << "[OPC-UA] Built NodeId identifier: " << identifier << " for address: " << address.address_string << std::endl;
    
    return identifier;
}

void OPCUACommunication::setNodeIdMapping(const std::string& plc_address, const std::string& node_id) {
    node_id_cache_[plc_address] = node_id;
    // std::cout << "[OPC-UA] Mapped address " << plc_address << " -> " << node_id << std::endl;
}

bool OPCUACommunication::readBool(const PLCAddress& address, bool& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] readBool: " << address.address_string << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_Variant variant;
    UA_Variant_init(&variant);
    
    UA_StatusCode status = UA_Client_readValueAttribute(client_, nodeId, &variant);
    
    if (status == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&variant, &UA_TYPES[UA_TYPES_BOOLEAN])) {
        value = *(UA_Boolean*)variant.data;
        // std::cout << "[OPC-UA] readBool SUCCESS: " << (value ? "TRUE" : "FALSE") << std::endl;
        UA_Variant_clear(&variant);
        UA_NodeId_clear(&nodeId);
        return true;
    }
    
    std::cerr << "[OPC-UA] readBool FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    // 除了特定的非连接类错误（如节点不存在、类型不匹配等），
    // 其他任何非 GOOD 的状态码都视为通信异常，触发重连机制
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::readWord(const PLCAddress& address, uint16_t& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] readWord: " << address.address_string << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_Variant variant;
    UA_Variant_init(&variant);
    
    UA_StatusCode status = UA_Client_readValueAttribute(client_, nodeId, &variant);
    
    if (status == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&variant, &UA_TYPES[UA_TYPES_UINT16])) {
        value = *(UA_UInt16*)variant.data;
        // std::cout << "[OPC-UA] readWord SUCCESS: " << value << std::endl;
        UA_Variant_clear(&variant);
        UA_NodeId_clear(&nodeId);
        return true;
    }
    
    std::cerr << "[OPC-UA] readWord FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::readInt(const PLCAddress& address, int16_t& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] readInt: " << address.address_string << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_Variant variant;
    UA_Variant_init(&variant);
    
    UA_StatusCode status = UA_Client_readValueAttribute(client_, nodeId, &variant);
    
    if (status == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&variant, &UA_TYPES[UA_TYPES_INT16])) {
        value = *(UA_Int16*)variant.data;
        // std::cout << "[OPC-UA] readInt SUCCESS: " << value << std::endl;
        UA_Variant_clear(&variant);
        UA_NodeId_clear(&nodeId);
        return true;
    }
    
    std::cerr << "[OPC-UA] readInt FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::readReal(const PLCAddress& address, float& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] readReal: " << address.address_string << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_Variant variant;
    UA_Variant_init(&variant);
    
    UA_StatusCode status = UA_Client_readValueAttribute(client_, nodeId, &variant);
    
    if (status == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&variant, &UA_TYPES[UA_TYPES_FLOAT])) {
        value = *(UA_Float*)variant.data;
        // std::cout << "[OPC-UA] readReal SUCCESS: " << value << std::endl;
        UA_Variant_clear(&variant);
        UA_NodeId_clear(&nodeId);
        return true;
    }
    
    std::cerr << "[OPC-UA] readReal FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::readDWord(const PLCAddress& address, uint32_t& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] readDWord: " << address.address_string << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_Variant variant;
    UA_Variant_init(&variant);
    
    UA_StatusCode status = UA_Client_readValueAttribute(client_, nodeId, &variant);
    
    if (status == UA_STATUSCODE_GOOD && UA_Variant_hasScalarType(&variant, &UA_TYPES[UA_TYPES_UINT32])) {
        value = *(UA_UInt32*)variant.data;
        // std::cout << "[OPC-UA] readDWord SUCCESS: " << value << std::endl;
        UA_Variant_clear(&variant);
        UA_NodeId_clear(&nodeId);
        return true;
    }
    
    std::cerr << "[OPC-UA] readDWord FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::writeBool(const PLCAddress& address, bool value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] writeBool: " << address.address_string 
    //           << " value=" << (value ? "TRUE" : "FALSE") << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_Boolean ua_value = value;
    UA_Variant variant;
    UA_Variant_init(&variant);
    UA_Variant_setScalarCopy(&variant, &ua_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
    
    UA_StatusCode status = UA_Client_writeValueAttribute(client_, nodeId, &variant);
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    
    if (status == UA_STATUSCODE_GOOD) {
        // std::cout << "[OPC-UA] writeBool SUCCESS" << std::endl;
        return true;
    }
    
    std::cerr << "[OPC-UA] writeBool FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec 
              << " (" << UA_StatusCode_name(status) << ")" << std::endl;
              
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::writeWord(const PLCAddress& address, uint16_t value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] writeWord: " << address.address_string 
    //           << " value=" << value << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_UInt16 ua_value = value;
    UA_Variant variant;
    UA_Variant_init(&variant);
    UA_Variant_setScalarCopy(&variant, &ua_value, &UA_TYPES[UA_TYPES_UINT16]);
    
    UA_StatusCode status = UA_Client_writeValueAttribute(client_, nodeId, &variant);
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    
    if (status == UA_STATUSCODE_GOOD) {
        // std::cout << "[OPC-UA] writeWord SUCCESS" << std::endl;
        return true;
    }
    
    std::cerr << "[OPC-UA] writeWord FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::writeInt(const PLCAddress& address, int16_t value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] writeInt: " << address.address_string 
    //           << " value=" << value << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_Int16 ua_value = value;
    UA_Variant variant;
    UA_Variant_init(&variant);
    UA_Variant_setScalarCopy(&variant, &ua_value, &UA_TYPES[UA_TYPES_INT16]);
    
    UA_StatusCode status = UA_Client_writeValueAttribute(client_, nodeId, &variant);
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    
    if (status == UA_STATUSCODE_GOOD) {
        // std::cout << "[OPC-UA] writeInt SUCCESS" << std::endl;
        return true;
    }
    
    std::cerr << "[OPC-UA] writeInt FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::writeReal(const PLCAddress& address, float value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] writeReal: " << address.address_string 
    //           << " value=" << value << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_Float ua_value = value;
    UA_Variant variant;
    UA_Variant_init(&variant);
    UA_Variant_setScalarCopy(&variant, &ua_value, &UA_TYPES[UA_TYPES_FLOAT]);
    
    UA_StatusCode status = UA_Client_writeValueAttribute(client_, nodeId, &variant);
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    
    if (status == UA_STATUSCODE_GOOD) {
        // std::cout << "[OPC-UA] writeReal SUCCESS" << std::endl;
        return true;
    }
    
    std::cerr << "[OPC-UA] writeReal FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::writeDWord(const PLCAddress& address, uint32_t value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    // std::cout << "[OPC-UA] writeDWord: " << address.address_string 
    //           << " value=" << value << std::endl;
    
#ifdef USE_OPEN62541
    if (!connected_ || !client_) {
        if (!attemptReconnect()) return false;
    }
    
    std::string node_id_str = buildNodeId(address);
    UA_NodeId nodeId = UA_NODEID_STRING_ALLOC(3, node_id_str.c_str());
    
    UA_UInt32 ua_value = value;
    UA_Variant variant;
    UA_Variant_init(&variant);
    UA_Variant_setScalarCopy(&variant, &ua_value, &UA_TYPES[UA_TYPES_UINT32]);
    
    UA_StatusCode status = UA_Client_writeValueAttribute(client_, nodeId, &variant);
    
    UA_Variant_clear(&variant);
    UA_NodeId_clear(&nodeId);
    
    if (status == UA_STATUSCODE_GOOD) {
        // std::cout << "[OPC-UA] writeDWord SUCCESS" << std::endl;
        return true;
    }
    
    std::cerr << "[OPC-UA] writeDWord FAILED: " << address.address_string << " status=0x" << std::hex << status << std::dec << std::endl;
    
    if (status != UA_STATUSCODE_GOOD && 
        status != UA_STATUSCODE_BADNODEIDUNKNOWN && 
        status != UA_STATUSCODE_BADNODEIDINVALID &&
        status != UA_STATUSCODE_BADTYPEMISMATCH) {
        connected_ = false;
    }
    
    return false;
#else
    return false;
#endif
}

bool OPCUACommunication::readMultiple(const std::vector<PLCAddress>& addresses,
                                      std::vector<bool>& bool_values,
                                      std::vector<uint16_t>& word_values,
                                      std::vector<int16_t>& int_values,
                                      std::vector<float>& real_values) {
    // 批量读取：逐个读取
    bool_values.clear();
    word_values.clear();
    int_values.clear();
    real_values.clear();
    
    for (const auto& addr : addresses) {
        if (addr.type == PLCAddressType::INPUT || addr.type == PLCAddressType::OUTPUT || 
            addr.type == PLCAddressType::MEMORY) {
            bool val;
            if (readBool(addr, val)) {
                bool_values.push_back(val);
            }
        } else if (addr.type == PLCAddressType::INPUT_WORD || addr.type == PLCAddressType::OUTPUT_WORD) {
            uint16_t val;
            if (readWord(addr, val)) {
                word_values.push_back(val);
            }
        }
    }
    
    return true;
}

// ========== S7Communication (snap7 implementation) ==========

S7Communication::S7Communication()
    : plc_ip_(""), rack_(0), slot_(1), connected_(false), client_(0), reconnect_attempts_(0) {
#ifdef USE_SNAP7
    client_ = Cli_Create();
    std::cout << "[S7] Client created (snap7 C API)" << std::endl;
#else
    std::cout << "[S7] WARNING: snap7 not enabled, S7 communication disabled" << std::endl;
#endif
}

S7Communication::~S7Communication() {
    disconnect();
#ifdef USE_SNAP7
    if (client_) {
        Cli_Destroy(&client_);
        client_ = 0;
    }
#endif
}

bool S7Communication::connect(const std::string& ip, int /*port*/) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
    std::cout << "[S7] ========== S7 CONNECT ==========" << std::endl;
    std::cout << "[S7] Connecting to: " << ip << " (rack=" << rack_ << ", slot=" << slot_ << ")" << std::endl;
    
#ifdef USE_SNAP7
    if (!client_) {
        std::cerr << "[S7] Client not initialized" << std::endl;
        return false;
    }

    // 无论是否标记为已连接，都先断开以清理旧状态
    Cli_Disconnect(client_);
    connected_ = false;

    // 设置超时参数（毫秒）- 避免操作卡死
    // 注意：snap7 没有 p_i32_ConnTimeout 参数，TCP 连接超时由操作系统控制
    // 可用参数: p_i32_PingTimeout(3), p_i32_SendTimeout(4), p_i32_RecvTimeout(5)
    int ping_timeout = 1000;   // Ping 测试超时 1 秒
    int recv_timeout = 2000;   // 接收超时 2 秒
    int send_timeout = 2000;   // 发送超时 2 秒
    Cli_SetParam(client_, p_i32_PingTimeout, &ping_timeout);
    Cli_SetParam(client_, p_i32_RecvTimeout, &recv_timeout);
    Cli_SetParam(client_, p_i32_SendTimeout, &send_timeout);
    std::cout << "[S7] Timeouts set: ping=" << ping_timeout << "ms, recv=" << recv_timeout << "ms, send=" << send_timeout << "ms" << std::endl;

    plc_ip_ = ip;
    int res = Cli_ConnectTo(client_, ip.c_str(), rack_, slot_);
    if (res == 0) {
        connected_ = true;
        reconnect_attempts_ = 0;
        std::cout << "[S7] SUCCESS: Connected" << std::endl;
        std::cout << "[S7] ========================================" << std::endl;
        return true;
    }

    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] FAILED: ConnectTo error=" << res << " (" << err_txt << ")" << std::endl;
    std::cout << "[S7] ========================================" << std::endl;
    return false;
#else
    std::cerr << "[S7] ERROR: snap7 not enabled" << std::endl;
    return false;
#endif
}

void S7Communication::disconnect() {
    std::lock_guard<std::mutex> lock(comm_mutex_);
    
#ifdef USE_SNAP7
    if (client_ && connected_) {
        std::cout << "[S7] Disconnecting..." << std::endl;
        Cli_Disconnect(client_);
    }
#endif
    connected_ = false;
}

bool S7Communication::isConnected() const {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    if (client_ && connected_) {
        // 使用 Cli_GetConnected 检查物理连接
        int connected = 0;
        int res = Cli_GetConnected(client_, &connected);
        if (res != 0 || connected == 0) {
            connected_ = false;
            return false;
        }
    }
#endif
    return connected_;
}

bool S7Communication::attemptReconnect() {
    // 快速检查：如果已达到最大重试次数，等待冷却期
    if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        static auto last_reset = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_reset).count();
        if (elapsed >= 5) {
            last_reset = now;
            reconnect_attempts_ = 0;
            std::cout << "[S7] Resetting reconnect counter after 30s cooldown" << std::endl;
        } else {
            return false;  // 静默失败，不尝试重连
        }
    }
    
    std::cout << "[S7] Attempting reconnect... (attempt " << (reconnect_attempts_ + 1)
              << "/" << MAX_RECONNECT_ATTEMPTS << ")" << std::endl;
    reconnect_attempts_++;

#ifdef _WIN32
    Sleep(RECONNECT_DELAY_MS);
#else
    usleep(RECONNECT_DELAY_MS * 1000);
#endif

#ifdef USE_SNAP7
    // 重连前先断开旧连接，清理残留状态
    std::cout << "[S7] Disconnecting before reconnect..." << std::endl;
    Cli_Disconnect(client_);
    
    // 设置超时参数
    int ping_timeout = 1000;
    int recv_timeout = 2000;
    int send_timeout = 2000;
    Cli_SetParam(client_, p_i32_PingTimeout, &ping_timeout);
    Cli_SetParam(client_, p_i32_RecvTimeout, &recv_timeout);
    Cli_SetParam(client_, p_i32_SendTimeout, &send_timeout);

    std::cout << "[S7] Reconnecting to " << plc_ip_ << " (rack=" << rack_ << ", slot=" << slot_ << ")..." << std::endl;
    int res = Cli_ConnectTo(client_, plc_ip_.c_str(), rack_, slot_);
    if (res == 0) {
        connected_ = true;
        reconnect_attempts_ = 0;
        std::cout << "[S7] Reconnect SUCCESS" << std::endl;
        return true;
    }
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] Reconnect FAILED: error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

int S7Communication::getAreaCode(PLCAddressType type) {
    switch (type) {
        case PLCAddressType::INPUT:
        case PLCAddressType::INPUT_WORD:
            return S7AreaPE;   // Process inputs
        case PLCAddressType::OUTPUT:
        case PLCAddressType::OUTPUT_WORD:
            return S7AreaPA;   // Process outputs
        case PLCAddressType::MEMORY:
            return S7AreaMK;   // Merker
        case PLCAddressType::DB_BLOCK:
            return S7AreaDB;   // Data Block
        default:
            return S7AreaMK;
    }
}

bool S7Communication::readBool(const PLCAddress& address, bool& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    if (!connected_) {
        if (!attemptReconnect()) return false;
    }
    uint8_t buffer = 0;
    int area = getAreaCode(address.type);
    int db_num = (address.type == PLCAddressType::DB_BLOCK) ? address.db_number : 0;
    int start = address.byte_offset;
    int res = Cli_ReadArea(client_, area, db_num, start, 1, S7WLByte, &buffer);
    if (res == 0) {
        value = (buffer & (1 << address.bit_offset)) != 0;
        return true;
    }
    connected_ = false;
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] readBool FAILED: error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

bool S7Communication::readWord(const PLCAddress& address, uint16_t& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    if (!connected_) {
        if (!attemptReconnect()) return false;
    }
    uint8_t buffer[2] = {0};
    int area = getAreaCode(address.type);
    int db_num = (address.type == PLCAddressType::DB_BLOCK) ? address.db_number : 0;
    int start = address.byte_offset;
    int res = Cli_ReadArea(client_, area, db_num, start, 2, S7WLByte, buffer);
    if (res == 0) {
        value = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
        return true;
    }
    connected_ = false;
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] readWord FAILED: error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

bool S7Communication::readInt(const PLCAddress& address, int16_t& value) {
    uint16_t tmp;
    if (!readWord(address, tmp)) return false;
    value = static_cast<int16_t>(tmp);
    return true;
}

bool S7Communication::readReal(const PLCAddress& address, float& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    if (!connected_) {
        if (!attemptReconnect()) return false;
    }
    uint8_t buffer[4] = {0};
    int area = getAreaCode(address.type);
    int db_num = (address.type == PLCAddressType::DB_BLOCK) ? address.db_number : 0;
    int start = address.byte_offset;
    int res = Cli_ReadArea(client_, area, db_num, start, 4, S7WLByte, buffer);
    if (res == 0) {
        uint32_t bits = (static_cast<uint32_t>(buffer[0]) << 24) |
                        (static_cast<uint32_t>(buffer[1]) << 16) |
                        (static_cast<uint32_t>(buffer[2]) << 8)  |
                        buffer[3];
        value = *reinterpret_cast<float*>(&bits);
        return true;
    }
    connected_ = false;
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] readReal FAILED: error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

bool S7Communication::readDWord(const PLCAddress& address, uint32_t& value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    if (!connected_) {
        if (!attemptReconnect()) return false;
    }
    uint8_t buffer[4] = {0};
    int area = getAreaCode(address.type);
    int db_num = (address.type == PLCAddressType::DB_BLOCK) ? address.db_number : 0;
    int start = address.byte_offset;
    int res = Cli_ReadArea(client_, area, db_num, start, 4, S7WLByte, buffer);
    if (res == 0) {
        value = (static_cast<uint32_t>(buffer[0]) << 24) |
                (static_cast<uint32_t>(buffer[1]) << 16) |
                (static_cast<uint32_t>(buffer[2]) << 8)  |
                buffer[3];
        return true;
    }
    connected_ = false;
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] readDWord FAILED: error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

bool S7Communication::writeBool(const PLCAddress& address, bool value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    // std::cout << "[S7] writeBool addr=" << address.address_string
    //           << " byte=" << address.byte_offset << " bit=" << address.bit_offset
    //           << " value=" << (value ? "TRUE" : "FALSE") << std::endl;
    if (!connected_) {
        if (!attemptReconnect()) return false;
    }
    uint8_t buffer = value ? 1 : 0;
    int area = getAreaCode(address.type);
    int db_num = (address.type == PLCAddressType::DB_BLOCK) ? address.db_number : 0;
    int start = address.byte_offset;
    // std::cout << "[S7]   area=" << area << " db=" << db_num << " start=" << start
    //           << " size=1 bytes hex=" << std::hex << static_cast<int>(buffer) << std::dec << std::endl;
    int res = Cli_WriteArea(client_, area, db_num, start, 1, S7WLByte, &buffer);
    if (res == 0) {
        return true;
    }
    connected_ = false;
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] writeBool FAILED: " << address.address_string << " error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

bool S7Communication::writeWord(const PLCAddress& address, uint16_t value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    // std::cout << "[S7] writeWord addr=" << address.address_string
    //           << " byte=" << address.byte_offset << " value=" << value << std::endl;
    if (!connected_) {
        if (!attemptReconnect()) return false;
    }
    uint8_t buffer[2];
    buffer[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[1] = static_cast<uint8_t>(value & 0xFF);
    int area = getAreaCode(address.type);
    int start = address.byte_offset;
    int db_num = (address.type == PLCAddressType::DB_BLOCK) ? address.db_number : 0;
    // std::cout << "[S7]   area=" << area << " db=" << db_num << " start=" << start
    //           << " size=2 bytes hex=" << std::hex << static_cast<int>(buffer[0]) << " "
    //           << static_cast<int>(buffer[1]) << std::dec << std::endl;
    int res = Cli_WriteArea(client_, area, db_num, start, 2, S7WLByte, buffer);
    if (res == 0) return true;
    connected_ = false;
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] writeWord FAILED: " << address.address_string << " error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

bool S7Communication::writeInt(const PLCAddress& address, int16_t value) {
    return writeWord(address, static_cast<uint16_t>(value));
}

bool S7Communication::writeReal(const PLCAddress& address, float value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    // std::cout << "[S7] writeReal addr=" << address.address_string
    //           << " byte=" << address.byte_offset << " value=" << value << std::endl;
    if (!connected_) {
        if (!attemptReconnect()) return false;
    }
    uint32_t bits = *reinterpret_cast<uint32_t*>(&value);
    uint8_t buffer[4];
    buffer[0] = static_cast<uint8_t>((bits >> 24) & 0xFF);
    buffer[1] = static_cast<uint8_t>((bits >> 16) & 0xFF);
    buffer[2] = static_cast<uint8_t>((bits >> 8) & 0xFF);
    buffer[3] = static_cast<uint8_t>(bits & 0xFF);
    int area = getAreaCode(address.type);
    int db_num = (address.type == PLCAddressType::DB_BLOCK) ? address.db_number : 0;
    int start = address.byte_offset;
    // std::cout << "[S7]   area=" << area << " db=" << db_num << " start=" << start
    //           << " size=4 bytes hex=" << std::hex
    //           << static_cast<int>(buffer[0]) << " " << static_cast<int>(buffer[1]) << " "
    //           << static_cast<int>(buffer[2]) << " " << static_cast<int>(buffer[3])
    //           << std::dec << std::endl;
    int res = Cli_WriteArea(client_, area, db_num, start, 4, S7WLByte, buffer);
    if (res == 0) return true;
    connected_ = false;
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] writeReal FAILED: " << address.address_string << " error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

bool S7Communication::writeDWord(const PLCAddress& address, uint32_t value) {
    std::lock_guard<std::mutex> lock(comm_mutex_);
#ifdef USE_SNAP7
    // std::cout << "[S7] writeDWord addr=" << address.address_string
    //           << " byte=" << address.byte_offset << " value=" << value << std::endl;
    if (!connected_) {
        if (!attemptReconnect()) return false;
    }
    uint8_t buffer[4];
    buffer[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    buffer[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buffer[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[3] = static_cast<uint8_t>(value & 0xFF);
    int area = getAreaCode(address.type);
    int db_num = (address.type == PLCAddressType::DB_BLOCK) ? address.db_number : 0;
    int start = address.byte_offset;
    // std::cout << "[S7]   area=" << area << " db=" << db_num << " start=" << start
    //           << " size=4 bytes hex=" << std::hex
    //           << static_cast<int>(buffer[0]) << " " << static_cast<int>(buffer[1]) << " "
    //           << static_cast<int>(buffer[2]) << " " << static_cast<int>(buffer[3])
    //           << std::dec << std::endl;
    int res = Cli_WriteArea(client_, area, db_num, start, 4, S7WLByte, buffer);
    if (res == 0) return true;
    connected_ = false;
    char err_txt[256] = {0};
    Cli_ErrorText(res, err_txt, sizeof(err_txt));
    std::cerr << "[S7] writeDWord FAILED: " << address.address_string << " error=" << res << " (" << err_txt << ")" << std::endl;
#endif
    return false;
}

bool S7Communication::readMultiple(const std::vector<PLCAddress>& addresses,
                                    std::vector<bool>& bool_values,
                                    std::vector<uint16_t>& word_values,
                                    std::vector<int16_t>& int_values,
                                    std::vector<float>& real_values) {
    bool_values.clear();
    word_values.clear();
    int_values.clear();
    real_values.clear();
    for (const auto& addr : addresses) {
        if (addr.type == PLCAddressType::INPUT || addr.type == PLCAddressType::OUTPUT ||
            addr.type == PLCAddressType::MEMORY) {
            bool v;
            if (readBool(addr, v)) bool_values.push_back(v);
        } else if (addr.type == PLCAddressType::INPUT_WORD || addr.type == PLCAddressType::OUTPUT_WORD) {
            uint16_t w;
            if (readWord(addr, w)) word_values.push_back(w);
        }
    }
    return true;
}

std::string S7Communication::getLastError() const {
#ifdef USE_SNAP7
    if (!client_) return "snap7 client not initialized";
    int err = 0;
    Cli_GetLastError(client_, &err);
    char err_txt[256] = {0};
    Cli_ErrorText(err, err_txt, sizeof(err_txt));
    return std::string(err_txt);
#else
    return "snap7 not enabled";
#endif
}

// ========== MockPLCCommunication ==========
MockPLCCommunication::MockPLCCommunication()
    : connected_(false) {
    // 初始化模拟数据
    bool_data_.resize(1000, false);
    word_data_.resize(1000, 0);
    int_data_.resize(1000, 0);
    real_data_.resize(1000, 0.0f);
}

bool MockPLCCommunication::connect(const std::string& /*ip*/, int /*port*/) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    connected_ = true;
    return true;
}

void MockPLCCommunication::disconnect() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    connected_ = false;
}

bool MockPLCCommunication::isConnected() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return connected_;
}

bool MockPLCCommunication::readBool(const PLCAddress& address, bool& value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset * 8 + address.bit_offset;
    if (index >= 0 && index < static_cast<int>(bool_data_.size())) {
        value = bool_data_[index];
        return true;
    }
    return false;
}

bool MockPLCCommunication::readWord(const PLCAddress& address, uint16_t& value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset / 2;
    if (index >= 0 && index < static_cast<int>(word_data_.size())) {
        value = word_data_[index];
        return true;
    }
    return false;
}

bool MockPLCCommunication::readInt(const PLCAddress& address, int16_t& value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset / 2;
    if (index >= 0 && index < static_cast<int>(int_data_.size())) {
        value = int_data_[index];
        return true;
    }
    return false;
}

bool MockPLCCommunication::readReal(const PLCAddress& address, float& value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset / 4;
    if (index >= 0 && index < static_cast<int>(real_data_.size())) {
        value = real_data_[index];
        return true;
    }
    return false;
}

bool MockPLCCommunication::readDWord(const PLCAddress& address, uint32_t& value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    // 使用WORD数组模拟DWORD
    int index = address.byte_offset / 2;
    if (index >= 0 && index + 1 < static_cast<int>(word_data_.size())) {
        value = (static_cast<uint32_t>(word_data_[index + 1]) << 16) | word_data_[index];
        return true;
    }
    return false;
}

bool MockPLCCommunication::writeBool(const PLCAddress& address, bool value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset * 8 + address.bit_offset;
    if (index >= 0 && index < static_cast<int>(bool_data_.size())) {
        bool_data_[index] = value;
        return true;
    }
    return false;
}

bool MockPLCCommunication::writeWord(const PLCAddress& address, uint16_t value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset / 2;
    if (index >= 0 && index < static_cast<int>(word_data_.size())) {
        word_data_[index] = value;
        return true;
    }
    return false;
}

bool MockPLCCommunication::writeInt(const PLCAddress& address, int16_t value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset / 2;
    if (index >= 0 && index < static_cast<int>(int_data_.size())) {
        int_data_[index] = value;
        return true;
    }
    return false;
}

bool MockPLCCommunication::writeReal(const PLCAddress& address, float value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset / 4;
    if (index >= 0 && index < static_cast<int>(real_data_.size())) {
        real_data_[index] = value;
        return true;
    }
    return false;
}

bool MockPLCCommunication::writeDWord(const PLCAddress& address, uint32_t value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    int index = address.byte_offset / 2;
    if (index >= 0 && index + 1 < static_cast<int>(word_data_.size())) {
        word_data_[index] = static_cast<uint16_t>(value & 0xFFFF);
        word_data_[index + 1] = static_cast<uint16_t>((value >> 16) & 0xFFFF);
        return true;
    }
    return false;
}

bool MockPLCCommunication::readMultiple(const std::vector<PLCAddress>& addresses,
                                        std::vector<bool>& bool_values,
                                        std::vector<uint16_t>& word_values,
                                        std::vector<int16_t>& int_values,
                                        std::vector<float>& real_values) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!connected_) return false;
    
    bool_values.clear();
    word_values.clear();
    int_values.clear();
    real_values.clear();
    
    for (const auto& addr : addresses) {
        if (addr.type == PLCAddressType::INPUT || addr.type == PLCAddressType::OUTPUT) {
            bool val;
            if (readBool(addr, val)) {
                bool_values.push_back(val);
            }
        } else if (addr.type == PLCAddressType::INPUT_WORD || addr.type == PLCAddressType::OUTPUT_WORD) {
            uint16_t val;
            if (readWord(addr, val)) {
                word_values.push_back(val);
            }
        }
    }
    
    return true;
}

} // namespace PLC
} // namespace Common
