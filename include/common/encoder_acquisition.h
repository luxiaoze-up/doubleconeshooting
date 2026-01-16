#ifndef ENCODER_ACQUISITION_H
#define ENCODER_ACQUISITION_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Common {

struct EncoderReading {
    uint32_t raw_value{0};      // 原始32位值
    uint16_t turns{0};          // 圈数 (高15位)
    uint32_t position{0};       // 位置 (低17位)
    double combined_value{0.0}; // 圈数+位置的完整值 (turns为整数部分，position/1000000为小数部分)
    std::chrono::steady_clock::time_point timestamp{};
    bool valid{false};
};

class EncoderAcquisitionClient {
public:
    EncoderAcquisitionClient(const std::string &ip, int port, int channel_offset,
                             int channel_count, std::vector<EncoderReading> &shared_readings,
                             std::mutex &shared_mutex);
    ~EncoderAcquisitionClient();

    void start();
    void stop();

    bool is_connected() const;
    std::string last_error() const;

private:
    void run();
    bool connect_socket();
    void close_socket();
    void parse_buffer();
    void handle_frame(const uint8_t *frame);

    const std::string ip_;
    const int port_;
    const int channel_offset_;
    const int channel_count_;

    int socket_fd_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::thread worker_;

    std::vector<uint8_t> buffer_;
    std::vector<EncoderReading> &readings_;
    std::mutex &readings_mutex_;

    mutable std::mutex state_mutex_;
    std::string last_error_;
};

// 编码器采集器配置结构
struct EncoderCollectorConfig {
    std::string ip;
    int port;
    int channel_offset;   // 全局通道偏移
    int channel_count;    // 通道数量
};

class EncoderAcquisitionManager {
public:
    // 默认构造函数 - 使用硬编码的默认配置 (192.168.1.15, 192.168.1.16)
    EncoderAcquisitionManager();
    
    // 可配置构造函数 - 使用自定义 IP/端口配置
    EncoderAcquisitionManager(const std::vector<std::string>& ips, 
                              const std::vector<int>& ports,
                              int channels_per_collector = 10);
    
    // 完全自定义构造函数
    explicit EncoderAcquisitionManager(const std::vector<EncoderCollectorConfig>& configs);
    
    ~EncoderAcquisitionManager();

    void start();
    void stop();

    bool get_reading(int global_channel, EncoderReading &out) const;
    bool has_any_connection() const;
    std::string status_summary() const;
    
    // 获取配置的采集器数量
    size_t collector_count() const { return clients_.size(); }

    static constexpr int kTotalChannels = 20;
    static constexpr int kDefaultPort = 5000;
    static constexpr int kDefaultChannelsPerCollector = 10;

private:
    void init_clients(const std::vector<EncoderCollectorConfig>& configs);
    
    std::vector<EncoderReading> readings_;
    mutable std::mutex readings_mutex_;
    std::vector<std::unique_ptr<EncoderAcquisitionClient>> clients_;
    std::atomic<bool> started_{false};
};

} // namespace Common

#endif // ENCODER_ACQUISITION_H
