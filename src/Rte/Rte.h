/**
 * \file    Rte.h
 * \brief   ランタイム環境 公開インタフェース (AUTOSAR SWS_RTE 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
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
Std_ReturnType Rte_Read_EngineStatus_EngineState(EngineState_t* data);

Std_ReturnType Rte_Write_EngineStatus_EngineState(EngineState_t state);

Std_ReturnType Rte_TriggerTransmit(Com_IPduIdType IPduId);

void Rte_ScheduleRunnables(void);

Std_ReturnType Rte_Read_WarningIndicator_EngineState(EngineState_t* data);
void Rte_ScheduleWarningIndicator(void);

/* Client/Server ポート — IoHwAb_Led_SetLevel へ委譲 */
Std_ReturnType Rte_Call_Led_SetLevel(uint8 level);

/* Client/Server ポート — IoHwAb_LedRunning_SetLevel へ委譲 */
Std_ReturnType Rte_Call_LedRunning_SetLevel(uint8 level);

/* Client/Server ポート — IoHwAb_LedFault_SetLevel へ委譲 */
Std_ReturnType Rte_Call_LedFault_SetLevel(uint8 level);

/* Client/Server ポート — IoHwAb_Button_GetLevel へ委譲 */
Std_ReturnType Rte_Call_Button_GetLevel(uint8* level);

/* ABS ECU シグナル読み取りポート (AbsInfo フレーム 0x110) */
Std_ReturnType Rte_Read_VehicleSensor_VehicleSpeed(VehicleSpeed_t* data);
Std_ReturnType Rte_Read_BrakeSensor_BrakeActive(BrakeActive_t* data);
Std_ReturnType Rte_Read_AbsSensor_AbsActive(AbsActive_t* data);

#ifdef __cplusplus
}
#endif

#endif
