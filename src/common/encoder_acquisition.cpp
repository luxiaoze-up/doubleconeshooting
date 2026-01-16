#include "common/encoder_acquisition.h"

#include <algorithm>
#include <cstring>
#include <cerrno>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Common {

namespace {
#ifdef _WIN32
bool ensure_winsock() {
    static std::once_flag once;
    static bool init_ok = false;
    std::call_once(once, []() {
        WSADATA wsa_data;
        init_ok = (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0);
    });
    return init_ok;
}
#endif

void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    usleep(ms * 1000);
#endif
}

constexpr uint8_t kFrameHead = 0x7E;
constexpr uint8_t kFrameTail = 0x7F;
constexpr size_t kFrameLen = 7;
constexpr int kReconnectDelayMs = 1000;
constexpr int kRecvTimeoutMs = 300;
} // namespace

EncoderAcquisitionClient::EncoderAcquisitionClient(const std::string &ip, int port, int channel_offset,
                                                   int channel_count, std::vector<EncoderReading> &shared_readings,
                                                   std::mutex &shared_mutex)
    : ip_(ip), port_(port), channel_offset_(channel_offset), channel_count_(channel_count), socket_fd_(-1),
      readings_(shared_readings), readings_mutex_(shared_mutex) {}

EncoderAcquisitionClient::~EncoderAcquisitionClient() { stop(); }

void EncoderAcquisitionClient::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&EncoderAcquisitionClient::run, this);
}

void EncoderAcquisitionClient::stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
    close_socket();
}

bool EncoderAcquisitionClient::is_connected() const { return connected_.load(); }

std::string EncoderAcquisitionClient::last_error() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return last_error_;
}

bool EncoderAcquisitionClient::connect_socket() {
#ifdef _WIN32
    if (!ensure_winsock()) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_error_ = "WSAStartup failed";
        return false;
    }
#endif

#ifdef _WIN32
    socket_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
#else
    socket_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
#endif
    if (socket_fd_ < 0) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_error_ = "socket() failed";
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    addr.sin_addr.s_addr = inet_addr(ip_.c_str());

    // Set recv timeout
#ifdef _WIN32
    DWORD timeout = kRecvTimeoutMs;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
    timeval tv {};
    tv.tv_sec = kRecvTimeoutMs / 1000;
    tv.tv_usec = (kRecvTimeoutMs % 1000) * 1000;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    if (connect(socket_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_error_ = "connect() failed";
        close_socket();
        return false;
    }

    connected_ = true;
    return true;
}

void EncoderAcquisitionClient::close_socket() {
    connected_ = false;
    if (socket_fd_ >= 0) {
#ifdef _WIN32
        closesocket(socket_fd_);
#else
        close(socket_fd_);
#endif
        socket_fd_ = -1;
    }
}

void EncoderAcquisitionClient::run() {
    while (running_) {
        if (!connected_) {
            if (!connect_socket()) {
                sleep_ms(kReconnectDelayMs);
                continue;
            }
        }

        uint8_t recv_buf[64];
            int received = 0;
    #ifdef _WIN32
            received = recv(socket_fd_, reinterpret_cast<char *>(recv_buf), static_cast<int>(sizeof(recv_buf)), 0);
            if (received <= 0) {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                    continue; // no data this cycle
                }
                std::lock_guard<std::mutex> lock(state_mutex_);
                last_error_ = "recv failed (err=" + std::to_string(err) + ")";
                close_socket();
                continue;
            }
    #else
            received = static_cast<int>(recv(socket_fd_, recv_buf, sizeof(recv_buf), 0));
            if (received <= 0) {
                int err = errno;
                if (err == EWOULDBLOCK || err == EAGAIN || err == ETIMEDOUT) {
                    continue; // timeout, keep connection
                }
                std::lock_guard<std::mutex> lock(state_mutex_);
                last_error_ = "recv failed (err=" + std::to_string(err) + ")";
                close_socket();
                continue;
            }
    #endif

        buffer_.insert(buffer_.end(), recv_buf, recv_buf + received);
        parse_buffer();
    }
}

void EncoderAcquisitionClient::parse_buffer() {
    // Simple resynchronization: find head, ensure tail matches
    while (buffer_.size() >= kFrameLen) {
        auto head_it = std::find(buffer_.begin(), buffer_.end(), kFrameHead);
        if (head_it == buffer_.end()) {
            buffer_.clear();
            break;
        }
        size_t head_idx = static_cast<size_t>(std::distance(buffer_.begin(), head_it));
        if (buffer_.size() - head_idx < kFrameLen) {
            // Not enough data yet
            if (head_idx > 0) buffer_.erase(buffer_.begin(), buffer_.begin() + head_idx);
            break;
        }
        if (buffer_[head_idx + kFrameLen - 1] != kFrameTail) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + head_idx + 1);
            continue;
        }
        handle_frame(&buffer_[head_idx]);
        buffer_.erase(buffer_.begin(), buffer_.begin() + head_idx + kFrameLen);
    }
}

void EncoderAcquisitionClient::handle_frame(const uint8_t *frame) {
    uint8_t channel = frame[1];
    if (channel >= static_cast<uint8_t>(channel_count_)) {
        return; // Ignore out-of-range channel
    }
    uint32_t raw = (static_cast<uint32_t>(frame[2]) << 24) |
                   (static_cast<uint32_t>(frame[3]) << 16) |
                   (static_cast<uint32_t>(frame[4]) << 8) |
                   (static_cast<uint32_t>(frame[5]));

    // 解析32位值: 高15位为圈数，低17位为位置
    uint16_t turns = static_cast<uint16_t>((raw >> 17) & 0x7FFF);  // 提取高15位
    uint32_t position = raw & 0x1FFFF;                              // 提取低17位
    
    // 计算完整的double值：turns为整数部分，position为小数部分
    // position直接除以1000000作为小数部分 (例: 65535→0.065535, 32768→0.032768)
    double combined_value = static_cast<double>(turns) + (static_cast<double>(position) / 1000000.0);

    int global_channel = channel_offset_ + static_cast<int>(channel);
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(readings_mutex_);
        if (global_channel >= 0 && global_channel < static_cast<int>(readings_.size())) {
            readings_[global_channel].raw_value = raw;
            readings_[global_channel].turns = turns;
            readings_[global_channel].position = position;
            readings_[global_channel].combined_value = combined_value;
            readings_[global_channel].timestamp = now;
            readings_[global_channel].valid = true;
        }
    }
}

// 默认构造函数 - 使用硬编码的默认配置
EncoderAcquisitionManager::EncoderAcquisitionManager() {
    std::vector<EncoderCollectorConfig> default_configs = {
        {"192.168.1.15", kDefaultPort, 0, kDefaultChannelsPerCollector},   // 通道 0-9
        {"192.168.1.16", kDefaultPort, 10, kDefaultChannelsPerCollector}   // 通道 10-19
    };
    init_clients(default_configs);
}

// 可配置构造函数 - 使用自定义 IP/端口
EncoderAcquisitionManager::EncoderAcquisitionManager(
    const std::vector<std::string>& ips, 
    const std::vector<int>& ports,
    int channels_per_collector) {
    
    std::vector<EncoderCollectorConfig> configs;
    int channel_offset = 0;
    
    for (size_t i = 0; i < ips.size(); ++i) {
        int port = (i < ports.size()) ? ports[i] : kDefaultPort;
        configs.push_back({ips[i], port, channel_offset, channels_per_collector});
        channel_offset += channels_per_collector;
    }
    
    init_clients(configs);
}

// 完全自定义构造函数
EncoderAcquisitionManager::EncoderAcquisitionManager(
    const std::vector<EncoderCollectorConfig>& configs) {
    init_clients(configs);
}

void EncoderAcquisitionManager::init_clients(
    const std::vector<EncoderCollectorConfig>& configs) {
    
    readings_.resize(kTotalChannels);
    
    for (const auto& cfg : configs) {
        clients_.emplace_back(std::make_unique<EncoderAcquisitionClient>(
            cfg.ip, cfg.port, cfg.channel_offset, cfg.channel_count, 
            readings_, readings_mutex_));
    }
}

EncoderAcquisitionManager::~EncoderAcquisitionManager() { stop(); }

void EncoderAcquisitionManager::start() {
    if (started_) return;
    started_ = true;
    for (auto &c : clients_) {
        c->start();
    }
}

void EncoderAcquisitionManager::stop() {
    started_ = false;
    for (auto &c : clients_) {
        c->stop();
    }
}

bool EncoderAcquisitionManager::get_reading(int global_channel, EncoderReading &out) const {
    if (global_channel < 0 || global_channel >= static_cast<int>(readings_.size())) return false;
    std::lock_guard<std::mutex> lock(readings_mutex_);
    out = readings_[static_cast<size_t>(global_channel)];
    return out.valid;
}

bool EncoderAcquisitionManager::has_any_connection() const {
    for (const auto &c : clients_) {
        if (c->is_connected()) return true;
    }
    return false;
}

std::string EncoderAcquisitionManager::status_summary() const {
    std::ostringstream ss;
    ss << "Encoder collectors: ";
    for (size_t i = 0; i < clients_.size(); ++i) {
        ss << "[#" << i << " " << (clients_[i]->is_connected() ? "ON" : "OFF") << "]";
        if (i + 1 < clients_.size()) ss << " ";
    }
    return ss.str();
}

} // namespace Common
