/**
 * \file    Nm.h
 * \brief   ネットワークマネジメント 公開インタフェース (AUTOSAR SWS_CANNM 準拠)
 * \details 本 ECU（メータ ECU）の CanNm 状態機械（Network Mode の Repeat
 *          Message/Normal Operation/Ready Sleep の3内部状態、Prepare
 *          Bus-Sleep Mode、Bus-Sleep Mode）を実装する。
 *          実車の CanNm は Com スタックを経由せず CanIf_Transmit()/
 *          CanIf_RxIndication() を直接やり取りするため、本モジュールも
 *          PduR/Com を介さない。
 *
 *          呼び出し関係（ComM が「通信の要否」を Nm に伝える）:
 *            ComM_BusSMIndication() がチャネルモードを FULL_COM/NO_COM へ
 *            確定させるたびに、Nm_NetworkRequest()/Nm_NetworkRelease() を
 *            呼ぶ。以降の実際の送受信・タイマ管理・状態遷移は本モジュールが
 *            自律的に行う（ComM は毎周期ポーリングしない）。
 *
 *          Bus-Sleep Mode への到達通知（協調スリープの要）:
 *            Nm が実際に Bus-Sleep Mode へ遷移した瞬間、CanSM_NmBusSleepMode()
 *            を直接呼ぶ。CanSM はこの通知を受けて初めて
 *            Can_SetControllerMode(CAN_T_SLEEP) で実際に CAN コントローラを
 *            スリープさせる（CanSM.c 参照）。これにより、他ノード（仮想他ECU）
 *            が NM フレームを送信し続けている間は本 ECU が実際にはスリープ
 *            しないことを実機で確認できる。
 *
 *          使い方:
 *            1. EcuM_Init 内で Nm_Init() を呼ぶ（ComM_Init 完了後）。
 *            2. Os スケジューラが NM_CYCLE_MS ごとに Nm_MainFunction() を呼ぶ
 *               （タイマ満了判定・PDU 再送信）。
 *            3. CanIf が CAN 0x400 受信のたびに Nm_RxIndication() を、
 *               送信完了のたびに Nm_TxConfirmation() を呼ぶ。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef NM_H
#define NM_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Nm_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   CanNm の3つの操作モード ([SWS_CanNm_00092])。
 */
typedef enum
{
    NM_MODE_BUS_SLEEP = 0,        /**< Bus-Sleep Mode */
    NM_MODE_PREPARE_BUS_SLEEP,    /**< Prepare Bus-Sleep Mode */
    NM_MODE_NETWORK               /**< Network Mode（内部状態は Nm_StateType 参照） */
} Nm_ModeType;

/**
 * \brief   CanNm の内部状態（[SWS_CanNm_00094] の Network Mode 内部3状態を含む）。
 */
typedef enum
{
    NM_STATE_BUS_SLEEP = 0,        /**< Bus-Sleep Mode */
    NM_STATE_PREPARE_BUS_SLEEP,    /**< Prepare Bus-Sleep Mode */
    NM_STATE_REPEAT_MESSAGE,       /**< Network Mode: Repeat Message State */
    NM_STATE_NORMAL_OPERATION,     /**< Network Mode: Normal Operation State */
    NM_STATE_READY_SLEEP           /**< Network Mode: Ready Sleep State */
} Nm_StateType;

/**
 * \brief   Nm モジュールを初期化する。Bus-Sleep Mode から開始する。
 *
 * \pre        CanIf_Init() / ComM_Init() が正常に完了していること。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_Init(void);

/**
 * \brief   Nm モジュールを未初期化状態に戻す。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_DeInit(void);

/**
 * \brief   通信が必要であることを Nm へ伝える（[SWS_CanNm_00104] 相当）。
 *
 * \details Bus-Sleep/Prepare Bus-Sleep Mode から呼ばれた場合は Repeat Message
 *          State へ、Ready Sleep State から呼ばれた場合は Normal Operation
 *          State へ遷移する。既に Repeat Message/Normal Operation State なら
 *          何もしない（冪等）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化。
 *
 * \AUTOSARReq     {SWS_CanNm_00208}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_NetworkRequest(void);

/**
 * \brief   通信が不要になったことを Nm へ伝える（[SWS_CanNm_00105] 相当）。
 *
 * \details Normal Operation State から呼ばれた場合は Ready Sleep State へ
 *          遷移する（NM フレーム送信を停止するが、NM-Timeout Timer が
 *          満了するまでは Prepare Bus-Sleep Mode へは移行しない）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_NetworkRelease(void);

/**
 * \brief   Repeat Message State への遷移を要求する（[SWS_CanNm_00120] 相当）。
 *
 * \details 本プロジェクトでは診断・デバッグ用途を想定するのみで、通常の
 *          運用フローからは呼ばない。Repeat Message State/Prepare
 *          Bus-Sleep Mode/Bus-Sleep Mode から呼ばれた場合は無視する
 *          （[SWS_CanNm_00137]）。
 *
 * \retval  E_OK      Repeat Message State へ遷移した。
 * \retval  E_NOT_OK  未初期化、または現在の状態では受理できない。
 *
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_RepeatMessageRequest(void);

/**
 * \brief   NM フレームの受信を通知する（CanIf から呼ばれる）。
 *
 * \details Network Mode 中は NM-Timeout Timer を再起動する
 *          （[SWS_CanNm_00098]）。Prepare Bus-Sleep Mode 中は Network Mode
 *          （Repeat Message State）へ自動遷移する（[SWS_CanNm_00124]）。
 *          Bus-Sleep Mode 中は状態遷移せず NM_E_NET_START_IND を DET へ
 *          報告するのみ（[SWS_CanNm_00126]/[SWS_CanNm_00336]。実際に
 *          ネットワークへ復帰するかどうかは上位層（本プロジェクトでは
 *          CanSM のウェイクアップ検証経由）が別途 Nm_NetworkRequest() を
 *          呼んで決める）。
 *
 * \param[in]  RxPduId     受信 PDU ID（本プロジェクトでは単一チャネルのため未使用）。
 * \param[in]  PduInfoPtr  受信データ。NULL 禁止。
 *
 * \ServiceID      {0x42}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/**
 * \brief   NM フレームの送信完了を通知する（CanIf から呼ばれる）。
 *
 * \details 送信成功時、Network Mode 中は NM-Timeout Timer を再起動する
 *          （[SWS_CanNm_00099]）。
 *
 * \param[in]  TxPduId  送信完了した PDU ID（本プロジェクトでは単一チャネルのため未使用）。
 * \param[in]  result   E_OK=送信成功。
 *
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/**
 * \brief   Nm の周期処理。タイマ満了判定と NM フレームの（再）送信を行う。
 *
 * \ServiceID      {0x13}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_MainFunction(void);

/**
 * \brief   診断 CommunicationControl (UDS SID 0x28) からの送信有効/無効要求を反映する。
 *
 * \details Enabled=0 の間、Repeat Message/Normal Operation State でも NM
 *          フレームを送信しない（[SWS_CanNm_00100] の passive mode 相当の
 *          抑制。状態機械自体は通常どおり遷移する）。
 *
 * \param[in]  Enabled  0=送信を抑制する、1=通常どおり送信する。
 *
 * \ServiceID      {0x0C}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_SetTxEnabled(uint8 Enabled);

/**
 * \brief   現在の CanNm 状態とモードを取得する（[SWS_CanNm_00091] 相当）。
 *
 * \param[out]  StatePtr  現在の内部状態の格納先。NULL 可（不要なら渡さなくてよい）。
 * \param[out]  ModePtr   現在の操作モードの格納先。NULL 可。
 *
 * \retval  E_OK      取得した。
 * \retval  E_NOT_OK  未初期化。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_GetState(Nm_StateType* StatePtr, Nm_ModeType* ModePtr);

/**
 * \brief   Nm モジュールのバージョン情報を取得する。
 *
 * \details Nm_Init と並び、未初期化時でも NM_E_UNINIT を報告しない例外 API
 *          （他 BSW モジュールと共通の慣例）のため、初期化状態は確認せず
 *          NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* NM_H */
