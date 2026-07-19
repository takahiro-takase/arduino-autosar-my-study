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
#include "ComM.h"

#ifdef __cplusplus
extern "C" {
#endif

/* EngineInfo (CAN 0x100) 由来の Read ポート。E2E Transformer 経由のため
 * Rte_IStatusType を返す（RTE_E_OK / RTE_E_COM_STOPPED /
 * RTE_E_HARD_TRANSFORMER_ERROR / RTE_E_SOFT_TRANSFORMER_ERROR。詳細は
 * Rte_Type.h と Rte.c の Rte_COMCbk_EngineInfo() を参照）。*data は
 * いずれのケースでも最後の正常値が書き込まれる（本実装の意図的な簡略化。
 * 詳細は Rte.c 冒頭のコメント参照）。 */
Rte_IStatusType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data);
Rte_IStatusType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data);
Rte_IStatusType Rte_Read_EngineStatus_EngineOnFlag(EngineOnFlag_t* data);

Std_ReturnType Rte_Read_EngineStatus_EngineState(EngineState_t* data);

Std_ReturnType Rte_Write_EngineStatus_EngineState(EngineState_t state);

void Rte_ScheduleRunnables(void);

/* -----------------------------------------------------------------------
 * ランプ IOControl（Dcm SID 0x2F InputOutputControlByIdentifier 用）
 *
 * App_WarningIndicator は 500ms 周期で Rte_Call_LedRunning_SetLevel() 等を
 * 呼び続けるが、Dcm が診断制御中（オーバーライド中）の間は、その呼び出しの
 * 引数を無視して固定値を出力し続ける。ASW は Dcm の存在を一切知らない
 * （Com の ComFilterAlgorithm と同じ「BSW/RTE が実際の反映要否を決める」
 * 責務分離を、CAN 送信ではなく物理出力の調停に適用したもの）。
 * ----------------------------------------------------------------------- */
typedef enum
{
    RTE_LAMP_RUN   = 0,
    RTE_LAMP_FAULT = 1,
    RTE_LAMP_ABS   = 2,
    RTE_LAMP_COUNT
} Rte_LampIdType;

/** 診断制御を解除し、ASW (App_WarningIndicator) に制御を返す。 */
Std_ReturnType Rte_IoControl_Lamp_ReturnControlToEcu(Rte_LampIdType lamp);
/** デフォルト値 (消灯) に固定する。returnControlToEcu まで ASW の値は無視される。 */
Std_ReturnType Rte_IoControl_Lamp_ResetToDefault(Rte_LampIdType lamp);
/** 現在の物理出力値をそのまま固定する。 */
Std_ReturnType Rte_IoControl_Lamp_FreezeCurrentState(Rte_LampIdType lamp);
/** 指定値 (0/1) に固定する。 */
Std_ReturnType Rte_IoControl_Lamp_ShortTermAdjustment(Rte_LampIdType lamp, uint8 level);
/** 現在 IoHwAb へ出力されている実際のレベルを取得する（Dcm の応答構築用）。 */
Std_ReturnType Rte_IoControl_Lamp_GetCurrentLevel(Rte_LampIdType lamp, uint8* level);

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

/* Client/Server ポート — IoHwAb_Adc_GetValue_mV へ委譲 */
Std_ReturnType Rte_Call_Adc_GetValue_mV(uint16* mv);

/* Client/Server ポート — FiM_GetFunctionPermission へ委譲 */
Std_ReturnType Rte_Call_FiM_GetFunctionPermission(uint8 functionId, uint8* status);

/* ABS ECU シグナル読み取りポート (AbsInfo フレーム 0x110)。
 * EngineInfo 系と同じ理由で Rte_IStatusType を返す。 */
Rte_IStatusType Rte_Read_VehicleSensor_VehicleSpeed(VehicleSpeed_t* data);
Rte_IStatusType Rte_Read_BrakeSensor_BrakeActive(BrakeActive_t* data);
Rte_IStatusType Rte_Read_AbsSensor_AbsActive(AbsActive_t* data);

/* WarningStatus シグナル書き込みポート (Signal Group メンバー、CAN 0x210) */
Std_ReturnType Rte_Write_WarningStatus_RunLamp(uint8 level);
Std_ReturnType Rte_Write_WarningStatus_FaultLamp(uint8 level);
Std_ReturnType Rte_Write_WarningStatus_AbsLamp(uint8 level);

/* WarningStatus Signal Group の確定コミット (Com_SendSignalGroup へ委譲) */
Std_ReturnType Rte_SendSignalGroup_WarningStatus(void);

/* Client/Server ポート — ComM_RequestComMode(COMM_USER_0, mode) へ委譲。
 * App_EngineManager が「エンジン OFF が一定サイクル継続 = 通信不要」と
 * 判断したときの通信モード要求（ボランタリスリープ）に使う。 */
Std_ReturnType Rte_Call_ComM_RequestComMode(ComM_ModeType mode);
/* Client/Server ポート — ComM_GetCurrentComMode(COMM_USER_0, mode) へ委譲。
 * 通信スリープからの復帰直後を検出するために使う。 */
Std_ReturnType Rte_Call_ComM_GetCurrentComMode(ComM_ModeType* mode);

#ifdef __cplusplus
}
#endif

#endif
