/**
 * @file vacuum_system_plc_mapping.h
 * @brief 真空系统 PLC 点位映射 - 基于西门子 PLC 点位表 (全新版本)
 * 
 * 设备服务: sys/vacuum/2
 * 通讯协议: OPC UA
 * 
 * 点位地址说明:
 * - %I: 输入信号 (Bool)
 * - %Q: 输出信号 (Bool)
 * - %IW: 输入字 (Word)
 * - %QW: 输出字 (Word)
 */

#ifndef VACUUM_SYSTEM_PLC_MAPPING_H
#define VACUUM_SYSTEM_PLC_MAPPING_H

#include "common/plc_communication.h"
#include <string>
#include <map>

namespace VacuumSystem {
namespace PLC {

using PLCAddress = Common::PLC::PLCAddress;
using PLCAddressType = Common::PLC::PLCAddressType;

/**
 * @brief 真空系统 PLC 点位映射类
 * 完全基于用户提供的西门子 PLC 点位表
 */
class VacuumSystemPLCMapping {
public:
    // ========================================================================
    // 输入信号 (I) - Bool 类型
    // ========================================================================
    
    // ----- 泵类设备上电反馈 -----
    static PLCAddress ScrewPumpPowerFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 0, 0);  // %I0.0 螺杆泵上电
    }
    static PLCAddress RootsPumpPowerFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 0, 1);  // %I0.1 罗茨泵上电
    }
    static PLCAddress MolecularPump1PowerFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 0, 2);  // %I0.2 分子泵1上电反馈
    }
    static PLCAddress MolecularPump2PowerFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 0, 3);  // %I0.3 分子泵2上电反馈
    }
    static PLCAddress MolecularPump3PowerFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 0, 4);  // %I0.4 分子泵3上电反馈
    }
    
    // ----- 系统保护信号 -----
    static PLCAddress PhaseSequenceProtection() {
        return PLCAddress(PLCAddressType::INPUT, 0, 5);  // %I0.5 相序保护
    }
    
    // ----- 电磁阀到位信号 -----
    static PLCAddress ElectromagneticValve1OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 0, 6);  // %I0.6 电磁阀1开到位信号
    }
    static PLCAddress ElectromagneticValve1CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 0, 7);  // %I0.7 电磁阀1关到位信号
    }
    static PLCAddress ElectromagneticValve2OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 1, 0);  // %I1.0 电磁阀2开到位信号
    }
    static PLCAddress ElectromagneticValve2CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 1, 1);  // %I1.1 电磁阀2关到位信号
    }
    static PLCAddress ElectromagneticValve3OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 1, 2);  // %I1.2 电磁阀3开到位信号
    }
    static PLCAddress ElectromagneticValve3CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 1, 3);  // %I1.3 电磁阀3关到位信号
    }
    static PLCAddress ElectromagneticValve4OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 1, 4);  // %I1.4 电磁阀4开到位信号
    }
    static PLCAddress ElectromagneticValve4CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 1, 5);  // %I1.5 电磁阀4关到位信号
    }
    
    // ----- 放气阀到位信号 -----
    static PLCAddress VentValve1OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 8, 0);  // %I8.0 放气阀1开到位信号
    }
    static PLCAddress VentValve1CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 8, 1);  // %I8.1 放气阀1关到位信号
    }
    static PLCAddress VentValve2OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 8, 2);  // %I8.2 放气阀2开到位信号
    }
    static PLCAddress VentValve2CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 8, 3);  // %I8.3 放气阀2关到位信号
    }
    
    // ----- 闸板阀到位信号 -----
    static PLCAddress GateValve1OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 8, 4);  // %I8.4 闸板阀1开到位
    }
    static PLCAddress GateValve1CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 8, 5);  // %I8.5 闸板阀1关到位
    }
    static PLCAddress GateValve2OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 8, 6);  // %I8.6 闸板阀2开到位
    }
    static PLCAddress GateValve2CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 8, 7);  // %I8.7 闸板阀2关到位
    }
    static PLCAddress GateValve3OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 9, 0);  // %I9.0 闸板阀3开到位
    }
    static PLCAddress GateValve3CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 9, 1);  // %I9.1 闸板阀3关到位
    }
    static PLCAddress GateValve4OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 9, 2);  // %I9.2 闸板阀4开到位
    }
    static PLCAddress GateValve4CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 9, 3);  // %I9.3 闸板阀4关到位
    }
    static PLCAddress GateValve5OpenFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 9, 4);  // %I9.4 闸板阀5开到位
    }
    static PLCAddress GateValve5CloseFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 9, 5);  // %I9.5 闸板阀5关到位
    }
    
    // ----- 运动控制系统相关信号 -----
    static PLCAddress MotionControlSystemOnline() {
        return PLCAddress(PLCAddressType::INPUT, 9, 6);  // %I9.6 运动控制系统设备在线
    }
    static PLCAddress GateValve5ActionPermit() {
        return PLCAddress(PLCAddressType::INPUT, 9, 7);  // %I9.7 闸板阀5动作允许信号
    }
    static PLCAddress MotionControlRequestOpenGateValve5() {
        return PLCAddress(PLCAddressType::INPUT, 12, 0);  // %I12.0 运动控制系统请求开闸板阀5
    }
    static PLCAddress MotionControlRequestCloseGateValve5() {
        return PLCAddress(PLCAddressType::INPUT, 12, 1);  // %I12.1 运动控制系统请求关闸板阀5
    }
    
    // ========================================================================
    // 模拟量输入 (IW) - Word 类型
    // ========================================================================
    
    static PLCAddress ResistanceGaugeVoltage() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 130, -1);  // %IW130 睿宝电阻规模拟量输入（电压）
    }
    static PLCAddress AirPressureSensorCurrent() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 132, -1);  // %IW132 气路压力传感器模拟量输入（电流）
    }
    static PLCAddress MolecularPump1Speed() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 24, -1);  // %IW24 分子泵1转速
    }
    static PLCAddress MolecularPump2Speed() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 36, -1);  // %IW36 分子泵2转速
    }
    static PLCAddress MolecularPump3Speed() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 48, -1);  // %IW48 分子泵3转速
    }
    
    // ========================================================================
    // 输出信号 (Q) - Bool 类型
    // ========================================================================
    
    // ----- 螺杆泵控制 -----
    static PLCAddress ScrewPumpStartStop() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 0);  // %Q0.0 螺杆泵启停
    }
    static PLCAddress ScrewPumpPowerOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 1);  // %Q0.1 螺杆泵上电输出
    }
    
    // ----- 罗茨泵控制 -----
    static PLCAddress RootsPumpPowerOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 2);  // %Q0.2 罗茨泵上电输出
    }
    
    // ----- 分子泵控制 -----
    static PLCAddress MolecularPump1PowerOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 3);  // %Q0.3 分子泵1上电
    }
    static PLCAddress MolecularPump2PowerOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 4);  // %Q0.4 分子泵2上电
    }
    static PLCAddress MolecularPump3PowerOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 5);  // %Q0.5 分子泵3上电
    }
    
    // ----- 电磁阀控制 -----
    static PLCAddress ElectromagneticValve1Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 6);  // %Q0.6 电磁阀1开关输出
    }
    static PLCAddress ElectromagneticValve2Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 7);  // %Q0.7 电磁阀2开关输出
    }
    static PLCAddress ElectromagneticValve3Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 0);  // %Q1.0 电磁阀3开关输出
    }
    static PLCAddress ElectromagneticValve4Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 8, 0);  // %Q8.0 电磁阀4开关输出
    }
    
    // ----- 放气阀控制 -----
    static PLCAddress VentValve1Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 8, 1);  // %Q8.1 放气阀1开关输出
    }
    static PLCAddress VentValve2Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 8, 2);  // %Q8.2 放气阀2开关输出
    }
    
    // ----- 闸板阀控制 -----
    static PLCAddress GateValve1OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 8, 3);  // %Q8.3 闸板阀1开输出
    }
    static PLCAddress GateValve1CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 8, 4);  // %Q8.4 闸板阀1关输出
    }
    static PLCAddress GateValve2OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 8, 5);  // %Q8.5 闸板阀2开输出
    }
    static PLCAddress GateValve2CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 8, 6);  // %Q8.6 闸板阀2关输出
    }
    static PLCAddress GateValve3OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 8, 7);  // %Q8.7 闸板阀3开输出
    }
    static PLCAddress GateValve3CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 9, 0);  // %Q9.0 闸板阀3关输出
    }
    static PLCAddress GateValve4OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 9, 1);  // %Q9.1 闸板阀4开输出
    }
    static PLCAddress GateValve4CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 9, 2);  // %Q9.2 闸板阀4关输出
    }
    static PLCAddress GateValve5OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 9, 3);  // %Q9.3 闸板阀5开输出
    }
    static PLCAddress GateValve5CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 9, 4);  // %Q9.4 闸板阀5关输出
    }
    
    // ----- 水电磁阀控制 -----
    static PLCAddress WaterValve1Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 12, 0);  // %Q12.0 水电磁阀1开关输出
    }
    static PLCAddress WaterValve2Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 12, 1);  // %Q12.1 水电磁阀2开关输出
    }
    static PLCAddress WaterValve3Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 12, 2);  // %Q12.2 水电磁阀3开关输出
    }
    static PLCAddress WaterValve4Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 12, 3);  // %Q12.3 水电磁阀4开关输出
    }
    static PLCAddress WaterValve5Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 12, 4);  // %Q12.4 水电磁阀5开关输出
    }
    static PLCAddress WaterValve6Output() {
        return PLCAddress(PLCAddressType::OUTPUT, 12, 5);  // %Q12.5 水电磁阀6开关输出
    }
    
    // ----- 气主电磁阀控制 -----
    static PLCAddress AirMainValveOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 12, 6);  // %Q12.6 气主电磁阀开关输出
    }
    
    // ----- 螺杆泵故障复位 -----
    static PLCAddress ScrewPumpFaultReset() {
        return PLCAddress(PLCAddressType::OUTPUT, 12, 7);  // %Q12.7 螺杆泵故障复位
    }
    
    // ----- 分子泵启停 -----
    static PLCAddress MolecularPump1StartStop() {
        return PLCAddress(PLCAddressType::OUTPUT, 13, 0);  // %Q13.0 分子泵1启停
    }
    static PLCAddress MolecularPump2StartStop() {
        return PLCAddress(PLCAddressType::OUTPUT, 13, 1);  // %Q13.1 分子泵2启停
    }
    static PLCAddress MolecularPump3StartStop() {
        return PLCAddress(PLCAddressType::OUTPUT, 13, 2);  // %Q13.2 分子泵3启停
    }
    
    // ----- 分子泵启用配置 -----
    static PLCAddress MolecularPump1Enabled() {
        return PLCAddress(PLCAddressType::OUTPUT, 13, 3);  // %Q13.3 分子泵1启用配置
    }
    static PLCAddress MolecularPump2Enabled() {
        return PLCAddress(PLCAddressType::OUTPUT, 13, 4);  // %Q13.4 分子泵2启用配置
    }
    static PLCAddress MolecularPump3Enabled() {
        return PLCAddress(PLCAddressType::OUTPUT, 13, 5);  // %Q13.5 分子泵3启用配置
    }
    
    // ========================================================================
    // 模拟量输出 (QW) - Word 类型 (Int)
    // ========================================================================
    
    static PLCAddress MolecularPump1AddressTransfer() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 22, -1);  // %QW22 分子泵1启停地址传送
    }
    static PLCAddress MolecularPump2AddressTransfer() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 34, -1);  // %QW34 分子泵2启停地址传送
    }
    static PLCAddress MolecularPump3AddressTransfer() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 46, -1);  // %QW46 分子泵3启停地址传送
    }
    
    // ========================================================================
    // OPC UA 节点 ID 映射
    // ========================================================================
    
    /**
     * @brief 获取 OPC UA 节点 ID
     * @param address PLC 地址
     * @return OPC UA 节点 ID 字符串 (格式: ns=3;s=变量名)
     */
    static std::string GetOPCUANodeId(const PLCAddress& address) {
        // 本项目的 OPC-UA 通信使用“字符串标识符”风格的 NodeId。
        // 对于本机 Python PLC 模拟器：节点 Identifier 即为 address.address_string（例如 %IW130）。
        return "ns=3;s=" + address.address_string;
    }
    
    /**
     * @brief 获取所有输入点位列表（用于批量轮询）
     */
    static std::vector<PLCAddress> GetAllInputAddresses() {
        return {
            // 泵状态
            ScrewPumpPowerFeedback(),
            RootsPumpPowerFeedback(),
            MolecularPump1PowerFeedback(),
            MolecularPump2PowerFeedback(),
            MolecularPump3PowerFeedback(),
            PhaseSequenceProtection(),
            // 电磁阀
            ElectromagneticValve1OpenFeedback(),
            ElectromagneticValve1CloseFeedback(),
            ElectromagneticValve2OpenFeedback(),
            ElectromagneticValve2CloseFeedback(),
            ElectromagneticValve3OpenFeedback(),
            ElectromagneticValve3CloseFeedback(),
            ElectromagneticValve4OpenFeedback(),
            ElectromagneticValve4CloseFeedback(),
            // 放气阀
            VentValve1OpenFeedback(),
            VentValve1CloseFeedback(),
            VentValve2OpenFeedback(),
            VentValve2CloseFeedback(),
            // 闸板阀
            GateValve1OpenFeedback(),
            GateValve1CloseFeedback(),
            GateValve2OpenFeedback(),
            GateValve2CloseFeedback(),
            GateValve3OpenFeedback(),
            GateValve3CloseFeedback(),
            GateValve4OpenFeedback(),
            GateValve4CloseFeedback(),
            GateValve5OpenFeedback(),
            GateValve5CloseFeedback(),
            // 系统信号
            MotionControlSystemOnline(),
            GateValve5ActionPermit(),
            MotionControlRequestOpenGateValve5(),
            MotionControlRequestCloseGateValve5()
        };
    }
    
    /**
     * @brief 获取所有模拟量输入点位列表
     */
    static std::vector<PLCAddress> GetAllAnalogInputAddresses() {
        return {
            ResistanceGaugeVoltage(),
            AirPressureSensorCurrent(),
            MolecularPump1Speed(),
            MolecularPump2Speed(),
            MolecularPump3Speed()
        };
    }
};

// ============================================================================
// 操作条件定义 - 基于真空系统操作流程文档
// ============================================================================

/**
 * @brief 设备操作先决条件
 */
struct OperationPrerequisite {
    std::string device_name;      // 设备名称
    std::string operation;        // 操作类型 (open/close/start/stop)
    std::vector<std::string> conditions;  // 先决条件列表
    
    OperationPrerequisite(const std::string& name, const std::string& op,
                          std::initializer_list<std::string> conds)
        : device_name(name), operation(op), conditions(conds) {}
};

/**
 * @brief 获取设备操作的先决条件
 */
class OperationConditions {
public:
    // ----- 螺杆泵开启条件 -----
    static std::vector<std::string> ScrewPumpStartConditions() {
        return {
            "4路水路正常（水流开关反馈有水流）",
            "电磁阀4处于开启状态",
            "无泵体故障码（变频器无报错）",
            "供电电源正常（接触器反馈吸合）"
        };
    }
    
    static std::vector<std::string> ScrewPumpStopConditions() {
        return {
            "罗茨泵已完全关闭（0赫兹状态）",
            "分子泵1-3均已关闭（0赫兹状态）"
        };
    }
    
    // ----- 罗茨泵开启条件 -----
    static std::vector<std::string> RootsPumpStartConditions() {
        return {
            "螺杆泵已启动且运行频率≥110赫兹",
            "真空计3读数≤7000帕",
            "电磁阀4处于开启状态",
            "无泵体故障码",
            "供电正常"
        };
    }
    
    static std::vector<std::string> RootsPumpStopConditions() {
        return {
            "分子泵1-3均已满转（518赫兹稳定状态）"
        };
    }
    
    // ----- 分子泵开启条件 -----
    static std::vector<std::string> MolecularPumpStartConditions() {
        return {
            "螺杆泵已启动且运行正常",
            "对应电磁阀1-3处于开启状态",
            "对应闸板阀1-3处于开启状态",
            "真空计1/2读数≤45帕",
            "4路水路正常",
            "无泵体故障码",
            "供电正常"
        };
    }
    
    static std::vector<std::string> MolecularPumpStopConditions() {
        return {}; // 可直接关闭
    }
    
    // ----- 电磁阀操作条件 -----
    static std::vector<std::string> ElectromagneticValve123OpenConditions() {
        return {
            "放气阀1处于关闭状态"
        };
    }
    
    static std::vector<std::string> ElectromagneticValve123CloseConditions() {
        return {
            "对应分子泵已关闭（0赫兹状态）",
            "对应闸板阀1-3已关闭"
        };
    }
    
    static std::vector<std::string> ElectromagneticValve4OpenConditions() {
        return {}; // 无前置条件
    }
    
    static std::vector<std::string> ElectromagneticValve4CloseConditions() {
        return {
            "螺杆泵、罗茨泵、分子泵1-3均已完全关闭"
        };
    }
    
    // ----- 闸板阀操作条件 -----
    static std::vector<std::string> GateValve123OpenConditions() {
        return {
            "放气阀2处于关闭状态",
            "闸板阀5处于关闭状态",
            "腔室与前级管道压差<3000帕",
            "对应电磁阀1-3已开启",
            "气源气压≥0.4兆帕"
        };
    }
    
    static std::vector<std::string> GateValve123CloseConditions() {
        return {
            "对应分子泵已关闭"
        };
    }
    
    static std::vector<std::string> GateValve4OpenConditions() {
        return {
            "放气阀2处于关闭状态",
            "闸板阀5处于关闭状态",
            "腔室真空度<3000帕",
            "螺杆泵已启动且运行正常（达110赫兹）",
            "气源气压≥0.4兆帕"
        };
    }
    
    static std::vector<std::string> GateValve4CloseConditions() {
        return {}; // 可直接关闭
    }
    
    static std::vector<std::string> GateValve5OpenConditions() {
        return {
            "闸板阀两侧气压差<3000帕",
            "闸板阀1-4均处于关闭状态",
            "放气阀2处于关闭状态",
            "外部大行程系统发出允许开启信号",
            "气源气压≥0.4兆帕"
        };
    }
    
    static std::vector<std::string> GateValve5CloseConditions() {
        return {
            "外部大行程系统发出允许关闭信号"
        };
    }
    
    // ----- 放气阀操作条件 -----
    static std::vector<std::string> VentValve1OpenConditions() {
        return {
            "闸板阀1-4均处于关闭状态"
        };
    }
    
    static std::vector<std::string> VentValve1CloseConditions() {
        return {
            "前级管道已放气至大气状态（真空计3读数≥80000帕）"
        };
    }
    
    static std::vector<std::string> VentValve2OpenConditions() {
        return {
            "闸板阀1-5均处于关闭状态"
        };
    }
    
    static std::vector<std::string> VentValve2CloseConditions() {
        return {
            "腔室已放气至大气状态（真空计1/2读数≥80000帕）"
        };
    }
};

// ============================================================================
// 报警类型定义
// ============================================================================

/**
 * @brief 报警类型枚举 (共40种)
 */
enum class AlarmType {
    // 阀开到位异常 (11个)
    GATE_VALVE_1_OPEN_TIMEOUT = 1,
    GATE_VALVE_2_OPEN_TIMEOUT,
    GATE_VALVE_3_OPEN_TIMEOUT,
    GATE_VALVE_4_OPEN_TIMEOUT,
    GATE_VALVE_5_OPEN_TIMEOUT,
    ELECTROMAGNETIC_VALVE_1_OPEN_TIMEOUT,
    ELECTROMAGNETIC_VALVE_2_OPEN_TIMEOUT,
    ELECTROMAGNETIC_VALVE_3_OPEN_TIMEOUT,
    ELECTROMAGNETIC_VALVE_4_OPEN_TIMEOUT,
    VENT_VALVE_1_OPEN_TIMEOUT,
    VENT_VALVE_2_OPEN_TIMEOUT,
    
    // 阀关到位异常 (11个)
    GATE_VALVE_1_CLOSE_TIMEOUT = 20,
    GATE_VALVE_2_CLOSE_TIMEOUT,
    GATE_VALVE_3_CLOSE_TIMEOUT,
    GATE_VALVE_4_CLOSE_TIMEOUT,
    GATE_VALVE_5_CLOSE_TIMEOUT,
    ELECTROMAGNETIC_VALVE_1_CLOSE_TIMEOUT,
    ELECTROMAGNETIC_VALVE_2_CLOSE_TIMEOUT,
    ELECTROMAGNETIC_VALVE_3_CLOSE_TIMEOUT,
    ELECTROMAGNETIC_VALVE_4_CLOSE_TIMEOUT,
    VENT_VALVE_1_CLOSE_TIMEOUT,
    VENT_VALVE_2_CLOSE_TIMEOUT,
    
    // 泵故障 (5个)
    SCREW_PUMP_FAULT = 40,
    ROOTS_PUMP_FAULT,
    MOLECULAR_PUMP_1_FAULT,
    MOLECULAR_PUMP_2_FAULT,
    MOLECULAR_PUMP_3_FAULT,
    
    // 电源异常 (4个)
    POWER_SUPPLY_1_FAULT = 50,
    POWER_SUPPLY_2_FAULT,
    POWER_SUPPLY_3_FAULT,
    POWER_SUPPLY_4_FAULT,
    
    // 水路断流 (4个)
    WATER_FLOW_1_FAULT = 60,
    WATER_FLOW_2_FAULT,
    WATER_FLOW_3_FAULT,
    WATER_FLOW_4_FAULT,
    
    // 其他 (5个)
    AIR_PRESSURE_LOW = 70,          // 气源压力不足
    VACUUM_GAUGE_1_FAULT,           // 真空计1异常
    VACUUM_GAUGE_2_FAULT,           // 真空计2异常
    VACUUM_GAUGE_3_FAULT,           // 真空计3异常
    PHASE_SEQUENCE_FAULT            // 主电源相序异常
};

/**
 * @brief 获取报警描述
 */
inline std::string GetAlarmDescription(AlarmType type) {
    static const std::map<AlarmType, std::string> descriptions = {
        {AlarmType::GATE_VALVE_1_OPEN_TIMEOUT, "闸板阀1开到位超时"},
        {AlarmType::GATE_VALVE_2_OPEN_TIMEOUT, "闸板阀2开到位超时"},
        {AlarmType::GATE_VALVE_3_OPEN_TIMEOUT, "闸板阀3开到位超时"},
        {AlarmType::GATE_VALVE_4_OPEN_TIMEOUT, "闸板阀4开到位超时"},
        {AlarmType::GATE_VALVE_5_OPEN_TIMEOUT, "闸板阀5开到位超时"},
        {AlarmType::ELECTROMAGNETIC_VALVE_1_OPEN_TIMEOUT, "电磁阀1开到位超时"},
        {AlarmType::ELECTROMAGNETIC_VALVE_2_OPEN_TIMEOUT, "电磁阀2开到位超时"},
        {AlarmType::ELECTROMAGNETIC_VALVE_3_OPEN_TIMEOUT, "电磁阀3开到位超时"},
        {AlarmType::ELECTROMAGNETIC_VALVE_4_OPEN_TIMEOUT, "电磁阀4开到位超时"},
        {AlarmType::VENT_VALVE_1_OPEN_TIMEOUT, "放气阀1开到位超时"},
        {AlarmType::VENT_VALVE_2_OPEN_TIMEOUT, "放气阀2开到位超时"},
        {AlarmType::GATE_VALVE_1_CLOSE_TIMEOUT, "闸板阀1关到位超时"},
        {AlarmType::GATE_VALVE_2_CLOSE_TIMEOUT, "闸板阀2关到位超时"},
        {AlarmType::GATE_VALVE_3_CLOSE_TIMEOUT, "闸板阀3关到位超时"},
        {AlarmType::GATE_VALVE_4_CLOSE_TIMEOUT, "闸板阀4关到位超时"},
        {AlarmType::GATE_VALVE_5_CLOSE_TIMEOUT, "闸板阀5关到位超时"},
        {AlarmType::ELECTROMAGNETIC_VALVE_1_CLOSE_TIMEOUT, "电磁阀1关到位超时"},
        {AlarmType::ELECTROMAGNETIC_VALVE_2_CLOSE_TIMEOUT, "电磁阀2关到位超时"},
        {AlarmType::ELECTROMAGNETIC_VALVE_3_CLOSE_TIMEOUT, "电磁阀3关到位超时"},
        {AlarmType::ELECTROMAGNETIC_VALVE_4_CLOSE_TIMEOUT, "电磁阀4关到位超时"},
        {AlarmType::VENT_VALVE_1_CLOSE_TIMEOUT, "放气阀1关到位超时"},
        {AlarmType::VENT_VALVE_2_CLOSE_TIMEOUT, "放气阀2关到位超时"},
        {AlarmType::SCREW_PUMP_FAULT, "螺杆泵故障"},
        {AlarmType::ROOTS_PUMP_FAULT, "罗茨泵故障"},
        {AlarmType::MOLECULAR_PUMP_1_FAULT, "分子泵1故障"},
        {AlarmType::MOLECULAR_PUMP_2_FAULT, "分子泵2故障"},
        {AlarmType::MOLECULAR_PUMP_3_FAULT, "分子泵3故障"},
        {AlarmType::POWER_SUPPLY_1_FAULT, "电源1异常"},
        {AlarmType::POWER_SUPPLY_2_FAULT, "电源2异常"},
        {AlarmType::POWER_SUPPLY_3_FAULT, "电源3异常"},
        {AlarmType::POWER_SUPPLY_4_FAULT, "电源4异常"},
        {AlarmType::WATER_FLOW_1_FAULT, "水路1断流"},
        {AlarmType::WATER_FLOW_2_FAULT, "水路2断流"},
        {AlarmType::WATER_FLOW_3_FAULT, "水路3断流"},
        {AlarmType::WATER_FLOW_4_FAULT, "水路4断流"},
        {AlarmType::AIR_PRESSURE_LOW, "气源压力不足"},
        {AlarmType::VACUUM_GAUGE_1_FAULT, "真空计1读数异常"},
        {AlarmType::VACUUM_GAUGE_2_FAULT, "真空计2读数异常"},
        {AlarmType::VACUUM_GAUGE_3_FAULT, "真空计3读数异常"},
        {AlarmType::PHASE_SEQUENCE_FAULT, "主电源相序异常"}
    };
    
    auto it = descriptions.find(type);
    return (it != descriptions.end()) ? it->second : "未知报警";
}

} // namespace PLC
} // namespace VacuumSystem

#endif // VACUUM_SYSTEM_PLC_MAPPING_H

