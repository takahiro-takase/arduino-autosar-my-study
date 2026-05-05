#ifndef RTE_H
#define RTE_H

#include "Rte_Type.h"
#include "Std_Types.h"
#include "Com_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

Std_ReturnType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data);
Std_ReturnType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data);
Std_ReturnType Rte_Read_EngineStatus_EngineOnFlag(EngineOnFlag_t* data);

Std_ReturnType Rte_Write_EngineCmd_EngineSpeed(EngineSpeed_t data);
Std_ReturnType Rte_Write_EngineCmd_CoolantTemp(CoolantTemp_t data);
Std_ReturnType Rte_Write_EngineCmd_EngineOnFlag(EngineOnFlag_t data);

Std_ReturnType Rte_TriggerTransmit(Com_IPduIdType IPduId);

void Rte_ScheduleRunnables(void);

#ifdef __cplusplus
}
#endif

#endif
