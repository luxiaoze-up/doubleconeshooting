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
    
    // ----- 水流量计反馈 -----
    static PLCAddress WaterFlowMeter1() {
        return PLCAddress(PLCAddressType::INPUT, 79, 7);  // %I79.7 水流量计1
    }
    static PLCAddress WaterFlowMeter2() {
        return PLCAddress(PLCAddressType::INPUT, 80, 0);  // %I80.0 水流量计2
    }
    static PLCAddress WaterFlowMeter3() {
        return PLCAddress(PLCAddressType::INPUT, 80, 1);  // %I80.1 水流量计3
    }
    static PLCAddress WaterFlowMeter4() {
        return PLCAddress(PLCAddressType::INPUT, 80, 2);  // %I80.2 水流量计4
    }
    static PLCAddress WaterFlowMeter5() {
        return PLCAddress(PLCAddressType::INPUT, 80, 3);  // %I80.3 水流量计5
    }
    static PLCAddress WaterFlowMeter6() {
        return PLCAddress(PLCAddressType::INPUT, 80, 4);  // %I80.4 水流量计6
    }
    
    // ----- 本地许可信号 -----
    static PLCAddress LocalPermitVentAtmosphere() {
        return PLCAddress(PLCAddressType::INPUT, 80, 5);  // %I80.5 本地允许放大气
    }
    static PLCAddress LocalPermitVacuum() {
        return PLCAddress(PLCAddressType::INPUT, 80, 6);  // %I80.6 本地允许抽真空
    }
    static PLCAddress LocalPermitTargetChamber() {
        return PLCAddress(PLCAddressType::INPUT, 80, 7);  // %I80.7 本地允许靶室连通
    }
    
    // ----- 螺杆泵反馈 -----
    static PLCAddress ScrewPumpSpeedFeedback() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 88, -1);  // %IW88 螺杆泵转速反馈
    }
    static PLCAddress ScrewPumpFaultFeedback() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 92, -1);  // %IW92 螺杆泵故障反馈
    }
    static PLCAddress ScrewPumpErrorCode() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 94, -1);  // %IW94 螺杆泵错误码
    }
    static PLCAddress ScrewPumpCommFault() {
        return PLCAddress(PLCAddressType::INPUT, 106, 0);  // %I106.0 螺杆泵通信异常
    }
    static PLCAddress ScrewPumpFaultResetState() {
        return PLCAddress(PLCAddressType::INPUT, 106, 1);  // %I106.1 螺杆泵故障复位状态
    }
    
    // ----- 真空规反馈 -----
    static PLCAddress VacuumGauge1() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 140, -1);  // %IW140 真空规1读数
    }
    static PLCAddress VacuumGauge2() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 144, -1);  // %IW144 真空规2读数
    }
    static PLCAddress VacuumGauge3() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 148, -1);  // %IW148 真空规3读数
    }
    static PLCAddress VacuumGauge1CommFault() {
        return PLCAddress(PLCAddressType::INPUT, 152, 0);  // %I152.0 真空规1通信异常
    }
    static PLCAddress VacuumGauge2CommFault() {
        return PLCAddress(PLCAddressType::INPUT, 152, 1);  // %I152.1 真空规2通信异常
    }
    
    // ----- 分子泵错误码和通信异常 -----
    static PLCAddress MolecularPump1ErrorCode() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 188, -1);  // %IW188 分子泵1错误码
    }
    static PLCAddress MolecularPump2ErrorCode() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 192, -1);  // %IW192 分子泵2错误码
    }
    static PLCAddress MolecularPump3ErrorCode() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 196, -1);  // %IW196 分子泵3错误码
    }
    static PLCAddress MolecularPump1CommFault() {
        return PLCAddress(PLCAddressType::INPUT, 200, 0);  // %I200.0 分子泵1通信异常
    }
    static PLCAddress MolecularPump2CommFault() {
        return PLCAddress(PLCAddressType::INPUT, 200, 1);  // %I200.1 分子泵2通信异常
    }
    static PLCAddress MolecularPump3CommFault() {
        return PLCAddress(PLCAddressType::INPUT, 200, 2);  // %I200.2 分子泵3通信异常
    }
    
    // ----- 运动控制备用位 -----
    static PLCAddress MotionControlReserved1() {
        return PLCAddress(PLCAddressType::INPUT, 226, 4);  // %I226.4 运动控制备用1
    }
    static PLCAddress MotionControlReserved2() {
        return PLCAddress(PLCAddressType::INPUT, 226, 5);  // %I226.5 运动控制备用2
    }
    static PLCAddress MotionControlReserved3() {
        return PLCAddress(PLCAddressType::INPUT, 226, 6);  // %I226.6 运动控制备用3
    }
    static PLCAddress MotionControlReserved4() {
        return PLCAddress(PLCAddressType::INPUT, 226, 7);  // %I226.7 运动控制备用4
    }
    
    // ----- 心跳和状态 -----
    static PLCAddress MachineHeartbeat() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 228, -1);  // %IW228 机组通信心跳
    }
    static PLCAddress SystemStatus() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 230, -1);  // %IW230 系统状态
    }
    static PLCAddress CentralMonitorHeartbeat() {
        return PLCAddress(PLCAddressType::INPUT_WORD, 232, -1);  // %IW232 集中监控心跳
    }
    
    // ----- 系统报警 -----
    static PLCAddress SystemAlarm() {
        return PLCAddress(PLCAddressType::INPUT, 72, 0);  // %I72.0 系统报警
    }
    
    // ----- 异常标志位 -----
    static PLCAddress GateValve1Fault() {
        return PLCAddress(PLCAddressType::INPUT, 70, 0);  // %I70.0 闸板阀1异常
    }
    static PLCAddress GateValve2Fault() {
        return PLCAddress(PLCAddressType::INPUT, 70, 1);  // %I70.1 闸板阀2异常
    }
    static PLCAddress GateValve3Fault() {
        return PLCAddress(PLCAddressType::INPUT, 70, 2);  // %I70.2 闸板阀3异常
    }
    static PLCAddress GateValve4Fault() {
        return PLCAddress(PLCAddressType::INPUT, 70, 3);  // %I70.3 闸板阀4异常
    }
    static PLCAddress GateValve5Fault() {
        return PLCAddress(PLCAddressType::INPUT, 70, 4);  // %I70.4 闸板阀5异常
    }
    static PLCAddress ElectromagneticValve1Fault() {
        return PLCAddress(PLCAddressType::INPUT, 70, 5);  // %I70.5 电磁阀1异常
    }
    static PLCAddress ElectromagneticValve2Fault() {
        return PLCAddress(PLCAddressType::INPUT, 70, 6);  // %I70.6 电磁阀2异常
    }
    static PLCAddress ElectromagneticValve3Fault() {
        return PLCAddress(PLCAddressType::INPUT, 70, 7);  // %I70.7 电磁阀3异常
    }
    static PLCAddress ElectromagneticValve4Fault() {
        return PLCAddress(PLCAddressType::INPUT, 71, 0);  // %I71.0 电磁阀4异常
    }
    static PLCAddress VentValve1Fault() {
        return PLCAddress(PLCAddressType::INPUT, 71, 1);  // %I71.1 放气阀1异常
    }
    static PLCAddress VentValve2Fault() {
        return PLCAddress(PLCAddressType::INPUT, 71, 2);  // %I71.2 放气阀2异常
    }
    static PLCAddress PhaseSequenceFault() {
        return PLCAddress(PLCAddressType::INPUT, 71, 3);  // %I71.3 相序异常
    }
    static PLCAddress ScrewPumpWaterFault() {
        return PLCAddress(PLCAddressType::INPUT, 71, 4);  // %I71.4 螺杆泵水路异常
    }
    static PLCAddress MolecularPump1WaterFault() {
        return PLCAddress(PLCAddressType::INPUT, 71, 5);  // %I71.5 分子泵1水路异常
    }
    static PLCAddress MolecularPump2WaterFault() {
        return PLCAddress(PLCAddressType::INPUT, 71, 6);  // %I71.6 分子泵2水路异常
    }
    static PLCAddress MolecularPump3WaterFault() {
        return PLCAddress(PLCAddressType::INPUT, 71, 7);  // %I71.7 分子泵3水路异常
    }
    
    // ----- 按钮功能 -----
    static PLCAddress LocalRemoteButton() {
        return PLCAddress(PLCAddressType::INPUT, 36, 0);  // %I36.0 本地/远程切换按钮
    }
    static PLCAddress ManualAutoButton() {
        return PLCAddress(PLCAddressType::INPUT, 36, 1);  // %I36.1 手动/自动切换按钮
    }
    static PLCAddress EmergencyStop() {
        return PLCAddress(PLCAddressType::INPUT, 36, 2);  // %I36.2 急停按钮
    }
    static PLCAddress OneKeyVacuumStart() {
        return PLCAddress(PLCAddressType::INPUT, 36, 3);  // %I36.3 一键抽真空启动
    }
    static PLCAddress OneKeyVacuumStop() {
        return PLCAddress(PLCAddressType::INPUT, 36, 4);  // %I36.4 一键抽真空停止
    }
    static PLCAddress VentStart() {
        return PLCAddress(PLCAddressType::INPUT, 36, 5);  // %I36.5 放气启动
    }
    static PLCAddress VentStop() {
        return PLCAddress(PLCAddressType::INPUT, 36, 6);  // %I36.6 放气停止
    }
    static PLCAddress AlarmReset() {
        return PLCAddress(PLCAddressType::INPUT, 36, 7);  // %I36.7 报警复位
    }
    
    // ----- 状态反馈 -----
    static PLCAddress AutoState() {
        return PLCAddress(PLCAddressType::INPUT, 37, 1);  // %I37.1 自动模式状态
    }
    static PLCAddress ManualState() {
        return PLCAddress(PLCAddressType::INPUT, 37, 2);  // %I37.2 手动模式状态
    }
    static PLCAddress LocalState() {
        return PLCAddress(PLCAddressType::INPUT, 37, 3);  // %I37.3 本地模式状态
    }
    static PLCAddress RemoteState() {
        return PLCAddress(PLCAddressType::INPUT, 37, 4);  // %I37.4 远程模式状态
    }
    
    // ----- 输出状态反馈 -----
    static PLCAddress ScrewPumpVFDEnableState() {
        return PLCAddress(PLCAddressType::INPUT, 82, 0);  // %I82.0 螺杆泵变频器驱动使能输出状态
    }
    static PLCAddress ScrewPumpPowerState() {
        return PLCAddress(PLCAddressType::INPUT, 82, 1);  // %I82.1 螺杆泵上电输出状态
    }
    static PLCAddress RootsPumpStartStopState() {
        return PLCAddress(PLCAddressType::INPUT, 82, 2);  // %I82.2 罗茨泵启停开关输出状态
    }
    static PLCAddress RootsPumpPowerState() {
        return PLCAddress(PLCAddressType::INPUT, 82, 3);  // %I82.3 罗茨泵上电输出状态
    }
    static PLCAddress MolecularPump1PowerState() {
        return PLCAddress(PLCAddressType::INPUT, 82, 4);  // %I82.4 分子泵1上电输出状态
    }
    static PLCAddress MolecularPump2PowerState() {
        return PLCAddress(PLCAddressType::INPUT, 82, 5);  // %I82.5 分子泵2上电输出状态
    }
    static PLCAddress MolecularPump3PowerState() {
        return PLCAddress(PLCAddressType::INPUT, 82, 6);  // %I82.6 分子泵3上电输出状态
    }
    
    static PLCAddress GateValve1OpenState() {
        return PLCAddress(PLCAddressType::INPUT, 83, 0);  // %I83.0 闸板阀1开输出状态
    }
    static PLCAddress GateValve1CloseState() {
        return PLCAddress(PLCAddressType::INPUT, 83, 1);  // %I83.1 闸板阀1关输出状态
    }
    static PLCAddress GateValve2OpenState() {
        return PLCAddress(PLCAddressType::INPUT, 83, 2);  // %I83.2 闸板阀2开输出状态
    }
    static PLCAddress GateValve2CloseState() {
        return PLCAddress(PLCAddressType::INPUT, 83, 3);  // %I83.3 闸板阀2关输出状态
    }
    static PLCAddress GateValve3OpenState() {
        return PLCAddress(PLCAddressType::INPUT, 83, 4);  // %I83.4 闸板阀3开输出状态
    }
    static PLCAddress GateValve3CloseState() {
        return PLCAddress(PLCAddressType::INPUT, 83, 5);  // %I83.5 闸板阀3关输出状态
    }
    static PLCAddress GateValve4OpenState() {
        return PLCAddress(PLCAddressType::INPUT, 83, 6);  // %I83.6 闸板阀4开输出状态
    }
    static PLCAddress GateValve4CloseState() {
        return PLCAddress(PLCAddressType::INPUT, 83, 7);  // %I83.7 闸板阀4关输出状态
    }
    static PLCAddress GateValve5OpenState() {
        return PLCAddress(PLCAddressType::INPUT, 84, 0);  // %I84.0 闸板阀5开输出状态
    }
    static PLCAddress GateValve5CloseState() {
        return PLCAddress(PLCAddressType::INPUT, 84, 1);  // %I84.1 闸板阀5关输出状态
    }
    static PLCAddress ExhaustOpenState() {
        return PLCAddress(PLCAddressType::INPUT, 78, 1);  // %I78.1 通排风开到位
    }
    static PLCAddress ExhaustCloseState() {
        return PLCAddress(PLCAddressType::INPUT, 78, 2);  // %I78.2 通排风关到位
    }
    static PLCAddress ExhaustOpenStateFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 84, 2);  // %I84.2 通排风开输出状态
    }
    static PLCAddress ExhaustCloseStateFeedback() {
        return PLCAddress(PLCAddressType::INPUT, 84, 3);  // %I84.3 通排风关输出状态
    }
    
    static PLCAddress ElectromagneticValve1State() {
        return PLCAddress(PLCAddressType::INPUT, 84, 4);  // %I84.4 电磁阀1开关输出状态
    }
    static PLCAddress ElectromagneticValve2State() {
        return PLCAddress(PLCAddressType::INPUT, 84, 5);  // %I84.5 电磁阀2开关输出状态
    }
    static PLCAddress ElectromagneticValve3State() {
        return PLCAddress(PLCAddressType::INPUT, 84, 6);  // %I84.6 电磁阀3开关输出状态
    }
    static PLCAddress ElectromagneticValve4State() {
        return PLCAddress(PLCAddressType::INPUT, 84, 7);  // %I84.7 电磁阀4开关输出状态
    }
    static PLCAddress VentValve1State() {
        return PLCAddress(PLCAddressType::INPUT, 85, 0);  // %I85.0 放气阀1开关输出状态
    }
    static PLCAddress VentValve2State() {
        return PLCAddress(PLCAddressType::INPUT, 85, 1);  // %I85.1 放气阀2开关输出状态
    }
    
    static PLCAddress WaterElectromagneticValve1State() {
        return PLCAddress(PLCAddressType::INPUT, 85, 2);  // %I85.2 水电磁阀1开关输出状态
    }
    static PLCAddress WaterElectromagneticValve2State() {
        return PLCAddress(PLCAddressType::INPUT, 85, 3);  // %I85.3 水电磁阀2开关输出状态
    }
    static PLCAddress WaterElectromagneticValve3State() {
        return PLCAddress(PLCAddressType::INPUT, 85, 4);  // %I85.4 水电磁阀3开关输出状态
    }
    static PLCAddress WaterElectromagneticValve4State() {
        return PLCAddress(PLCAddressType::INPUT, 85, 5);  // %I85.5 水电磁阀4开关输出状态
    }
    static PLCAddress WaterElectromagneticValve5State() {
        return PLCAddress(PLCAddressType::INPUT, 85, 6);  // %I85.6 水电磁阀5开关输出状态
    }
    static PLCAddress WaterElectromagneticValve6State() {
        return PLCAddress(PLCAddressType::INPUT, 85, 7);  // %I85.7 水电磁阀6开关输出状态
    }
    static PLCAddress AirMainElectromagneticValveState() {
        return PLCAddress(PLCAddressType::INPUT, 86, 0);  // %I86.0 气主电磁阀开关输出状态
    }
    static PLCAddress MotionControlSystemOpenState() {
        return PLCAddress(PLCAddressType::INPUT, 86, 1);  // %I86.1 运动控制系统开输出状态
    }
    static PLCAddress MotionControlSystemCloseState() {
        return PLCAddress(PLCAddressType::INPUT, 86, 2);  // %I86.2 运动控制系统关输出状态
    }
    static PLCAddress MolecularPump1PowerOutputState() {
        return PLCAddress(PLCAddressType::INPUT, 86, 3);  // %I86.3 分子泵1电源输出状态
    }
    static PLCAddress MolecularPump2PowerOutputState() {
        return PLCAddress(PLCAddressType::INPUT, 86, 4);  // %I86.4 分子泵2电源输出状态
    }
    static PLCAddress MolecularPump3PowerOutputState() {
        return PLCAddress(PLCAddressType::INPUT, 86, 5);  // %I86.5 分子泵3电源输出状态
    }
    
    // ========================================================================
    // 输出信号 (Q) - Bool 类型
    // ========================================================================
    
    // ========== 本地手动输出 (DB1.DBX0-7) ==========
    
    // ----- 螺杆泵本地手动控制 -----
    static PLCAddress ScrewPumpPowerLocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 0);  // %Q0.0 螺杆泵电源本地手动开
    }
    static PLCAddress ScrewPumpPowerLocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 1);  // %Q0.1 螺杆泵电源本地手动关
    }
    static PLCAddress ScrewPumpLocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 2);  // %Q0.2 螺杆泵本地手动开
    }
    static PLCAddress ScrewPumpLocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 3);  // %Q0.3 螺杆泵本地手动关
    }
    static PLCAddress RootsPumpPowerLocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 4);  // %Q0.4 罗茨泵电源本地手动开
    }
    static PLCAddress RootsPumpPowerLocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 5);  // %Q0.5 罗茨泵电源本地手动关
    }
    static PLCAddress RootsPumpLocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 6);  // %Q0.6 罗茨泵本地手动开
    }
    static PLCAddress RootsPumpLocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 0, 7);  // %Q0.7 罗茨泵本地手动关
    }
    
    // ----- 分子泵1本地手动控制 -----
    static PLCAddress MolecularPump1PowerLocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 0);  // %Q1.0 分子泵1电源本地手动开
    }
    static PLCAddress MolecularPump1PowerLocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 1);  // %Q1.1 分子泵1电源本地手动关
    }
    static PLCAddress MolecularPump1LocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 2);  // %Q1.2 分子泵1本地手动开
    }
    static PLCAddress MolecularPump1LocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 3);  // %Q1.3 分子泵1本地手动关
    }
    
    // ----- 分子泵2本地手动控制 -----
    static PLCAddress MolecularPump2PowerLocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 4);  // %Q1.4 分子泵2电源本地手动开
    }
    static PLCAddress MolecularPump2PowerLocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 5);  // %Q1.5 分子泵2电源本地手动关
    }
    static PLCAddress MolecularPump2LocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 6);  // %Q1.6 分子泵2本地手动开
    }
    static PLCAddress MolecularPump2LocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 1, 7);  // %Q1.7 分子泵2本地手动关
    }
    
    // ----- 分子泵3本地手动控制 -----
    static PLCAddress MolecularPump3PowerLocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 2, 0);  // %Q2.0 分子泵3电源本地手动开
    }
    static PLCAddress MolecularPump3PowerLocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 2, 1);  // %Q2.1 分子泵3电源本地手动关
    }
    static PLCAddress MolecularPump3LocalOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 2, 2);  // %Q2.2 分子泵3本地手动开
    }
    static PLCAddress MolecularPump3LocalOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 2, 3);  // %Q2.3 分子泵3本地手动关
    }
    
    // ----- 电磁阀本地手动控制 -----
    static PLCAddress ElectromagneticValve1LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 2, 4);  // %Q2.4 电磁阀1本地手动开
    }
    static PLCAddress ElectromagneticValve1LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 2, 5);  // %Q2.5 电磁阀1本地手动关
    }
    static PLCAddress ElectromagneticValve2LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 2, 6);  // %Q2.6 电磁阀2本地手动开
    }
    static PLCAddress ElectromagneticValve2LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 2, 7);  // %Q2.7 电磁阀2本地手动关
    }
    static PLCAddress ElectromagneticValve3LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 3, 0);  // %Q3.0 电磁阀3本地手动开
    }
    static PLCAddress ElectromagneticValve3LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 3, 1);  // %Q3.1 电磁阀3本地手动关
    }
    static PLCAddress ElectromagneticValve4LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 3, 2);  // %Q3.2 电磁阀4本地手动开
    }
    static PLCAddress ElectromagneticValve4LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 3, 3);  // %Q3.3 电磁阀4本地手动关
    }
    
    // ----- 放气阀本地手动控制 -----
    static PLCAddress VentValve1LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 3, 4);  // %Q3.4 放气阀1本地手动开
    }
    static PLCAddress VentValve1LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 3, 5);  // %Q3.5 放气阀1本地手动关
    }
    static PLCAddress VentValve2LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 3, 6);  // %Q3.6 放气阀2本地手动开
    }
    static PLCAddress VentValve2LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 3, 7);  // %Q3.7 放气阀2本地手动关
    }
    
    // ----- 闸板阀本地手动控制 -----
    static PLCAddress GateValve1LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 4, 0);  // %Q4.0 闸板阀1本地手动开
    }
    static PLCAddress GateValve1LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 4, 1);  // %Q4.1 闸板阀1本地手动关
    }
    static PLCAddress GateValve2LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 4, 2);  // %Q4.2 闸板阀2本地手动开
    }
    static PLCAddress GateValve2LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 4, 3);  // %Q4.3 闸板阀2本地手动关
    }
    static PLCAddress GateValve3LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 4, 4);  // %Q4.4 闸板阀3本地手动开
    }
    static PLCAddress GateValve3LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 4, 5);  // %Q4.5 闸板阀3本地手动关
    }
    static PLCAddress GateValve4LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 4, 6);  // %Q4.6 闸板阀4本地手动开
    }
    static PLCAddress GateValve4LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 4, 7);  // %Q4.7 闸板阀4本地手动关
    }
    static PLCAddress GateValve5LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 5, 0);  // %Q5.0 闸板阀5本地手动开
    }
    static PLCAddress GateValve5LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 5, 1);  // %Q5.1 闸板阀5本地手动关
    }
    
    // ----- 通排风本地手动控制 -----
    static PLCAddress ExhaustLocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 5, 2);  // %Q5.2 通排风本地手动开
    }
    static PLCAddress ExhaustLocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 5, 3);  // %Q5.3 通排风本地手动关
    }
    
    // ----- 水电磁阀本地手动控制 -----
    static PLCAddress WaterElectromagneticValve1LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 5, 4);  // %Q5.4 水电磁阀1本地手动开
    }
    static PLCAddress WaterElectromagneticValve1LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 5, 5);  // %Q5.5 水电磁阀1本地手动关
    }
    static PLCAddress WaterElectromagneticValve2LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 5, 6);  // %Q5.6 水电磁阀2本地手动开
    }
    static PLCAddress WaterElectromagneticValve2LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 5, 7);  // %Q5.7 水电磁阀2本地手动关
    }
    static PLCAddress WaterElectromagneticValve3LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 6, 0);  // %Q6.0 水电磁阀3本地手动开
    }
    static PLCAddress WaterElectromagneticValve3LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 6, 1);  // %Q6.1 水电磁阀3本地手动关
    }
    static PLCAddress WaterElectromagneticValve4LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 6, 2);  // %Q6.2 水电磁阀4本地手动开
    }
    static PLCAddress WaterElectromagneticValve4LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 6, 3);  // %Q6.3 水电磁阀4本地手动关
    }
    static PLCAddress WaterElectromagneticValve5LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 6, 4);  // %Q6.4 水电磁阀5本地手动开
    }
    static PLCAddress WaterElectromagneticValve5LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 6, 5);  // %Q6.5 水电磁阀5本地手动关
    }
    static PLCAddress WaterElectromagneticValve6LocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 6, 6);  // %Q6.6 水电磁阀6本地手动开
    }
    static PLCAddress WaterElectromagneticValve6LocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 6, 7);  // %Q6.7 水电磁阀6本地手动关
    }
    
    // ----- 气主电磁阀和螺杆泵故障复位本地手动控制 -----
    static PLCAddress AirMainElectromagneticValveLocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 7, 0);  // %Q7.0 气主电磁阀本地手动开
    }
    static PLCAddress AirMainElectromagneticValveLocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 7, 1);  // %Q7.1 气主电磁阀本地手动关
    }
    static PLCAddress ScrewPumpFaultResetLocalOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 7, 2);  // %Q7.2 螺杆泵故障本地手动复位开
    }
    static PLCAddress ScrewPumpFaultResetLocalClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 7, 3);  // %Q7.3 螺杆泵故障本地手动复位关
    }
    
    // ========== 自动输出 (DB1.DBX24-31) ==========
    
    // ----- 螺杆泵自动控制 -----
    static PLCAddress ScrewPumpPowerOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 24, 0);  // %Q24.0 螺杆泵电源自动开
    }
    static PLCAddress ScrewPumpPowerOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 24, 1);  // %Q24.1 螺杆泵电源自动关
    }
    static PLCAddress ScrewPumpStart() {
        return PLCAddress(PLCAddressType::OUTPUT, 24, 2);  // %Q24.2 螺杆泵自动开
    }
    static PLCAddress ScrewPumpStop() {
        return PLCAddress(PLCAddressType::OUTPUT, 24, 3);  // %Q24.3 螺杆泵自动关
    }
    
    // ----- 罗茨泵自动控制 -----
    static PLCAddress RootsPumpPowerOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 24, 4);  // %Q24.4 罗茨泵电源自动开
    }
    static PLCAddress RootsPumpPowerOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 24, 5);  // %Q24.5 罗茨泵电源自动关
    }
    static PLCAddress RootsPumpStart() {
        return PLCAddress(PLCAddressType::OUTPUT, 24, 6);  // %Q24.6 罗茨泵自动开
    }
    static PLCAddress RootsPumpStop() {
        return PLCAddress(PLCAddressType::OUTPUT, 24, 7);  // %Q24.7 罗茨泵自动关
    }
    
    // ----- 分子泵1自动控制 -----
    static PLCAddress MolecularPump1PowerOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 25, 0);  // %Q25.0 分子泵1电源自动开
    }
    static PLCAddress MolecularPump1PowerOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 25, 1);  // %Q25.1 分子泵1电源自动关
    }
    static PLCAddress MolecularPump1Start() {
        return PLCAddress(PLCAddressType::OUTPUT, 25, 2);  // %Q25.2 分子泵1自动开
    }
    static PLCAddress MolecularPump1Stop() {
        return PLCAddress(PLCAddressType::OUTPUT, 25, 3);  // %Q25.3 分子泵1自动关
    }
    
    // ----- 分子泵2自动控制 -----
    static PLCAddress MolecularPump2PowerOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 25, 4);  // %Q25.4 分子泵2电源自动开
    }
    static PLCAddress MolecularPump2PowerOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 25, 5);  // %Q25.5 分子泵2电源自动关
    }
    static PLCAddress MolecularPump2Start() {
        return PLCAddress(PLCAddressType::OUTPUT, 25, 6);  // %Q25.6 分子泵2自动开
    }
    static PLCAddress MolecularPump2Stop() {
        return PLCAddress(PLCAddressType::OUTPUT, 25, 7);  // %Q25.7 分子泵2自动关
    }
    
    // ----- 分子泵3自动控制 -----
    static PLCAddress MolecularPump3PowerOn() {
        return PLCAddress(PLCAddressType::OUTPUT, 26, 0);  // %Q26.0 分子泵3电源自动开
    }
    static PLCAddress MolecularPump3PowerOff() {
        return PLCAddress(PLCAddressType::OUTPUT, 26, 1);  // %Q26.1 分子泵3电源自动关
    }
    static PLCAddress MolecularPump3Start() {
        return PLCAddress(PLCAddressType::OUTPUT, 26, 2);  // %Q26.2 分子泵3自动开
    }
    static PLCAddress MolecularPump3Stop() {
        return PLCAddress(PLCAddressType::OUTPUT, 26, 3);  // %Q26.3 分子泵3自动关
    }
    
    // ----- 电磁阀自动控制 -----
    static PLCAddress ElectromagneticValve1OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 26, 4);  // %Q26.4 电磁阀1自动开
    }
    static PLCAddress ElectromagneticValve1CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 26, 5);  // %Q26.5 电磁阀1自动关
    }
    static PLCAddress ElectromagneticValve2OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 26, 6);  // %Q26.6 电磁阀2自动开
    }
    static PLCAddress ElectromagneticValve2CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 26, 7);  // %Q26.7 电磁阀2自动关
    }
    static PLCAddress ElectromagneticValve3OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 27, 0);  // %Q27.0 电磁阀3自动开
    }
    static PLCAddress ElectromagneticValve3CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 27, 1);  // %Q27.1 电磁阀3自动关
    }
    static PLCAddress ElectromagneticValve4OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 27, 2);  // %Q27.2 电磁阀4自动开
    }
    static PLCAddress ElectromagneticValve4CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 27, 3);  // %Q27.3 电磁阀4自动关
    }
    
    // ----- 放气阀自动控制 -----
    static PLCAddress VentValve1OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 27, 4);  // %Q27.4 放气阀1自动开
    }
    static PLCAddress VentValve1CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 27, 5);  // %Q27.5 放气阀1自动关
    }
    static PLCAddress VentValve2OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 27, 6);  // %Q27.6 放气阀2自动开
    }
    static PLCAddress VentValve2CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 27, 7);  // %Q27.7 放气阀2自动关
    }
    
    // ----- 闸板阀自动控制 -----
    static PLCAddress GateValve1OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 28, 0);  // %Q28.0 闸板阀1自动开
    }
    static PLCAddress GateValve1CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 28, 1);  // %Q28.1 闸板阀1自动关
    }
    static PLCAddress GateValve2OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 28, 2);  // %Q28.2 闸板阀2自动开
    }
    static PLCAddress GateValve2CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 28, 3);  // %Q28.3 闸板阀2自动关
    }
    static PLCAddress GateValve3OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 28, 4);  // %Q28.4 闸板阀3自动开
    }
    static PLCAddress GateValve3CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 28, 5);  // %Q28.5 闸板阀3自动关
    }
    static PLCAddress GateValve4OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 28, 6);  // %Q28.6 闸板阀4自动开
    }
    static PLCAddress GateValve4CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 28, 7);  // %Q28.7 闸板阀4自动关
    }
    static PLCAddress GateValve5OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 29, 0);  // %Q29.0 闸板阀5自动开
    }
    static PLCAddress GateValve5CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 29, 1);  // %Q29.1 闸板阀5自动关
    }
    
    // ----- 通排风自动控制 -----
    static PLCAddress ExhaustOpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 29, 2);  // %Q29.2 通排风自动开
    }
    static PLCAddress ExhaustCloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 29, 3);  // %Q29.3 通排风自动关
    }
    
    // ----- 水电磁阀自动控制 -----
    static PLCAddress WaterElectromagneticValve1OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 29, 4);  // %Q29.4 水电磁阀1自动开
    }
    static PLCAddress WaterElectromagneticValve1CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 29, 5);  // %Q29.5 水电磁阀1自动关
    }
    static PLCAddress WaterElectromagneticValve2OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 29, 6);  // %Q29.6 水电磁阀2自动开
    }
    static PLCAddress WaterElectromagneticValve2CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 29, 7);  // %Q29.7 水电磁阀2自动关
    }
    static PLCAddress WaterElectromagneticValve3OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 30, 0);  // %Q30.0 水电磁阀3自动开
    }
    static PLCAddress WaterElectromagneticValve3CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 30, 1);  // %Q30.1 水电磁阀3自动关
    }
    static PLCAddress WaterElectromagneticValve4OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 30, 2);  // %Q30.2 水电磁阀4自动开
    }
    static PLCAddress WaterElectromagneticValve4CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 30, 3);  // %Q30.3 水电磁阀4自动关
    }
    static PLCAddress WaterElectromagneticValve5OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 30, 4);  // %Q30.4 水电磁阀5自动开
    }
    static PLCAddress WaterElectromagneticValve5CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 30, 5);  // %Q30.5 水电磁阀5自动关
    }
    static PLCAddress WaterElectromagneticValve6OpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 30, 6);  // %Q30.6 水电磁阀6自动开
    }
    static PLCAddress WaterElectromagneticValve6CloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 30, 7);  // %Q30.7 水电磁阀6自动关
    }
    
    // ----- 气主电磁阀自动控制 -----
    static PLCAddress AirMainElectromagneticValveOpenOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 31, 0);  // %Q31.0 气主电磁阀自动开
    }
    static PLCAddress AirMainElectromagneticValveCloseOutput() {
        return PLCAddress(PLCAddressType::OUTPUT, 31, 1);  // %Q31.1 气主电磁阀自动关
    }
    
    // ----- 螺杆泵故障复位自动控制 -----
    static PLCAddress ScrewPumpFaultResetOpen() {
        return PLCAddress(PLCAddressType::OUTPUT, 31, 2);  // %Q31.2 螺杆泵故障自动复位开
    }
    static PLCAddress ScrewPumpFaultResetClose() {
        return PLCAddress(PLCAddressType::OUTPUT, 31, 3);  // %Q31.3 螺杆泵故障自动复位关
    }
    
    // ========== 内存地址 (MW - DB1.DBD40-44) ==========
    
    static PLCAddress MolecularPumpStartStopSelect() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 40, -1);  // %QW40 分子泵启停选择
    }
    static PLCAddress GaugeCriterion() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 42, -1);  // %QW42 真空规判据
    }
    static PLCAddress MolecularPumpCriterion() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 44, -1);  // %QW44 分子泵判据
    }
    
    // ========================================================================
    // 模拟量输出 (QW) - Word 类型 (Int)
    // ========================================================================
    
    // ----- 分子泵地址传送 -----
    static PLCAddress MolecularPump1AddressTransfer() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 22, -1);  // %QW22 分子泵1启停地址传送
    }
    static PLCAddress MolecularPump2AddressTransfer() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 34, -1);  // %QW34 分子泵2启停地址传送
    }
    static PLCAddress MolecularPump3AddressTransfer() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 46, -1);  // %QW46 分子泵3启停地址传送
    }
    
    // ----- 螺杆泵转速输出 -----
    static PLCAddress ScrewPumpSpeedOutput() {
        return PLCAddress(PLCAddressType::OUTPUT_WORD, 100, -1);  // %QW100 螺杆泵转速输出
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
            SystemAlarm(),
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
            MotionControlRequestCloseGateValve5(),
            MotionControlReserved1(),
            MotionControlReserved2(),
            MotionControlReserved3(),
            MotionControlReserved4(),
            // 水流量计
            WaterFlowMeter1(),
            WaterFlowMeter2(),
            WaterFlowMeter3(),
            WaterFlowMeter4(),
            WaterFlowMeter5(),
            WaterFlowMeter6(),
            // 本地许可信号
            LocalPermitVentAtmosphere(),
            LocalPermitVacuum(),
            LocalPermitTargetChamber(),
            // 通信异常
            ScrewPumpCommFault(),
            VacuumGauge1CommFault(),
            VacuumGauge2CommFault(),
            MolecularPump1CommFault(),
            MolecularPump2CommFault(),
            MolecularPump3CommFault(),
            // 异常标志
            GateValve1Fault(),
            GateValve2Fault(),
            GateValve3Fault(),
            GateValve4Fault(),
            GateValve5Fault(),
            ElectromagneticValve1Fault(),
            ElectromagneticValve2Fault(),
            ElectromagneticValve3Fault(),
            ElectromagneticValve4Fault(),
            VentValve1Fault(),
            VentValve2Fault(),
            PhaseSequenceFault(),
            ScrewPumpWaterFault(),
            MolecularPump1WaterFault(),
            MolecularPump2WaterFault(),
            MolecularPump3WaterFault(),
            // 按钮
            LocalRemoteButton(),
            ManualAutoButton(),
            EmergencyStop(),
            OneKeyVacuumStart(),
            OneKeyVacuumStop(),
            VentStart(),
            VentStop(),
            AlarmReset(),
            // 状态反馈
            AutoState(),
            ManualState(),
            LocalState(),
            RemoteState(),
            ScrewPumpFaultResetState(),
            // 输出状态反馈
            ScrewPumpVFDEnableState(),
            ScrewPumpPowerState(),
            RootsPumpStartStopState(),
            RootsPumpPowerState(),
            MolecularPump1PowerState(),
            MolecularPump2PowerState(),
            MolecularPump3PowerState(),
            GateValve1OpenState(),
            GateValve1CloseState(),
            GateValve2OpenState(),
            GateValve2CloseState(),
            GateValve3OpenState(),
            GateValve3CloseState(),
            GateValve4OpenState(),
            GateValve4CloseState(),
            GateValve5OpenState(),
            GateValve5CloseState(),
            ExhaustOpenState(),
            ExhaustCloseState(),
            ExhaustOpenStateFeedback(),
            ExhaustCloseStateFeedback(),
            ElectromagneticValve1State(),
            ElectromagneticValve2State(),
            ElectromagneticValve3State(),
            ElectromagneticValve4State(),
            VentValve1State(),
            VentValve2State(),
            WaterElectromagneticValve1State(),
            WaterElectromagneticValve2State(),
            WaterElectromagneticValve3State(),
            WaterElectromagneticValve4State(),
            WaterElectromagneticValve5State(),
            WaterElectromagneticValve6State(),
            AirMainElectromagneticValveState(),
            MotionControlSystemOpenState(),
            MotionControlSystemCloseState(),
            MolecularPump1PowerOutputState(),
            MolecularPump2PowerOutputState(),
            MolecularPump3PowerOutputState()
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
            MolecularPump3Speed(),
            ScrewPumpSpeedFeedback(),
            ScrewPumpFaultFeedback(),
            ScrewPumpErrorCode(),
            VacuumGauge1(),
            VacuumGauge2(),
            VacuumGauge3(),
            MolecularPump1ErrorCode(),
            MolecularPump2ErrorCode(),
            MolecularPump3ErrorCode(),
            MachineHeartbeat(),
            SystemStatus(),
            CentralMonitorHeartbeat()
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

