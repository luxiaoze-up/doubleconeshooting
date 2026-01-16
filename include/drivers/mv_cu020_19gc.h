#ifndef MV_CU020_19GC_H
#define MV_CU020_19GC_H

#include <string>
#include <memory>
#include <mutex>
#include <vector>

namespace Hikvision {

/**
 * @brief 海康MV-CU020-19GC相机驱动封装类
 * 
 * 功能：
 * - 相机初始化和关闭
 * - 相机开关控制
 * - 参数设置（曝光、增益、亮度、对比度、分辨率、触发模式）
 * - 图像捕获和保存
 * - Base64编码图像获取
 */
class MV_CU020_19GC {
public:
    /**
     * @brief 相机状态枚举
     */
    enum CameraState {
        CAMERA_OFF = 0,      // 关闭
        CAMERA_INITIALIZING, // 初始化中
        CAMERA_READY,        // 就绪
        CAMERA_CAPTURING,    // 采集中
        CAMERA_ERROR         // 错误
    };

    /**
     * @brief 触发模式枚举
     */
    enum TriggerMode {
        TRIGGER_SOFTWARE = 0,  // 软件触发
        TRIGGER_HARDWARE,       // 硬件触发
        TRIGGER_CONTINUOUS      // 连续采集
    };

    /**
     * @brief 构造函数
     * @param camera_id 相机ID（从配置中解析，如"upper_ccd_1x"）
     * @param device_index 设备索引（用于枚举设备）
     */
    explicit MV_CU020_19GC(const std::string& camera_id, int device_index = -1);
    
    /**
     * @brief 析构函数
     */
    ~MV_CU020_19GC();

    // 禁止拷贝和赋值
    MV_CU020_19GC(const MV_CU020_19GC&) = delete;
    MV_CU020_19GC& operator=(const MV_CU020_19GC&) = delete;

    /**
     * @brief 初始化相机
     * @return 成功返回true，失败返回false
     */
    bool initialize();

    /**
     * @brief 关闭相机
     */
    void shutdown();

    /**
     * @brief 打开相机（开始采集）
     * @return 成功返回true，失败返回false
     */
    bool open();

    /**
     * @brief 关闭相机（停止采集）
     */
    void close();

    /**
     * @brief 获取相机状态
     * @return 相机状态
     */
    CameraState getState() const { return state_; }

    /**
     * @brief 检查相机是否就绪
     * @return 就绪返回true
     */
    bool isReady() const { return state_ == CAMERA_READY; }

    // ===== 参数设置 =====

    /**
     * @brief 设置曝光时间
     * @param exposure_ms 曝光时间（毫秒）
     * @return 成功返回true
     */
    bool setExposureTime(double exposure_ms);

    /**
     * @brief 获取曝光时间
     * @return 曝光时间（毫秒）
     */
    double getExposureTime() const { return exposure_time_ms_; }

    /**
     * @brief 设置增益
     * @param gain 增益值（0.0-100.0）
     * @return 成功返回true
     */
    bool setGain(double gain);

    /**
     * @brief 获取增益
     * @return 增益值
     */
    double getGain() const { return gain_; }

    /**
     * @brief 设置亮度
     * @param brightness 亮度值（-100.0到100.0）
     * @return 成功返回true
     */
    bool setBrightness(double brightness);

    /**
     * @brief 获取亮度
     * @return 亮度值
     */
    double getBrightness() const { return brightness_; }

    /**
     * @brief 设置对比度
     * @param contrast 对比度值（-100.0到100.0）
     * @return 成功返回true
     */
    bool setContrast(double contrast);

    /**
     * @brief 获取对比度
     * @return 对比度值
     */
    double getContrast() const { return contrast_; }

    /**
     * @brief 设置分辨率
     * @param width 宽度
     * @param height 高度
     * @return 成功返回true
     */
    bool setResolution(long width, long height);

    /**
     * @brief 获取分辨率宽度
     * @return 宽度
     */
    long getWidth() const { return width_; }

    /**
     * @brief 获取分辨率高度
     * @return 高度
     */
    long getHeight() const { return height_; }

    /**
     * @brief 设置触发模式
     * @param mode 触发模式
     * @return 成功返回true
     */
    bool setTriggerMode(TriggerMode mode);

    /**
     * @brief 获取触发模式
     * @return 触发模式
     */
    TriggerMode getTriggerMode() const { return trigger_mode_; }

    /**
     * @brief 设置环形光开关
     * @param on 开启/关闭
     * @return 成功返回true
     */
    bool setRingLight(bool on);

    /**
     * @brief 获取环形光状态
     * @return 开启返回true
     */
    bool isRingLightOn() const { return ring_light_on_; }

    // ===== 图像操作 =====

    /**
     * @brief 捕获图像并保存到文件
     * @param file_path 保存路径
     * @return 成功返回true
     */
    bool captureImage(const std::string& file_path);

    /**
     * @brief 获取最新图像（Base64编码）
     * @return Base64编码的图像数据字符串
     */
    std::string getLatestImageBase64();

    /**
     * @brief 获取错误信息
     * @return 错误信息字符串
     */
    std::string getLastError() const { return last_error_; }

private:
    std::string camera_id_;           // 相机ID
    int device_index_;                // 设备索引
    CameraState state_;               // 相机状态
    std::mutex mutex_;                // 互斥锁

    // 相机句柄（根据实际SDK类型定义）
    // 这里使用void*作为占位符，实际应该使用SDK提供的句柄类型
    void* camera_handle_;             // 相机句柄

    // 参数缓存
    double exposure_time_ms_;          // 曝光时间（毫秒）
    double gain_;                     // 增益
    double brightness_;               // 亮度
    double contrast_;                 // 对比度
    long width_;                      // 宽度
    long height_;                     // 高度
    TriggerMode trigger_mode_;        // 触发模式
    bool ring_light_on_;              // 环形光状态

    std::string last_error_;          // 最后错误信息

    /**
     * @brief 设置错误信息
     */
    void setError(const std::string& error);

    /**
     * @brief 检查相机是否已初始化
     */
    bool checkInitialized() const;

    /**
     * @brief 应用参数到硬件（内部函数）
     */
    bool applyParameters();

    /**
     * @brief 从硬件读取参数（内部函数）
     */
    bool readParameters();
};

} // namespace Hikvision

#endif // MV_CU020_19GC_H

