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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Rte_Type.h"
#include "Std_Types.h"
#include "Com_Types.h"
#include "ComM.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ----------------------------------------------------------------------
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

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ======================================================================
 * AUTOSAR_SWS_RTE 5.6  RTE API Reference（ポート API）
 *   名前は <p>（ポート名）と <o>（データ要素/オペレーション名）で決まる。
 *   本プロジェクトで使うポート・要素の関数だけを宣言する。
 *   定義側 (Rte.c) も同じ並び。
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Rte_Write_<p>_<o>  (RTE 5.6.4, SWS_Rte_01071)
 * ---------------------------------------------------------------------- */
Std_ReturnType Rte_Write_EngineStatus_EngineState(EngineState_t state);

/* WarningStatus シグナル書き込みポート (Signal Group メンバー、CAN 0x210) */
Std_ReturnType Rte_Write_WarningStatus_RunLamp(uint8 level);
Std_ReturnType Rte_Write_WarningStatus_FaultLamp(uint8 level);
Std_ReturnType Rte_Write_WarningStatus_AbsLamp(uint8 level);

/* MeterStatus 拡張シグナル書き込みポート (CAN 0x200、uds_tester の仮想メータ
 * 表示タブ向けミラー。EngineSpeed/CoolantTemp は App_EngineManager が
 * EngineInfo(RX) の検証済み値を、RunLamp/FaultLamp/AbsLamp は
 * App_WarningIndicator が WarningStatus と同じ値をそれぞれ書き込む） */
Std_ReturnType Rte_Write_MeterStatus_EngineSpeed(EngineSpeed_t speed);
Std_ReturnType Rte_Write_MeterStatus_RunLamp(uint8 level);
Std_ReturnType Rte_Write_MeterStatus_FaultLamp(uint8 level);
Std_ReturnType Rte_Write_MeterStatus_AbsLamp(uint8 level);
Std_ReturnType Rte_Write_MeterStatus_CoolantTemp(CoolantTemp_t temp);

/* ----------------------------------------------------------------------
 * Rte_Invalidate_<p>_<o>  (RTE 5.6.7, SWS_Rte_01206)
 * ---------------------------------------------------------------------- */
/* CoolantTemp ミラーの無効化ポート (Com_InvalidateSignal へ委譲。
 * Rte_COMCbkInv_CoolantTemp() から呼ばれる) */
uint8 Rte_Invalidate_MeterStatus_CoolantTemp(void);

/* ----------------------------------------------------------------------
 * Rte_Read_<p>_<o>  (RTE 5.6.10, SWS_Rte_01091)
 * ---------------------------------------------------------------------- */
/* EngineInfo (CAN 0x100) 由来の Read ポート。E2E Transformer 経由のため
 * Rte_IStatusType を返す（RTE_E_OK / RTE_E_COM_STOPPED /
 * RTE_E_HARD_TRANSFORMER_ERROR / RTE_E_SOFT_TRANSFORMER_ERROR。詳細は
 * Rte_Type.h と Rte.c の Rte_COMRxInd_EngineInfo() を参照）。*data は
 * いずれのケースでも最後の正常値が書き込まれる（本実装の意図的な簡略化。
 * 詳細は Rte.c 冒頭のコメント参照）。 */
Rte_IStatusType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data);
Rte_IStatusType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data);
Rte_IStatusType Rte_Read_EngineStatus_EngineOnFlag(EngineOnFlag_t* data);

Std_ReturnType Rte_Read_EngineStatus_EngineState(EngineState_t* data);

Std_ReturnType Rte_Read_WarningIndicator_EngineState(EngineState_t* data);

/* ABS ECU シグナル読み取りポート (AbsInfo フレーム 0x110)。
 * EngineInfo 系と同じ理由で Rte_IStatusType を返す。 */
Rte_IStatusType Rte_Read_VehicleSensor_VehicleSpeed(VehicleSpeed_t* data);
Rte_IStatusType Rte_Read_BrakeSensor_BrakeActive(BrakeActive_t* data);
Rte_IStatusType Rte_Read_AbsSensor_AbsActive(AbsActive_t* data);

/* ----------------------------------------------------------------------
 * Rte_Call_<p>_<o>  (RTE 5.6.13, SWS_Rte_01102)
 * ---------------------------------------------------------------------- */
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
Std_ReturnType Rte_Call_FiM_GetFunctionPermission(uint8 functionId, boolean* permission);

/* Client/Server ポート — ComM_RequestComMode(COMM_USER_0, mode) へ委譲。
 * App_EngineManager が「エンジン OFF が一定サイクル継続 = 通信不要」と
 * 判断したときの通信モード要求（ボランタリスリープ）に使う。 */
Std_ReturnType Rte_Call_ComM_RequestComMode(ComM_ModeType mode);
/* Client/Server ポート — ComM_GetCurrentComMode(COMM_USER_0, mode) へ委譲。
 * 通信スリープからの復帰直後を検出するために使う。 */
Std_ReturnType Rte_Call_ComM_GetCurrentComMode(ComM_ModeType* mode);

/* ======================================================================
 * AUTOSAR_SWS_RTE 5.8  RTE Lifecycle API Reference
 *   （[SWS_Rte_02569]/[SWS_Rte_02570]/[SWS_Rte_06749]、詳細は Rte.c の
 *   Rte_Start() 付近の「RTE ライフサイクル API」コメント参照）。
 *   EcuM_Init() から Rte_Start() → Rte_Init_EngineManager() →
 *   Rte_Init_WarningIndicator() の順に一度だけ呼び出すこと。Rte_Stop() は
 *   本プロジェクトに BSW シャットダウンシーケンス自体が無いため未使用。
 * ====================================================================== */

/* Rte_Start  (RTE 5.8.1, SWS_Rte_02569) */
Std_ReturnType Rte_Start(void);
/* Rte_Stop  (RTE 5.8.2, SWS_Rte_02570) */
Std_ReturnType Rte_Stop(void);
/* Rte_Init_<InitContainer>  (RTE 5.8.6, SWS_Rte_06749) */
void           Rte_Init_EngineManager(void);
void           Rte_Init_WarningIndicator(void);

/* ======================================================================
 * 本プロジェクト独自（AUTOSAR 仕様書に対応する関数なし）
 * ====================================================================== */

/* Rte_SendSignalGroup_<sg>: WarningStatus Signal Group の確定コミット
 * (Com_SendSignalGroup へ委譲) */
Std_ReturnType Rte_SendSignalGroup_WarningStatus(void);

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

/* Rte_Schedule*: Os タスクの代わりに SW-C の Runnable を起動するスタンドイン */
void Rte_ScheduleRunnables(void);
void Rte_ScheduleWarningIndicator(void);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* RTE_H */
