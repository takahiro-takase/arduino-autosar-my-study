/**
 * \file    Rte.c
 * \brief   ランタイム環境 (AUTOSAR SWS_RTE 準拠)
 * \details SW-Component (SW-C) と BSW の COM モジュール間のデータ交換を
 *          仲介する AUTOSAR RTE API 層を実装する。
 *          型付きの Read/Write ポートアクセサと、Arduino の millis() タイマで
 *          駆動するシンプルな Runnable スケジューラを提供する。
 *          AUTOSAR 4.3.1 SWS_RTE の API 命名規則
 *          (Rte_Read_<p>_<o> / Rte_Write_<p>_<o>) に準拠する。
 *          シングルインスタンス SW-C かつトランスフォーマチェーン不使用のため、
 *          オプション引数 Rte_Instance / Rte_TransformerError は省略している。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Rte.h"
#include "Com.h"
#include "IoHwAb.h"
#include "SchM.h"

/* シグナル ID は Com_Cfg.h の COM_SIGNAL_* を使用（重複定義を排除） */

/* App_EngineManager.c が定義する SW-C Runnable の前方宣言 */
extern void App_EngineManager_Run(void);

/* App_WarningIndicator.c が定義する SW-C Runnable の前方宣言 */
extern void App_WarningIndicator_Run(void);

/* EngineState の内部ミラー変数。
 * Rte_Write_EngineStatus_EngineState() が書き込み、
 * Dcm の Rte_Read_EngineStatus_EngineState() が読み出す。
 * COM TX バッファは Com_ReceiveSignal で読めないため、ここで保持する。 */
static EngineState_t Rte_EngineStateMirror = ENGINE_STATE_OFF;

/**
 * \brief   SpeedSensor 要求ポートから EngineSpeed シグナルを読み取る。
 *
 * \details Com_ReceiveSignal() へ委譲し、COM の RX I-PDU バッファから
 *          EngineSpeed シグナルを呼び出し元変数へアンパックする
 *          (AUTOSAR SWS_RTE の Rte_Read_<p>_<o> パターン)。
 *
 * \param[out] data  エンジン回転数を受け取る変数へのポインタ。NULL 禁止。
 *                   E_OK が返された場合のみ有効な値が格納される。
 *
 * \retval  E_OK      COM バッファからシグナルを正常に読み取った。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \pre        Com_Init() と Com_RxIndication() が呼ばれていること。
 *
 * \ServiceID      {0xF2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data)
{
    return Com_ReceiveSignal(COM_SIGNAL_ENGINE_SPEED, data);
}

/**
 * \brief   TempSensor 要求ポートから CoolantTemp シグナルを読み取る。
 *
 * \details Com_ReceiveSignal() へ委譲し、COM の RX I-PDU バッファから
 *          CoolantTemp シグナルを呼び出し元変数へアンパックする
 *          (AUTOSAR SWS_RTE の Rte_Read_<p>_<o> パターン)。
 *
 * \param[out] data  冷却水温を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      COM バッファからシグナルを正常に読み取った。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \pre        Com_Init() と Com_RxIndication() が呼ばれていること。
 *
 * \ServiceID      {0xF3}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data)
{
    return Com_ReceiveSignal(COM_SIGNAL_COOLANT_TEMP, data);
}

/**
 * \brief   EngineStatus 要求ポートから EngineOnFlag シグナルを読み取る。
 *
 * \details Com_ReceiveSignal() へ委譲し、COM の RX I-PDU バッファから
 *          EngineOnFlag シグナルを呼び出し元変数へアンパックする
 *          (AUTOSAR SWS_RTE の Rte_Read_<p>_<o> パターン)。
 *
 * \param[out] data  エンジン起動フラグを受け取る変数へのポインタ
 *                   （0 = 停止、1 = 起動）。NULL 禁止。
 *
 * \retval  E_OK      COM バッファからシグナルを正常に読み取った。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \pre        Com_Init() と Com_RxIndication() が呼ばれていること。
 *
 * \ServiceID      {0xF4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_EngineStatus_EngineOnFlag(EngineOnFlag_t* data)
{
    return Com_ReceiveSignal(COM_SIGNAL_ENGINE_ON_FLAG, data);
}

/**
 * \brief   EngineStatus 提供ポートへ EngineState シグナルを書き込む。
 *
 * \details Com_SendSignal() 経由で EngineState 値を COM の TX I-PDU バッファへ
 *          パックする (AUTOSAR SWS_RTE の Rte_Write_<p>_<o> パターン)。
 *          I-PDU は即座には送信されない。
 *          送信するには Rte_TriggerTransmit() を呼び出すこと。
 *
 * \param[in]  state  書き込むエンジン状態
 *                    (OFF / STARTING / RUNNING / FAULT)。
 *
 * \retval  E_OK      COM の TX バッファへ正常にパックした。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \pre        Com_Init() が正常に完了していること。
 *
 * \ServiceID      {0xF5}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Write_EngineStatus_EngineState(EngineState_t state)
{
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    Rte_EngineStateMirror = state;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();
    uint8 val = (uint8)state;
    return Com_SendSignal(COM_SIGNAL_ENGINE_STATE, &val);
}

/**
 * \brief   EngineStatus 提供ポートから EngineState を読み取る。
 *
 * \details App_EngineManager が Rte_Write_EngineStatus_EngineState() で
 *          書き込んだ最新値を返す。DCM の DID 0x0103 読み出しに使用する。
 *
 * \param[out] data  エンジン状態を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK     常に成功。
 * \retval  E_NOT_OK data が NULL。
 *
 * \ServiceID      {0xF8}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_EngineStatus_EngineState(EngineState_t* data)
{
    if (data == NULL)
        return E_NOT_OK;
    SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA();
    *data = Rte_EngineStateMirror;
    SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA();
    return E_OK;
}

/**
 * \brief   COM の TX I-PDU を即座に送信する。
 *
 * \details COM モジュールへの直接依存なしに SW-C が CAN フレーム送信を
 *          要求できるよう、Com_TriggerIPDUSend() をラップする。
 *          この関数はプロジェクト固有の RTE 拡張であり、AUTOSAR 標準の
 *          Rte_Trigger_<p>_<o> API（SW-C 内部イベントトリガ用）とは
 *          異なる目的で使用する。
 *
 * \param[in]  IPduId  送信する I-PDU の COM ハンドル。
 *
 * \retval  E_OK      I-PDU が PduR および CanIf へ正常に転送された。
 * \retval  E_NOT_OK  COM 未初期化、I-PDU が見つからない、
 *                    または下位層の送信が失敗した。
 *
 * \pre        Com_Init() が正常に完了していること。
 * \pre        送信前に Rte_Write_EngineStatus_EngineState() で
 *             TX バッファへ値を設定しておくこと。
 * \note       AUTOSAR 非標準 API。App_EngineManager が COM を
 *             直接呼び出さないようにするために追加した。
 *
 * \ServiceID      {0xF6}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_TriggerTransmit(Com_IPduIdType IPduId)
{
    return Com_TriggerIPDUSend(IPduId);
}

/**
 * \brief   マッピングされた SW-C Runnable を起動する。
 *
 * \details OS タスク (Task 2, 3000 ms 周期) から呼び出される。
 *          実行周期の管理は Os_PBCfg.c のタスクテーブルが担うため、
 *          この関数は App_EngineManager_Run() を無条件に呼び出すだけでよい。
 *
 *          AUTOSAR OS 環境では OsTask が直接 Runnable を呼び出すが、
 *          本実装では RTE が仲介することで SW-C と OS の直接依存を断つ。
 *
 * \pre        App_EngineManager_Init() が正常に完了していること。
 *
 * \ServiceID      {0xF7}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Rte_ScheduleRunnables(void)
{
    App_EngineManager_Run();
}

/**
 * \brief   WarningIndicator SW-C の EngineState ポートを読み取る。
 *
 * \details App_EngineManager が書き込んだ Rte_EngineStateMirror を返す。
 *          同一ミラー変数を複数の SW-C が読み取れるよう、
 *          WarningIndicator 用の独立したポートとして定義する
 *          (AUTOSAR Sender/Receiver の n:1 受信パターン)。
 *
 * \param[out] data  エンジン状態を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      正常に読み取った。
 * \retval  E_NOT_OK  data が NULL。
 *
 * \ServiceID      {0xF9}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_WarningIndicator_EngineState(EngineState_t* data)
{
    return Rte_Read_EngineStatus_EngineState(data);
}

/**
 * \brief   WarningIndicator SW-C の Runnable を起動する。
 *
 * \details OS タスク (Task 3, 500 ms 周期) から呼び出される。
 *          App_WarningIndicator_Run() を無条件に呼び出す。
 *
 * \pre        App_WarningIndicator_Init() が正常に完了していること。
 *
 * \ServiceID      {0xFA}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Rte_ScheduleWarningIndicator(void)
{
    App_WarningIndicator_Run();
}

/**
 * \brief   警告灯 LED レベル設定の Client/Server ポート。
 *
 * \details SW-C (App_WarningIndicator) から呼び出され、
 *          IoHwAb_Led_SetLevel() へ委譲する。
 *          C/S ポートにより SW-C は IoHwAb の存在を知らない
 *          (AUTOSAR SWS_RTE の Rte_Call_<p>_<o> パターン)。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFB}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_Led_SetLevel(uint8 level)
{
    return IoHwAb_Led_SetLevel(level);
}

/**
 * \brief   RUNNING LED (D6) レベル設定の Client/Server ポート。
 *
 * \details SW-C (App_WarningIndicator) から呼び出され、
 *          IoHwAb_LedRunning_SetLevel() へ委譲する。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFC}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_LedRunning_SetLevel(uint8 level)
{
    return IoHwAb_LedRunning_SetLevel(level);
}

/**
 * \brief   FAULT LED (D7) レベル設定の Client/Server ポート。
 *
 * \details SW-C (App_WarningIndicator) から呼び出され、
 *          IoHwAb_LedFault_SetLevel() へ委譲する。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFD}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_LedFault_SetLevel(uint8 level)
{
    return IoHwAb_LedFault_SetLevel(level);
}

/**
 * \brief   エンジン起動ボタン押下状態取得の Client/Server ポート。
 *
 * \details SW-C (App_EngineManager) から呼び出され、
 *          IoHwAb_Button_GetLevel() へ委譲する。
 *          C/S ポートにより SW-C は Dio チャネル番号やプルアップ配線を知らない
 *          (AUTOSAR SWS_RTE の Rte_Call_<p>_<o> パターン)。
 *
 * \param[out] level  押下状態 (0=解放, 1=押下)。NULL 禁止。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xFF}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Call_Button_GetLevel(uint8* level)
{
    return IoHwAb_Button_GetLevel(level);
}

/**
 * \brief   VehicleSensor 要求ポートから VehicleSpeed シグナルを読み取る。
 *
 * \details Com_ReceiveSignal() へ委譲し、COM の RX I-PDU バッファ (IPduId=1) から
 *          VehicleSpeed シグナルをアンパックする（0.01 km/h 単位）。
 *          シグナルの発信元は ABS ECU (CAN ID 0x110)。
 *
 * \param[out] data  車速を受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      正常に読み取った。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \ServiceID      {0xFC}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_VehicleSensor_VehicleSpeed(VehicleSpeed_t* data)
{
    return Com_ReceiveSignal(COM_SIGNAL_VEHICLE_SPEED, data);
}

/**
 * \brief   BrakeSensor 要求ポートから BrakeActive シグナルを読み取る。
 *
 * \details Com_ReceiveSignal() へ委譲し、COM の RX I-PDU バッファ (IPduId=1) から
 *          BrakeActive シグナルをアンパックする（0=解除 / 1=作動）。
 *          シグナルの発信元は ABS ECU (CAN ID 0x110)。
 *
 * \param[out] data  ブレーキ作動フラグを受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      正常に読み取った。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \ServiceID      {0xFD}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_BrakeSensor_BrakeActive(BrakeActive_t* data)
{
    return Com_ReceiveSignal(COM_SIGNAL_BRAKE_ACTIVE, data);
}

/**
 * \brief   AbsSensor 要求ポートから AbsActive シグナルを読み取る。
 *
 * \details Com_ReceiveSignal() へ委譲し、COM の RX I-PDU バッファ (IPduId=1) から
 *          AbsActive シグナルをアンパックする（0=非作動 / 1=ABS 作動中）。
 *          シグナルの発信元は ABS ECU (CAN ID 0x110)。
 *
 * \param[out] data  ABS 作動フラグを受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      正常に読み取った。
 * \retval  E_NOT_OK  COM 未初期化またはシグナル ID が見つからない。
 *
 * \ServiceID      {0xFE}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_AbsSensor_AbsActive(AbsActive_t* data)
{
    return Com_ReceiveSignal(COM_SIGNAL_ABS_ACTIVE, data);
}
