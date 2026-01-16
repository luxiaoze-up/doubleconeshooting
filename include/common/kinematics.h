#ifndef KINEMATICS_H
#define KINEMATICS_H

#include <vector>
#include <array>
#include <cmath>
#include <string>

namespace Common {

/**
 * @brief 六自由度位姿结构体
 */
struct Pose {
    double x, y, z;       // 平移量 (mm)
    double rx, ry, rz;    // 旋转量 (度)
    
    Pose() : x(0), y(0), z(0), rx(0), ry(0), rz(0) {}
    Pose(double x_, double y_, double z_, double rx_, double ry_, double rz_)
        : x(x_), y(y_), z(z_), rx(rx_), ry(ry_), rz(rz_) {}
};

/**
 * @brief Stewart平台几何配置
 * 参考BGsystem的参数化配置方式
 */
struct PlatformGeometry {
    double base_radius;           // 下平台（基座）半径 R_down (mm)
    double platform_radius;       // 上平台（动平台）半径 R_up (mm)
    double base_half_angle;       // 下铰点对半角 a_down (度)
    double platform_half_angle;   // 上铰点对半角 a_up (度)
    double initial_height;        // 初始高度 H (mm)
    double nominal_leg_length;    // 标称连杆长度 L (mm), 0表示自动计算
    double min_leg_length;        // 最小连杆长度 (mm)
    double max_leg_length;        // 最大连杆长度 (mm)
    
    // 双锥打靶系统特定参数
    double H_target_upd;          // 靶点到上平台距离 (mm)
    double H_upd_up;              // 上平台到上铰点面距离 (mm)
    double H_up_down;             // 上铰点面到下铰点面距离 (mm)
    
    PlatformGeometry() 
        : base_radius(200.0), platform_radius(100.0),
          base_half_angle(10.0), platform_half_angle(10.0),
          initial_height(300.0), nominal_leg_length(0),
          min_leg_length(250.0), max_leg_length(400.0),
          H_target_upd(0), H_upd_up(0), H_up_down(0) {}
};

/**
 * @brief PVT轨迹点
 */
struct PVTPoint {
    double position;    // 位置 (脉冲或mm)
    double velocity;    // 速度 (脉冲/s或mm/s)
    double time;        // 时间 (s)
};

/**
 * @brief 轨迹规划器 - 余弦加减速PVT规划
 * 参考BGsystem的PVT_6Move函数实现
 */
class TrajectoryPlanner {
public:
    /**
     * @brief 根据位移量规划余弦加减速轨迹
     * @param displacement 目标位移量 (mm)
     * @param speed_level 速度等级 (0-10, 越大越快)
     * @param trajectory 输出轨迹点数组
     * @return 规划的总时间 (s)
     */
    static double planCosineProfile(double displacement, int speed_level,
                                    std::vector<PVTPoint>& trajectory);
    
    /**
     * @brief 为6轴同时规划轨迹
     * @param displacements 6轴位移量
     * @param speed_level 速度等级
     * @param trajectories 6轴轨迹输出
     * @return 规划的总时间 (s)
     */
    static double planSixAxisTrajectory(const std::array<double, 6>& displacements,
                                        int speed_level,
                                        std::array<std::vector<PVTPoint>, 6>& trajectories);

    /**
     * @brief 将位移转换为脉冲数
     * @param displacement_mm 位移量 (mm)
     * @param pulse_per_mm 每毫米脉冲数
     * @return 脉冲数
     */
    static long displacementToPulse(double displacement_mm, double pulse_per_mm = 1000.0);
};

/**
 * @brief Stewart平台逆运动学求解器
 * 结合BGsystem的算法和标准Stewart平台几何
 */
class StewartPlatformKinematics {
public:
    /**
     * @brief 使用几何配置构造
     */
    explicit StewartPlatformKinematics(const PlatformGeometry& geometry);
    
    /**
     * @brief 兼容旧接口的构造函数
     */
    StewartPlatformKinematics(double base_radius, double platform_radius, 
                              double min_leg_length, double max_leg_length);

    /**
     * @brief 计算给定位姿下的腿长（绝对值）
     * @param pose 目标位姿
     * @param leg_lengths 输出6条腿的长度
     * @return 是否在可达范围内
     */
    bool calculateInverseKinematics(const Pose& pose, std::array<double, 6>& leg_lengths);
    
    /**
     * @brief 计算增量位移（相对于当前位置）
     * 参考BGsystem的Cal_s函数: m[i] = m[i] - pre_m[i]
     * @param target_pose 目标位姿
     * @param current_leg_lengths 当前腿长
     * @param delta_lengths 输出增量位移
     * @return 是否在可达范围内
     */
    bool calculateDisplacement(const Pose& target_pose,
                              const std::array<double, 6>& current_leg_lengths,
                              std::array<double, 6>& delta_lengths);
    
    /**
     * @brief 使用BGsystem算法计算Z轴投影位移
     * 适用于直线推杆只能在Z方向移动的情况
     * @param pose 上平台铰点面的位姿（不是靶点！）
     * @param z_displacements 输出6个推杆的Z向位移（绝对值）
     * @return 是否在可达范围内
     */
    bool calculateZAxisDisplacement(const Pose& pose, std::array<double, 6>& z_displacements);
    
    /**
     * @brief 将靶点pose转换为上平台铰点面pose（BGsystem兼容）
     * 使用 H_target_upd 和 H_upd_up 做几何偏移
     * @param target_pose 靶点的位姿（输入）
     * @param platform_pose 上平台铰点面的位姿（输出）
     */
    void convertTargetPoseToPlatformPose(const Pose& target_pose, Pose& platform_pose) const;
    
    /**
     * @brief 直接从靶点pose计算Z轴投影位移（BGsystem Cal_s 等价接口）
     * 内部会先转换为铰点面pose，再调用 calculateZAxisDisplacement
     * @param target_pose 靶点的位姿（与BGsystem的xx,yy,zz,aa,bb,cc对应）
     * @param z_displacements 输出6个推杆的Z向位移
     * @return 是否在可达范围内
     */
    bool calculateZAxisDisplacementFromTarget(const Pose& target_pose, std::array<double, 6>& z_displacements);

    // Getters
    double getBaseRadius() const { return geometry_.base_radius; }
    double getPlatformRadius() const { return geometry_.platform_radius; }
    double getInitialHeight() const { return geometry_.initial_height; }
    double getNominalLegLength() const { return nominal_leg_length_; }
    const PlatformGeometry& getGeometry() const { return geometry_; }
    
    /**
     * @brief 获取铰点坐标（用于调试和可视化）
     */
    void getBasePoints(std::array<std::array<double, 3>, 6>& points) const;
    void getPlatformPoints(std::array<std::array<double, 3>, 6>& points) const;

private:
    PlatformGeometry geometry_;
    double nominal_leg_length_;  // 计算得到的标称连杆长度
    
    // 基座铰点 (静坐标系)
    std::array<std::array<double, 3>, 6> base_points_;
    // 平台铰点 (动坐标系)
    std::array<std::array<double, 3>, 6> platform_points_;

    /**
     * @brief 初始化几何参数
     * 参考BGsystem的铰点角度计算方式
     */
    void initializeGeometry();
    
    /**
     * @brief 计算旋转矩阵 (Z-Y-X欧拉角)
     */
    void getRotationMatrix(double rx, double ry, double rz, 
                          std::array<std::array<double, 3>, 3>& R);
    
    /**
     * @brief 计算标称连杆长度
     * 参考BGsystem: L = sqrt(H_up_down² + d1² + d2²)
     */
    void calculateNominalLegLength();
};

} // namespace Common

#endif // KINEMATICS_H
