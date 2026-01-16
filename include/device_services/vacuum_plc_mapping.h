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
    // 输入地址映射 (I)
    static Common::PLC::PLCAddress ScrewPumpPower() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 0, 1); }  // DB1.DBX0.0
    static Common::PLC::PLCAddress RootsPumpPower() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 1, 1); }  // DB1.DBX0.1
    static Common::PLC::PLCAddress MolecularPump1Power() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 2, 1); }  // DB1.DBX0.2
    static Common::PLC::PLCAddress MolecularPump2Power() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 3, 1); }  // DB1.DBX0.3
    static Common::PLC::PLCAddress MolecularPump3Power() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 4, 1); }  // DB1.DBX0.4
    static Common::PLC::PLCAddress PhaseSequenceProtection() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 0, 5, 1); }  // DB1.DBX0.5
    
    // 电磁阀到位信号
    static Common::PLC::PLCAddress ElectromagneticValve1Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 4, 1); }  // DB1.DBX2.4
    static Common::PLC::PLCAddress ElectromagneticValve1Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 5, 1); }  // DB1.DBX2.5
    static Common::PLC::PLCAddress ElectromagneticValve2Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 6, 1); }  // DB1.DBX2.6
    static Common::PLC::PLCAddress ElectromagneticValve2Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 2, 7, 1); }  // DB1.DBX2.7
    static Common::PLC::PLCAddress ElectromagneticValve3Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 0, 1); }  // DB1.DBX3.0
    static Common::PLC::PLCAddress ElectromagneticValve3Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 1, 1); }  // DB1.DBX3.1
    static Common::PLC::PLCAddress ElectromagneticValve4Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 2, 1); }  // DB1.DBX3.2
    static Common::PLC::PLCAddress ElectromagneticValve4Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 3, 1); }  // DB1.DBX3.3
    
    // 放气阀到位信号
    static Common::PLC::PLCAddress VentValve1Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 4, 1); }  // DB1.DBX3.4
    static Common::PLC::PLCAddress VentValve1Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 5, 1); }  // DB1.DBX3.5
    static Common::PLC::PLCAddress VentValve2Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 6, 1); }  // DB1.DBX3.6
    static Common::PLC::PLCAddress VentValve2Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 3, 7, 1); }  // DB1.DBX3.7
    
    // 闸板阀到位信号
    static Common::PLC::PLCAddress GateValve1Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 0, 1); }  // DB1.DBX4.0
    static Common::PLC::PLCAddress GateValve1Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 1, 1); }  // DB1.DBX4.1
    static Common::PLC::PLCAddress GateValve2Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 2, 1); }  // DB1.DBX4.2
    static Common::PLC::PLCAddress GateValve2Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 3, 1); }  // DB1.DBX4.3
    static Common::PLC::PLCAddress GateValve3Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 4, 1); }  // DB1.DBX4.4
    static Common::PLC::PLCAddress GateValve3Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 5, 1); }  // DB1.DBX4.5
    static Common::PLC::PLCAddress GateValve4Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 6, 1); }  // DB1.DBX4.6
    static Common::PLC::PLCAddress GateValve4Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 4, 7, 1); }  // DB1.DBX4.7
    static Common::PLC::PLCAddress GateValve5Open() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 0, 1); }  // DB1.DBX5.0
    static Common::PLC::PLCAddress GateValve5Close() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 5, 1, 1); }  // DB1.DBX5.1
    
    // 水流量计信号
    static Common::PLC::PLCAddress WaterFlowMeter1() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 79, 7, 1); }  // DB1.DBX79.7
    static Common::PLC::PLCAddress WaterFlowMeter2() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 0, 1); }  // DB1.DBX80.0
    static Common::PLC::PLCAddress WaterFlowMeter3() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 1, 1); }  // DB1.DBX80.1
    static Common::PLC::PLCAddress WaterFlowMeter4() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 2, 1); }  // DB1.DBX80.2
    static Common::PLC::PLCAddress WaterFlowMeter5() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 3, 1); }  // DB1.DBX80.3
    static Common::PLC::PLCAddress WaterFlowMeter6() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 80, 4, 1); }  // DB1.DBX80.4
    
    // 模拟量输入 (IW)
    static Common::PLC::PLCAddress ScrewPumpSpeedFeedback() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 88, -1, 1); }  // DB1.DBD88 (Real)
    
    // 真空规 (Real - M Memory)
    static Common::PLC::PLCAddress VacuumGauge1() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 140, -1, 1); }  // DB1.DBD140 (Real)
    static Common::PLC::PLCAddress VacuumGauge2() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 144, -1, 1); }  // DB1.DBD144 (Real)
    static Common::PLC::PLCAddress VacuumGauge3() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 148, -1, 1); }  // DB1.DBD148 (Real)
    
    // 压力传感器 (IW)
    static Common::PLC::PLCAddress AirPressureSensor() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 112, -1, 1); }  // DB1.DBD112 (Real)
    static Common::PLC::PLCAddress WaterPressureSensor() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 108, -1, 1); }  // DB1.DBD108 (Real)
    
    static Common::PLC::PLCAddress MolecularPump1Speed() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 182, -1, 1); }  // DB1.DBD182 (Int -> use word read)
    static Common::PLC::PLCAddress MolecularPump2Speed() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 184, -1, 1); }  // DB1.DBD184
    static Common::PLC::PLCAddress MolecularPump3Speed() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 186, -1, 1); }  // DB1.DBD186
    
    // 输出地址映射 (Q)
    static Common::PLC::PLCAddress ScrewPumpStartStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 2, 1); }  // DB1.DBX24.2
    static Common::PLC::PLCAddress ScrewPumpPowerOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 0, 1); }  // DB1.DBX24.0
    static Common::PLC::PLCAddress RootsPumpPowerOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 24, 4, 1); }  // DB1.DBX24.4
    static Common::PLC::PLCAddress MolecularPump1PowerOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 0, 1); }  // DB1.DBX25.0
    static Common::PLC::PLCAddress MolecularPump2PowerOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 4, 1); }  // DB1.DBX25.4
    static Common::PLC::PLCAddress MolecularPump3PowerOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 0, 1); }  // DB1.DBX26.0
    
    // 电磁阀输出
    static Common::PLC::PLCAddress ElectromagneticValve1Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 4, 1); }  // DB1.DBX26.4
    static Common::PLC::PLCAddress ElectromagneticValve2Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 6, 1); }  // DB1.DBX26.6
    static Common::PLC::PLCAddress ElectromagneticValve3Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 0, 1); }  // DB1.DBX27.0
    static Common::PLC::PLCAddress ElectromagneticValve4Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 2, 1); }  // DB1.DBX27.2
    
    // 放气阀输出 - 修正：与输入信号地址对应
    static Common::PLC::PLCAddress VentValve1Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 4, 1); }  // DB1.DBX27.4
    static Common::PLC::PLCAddress VentValve2Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 27, 6, 1); }  // DB1.DBX27.6
    
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
    
    // 分子泵启停
    static Common::PLC::PLCAddress MolecularPump1StartStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 2, 1); }  // DB1.DBX25.2
    static Common::PLC::PLCAddress MolecularPump2StartStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 25, 6, 1); }  // DB1.DBX25.6
    static Common::PLC::PLCAddress MolecularPump3StartStop() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 26, 2, 1); }  // DB1.DBX26.2
    
    // 水电磁阀输出
    static Common::PLC::PLCAddress WaterElectromagneticValve1Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 4, 1); }  // DB1.DBX29.4
    static Common::PLC::PLCAddress WaterElectromagneticValve2Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 5, 1); }  // DB1.DBX29.5
    static Common::PLC::PLCAddress WaterElectromagneticValve3Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 6, 1); }  // DB1.DBX29.6
    static Common::PLC::PLCAddress WaterElectromagneticValve4Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 29, 7, 1); }  // DB1.DBX29.7
    static Common::PLC::PLCAddress WaterElectromagneticValve5Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 0, 1); }  // DB1.DBX30.0
    static Common::PLC::PLCAddress WaterElectromagneticValve6Output() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 30, 1, 1); }  // DB1.DBX30.1
    static Common::PLC::PLCAddress AirMainElectromagneticValveOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 31, 0, 1); }  // DB1.DBX31.0
    static Common::PLC::PLCAddress ScrewPumpFaultReset() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 31, 2, 1); }  // DB1.DBX31.2
    
    // 模拟量输出 (QW)
    static Common::PLC::PLCAddress ScrewPumpSpeedOutput() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 100, -1, 1); }  // DB1.DBD100 (Real)
    static Common::PLC::PLCAddress MolecularPump1StartStopAddress() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 22, -1, 1); }  // DB1.DBD22 (word)
    static Common::PLC::PLCAddress MolecularPump2StartStopAddress() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 34, -1, 1); }  // DB1.DBD34
    static Common::PLC::PLCAddress MolecularPump3StartStopAddress() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 46, -1, 1); }  // DB1.DBD46
    
    // 内存地址 (MW) - 从PLC Tags-2的Static结构体
    static Common::PLC::PLCAddress AutoStartSequenceFlag() { return Common::PLC::PLCAddress(Common::PLC::PLCAddressType::DB_BLOCK, 400, -1, 1); }  // DB1.DBD400 (word)
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
