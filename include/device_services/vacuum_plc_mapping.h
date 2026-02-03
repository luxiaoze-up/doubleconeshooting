#ifndef VACUUM_PLC_MAPPING_H
#define VACUUM_PLC_MAPPING_H

#include "common/plc_communication.h"
#include <map>
#include <string>

namespace Vacuum {
namespace PLC {

// PLC地址映射配置 - 基于过滤后的PLC Tags
class VacuumPLCMapping {
public:
    // ========== 输入状态反馈 (从DB1.DBX76开始) ==========
    // 泵上电反馈
    static Common::PLC::PLCAddress ScrewPumpPower() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 76, 0, 1); }  // DB1.DBX76.0 螺杆泵上电反馈
    static Common::PLC::PLCAddress RootsPumpPower() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 76, 1, 1); }  // DB1.DBX76.1 罗茨泵上电反馈
    static Common::PLC::PLCAddress MolecularPump1Power() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 76, 2, 1); }  // DB1.DBX76.2 分子泵1上电反馈
    static Common::PLC::PLCAddress MolecularPump2Power() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 76, 3, 1); }  // DB1.DBX76.3 分子泵2上电反馈
    static Common::PLC::PLCAddress MolecularPump3Power() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 76, 4, 1); }  // DB1.DBX76.4 分子泵3上电反馈
    static Common::PLC::PLCAddress PhaseSequenceProtection() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 3, 1); }  // DB1.DBX71.3 相序异常报警
    static Common::PLC::PLCAddress SystemAlarm() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 72, 0, 1); }  // DB1.DBX72.0 系统报警
    
    // 闸板阀到位信号
    static Common::PLC::PLCAddress GateValve1Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 76, 7, 1); }  // DB1.DBX76.7 闸板阀1开到位
    static Common::PLC::PLCAddress GateValve1Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 77, 0, 1); }  // DB1.DBX77.0 闸板阀1关到位
    static Common::PLC::PLCAddress GateValve2Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 77, 1, 1); }  // DB1.DBX77.1 闸板阀2开到位
    static Common::PLC::PLCAddress GateValve2Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 77, 2, 1); }  // DB1.DBX77.2 闸板阀2关到位
    static Common::PLC::PLCAddress GateValve3Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 77, 3, 1); }  // DB1.DBX77.3 闸板阀3开到位
    static Common::PLC::PLCAddress GateValve3Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 77, 4, 1); }  // DB1.DBX77.4 闸板阀3关到位
    static Common::PLC::PLCAddress GateValve4Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 77, 5, 1); }  // DB1.DBX77.5 闸板阀4开到位
    static Common::PLC::PLCAddress GateValve4Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 77, 6, 1); }  // DB1.DBX77.6 闸板阀4关到位
    static Common::PLC::PLCAddress GateValve5Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 77, 7, 1); }  // DB1.DBX77.7 闸板阀5开到位
    static Common::PLC::PLCAddress GateValve5Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 78, 0, 1); }  // DB1.DBX78.0 闸板阀5关到位
    
    // 电磁阀到位信号
    static Common::PLC::PLCAddress ElectromagneticValve1Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 78, 3, 1); }  // DB1.DBX78.3 电磁阀1开到位
    static Common::PLC::PLCAddress ElectromagneticValve1Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 78, 4, 1); }  // DB1.DBX78.4 电磁阀1关到位
    static Common::PLC::PLCAddress ElectromagneticValve2Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 78, 5, 1); }  // DB1.DBX78.5 电磁阀2开到位
    static Common::PLC::PLCAddress ElectromagneticValve2Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 78, 6, 1); }  // DB1.DBX78.6 电磁阀2关到位
    static Common::PLC::PLCAddress ElectromagneticValve3Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 78, 7, 1); }  // DB1.DBX78.7 电磁阀3开到位
    static Common::PLC::PLCAddress ElectromagneticValve3Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 0, 1); }  // DB1.DBX79.0 电磁阀3关到位
    static Common::PLC::PLCAddress ElectromagneticValve4Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 1, 1); }  // DB1.DBX79.1 电磁阀4开到位
    static Common::PLC::PLCAddress ElectromagneticValve4Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 2, 1); }  // DB1.DBX79.2 电磁阀4关到位
    
    // 放气阀到位信号
    static Common::PLC::PLCAddress VentValve1Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 3, 1); }  // DB1.DBX79.3 放气阀1开到位
    static Common::PLC::PLCAddress VentValve1Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 4, 1); }  // DB1.DBX79.4 放气阀1关到位
    static Common::PLC::PLCAddress VentValve2Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 5, 1); }  // DB1.DBX79.5 放气阀2开到位
    static Common::PLC::PLCAddress VentValve2Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 6, 1); }  // DB1.DBX79.6 放气阀2关到位
    static Common::PLC::PLCAddress ExhaustOpenState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 78, 1, 1); }  // DB1.DBX78.1 通排风开到位
    static Common::PLC::PLCAddress ExhaustCloseState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 78, 2, 1); }  // DB1.DBX78.2 通排风关到位
    
    // 运动控制系统相关信号
    static Common::PLC::PLCAddress MotionControlSystemOnline() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 226, 0, 1); }  // DB1.DBX226.0 运动控制系统设备在线
    static Common::PLC::PLCAddress GateValve5ActionPermit() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 226, 1, 1); }  // DB1.DBX226.1 闸板阀5动作允许信号
    static Common::PLC::PLCAddress MotionControlRequestOpenGateValve5() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 226, 2, 1); }  // DB1.DBX226.2 请求开闸板阀5
    static Common::PLC::PLCAddress MotionControlRequestCloseGateValve5() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 226, 3, 1); }  // DB1.DBX226.3 请求关闸板阀5
    static Common::PLC::PLCAddress MotionControlReserved1() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 226, 4, 1); }  // DB1.DBX226.4 运动控制备用1
    static Common::PLC::PLCAddress MotionControlReserved2() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 226, 5, 1); }  // DB1.DBX226.5 运动控制备用2
    static Common::PLC::PLCAddress MotionControlReserved3() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 226, 6, 1); }  // DB1.DBX226.6 运动控制备用3
    static Common::PLC::PLCAddress MotionControlReserved4() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 226, 7, 1); }  // DB1.DBX226.7 运动控制备用4
    static Common::PLC::PLCAddress MachineHeartbeat() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 228, -1, 1); }  // DB1.DBD228 机组通信心跳
    static Common::PLC::PLCAddress SystemStatus() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 230, -1, 1); }       // DB1.DBD230 系统状态
    static Common::PLC::PLCAddress CentralMonitorHeartbeat() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 232, -1, 1); } // DB1.DBD232 集中监控心跳

    // 输出状态反馈
    static Common::PLC::PLCAddress ScrewPumpVFDEnableState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 82, 0, 1); }  // DB1.DBX82.0 螺杆泵变频器驱动使能输出状态
    static Common::PLC::PLCAddress ScrewPumpPowerState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 82, 1, 1); }       // DB1.DBX82.1 螺杆泵上电输出状态
    static Common::PLC::PLCAddress RootsPumpStartStopState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 82, 2, 1); }    // DB1.DBX82.2 罗茨泵启停开关输出状态
    static Common::PLC::PLCAddress RootsPumpPowerState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 82, 3, 1); }        // DB1.DBX82.3 罗茨泵上电输出状态
    static Common::PLC::PLCAddress MolecularPump1PowerState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 82, 4, 1); }   // DB1.DBX82.4 分子泵1上电输出状态
    static Common::PLC::PLCAddress MolecularPump2PowerState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 82, 5, 1); }   // DB1.DBX82.5 分子泵2上电输出状态
    static Common::PLC::PLCAddress MolecularPump3PowerState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 82, 6, 1); }   // DB1.DBX82.6 分子泵3上电输出状态

    static Common::PLC::PLCAddress GateValve1OpenState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 83, 0, 1); }  // DB1.DBX83.0 闸板阀1开输出状态
    static Common::PLC::PLCAddress GateValve1CloseState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 83, 1, 1); } // DB1.DBX83.1 闸板阀1关输出状态
    static Common::PLC::PLCAddress GateValve2OpenState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 83, 2, 1); }  // DB1.DBX83.2 闸板阀2开输出状态
    static Common::PLC::PLCAddress GateValve2CloseState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 83, 3, 1); } // DB1.DBX83.3 闸板阀2关输出状态
    static Common::PLC::PLCAddress GateValve3OpenState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 83, 4, 1); }  // DB1.DBX83.4 闸板阀3开输出状态
    static Common::PLC::PLCAddress GateValve3CloseState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 83, 5, 1); } // DB1.DBX83.5 闸板阀3关输出状态
    static Common::PLC::PLCAddress GateValve4OpenState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 83, 6, 1); }  // DB1.DBX83.6 闸板阀4开输出状态
    static Common::PLC::PLCAddress GateValve4CloseState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 83, 7, 1); } // DB1.DBX83.7 闸板阀4关输出状态
    static Common::PLC::PLCAddress GateValve5OpenState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 84, 0, 1); }  // DB1.DBX84.0 闸板阀5开输出状态
    static Common::PLC::PLCAddress GateValve5CloseState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 84, 1, 1); } // DB1.DBX84.1 闸板阀5关输出状态
    static Common::PLC::PLCAddress ExhaustOpenStateFeedback() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 84, 2, 1); }  // DB1.DBX84.2 通排风开输出状态
    static Common::PLC::PLCAddress ExhaustCloseStateFeedback() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 84, 3, 1); } // DB1.DBX84.3 通排风关输出状态

    static Common::PLC::PLCAddress ElectromagneticValve1State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 84, 4, 1); } // DB1.DBX84.4 电磁阀1开关输出状态
    static Common::PLC::PLCAddress ElectromagneticValve2State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 84, 5, 1); } // DB1.DBX84.5 电磁阀2开关输出状态
    static Common::PLC::PLCAddress ElectromagneticValve3State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 84, 6, 1); } // DB1.DBX84.6 电磁阀3开关输出状态
    static Common::PLC::PLCAddress ElectromagneticValve4State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 84, 7, 1); } // DB1.DBX84.7 电磁阀4开关输出状态
    static Common::PLC::PLCAddress VentValve1State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 85, 0, 1); }              // DB1.DBX85.0 放气阀1开关输出状态
    static Common::PLC::PLCAddress VentValve2State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 85, 1, 1); }              // DB1.DBX85.1 放气阀2开关输出状态

    static Common::PLC::PLCAddress WaterElectromagneticValve1State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 85, 2, 1); } // DB1.DBX85.2 水电磁阀1开关输出状态
    static Common::PLC::PLCAddress WaterElectromagneticValve2State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 85, 3, 1); } // DB1.DBX85.3 水电磁阀2开关输出状态
    static Common::PLC::PLCAddress WaterElectromagneticValve3State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 85, 4, 1); } // DB1.DBX85.4 水电磁阀3开关输出状态
    static Common::PLC::PLCAddress WaterElectromagneticValve4State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 85, 5, 1); } // DB1.DBX85.5 水电磁阀4开关输出状态
    static Common::PLC::PLCAddress WaterElectromagneticValve5State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 85, 6, 1); } // DB1.DBX85.6 水电磁阀5开关输出状态
    static Common::PLC::PLCAddress WaterElectromagneticValve6State() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 85, 7, 1); } // DB1.DBX85.7 水电磁阀6开关输出状态
    static Common::PLC::PLCAddress AirMainElectromagneticValveState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 86, 0, 1); }  // DB1.DBX86.0 气主电磁阀开关输出状态
    static Common::PLC::PLCAddress MotionControlSystemOpenState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 86, 1, 1); }      // DB1.DBX86.1 运动控制系统开输出状态
    static Common::PLC::PLCAddress MotionControlSystemCloseState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 86, 2, 1); }     // DB1.DBX86.2 运动控制系统关输出状态
    static Common::PLC::PLCAddress MolecularPump1PowerOutputState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 86, 3, 1); }    // DB1.DBX86.3 分子泵1电源输出状态
    static Common::PLC::PLCAddress MolecularPump2PowerOutputState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 86, 4, 1); }    // DB1.DBX86.4 分子泵2电源输出状态
    static Common::PLC::PLCAddress MolecularPump3PowerOutputState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 86, 5, 1); }    // DB1.DBX86.5 分子泵3电源输出状态
    
    // ========== 模拟量输入 (Real/Int) ==========
    static Common::PLC::PLCAddress WaterFlowMeter1() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 7, 1); }  // DB1.DBX79.7
    static Common::PLC::PLCAddress WaterFlowMeter2() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 0, 1); }  // DB1.DBX80.0
    static Common::PLC::PLCAddress WaterFlowMeter3() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 1, 1); }  // DB1.DBX80.1
    static Common::PLC::PLCAddress WaterFlowMeter4() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 2, 1); }  // DB1.DBX80.2
    static Common::PLC::PLCAddress WaterFlowMeter5() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 3, 1); }  // DB1.DBX80.3
    static Common::PLC::PLCAddress WaterFlowMeter6() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 4, 1); }  // DB1.DBX80.4
    static Common::PLC::PLCAddress LocalPermitVentAtmosphere() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 5, 1); }  // DB1.DBX80.5 本地允许放大气
    static Common::PLC::PLCAddress LocalPermitVacuum() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 6, 1); }  // DB1.DBX80.6 本地允许抽真空
    static Common::PLC::PLCAddress LocalPermitTargetChamber() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 7, 1); }  // DB1.DBX80.7 本地允许靶室连通
    
    // 模拟量输入 (IW)
    static Common::PLC::PLCAddress ScrewPumpSpeedFeedback() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 88, -1, 1); }  // DB1.DBD88 (Real)
    
    // 真空规 (Real - M Memory)
    static Common::PLC::PLCAddress VacuumGauge1() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 140, -1, 1); }  // DB1.DBD140 (Real)
    static Common::PLC::PLCAddress VacuumGauge2() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 144, -1, 1); }  // DB1.DBD144 (Real)
    static Common::PLC::PLCAddress VacuumGauge3() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 148, -1, 1); }  // DB1.DBD148 (Real)
    static Common::PLC::PLCAddress VacuumGauge1CommFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 152, 0, 1); }  // DB1.DBX152.0 真空规1通讯异常
    static Common::PLC::PLCAddress VacuumGauge2CommFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 152, 1, 1); }  // DB1.DBX152.1 真空规2通讯异常
    
    // 压力传感器 (IW)
    static Common::PLC::PLCAddress AirPressureSensor() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 112, -1, 1); }  // DB1.DBD112 (Real)
    static Common::PLC::PLCAddress WaterPressureSensor() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 108, -1, 1); }  // DB1.DBD108 (Real)
    static Common::PLC::PLCAddress ScrewPumpFaultFeedback() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 92, -1, 1); }  // DB1.DBD92 (Int)
    static Common::PLC::PLCAddress ScrewPumpErrorCode() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 94, -1, 1); }  // DB1.DBD94 (Real)
    
    static Common::PLC::PLCAddress MolecularPump1Speed() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 182, -1, 1); }  // DB1.DBD182 (Int -> use word read)
    static Common::PLC::PLCAddress MolecularPump2Speed() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 184, -1, 1); }  // DB1.DBD184
    static Common::PLC::PLCAddress MolecularPump3Speed() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 186, -1, 1); }  // DB1.DBD186
    static Common::PLC::PLCAddress MolecularPump1ErrorCode() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 188, -1, 1); }  // DB1.DBD188 (Real)
    static Common::PLC::PLCAddress MolecularPump2ErrorCode() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 192, -1, 1); }  // DB1.DBD192 (Real)
    static Common::PLC::PLCAddress MolecularPump3ErrorCode() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 196, -1, 1); }  // DB1.DBD196 (Real)
    static Common::PLC::PLCAddress MolecularPump1CommFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 200, 0, 1); }  // DB1.DBX200.0 分子泵1通信异常
    static Common::PLC::PLCAddress MolecularPump2CommFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 200, 1, 1); }  // DB1.DBX200.1 分子泵2通信异常
    static Common::PLC::PLCAddress MolecularPump3CommFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 200, 2, 1); }  // DB1.DBX200.2 分子泵3通信异常
    
    // 输出地址映射 (Q) - 开关成对，覆盖文档中的开/关位
    static Common::PLC::PLCAddress ScrewPumpPowerOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 0, 1); }   // DB1.DBX24.0 螺杆泵电源自动开
    static Common::PLC::PLCAddress ScrewPumpPowerOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 1, 1); }  // DB1.DBX24.1 螺杆泵电源自动关
    static Common::PLC::PLCAddress ScrewPumpStart() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 2, 1); }     // DB1.DBX24.2 螺杆泵自动开
    static Common::PLC::PLCAddress ScrewPumpStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 3, 1); }      // DB1.DBX24.3 螺杆泵自动关
    static Common::PLC::PLCAddress RootsPumpPowerOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 4, 1); }   // DB1.DBX24.4 罗茨泵电源自动开
    static Common::PLC::PLCAddress RootsPumpPowerOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 5, 1); }  // DB1.DBX24.5 罗茨泵电源自动关
    static Common::PLC::PLCAddress RootsPumpStart() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 6, 1); }     // DB1.DBX24.6 罗茨泵自动开
    static Common::PLC::PLCAddress RootsPumpStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 7, 1); }      // DB1.DBX24.7 罗茨泵自动关

    static Common::PLC::PLCAddress MolecularPump1PowerOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 0, 1); }  // DB1.DBX25.0 分子泵1电源自动开
    static Common::PLC::PLCAddress MolecularPump1PowerOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 1, 1); } // DB1.DBX25.1 分子泵1电源自动关
    static Common::PLC::PLCAddress MolecularPump1Start() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 2, 1); }     // DB1.DBX25.2 分子泵1自动开
    static Common::PLC::PLCAddress MolecularPump1Stop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 3, 1); }      // DB1.DBX25.3 分子泵1自动关

    static Common::PLC::PLCAddress MolecularPump2PowerOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 4, 1); }  // DB1.DBX25.4 分子泵2电源自动开
    static Common::PLC::PLCAddress MolecularPump2PowerOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 5, 1); } // DB1.DBX25.5 分子泵2电源自动关
    static Common::PLC::PLCAddress MolecularPump2Start() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 6, 1); }     // DB1.DBX25.6 分子泵2自动开
    static Common::PLC::PLCAddress MolecularPump2Stop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 7, 1); }      // DB1.DBX25.7 分子泵2自动关

    static Common::PLC::PLCAddress MolecularPump3PowerOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 0, 1); }  // DB1.DBX26.0 分子泵3电源自动开
    static Common::PLC::PLCAddress MolecularPump3PowerOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 1, 1); } // DB1.DBX26.1 分子泵3电源自动关
    static Common::PLC::PLCAddress MolecularPump3Start() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 2, 1); }     // DB1.DBX26.2 分子泵3自动开
    static Common::PLC::PLCAddress MolecularPump3Stop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 3, 1); }      // DB1.DBX26.3 分子泵3自动关

    // 电磁阀输出（开/关成对）
    static Common::PLC::PLCAddress ElectromagneticValve1OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 4, 1); }  // DB1.DBX26.4 电磁阀1自动开
    static Common::PLC::PLCAddress ElectromagneticValve1CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 5, 1); } // DB1.DBX26.5 电磁阀1自动关
    static Common::PLC::PLCAddress ElectromagneticValve2OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 6, 1); }  // DB1.DBX26.6 电磁阀2自动开
    static Common::PLC::PLCAddress ElectromagneticValve2CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 7, 1); } // DB1.DBX26.7 电磁阀2自动关
    static Common::PLC::PLCAddress ElectromagneticValve3OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 0, 1); }  // DB1.DBX27.0 电磁阀3自动开
    static Common::PLC::PLCAddress ElectromagneticValve3CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 1, 1); } // DB1.DBX27.1 电磁阀3自动关
    static Common::PLC::PLCAddress ElectromagneticValve4OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 2, 1); }  // DB1.DBX27.2 电磁阀4自动开
    static Common::PLC::PLCAddress ElectromagneticValve4CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 3, 1); } // DB1.DBX27.3 电磁阀4自动关
    
    // 放气阀输出 - 开/关成对
    static Common::PLC::PLCAddress VentValve1OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 4, 1); }  // DB1.DBX27.4 放气阀1自动开
    static Common::PLC::PLCAddress VentValve1CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 5, 1); } // DB1.DBX27.5 放气阀1自动关
    static Common::PLC::PLCAddress VentValve2OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 6, 1); }  // DB1.DBX27.6 放气阀2自动开
    static Common::PLC::PLCAddress VentValve2CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 7, 1); } // DB1.DBX27.7 放气阀2自动关
    
    // 闸板阀输出 - 修正：交换Open/Close地址以修复逻辑颠倒问题
    static Common::PLC::PLCAddress GateValve1OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 28, 0, 1); }  // DB1.DBX28.0
    static Common::PLC::PLCAddress GateValve1CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 28, 1, 1); }  // DB1.DBX28.1
    static Common::PLC::PLCAddress GateValve2OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 28, 2, 1); }  // DB1.DBX28.2
    static Common::PLC::PLCAddress GateValve2CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 28, 3, 1); }  // DB1.DBX28.3
    static Common::PLC::PLCAddress GateValve3OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 28, 4, 1); }  // DB1.DBX28.4
    static Common::PLC::PLCAddress GateValve3CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 28, 5, 1); }  // DB1.DBX28.5
    static Common::PLC::PLCAddress GateValve4OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 28, 6, 1); }  // DB1.DBX28.6
    static Common::PLC::PLCAddress GateValve4CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 28, 7, 1); }  // DB1.DBX28.7
    static Common::PLC::PLCAddress GateValve5OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 0, 1); }  // DB1.DBX29.0
    static Common::PLC::PLCAddress GateValve5CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 1, 1); }  // DB1.DBX29.1
    
    // 分子泵启停（使用独立开/关线圈）
    
    // 水电磁阀输出（开/关成对，按文档顺序展开）
    static Common::PLC::PLCAddress WaterElectromagneticValve1OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 4, 1); }  // DB1.DBX29.4 水电磁阀1自动开
    static Common::PLC::PLCAddress WaterElectromagneticValve1CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 5, 1); } // DB1.DBX29.5 水电磁阀1自动关
    static Common::PLC::PLCAddress WaterElectromagneticValve2OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 6, 1); }  // DB1.DBX29.6 水电磁阀2自动开
    static Common::PLC::PLCAddress WaterElectromagneticValve2CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 7, 1); } // DB1.DBX29.7 水电磁阀2自动关
    static Common::PLC::PLCAddress WaterElectromagneticValve3OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 0, 1); }  // DB1.DBX30.0 水电磁阀3自动开
    static Common::PLC::PLCAddress WaterElectromagneticValve3CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 1, 1); } // DB1.DBX30.1 水电磁阀3自动关
    static Common::PLC::PLCAddress WaterElectromagneticValve4OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 2, 1); }  // DB1.DBX30.2 水电磁阀4自动开
    static Common::PLC::PLCAddress WaterElectromagneticValve4CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 3, 1); } // DB1.DBX30.3 水电磁阀4自动关
    static Common::PLC::PLCAddress WaterElectromagneticValve5OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 4, 1); }  // DB1.DBX30.4 水电磁阀5自动开
    static Common::PLC::PLCAddress WaterElectromagneticValve5CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 5, 1); } // DB1.DBX30.5 水电磁阀5自动关
    static Common::PLC::PLCAddress WaterElectromagneticValve6OpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 6, 1); }  // DB1.DBX30.6 水电磁阀6自动开
    static Common::PLC::PLCAddress WaterElectromagneticValve6CloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 7, 1); } // DB1.DBX30.7 水电磁阀6自动关

    static Common::PLC::PLCAddress ExhaustOpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 2, 1); }  // DB1.DBX29.2 通排风自动开
    static Common::PLC::PLCAddress ExhaustCloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 3, 1); } // DB1.DBX29.3 通排风自动关

    static Common::PLC::PLCAddress AirMainElectromagneticValveOpenOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 31, 0, 1); }  // DB1.DBX31.0 气主电磁阀自动开
    static Common::PLC::PLCAddress AirMainElectromagneticValveCloseOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 31, 1, 1); } // DB1.DBX31.1 气主电磁阀自动关

    static Common::PLC::PLCAddress ScrewPumpFaultResetOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 31, 2, 1); }  // DB1.DBX31.2 螺杆泵故障自动复位开
    static Common::PLC::PLCAddress ScrewPumpFaultResetClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 31, 3, 1); } // DB1.DBX31.3 螺杆泵故障自动复位关

    // 本地手动输出 (Q)
    static Common::PLC::PLCAddress ScrewPumpPowerLocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 0, 1); }   // DB1.DBX0.0 螺杆泵电源本地手动开
    static Common::PLC::PLCAddress ScrewPumpPowerLocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 1, 1); }  // DB1.DBX0.1 螺杆泵电源本地手动关
    static Common::PLC::PLCAddress ScrewPumpLocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 2, 1); }        // DB1.DBX0.2 螺杆泵本地手动开
    static Common::PLC::PLCAddress ScrewPumpLocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 3, 1); }       // DB1.DBX0.3 螺杆泵本地手动关
    static Common::PLC::PLCAddress RootsPumpPowerLocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 4, 1); }  // DB1.DBX0.4 罗茨泵电源本地手动开
    static Common::PLC::PLCAddress RootsPumpPowerLocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 5, 1); } // DB1.DBX0.5 罗茨泵电源本地手动关
    static Common::PLC::PLCAddress RootsPumpLocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 6, 1); }        // DB1.DBX0.6 罗茨泵本地手动开
    static Common::PLC::PLCAddress RootsPumpLocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 7, 1); }       // DB1.DBX0.7 罗茨泵本地手动关

    static Common::PLC::PLCAddress MolecularPump1PowerLocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 1, 0, 1); }  // DB1.DBX1.0 分子泵1电源本地手动开
    static Common::PLC::PLCAddress MolecularPump1PowerLocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 1, 1, 1); } // DB1.DBX1.1 分子泵1电源本地手动关
    static Common::PLC::PLCAddress MolecularPump1LocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 1, 2, 1); }        // DB1.DBX1.2 分子泵1本地手动开
    static Common::PLC::PLCAddress MolecularPump1LocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 1, 3, 1); }       // DB1.DBX1.3 分子泵1本地手动关

    static Common::PLC::PLCAddress MolecularPump2PowerLocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 1, 4, 1); }  // DB1.DBX1.4 分子泵2电源本地手动开
    static Common::PLC::PLCAddress MolecularPump2PowerLocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 1, 5, 1); } // DB1.DBX1.5 分子泵2电源本地手动关
    static Common::PLC::PLCAddress MolecularPump2LocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 1, 6, 1); }        // DB1.DBX1.6 分子泵2本地手动开
    static Common::PLC::PLCAddress MolecularPump2LocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 1, 7, 1); }       // DB1.DBX1.7 分子泵2本地手动关

    static Common::PLC::PLCAddress MolecularPump3PowerLocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 0, 1); }  // DB1.DBX2.0 分子泵3电源本地手动开
    static Common::PLC::PLCAddress MolecularPump3PowerLocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 1, 1); } // DB1.DBX2.1 分子泵3电源本地手动关
    static Common::PLC::PLCAddress MolecularPump3LocalOn() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 2, 1); }        // DB1.DBX2.2 分子泵3本地手动开
    static Common::PLC::PLCAddress MolecularPump3LocalOff() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 3, 1); }       // DB1.DBX2.3 分子泵3本地手动关

    static Common::PLC::PLCAddress ElectromagneticValve1LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 4, 1); }  // DB1.DBX2.4 电磁阀1本地手动开
    static Common::PLC::PLCAddress ElectromagneticValve1LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 5, 1); } // DB1.DBX2.5 电磁阀1本地手动关
    static Common::PLC::PLCAddress ElectromagneticValve2LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 6, 1); }  // DB1.DBX2.6 电磁阀2本地手动开
    static Common::PLC::PLCAddress ElectromagneticValve2LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 7, 1); } // DB1.DBX2.7 电磁阀2本地手动关
    static Common::PLC::PLCAddress ElectromagneticValve3LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 0, 1); }  // DB1.DBX3.0 电磁阀3本地手动开
    static Common::PLC::PLCAddress ElectromagneticValve3LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 1, 1); } // DB1.DBX3.1 电磁阀3本地手动关
    static Common::PLC::PLCAddress ElectromagneticValve4LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 2, 1); }  // DB1.DBX3.2 电磁阀4本地手动开
    static Common::PLC::PLCAddress ElectromagneticValve4LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 3, 1); } // DB1.DBX3.3 电磁阀4本地手动关

    static Common::PLC::PLCAddress VentValve1LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 4, 1); }  // DB1.DBX3.4 放气阀1本地手动开
    static Common::PLC::PLCAddress VentValve1LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 5, 1); } // DB1.DBX3.5 放气阀1本地手动关
    static Common::PLC::PLCAddress VentValve2LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 6, 1); }  // DB1.DBX3.6 放气阀2本地手动开
    static Common::PLC::PLCAddress VentValve2LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 7, 1); } // DB1.DBX3.7 放气阀2本地手动关

    static Common::PLC::PLCAddress GateValve1LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 0, 1); }  // DB1.DBX4.0 闸板阀1本地手动开
    static Common::PLC::PLCAddress GateValve1LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 1, 1); } // DB1.DBX4.1 闸板阀1本地手动关
    static Common::PLC::PLCAddress GateValve2LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 2, 1); }  // DB1.DBX4.2 闸板阀2本地手动开
    static Common::PLC::PLCAddress GateValve2LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 3, 1); } // DB1.DBX4.3 闸板阀2本地手动关
    static Common::PLC::PLCAddress GateValve3LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 4, 1); }  // DB1.DBX4.4 闸板阀3本地手动开
    static Common::PLC::PLCAddress GateValve3LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 5, 1); } // DB1.DBX4.5 闸板阀3本地手动关
    static Common::PLC::PLCAddress GateValve4LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 6, 1); }  // DB1.DBX4.6 闸板阀4本地手动开
    static Common::PLC::PLCAddress GateValve4LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 7, 1); } // DB1.DBX4.7 闸板阀4本地手动关
    static Common::PLC::PLCAddress GateValve5LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 0, 1); }  // DB1.DBX5.0 闸板阀5本地手动开
    static Common::PLC::PLCAddress GateValve5LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 1, 1); } // DB1.DBX5.1 闸板阀5本地手动关

    static Common::PLC::PLCAddress ExhaustLocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 2, 1); }  // DB1.DBX5.2 通排风本地手动开
    static Common::PLC::PLCAddress ExhaustLocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 3, 1); } // DB1.DBX5.3 通排风本地手动关

    static Common::PLC::PLCAddress WaterElectromagneticValve1LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 4, 1); }  // DB1.DBX5.4 水电磁阀1本地手动开
    static Common::PLC::PLCAddress WaterElectromagneticValve1LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 5, 1); } // DB1.DBX5.5 水电磁阀1本地手动关
    static Common::PLC::PLCAddress WaterElectromagneticValve2LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 6, 1); }  // DB1.DBX5.6 水电磁阀2本地手动开
    static Common::PLC::PLCAddress WaterElectromagneticValve2LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 7, 1); } // DB1.DBX5.7 水电磁阀2本地手动关

    static Common::PLC::PLCAddress WaterElectromagneticValve3LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 6, 0, 1); }  // DB1.DBX6.0 水电磁阀3本地手动开
    static Common::PLC::PLCAddress WaterElectromagneticValve3LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 6, 1, 1); } // DB1.DBX6.1 水电磁阀3本地手动关
    static Common::PLC::PLCAddress WaterElectromagneticValve4LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 6, 2, 1); }  // DB1.DBX6.2 水电磁阀4本地手动开
    static Common::PLC::PLCAddress WaterElectromagneticValve4LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 6, 3, 1); } // DB1.DBX6.3 水电磁阀4本地手动关
    static Common::PLC::PLCAddress WaterElectromagneticValve5LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 6, 4, 1); }  // DB1.DBX6.4 水电磁阀5本地手动开
    static Common::PLC::PLCAddress WaterElectromagneticValve5LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 6, 5, 1); } // DB1.DBX6.5 水电磁阀5本地手动关
    static Common::PLC::PLCAddress WaterElectromagneticValve6LocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 6, 6, 1); }  // DB1.DBX6.6 水电磁阀6本地手动开
    static Common::PLC::PLCAddress WaterElectromagneticValve6LocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 6, 7, 1); } // DB1.DBX6.7 水电磁阀6本地手动关

    static Common::PLC::PLCAddress AirMainElectromagneticValveLocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 7, 0, 1); }  // DB1.DBX7.0 气主电磁阀本地手动开
    static Common::PLC::PLCAddress AirMainElectromagneticValveLocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 7, 1, 1); } // DB1.DBX7.1 气主电磁阀本地手动关
    static Common::PLC::PLCAddress ScrewPumpFaultResetLocalOpen() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 7, 2, 1); }       // DB1.DBX7.2 螺杆泵故障本地手动复位开
    static Common::PLC::PLCAddress ScrewPumpFaultResetLocalClose() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 7, 3, 1); }      // DB1.DBX7.3 螺杆泵故障本地手动复位关
    
    // 模拟量输出 (QW)，使用存疑，先保留
    static Common::PLC::PLCAddress ScrewPumpSpeedOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 100, -1, 1); }  // DB1.DBD100 (Real)
    
    // 内存地址 (MW) - 从PLC Tags-2的Static结构体
    static Common::PLC::PLCAddress MolecularPumpStartStopSelect() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 40, -1, 1); }  // DB1.DBD40
    static Common::PLC::PLCAddress GaugeCriterion() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 42, -1, 1); }  // DB1.DBD42
    static Common::PLC::PLCAddress MolecularPumpCriterion() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 44, -1, 1); }  // DB1.DBD44
    
    // 按钮功能 (从PLC Tags-2)
    static Common::PLC::PLCAddress LocalRemoteButton() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 36, 0, 1); }  // DB1.DBX36.0
    static Common::PLC::PLCAddress ManualAutoButton() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 36, 1, 1); }  // DB1.DBX36.1
    static Common::PLC::PLCAddress EmergencyStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 36, 2, 1); }  // DB1.DBX36.2
    static Common::PLC::PLCAddress OneKeyVacuumStart() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 36, 3, 1); }  // DB1.DBX36.3
    static Common::PLC::PLCAddress OneKeyVacuumStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 36, 4, 1); }  // DB1.DBX36.4
    static Common::PLC::PLCAddress VentStart() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 36, 5, 1); }  // DB1.DBX36.5
    static Common::PLC::PLCAddress VentStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 36, 6, 1); }  // DB1.DBX36.6
    static Common::PLC::PLCAddress AlarmReset() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 36, 7, 1); }  // DB1.DBX36.7
    
    // 状态反馈 (从PLC Tags-2)
    static Common::PLC::PLCAddress AutoState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 37, 1, 1); }  // DB1.DBX37.1
    static Common::PLC::PLCAddress ManualState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 37, 2, 1); }  // DB1.DBX37.2
    static Common::PLC::PLCAddress LocalState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 37, 3, 1); }  // DB1.DBX37.3
    static Common::PLC::PLCAddress RemoteState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 37, 4, 1); }  // DB1.DBX37.4
    static Common::PLC::PLCAddress ScrewPumpCommFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 106, 0, 1); }  // DB1.DBX106.0 螺杆泵通信异常
    static Common::PLC::PLCAddress ScrewPumpFaultResetState() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 106, 1, 1); }  // DB1.DBX106.1 螺杆泵故障复位状态
    
    // 异常标志位 (从PLC Tags-2)
    static Common::PLC::PLCAddress GateValve1Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 70, 0, 1); }  // DB1.DBX70.0
    static Common::PLC::PLCAddress GateValve2Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 70, 1, 1); }  // DB1.DBX70.1
    static Common::PLC::PLCAddress GateValve3Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 70, 2, 1); }  // DB1.DBX70.2
    static Common::PLC::PLCAddress GateValve4Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 70, 3, 1); }  // DB1.DBX70.3
    static Common::PLC::PLCAddress GateValve5Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 70, 4, 1); }  // DB1.DBX70.4
    static Common::PLC::PLCAddress ElectromagneticValve1Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 70, 5, 1); }  // DB1.DBX70.5
    static Common::PLC::PLCAddress ElectromagneticValve2Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 70, 6, 1); }  // DB1.DBX70.6
    static Common::PLC::PLCAddress ElectromagneticValve3Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 70, 7, 1); }  // DB1.DBX70.7
    static Common::PLC::PLCAddress ElectromagneticValve4Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 0, 1); }  // DB1.DBX71.0
    static Common::PLC::PLCAddress VentValve1Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 1, 1); }  // DB1.DBX71.1
    static Common::PLC::PLCAddress VentValve2Fault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 2, 1); }  // DB1.DBX71.2
    static Common::PLC::PLCAddress PhaseSequenceFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 3, 1); }  // DB1.DBX71.3
    static Common::PLC::PLCAddress ScrewPumpWaterFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 4, 1); }  // DB1.DBX71.4
    static Common::PLC::PLCAddress MolecularPump1WaterFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 5, 1); }  // DB1.DBX71.5
    static Common::PLC::PLCAddress MolecularPump2WaterFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 6, 1); }  // DB1.DBX71.6
    static Common::PLC::PLCAddress MolecularPump3WaterFault() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 71, 7, 1); }  // DB1.DBX71.7
};

} // namespace PLC
} // namespace Vacuum

#endif // VACUUM_PLC_MAPPING_H
