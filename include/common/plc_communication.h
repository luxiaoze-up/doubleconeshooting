#ifndef PLC_COMMUNICATION_H
#define PLC_COMMUNICATION_H

#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <cstdint>
#include <map>

// OPC UA library (open62541)
#ifdef USE_OPEN62541
extern "C" {
#include <open62541.h>
}
#endif

// Snap7 library (S7 protocol)
#ifdef USE_SNAP7
#include <snap7.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace Common {
namespace PLC {

// PLC数据类型
enum class PLCDataType {
    BOOL,
    WORD,
    INT,
    REAL,
    DWORD
};

// PLC地址类型
enum class PLCAddressType {
    INPUT,      // I - 输入
    OUTPUT,     // Q - 输出
    MEMORY,     // M - 内存
    INPUT_WORD, // IW - 输入字
    OUTPUT_WORD,// QW - 输出字
    DB_BLOCK    // DB - 数据块
};

// PLC地址结构
struct PLCAddress {
    PLCAddressType type;
    int byte_offset;
    int bit_offset;  // 对于BOOL类型，0-7
    int db_number;   // 对于DB类型，指定DB编号
    std::string address_string;  // 如 "%I0.0", "%Q0.1", "%IW128", "DB1.DBX0.0"
    
    PLCAddress(PLCAddressType t, int byte_off, int bit_off = 0, int db_num = 0)
        : type(t), byte_offset(byte_off), bit_offset(bit_off), db_number(db_num) {
        // 生成地址字符串
        if (t == PLCAddressType::DB_BLOCK) {
            if (bit_off >= 0) {
                address_string = "DB" + std::to_string(db_num) + ".DBX" + 
                               std::to_string(byte_off) + "." + std::to_string(bit_off);
            } else {
                address_string = "DB" + std::to_string(db_num) + ".DBB" + 
                               std::to_string(byte_off);
            }
            return;
        }
        
        char prefix = 'I';
        if (t == PLCAddressType::OUTPUT) prefix = 'Q';
        else if (t == PLCAddressType::MEMORY) prefix = 'M';
        else if (t == PLCAddressType::INPUT_WORD) {
            address_string = "%IW" + std::to_string(byte_off);
            return;
        } else if (t == PLCAddressType::OUTPUT_WORD) {
            address_string = "%QW" + std::to_string(byte_off);
            return;
        }
        
        if (bit_off >= 0) {
            address_string = "%" + std::string(1, prefix) + 
                           std::to_string(byte_off) + "." + 
                           std::to_string(bit_off);
        } else {
            address_string = "%" + std::string(1, prefix) + 
                           std::to_string(byte_off);
        }
    }
};

// PLC通信接口
class IPLCCommunication {
public:
    virtual ~IPLCCommunication() = default;
    
    // 连接和断开
    virtual bool connect(const std::string& ip, int port = 102) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    // 读取数据
    virtual bool readBool(const PLCAddress& address, bool& value) = 0;
    virtual bool readWord(const PLCAddress& address, uint16_t& value) = 0;
    virtual bool readInt(const PLCAddress& address, int16_t& value) = 0;
    virtual bool readReal(const PLCAddress& address, float& value) = 0;
    virtual bool readDWord(const PLCAddress& address, uint32_t& value) = 0;
    
    // 写入数据
    virtual bool writeBool(const PLCAddress& address, bool value) = 0;
    virtual bool writeWord(const PLCAddress& address, uint16_t value) = 0;
    virtual bool writeInt(const PLCAddress& address, int16_t value) = 0;
    virtual bool writeReal(const PLCAddress& address, float value) = 0;
    virtual bool writeDWord(const PLCAddress& address, uint32_t value) = 0;
    
    // 批量读取
    virtual bool readMultiple(const std::vector<PLCAddress>& addresses, 
                             std::vector<bool>& bool_values,
                             std::vector<uint16_t>& word_values,
                             std::vector<int16_t>& int_values,
                             std::vector<float>& real_values) = 0;
};

// OPC UA通信实现（使用 open62541 库）
// 用于连接西门子 S7-1200 PLC 的 OPC UA 服务器
class OPCUACommunication : public IPLCCommunication {
private:
    std::string server_url_;
    mutable bool connected_;
    mutable std::mutex comm_mutex_;
    
#ifdef USE_OPEN62541
    UA_Client* client_;
#else
    void* client_;  // placeholder
#endif
    
    // 重连相关
    int reconnect_attempts_;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 3;
    static constexpr int RECONNECT_DELAY_MS = 1000;
    
    // NodeId 缓存 (地址字符串 -> OPC UA NodeId)
    std::map<std::string, std::string> node_id_cache_;
    
    // 内部方法
    bool attemptReconnect();
    std::string buildNodeId(const PLCAddress& address);
    
public:
    OPCUACommunication();
    ~OPCUACommunication();
    
    // IPLCCommunication接口实现
    bool connect(const std::string& ip, int port = 4840) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool readBool(const PLCAddress& address, bool& value) override;
    bool readWord(const PLCAddress& address, uint16_t& value) override;
    bool readInt(const PLCAddress& address, int16_t& value) override;
    bool readReal(const PLCAddress& address, float& value) override;
    bool readDWord(const PLCAddress& address, uint32_t& value) override;
    
    bool writeBool(const PLCAddress& address, bool value) override;
    bool writeWord(const PLCAddress& address, uint16_t value) override;
    bool writeInt(const PLCAddress& address, int16_t value) override;
    bool writeReal(const PLCAddress& address, float value) override;
    bool writeDWord(const PLCAddress& address, uint32_t value) override;
    
    bool readMultiple(const std::vector<PLCAddress>& addresses,
                     std::vector<bool>& bool_values,
                     std::vector<uint16_t>& word_values,
                     std::vector<int16_t>& int_values,
                     std::vector<float>& real_values) override;
    
    // 设置自定义节点ID映射
    void setNodeIdMapping(const std::string& plc_address, const std::string& node_id);
};

// S7通信实现（使用 snap7 库）
// 用于连接西门子 S7-1200/S7-1500 PLC
class S7Communication : public IPLCCommunication {
private:
    std::string plc_ip_;
    int rack_;      // 机架号，S7-1200 通常为 0
    int slot_;      // 槽号，S7-1200 通常为 1
    mutable bool connected_;
    mutable std::mutex comm_mutex_;
    
#ifdef USE_SNAP7
    S7Object client_;
#else
    void* client_;  // placeholder
#endif
    
    // 重连相关
    int reconnect_attempts_;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 3;
    static constexpr int RECONNECT_DELAY_MS = 1000;
    
    // 内部方法
    bool attemptReconnect();
    int getAreaCode(PLCAddressType type);
    
public:
    S7Communication();
    ~S7Communication();
    
    // IPLCCommunication接口实现
    bool connect(const std::string& ip, int port = 102) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool readBool(const PLCAddress& address, bool& value) override;
    bool readWord(const PLCAddress& address, uint16_t& value) override;
    bool readInt(const PLCAddress& address, int16_t& value) override;
    bool readReal(const PLCAddress& address, float& value) override;
    bool readDWord(const PLCAddress& address, uint32_t& value) override;
    
    bool writeBool(const PLCAddress& address, bool value) override;
    bool writeWord(const PLCAddress& address, uint16_t value) override;
    bool writeInt(const PLCAddress& address, int16_t value) override;
    bool writeReal(const PLCAddress& address, float value) override;
    bool writeDWord(const PLCAddress& address, uint32_t value) override;
    
    bool readMultiple(const std::vector<PLCAddress>& addresses,
                     std::vector<bool>& bool_values,
                     std::vector<uint16_t>& word_values,
                     std::vector<int16_t>& int_values,
                     std::vector<float>& real_values) override;
    
    // S7特有设置
    void setRackSlot(int rack, int slot) { rack_ = rack; slot_ = slot; }
    
    // 获取最后错误信息
    std::string getLastError() const;
};

// 模拟PLC通信（用于测试）
class MockPLCCommunication : public IPLCCommunication {
private:
    bool connected_;
    mutable std::mutex data_mutex_;
    
    // 模拟数据存储
    std::vector<bool> bool_data_;
    std::vector<uint16_t> word_data_;
    std::vector<int16_t> int_data_;
    std::vector<float> real_data_;
    
public:
    MockPLCCommunication();
    ~MockPLCCommunication() = default;
    
    bool connect(const std::string& ip, int port = 102) override;
    void disconnect() override;
    bool isConnected() const override;
    
    bool readBool(const PLCAddress& address, bool& value) override;
    bool readWord(const PLCAddress& address, uint16_t& value) override;
    bool readInt(const PLCAddress& address, int16_t& value) override;
    bool readReal(const PLCAddress& address, float& value) override;
    bool readDWord(const PLCAddress& address, uint32_t& value) override;
    
    bool writeBool(const PLCAddress& address, bool value) override;
    bool writeWord(const PLCAddress& address, uint16_t value) override;
    bool writeInt(const PLCAddress& address, int16_t value) override;
    bool writeReal(const PLCAddress& address, float value) override;
    bool writeDWord(const PLCAddress& address, uint32_t value) override;
    
    bool readMultiple(const std::vector<PLCAddress>& addresses,
                     std::vector<bool>& bool_values,
                     std::vector<uint16_t>& word_values,
                     std::vector<int16_t>& int_values,
                     std::vector<float>& real_values) override;
};

} // namespace PLC
} // namespace Common

#endif // PLC_COMMUNICATION_H
