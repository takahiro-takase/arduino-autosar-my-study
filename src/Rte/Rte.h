#ifndef RTE_H
#define RTE_H

#include "Rte_Type.h"
#include "Std_Types.h"
#include "Com_Types.h"

// -------------------------------------------------------
// RTE Read API
// 命名規則: Rte_Read_<PortName>_<DataElementName>
//
// SW-C はポート名だけを知る。
// COM の Signal ID（COM_SIGNAL_ENGINE_SPEED など）は RTE が隠蔽する。
// -------------------------------------------------------
Std_ReturnType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data);
Std_ReturnType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data);
Std_ReturnType Rte_Read_EngineStatus_EngineOnFlag(EngineOnFlag_t* data);

// -------------------------------------------------------
// RTE Write API
// 命名規則: Rte_Write_<PortName>_<DataElementName>
// -------------------------------------------------------
Std_ReturnType Rte_Write_EngineCmd_EngineSpeed(EngineSpeed_t data);
Std_ReturnType Rte_Write_EngineCmd_CoolantTemp(CoolantTemp_t data);
Std_ReturnType Rte_Write_EngineCmd_EngineOnFlag(EngineOnFlag_t data);

// -------------------------------------------------------
// RTE Trigger Transmit
// SW-C が送信をトリガする唯一のインタフェース。
// COM の TriggerIPDUSend を呼ぶだけだが、SW-C が COM を直接知る必要をなくす。
// -------------------------------------------------------
Std_ReturnType Rte_TriggerTransmit(Com_IPduIdType IPduId);

// -------------------------------------------------------
// RTE Runnable スケジューラ
// RTOS がある環境ではタスクから呼ばれる。
// Arduino では loop() から手動で呼ぶ。
// -------------------------------------------------------
void Rte_ScheduleRunnables(void);

#endif
