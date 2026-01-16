#include "common/kinematics.h"
#include <cmath>
#include <iostream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Common {


StewartPlatformKinematics::StewartPlatformKinematics(const PlatformGeometry& geometry)
        : geometry_(geometry), nominal_leg_length_(geometry.nominal_leg_length) {
        initializeGeometry();
        calculateNominalLegLength();
}

StewartPlatformKinematics::StewartPlatformKinematics(double base_radius, double platform_radius,
                                                                                                         double min_leg_length, double max_leg_length)
        : geometry_(), nominal_leg_length_(0) {
        geometry_.base_radius = base_radius;
        geometry_.platform_radius = platform_radius;
        geometry_.min_leg_length = min_leg_length;
        geometry_.max_leg_length = max_leg_length;
        initializeGeometry();
        calculateNominalLegLength();
}

void StewartPlatformKinematics::initializeGeometry() {
    // 参数化几何配置，参考BGsystem
    const double deg_to_rad = M_PI / 180.0;
    double R_down = geometry_.base_radius;
    double R_up = geometry_.platform_radius;
    double a_down = geometry_.base_half_angle;
    double a_up = geometry_.platform_half_angle;

    // 下平台铰点角度
    for (int i = 0; i < 6; ++i) {
        double angle;
        if (i % 2 == 0)
            angle = M_PI / 3 * i + a_down * deg_to_rad;
        else
            angle = M_PI / 3 * (i + 1) - a_down * deg_to_rad;
        base_points_[i][0] = R_down * cos(angle);
        base_points_[i][1] = R_down * sin(angle);
        base_points_[i][2] = 0.0;
    }
    // 上平台铰点角度
    for (int i = 0; i < 6; ++i) {
        double angle;
        if (i % 2 == 0)
            angle = M_PI / 3 * i + a_up * deg_to_rad;
        else
            angle = M_PI / 3 * (i + 1) - a_up * deg_to_rad;
        platform_points_[i][0] = R_up * cos(angle);
        platform_points_[i][1] = R_up * sin(angle);
        platform_points_[i][2] = 0.0;
    }
}

void StewartPlatformKinematics::getRotationMatrix(double rx, double ry, double rz, std::array<std::array<double, 3>, 3>& R) {
    // 标准 Rx * Ry * Rz 旋转矩阵（已修正）
    // rx=aa(Roll), ry=bb(Pitch), rz=cc(Yaw)
    // 对应 BGsystem 的 arf, br, cr
    //
    // 修正说明：
    // BGsystem 原始代码 (BGsystem.cpp 第358行) 的 R[2][0] 为:
    //   sin(arf)*sin(br) - cos(arf)*sin(br)*cos(cr)  即 sa*sb - ca*sb*cc
    // 标准 Rx*Ry*Rz 旋转矩阵的 R[2][0] 应为:
    //   sin(arf)*sin(cr) - cos(arf)*sin(br)*cos(cr)  即 sa*sc - ca*sb*cc
    //
    // 差异影响分析：
    //   差异 = sin(roll) * (sin(pitch) - sin(yaw))
    //   - 纯平移运动：无影响（roll=0）
    //   - 纯单轴旋转：无影响
    //   - 组合旋转且 roll≠0, pitch≠yaw：有影响
    //     例如 roll=10°, pitch=5°, yaw=0° 时，最大差异约 1.5mm
    //
    const double deg_to_rad = M_PI / 180.0;
    double ca = cos(rx * deg_to_rad);  // cos(arf) - Roll
    double sa = sin(rx * deg_to_rad);  // sin(arf)
    double cb = cos(ry * deg_to_rad);  // cos(br) - Pitch
    double sb = sin(ry * deg_to_rad);  // sin(br)
    double cc = cos(rz * deg_to_rad);  // cos(cr) - Yaw
    double sc = sin(rz * deg_to_rad);  // sin(cr)

    // 标准 Rx * Ry * Rz 旋转矩阵
    // 与 BGsystem 原始代码仅 R[2][0] 元素不同
    R[0][0] = cc * cb;
    R[0][1] = -cb * sc;
    R[0][2] = sb;

    R[1][0] = cc * sb * sa + ca * sc;
    R[1][1] = cc * ca - sc * sb * sa;
    R[1][2] = -sa * cb;

    // 修正后：sa * sc（标准形式），BGsystem 原始为 sa * sb
    R[2][0] = sa * sc - ca * sb * cc;
    R[2][1] = ca * sb * sc + sa * cc;
    R[2][2] = cb * ca;
}

void StewartPlatformKinematics::calculateNominalLegLength() {
    // 若外部已配置标称腿长，则直接使用（避免被自动计算覆盖）
    // if (geometry_.nominal_leg_length > 0) {
    //     nominal_leg_length_ = geometry_.nominal_leg_length;
    //     return;
    // }

    // 参考BGsystem: L = sqrt(H_up_down^2 + d1^2 + d2^2)
    // 兼容：若未设置H_up_down，则回退使用initial_height（两者在本项目里都表示铰点面间距离）
    double d1 = platform_points_[0][0] - base_points_[0][0];
    double d2 = platform_points_[0][1] - base_points_[0][1];
    double H = (geometry_.H_up_down > 0) ? geometry_.H_up_down : geometry_.initial_height;
    nominal_leg_length_ = sqrt(H * H + d1 * d1 + d2 * d2);
    std::cout << "Calculated nominal leg length: " << nominal_leg_length_ << std::endl;
}

void StewartPlatformKinematics::getBasePoints(std::array<std::array<double, 3>, 6>& points) const {
    points = base_points_;
}
void StewartPlatformKinematics::getPlatformPoints(std::array<std::array<double, 3>, 6>& points) const {
    points = platform_points_;
}

bool StewartPlatformKinematics::calculateInverseKinematics(const Pose& pose, std::array<double, 6>& leg_lengths) {
    std::array<std::array<double, 3>, 3> R;
    getRotationMatrix(pose.rx, pose.ry, pose.rz, R);
    std::cout << pose.rx << ", " << pose.ry << ", " << pose.rz << std::endl;
    std::cout << "R: " << std::endl;
    for (int i = 0; i < 3; ++i) {   
        std::cout << R[i][0] << ", " << R[i][1] << ", " << R[i][2] << std::endl;
    }
    std::cout << "cal done!" << std::endl;
    // Translation vector T
    std::cout << "Pose: " << pose.x << ", " << pose.y << ", " << pose.z << ", " << geometry_.initial_height << std::endl;
    double T[3] = {pose.x, pose.y, pose.z + geometry_.initial_height};
    std::cout << "T: " << T[0] << ", " << T[1] << ", " << T[2] << std::endl;

    for (int i = 0; i < 6; ++i) {
        // Calculate transformed platform point: q_i = T + R * p_i
        double q[3];
        q[0] = T[0] + R[0][0] * platform_points_[i][0] + R[0][1] * platform_points_[i][1] + R[0][2] * platform_points_[i][2];
        q[1] = T[1] + R[1][0] * platform_points_[i][0] + R[1][1] * platform_points_[i][1] + R[1][2] * platform_points_[i][2];
        q[2] = T[2] + R[2][0] * platform_points_[i][0] + R[2][1] * platform_points_[i][1] + R[2][2] * platform_points_[i][2];
        std::cout << "q[" << i << "]: " << q[0] << ", " << q[1] << ", " << q[2] << std::endl;

        // Calculate vector l_i = q_i - b_i
        double l[3];
        l[0] = q[0] - base_points_[i][0];
        l[1] = q[1] - base_points_[i][1];
        l[2] = q[2] - base_points_[i][2];
        std::cout << "l[" << i << "]: " << l[0] << ", " << l[1] << ", " << l[2] << std::endl;

        // Calculate length
        double length = sqrt(l[0] * l[0] + l[1] * l[1] + l[2] * l[2]);
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Leg " << i << " length: " << length << std::endl;
        // Check limits (if limits are set to 0, ignore them for now or assume valid)
        if (geometry_.min_leg_length > 0 && length < geometry_.min_leg_length) return false;
        if (geometry_.max_leg_length > 0 && length > geometry_.max_leg_length) return false;
        leg_lengths[i] = length;
    }

    return true;
}

bool StewartPlatformKinematics::calculateDisplacement(const Pose& target_pose,
                              const std::array<double, 6>& current_leg_lengths,
                              std::array<double, 6>& delta_lengths) {
    std::array<double, 6> target_lengths;
    if (!calculateInverseKinematics(target_pose, target_lengths)) return false;
    for (int i = 0; i < 6; ++i) {
        delta_lengths[i] = target_lengths[i] - current_leg_lengths[i];
    }
    return true;
}

void StewartPlatformKinematics::convertTargetPoseToPlatformPose(const Pose& target_pose, Pose& platform_pose) const {
    // BGsystem中的关键理解：
    // 1. 靶点 pose 的 z=0 表示靶点在某个参考高度
    // 2. BGsystem 的 H = H_target_upd + H_upd_up + H_up_down 是"靶点到下铰点面"的总高度
    // 3. 上平台铰点面在靶点下方 (H_target_upd + H_upd_up) 的距离
    // 4. 上平台铰点面到下铰点面的距离是 H_up_down
    
    // 姿态不变（绕同一个点旋转）
    platform_pose.rx = target_pose.rx;
    platform_pose.ry = target_pose.ry;
    platform_pose.rz = target_pose.rz;
    
    // BGsystem 的平移向量：p = [xx, yy, zz + H]
    // 其中 H = H_target_upd + H_upd_up + H_up_down
    // 
    // 而我们的 calculateZAxisDisplacement 中使用：T = [pose.x, pose.y, pose.z + H_up_down]
    // 这意味着 pose.z 应该是"铰点面相对下铰点面的高度"
    //
    // 从靶点到铰点面：靶点 z + H - H_up_down = 靶点 z + H_target_upd + H_upd_up
    // 所以铰点面的 z = 靶点 z + H_target_upd + H_upd_up
    
    platform_pose.x = target_pose.x;
    platform_pose.y = target_pose.y;
    platform_pose.z = target_pose.z + geometry_.H_target_upd + geometry_.H_upd_up;
}

bool StewartPlatformKinematics::calculateZAxisDisplacement(const Pose& pose, std::array<double, 6>& z_displacements) {
    // BGsystem风格：计算各腿的Z轴投影位移
    // 用于垂直驱动杆的Stewart平台，驱动杆只沿Z轴运动
    // 重要：pose必须是"上平台铰点面"的位姿，不是靶点！
    // 如果输入是靶点pose，请先调用 convertTargetPoseToPlatformPose() 或使用 calculateZAxisDisplacementFromTarget()
    
    std::array<std::array<double, 3>, 3> R;
    getRotationMatrix(pose.rx, pose.ry, pose.rz, R);
    
    // BGsystem的H: 靶点到下铰点面的总高度
    // 这里pose.z是铰点面相对某参考的高度，需要加上到下铰点面的距离
    double T[3] = {pose.x, pose.y, pose.z + geometry_.H_up_down};
    
    double L = nominal_leg_length_;
    double L_squared = L * L;
    
    for (int i = 0; i < 6; ++i) {
        // 计算平台铰点在世界坐标系中的位置
        double q[3];
        q[0] = T[0] + R[0][0] * platform_points_[i][0] + R[0][1] * platform_points_[i][1] + R[0][2] * platform_points_[i][2];
        q[1] = T[1] + R[1][0] * platform_points_[i][0] + R[1][1] * platform_points_[i][1] + R[1][2] * platform_points_[i][2];
        q[2] = T[2] + R[2][0] * platform_points_[i][0] + R[2][1] * platform_points_[i][1] + R[2][2] * platform_points_[i][2];
        
        // 计算水平面内的距离分量
        double d1 = q[0] - base_points_[i][0];
        double d2 = q[1] - base_points_[i][1];
        double d_horizontal_squared = d1 * d1 + d2 * d2;
        
        // 边界检查：确保水平距离不超过腿长
        if (d_horizontal_squared >= L_squared) {
            // 位姿不可达，水平偏移过大
            return false;
        }
        
        double z = q[2] - base_points_[i][2];
        // Z轴投影：当前Z高度减去腿在Z方向的理论投影长度
        // 完全对应 BGsystem 的: m[i] = Tran_l[2][i] - sqrt(L*L - Tran_l[0]^2 - Tran_l[1]^2)
        double z_proj = z - sqrt(L_squared - d_horizontal_squared);
        z_displacements[i] = z_proj;
    }
    return true;
}

bool StewartPlatformKinematics::calculateZAxisDisplacementFromTarget(const Pose& target_pose, std::array<double, 6>& z_displacements) {
    // 完全按照 BGsystem Cal_s 函数实现
    // 参数: target_pose 对应 BGsystem 的 (xx, yy, zz, aa, bb, cc)
    // 其中 rx=aa(Roll), ry=bb(Pitch), rz=cc(Yaw)
    
    const double deg_to_rad = M_PI / 180.0;
    double R_up = geometry_.platform_radius;
    double R_down = geometry_.base_radius;
    double a_up_rad = geometry_.platform_half_angle * deg_to_rad;
    double a_down_rad = geometry_.base_half_angle * deg_to_rad;
    
    // BGsystem 的 H = H_target_upd + H_upd_up + H_up_down（靶点到下铰点面的总高度）
    double H = geometry_.H_target_upd + geometry_.H_upd_up + geometry_.H_up_down;
    double L = nominal_leg_length_;
    double L_squared = L * L;
    
    // 获取旋转矩阵（已修改为BGsystem兼容格式）
    std::array<std::array<double, 3>, 3> T;
    getRotationMatrix(target_pose.rx, target_pose.ry, target_pose.rz, T);
    
    // 靶点的位置向量 p = [xx, yy, zz + H]（BGsystem 第354行）
    double p[3] = {target_pose.x, target_pose.y, target_pose.z + H};
    
    for (int i = 0; i < 6; ++i) {
        // 计算铰点角度（与 BGsystem 完全一致）
        double angle_up, angle_down;
        if (i % 2 == 0) {
            angle_up = M_PI / 3 * i + a_up_rad;
            angle_down = M_PI / 3 * i + a_down_rad;
        } else {
            angle_up = M_PI / 3 * (i + 1) - a_up_rad;
            angle_down = M_PI / 3 * (i + 1) - a_down_rad;
        }
        
        // 上铰点在动坐标系中的位置（BGsystem 第331-337行）
        // 关键：Z坐标为负值，表示铰点在靶点下方
        // 这使得旋转中心在靶点！
        double p_u[3] = {
            R_up * cos(angle_up),
            R_up * sin(angle_up),
            -(geometry_.H_target_upd + geometry_.H_upd_up)  // BGsystem: -H_target_upd - H_upd_up
        };
        
        // 下铰点在静坐标系中的位置（BGsystem 第339-344行）
        double p_d[3] = {
            R_down * cos(angle_down),
            R_down * sin(angle_down),
            0.0
        };
        
        // 计算上铰点在静坐标系中的位置: Tran_p_u = T * p_u
        double Tran_p_u[3];
        for (int m = 0; m < 3; ++m) {
            Tran_p_u[m] = T[m][0] * p_u[0] + T[m][1] * p_u[1] + T[m][2] * p_u[2];
        }
        
        // 计算连杆向量: Tran_l = Tran_p_u + p - p_d（BGsystem 第381行）
        double Tran_l[3];
        for (int j = 0; j < 3; ++j) {
            Tran_l[j] = Tran_p_u[j] + p[j] - p_d[j];
        }
        
        // 计算水平距离的平方
        double d_horizontal_squared = Tran_l[0] * Tran_l[0] + Tran_l[1] * Tran_l[1];
        
        // 边界检查
        if (d_horizontal_squared >= L_squared) {
            return false;  // 位姿不可达
        }
        
        // Z轴投影位移（BGsystem 第387行）
        // m[i] = Tran_l[2] - sqrt(L*L - Tran_l[0]^2 - Tran_l[1]^2)
        z_displacements[i] = Tran_l[2] - sqrt(L_squared - d_horizontal_squared);
    }
    
    return true;
}

// ================== TrajectoryPlanner 实现 ==================
double TrajectoryPlanner::planCosineProfile(double displacement, int speed_level,
                                    std::vector<PVTPoint>& trajectory) {
    // 速度等级越高，时间越短 (speed_level: 1-9)
    int T_temp = 10 - speed_level;
    if (T_temp < 1) T_temp = 1;  // 防止除零
    int C_temp = T_temp + 1;
    double T_time = 1.0; // 每步时间间隔 (s)
    
    // 余弦速度曲线：使用 (1 - cos) / 2 实现从0到displacement的平滑过渡
    // 位置: s(t) = displacement * (1 - cos(π * t / T)) / 2
    // 速度: v(t) = displacement * π / (2 * T) * sin(π * t / T)
    double omega = M_PI / T_temp;  // 角频率
    
    trajectory.clear();
    for (int i = 0; i < C_temp; ++i) {
        double t_ratio = static_cast<double>(i) / T_temp;  // 归一化时间 [0, 1]
        double pos = displacement * (1.0 - cos(M_PI * t_ratio)) / 2.0;
        double vel = displacement * omega * sin(M_PI * t_ratio) / 2.0;
        double time = T_time * i;
        
        // 确保起点和终点精确
        if (i == 0) { pos = 0.0; vel = 0.0; }
        if (i == T_temp) { pos = displacement; vel = 0.0; }
        
        trajectory.push_back({pos, vel, time});
    }
    return T_temp * T_time;
}

double TrajectoryPlanner::planSixAxisTrajectory(const std::array<double, 6>& displacements,
                                        int speed_level,
                                        std::array<std::vector<PVTPoint>, 6>& trajectories) {
    double max_time = 0.0;
    for (int axis = 0; axis < 6; ++axis) {
        double t = planCosineProfile(displacements[axis], speed_level, trajectories[axis]);
        if (t > max_time) max_time = t;
    }
    return max_time;
}

long TrajectoryPlanner::displacementToPulse(double displacement_mm, double pulse_per_mm) {
    return static_cast<long>(displacement_mm * pulse_per_mm);
}

} // namespace Common
