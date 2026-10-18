/**
 * \file    CanTSyn.h
 * \brief   CAN 時刻同期 公開インタフェース (AUTOSAR SWS_CanTSyn 準拠、時間マスターのみ)
 * \details 本 ECU の時刻（稼働時間）を、AUTOSAR の CAN 時刻同期（SYNC/FUP の2段階）の
 *          フォーマットで CAN バスへ周期配信する。受信側（CAN ツール）は、SYNC の
 *          受信時刻と FUP が示す「SYNC を実際に送信した時刻」の差から、ECU 時刻と
 *          自分の時刻のずれを求められる。
 *
 *          シーケンス（[SWS_CanTSyn_00025]/[SWS_CanTSyn_00045]）:
 *            1. SYNC 送信: 時刻 T0 の秒部分を載せて CanIf_Transmit() する。
 *            2. SYNC の送信完了通知（CanTSyn_TxConfirmation()）で、T0 からの経過時間を
 *               求め T4 = (T0 の ns 部分) + 経過時間 を計算する。
 *            3. FUP 送信: T4 の ns 部分を載せる（ns が 1 秒を超えた分は OVS）。
 *               送信遅延（キュー待ち等）を含めた「SYNC を送った時刻」が配信される。
 *
 *          フォーマット（CRC なし、DLC=8、時刻はビッグエンディアン）:
 *            SYNC: Byte0=0x10 / Byte2=時間領域(上位4bit)+SC(下位4bit) / Byte4-7=秒
 *            FUP : Byte0=0x18 / Byte2=時間領域+SC（SYNC と同じ値）
 *                  / Byte3=SGW+OVS / Byte4-7=ナノ秒
 *
 *          対応範囲は CanTSyn_Cfg.h を参照（時間マスターのみ。StbM は未実装で、
 *          時刻源は Os カウンタ）。
 *
 *          使い方:
 *            1. EcuM_Init 内で CanTSyn_Init() を呼ぶ（CanIf_Init() の後）。
 *            2. Os スケジューラが CANTSYN_MAIN_FUNCTION_PERIOD_MS ごとに
 *               CanTSyn_MainFunction() を呼ぶ（SYNC の周期送信と、送信完了待ちの監視）。
 *            3. CanIf が送信完了のたびに CanTSyn_TxConfirmation() を呼ぶ。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANTSYN_H
#define CANTSYN_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "CanTSyn_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/**
 * \brief   CanTSyn_Init() の設定引数型（不透明型）。
 *
 * \details SWS_CanTSyn_00093 は post-build 設定構造体へのポインタを要求するが、
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を持たないため、
 *          中身を定義しない不透明型とし、ポインタとしてのみ扱う
 *          （`CanNm_ConfigType` と同じ簡略化パターン）。
 */
typedef struct CanTSyn_ConfigType_Tag CanTSyn_ConfigType;

/**
 * \brief   送信機能のオン/オフ（[SWS_CanTSyn_00095]）。
 */
typedef enum
{
    CANTSYN_TX_OFF = 0,  /**< 送信しない */
    CANTSYN_TX_ON        /**< 送信する（既定） */
} CanTSyn_TransmissionModeType;

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/**
 * \brief   CanTSyn モジュールを初期化する。送信機能はオンで、最初の SYNC は
 *          次の CanTSyn_MainFunction() 呼び出しで送る。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build
 *                        設定を持たないため）。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00093}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_Init(const CanTSyn_ConfigType* ConfigPtr);

/**
 * \brief   CanTSyn のバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL を渡すと
 *                           CANTSYN_E_NULL_POINTER を DET へ報告し、何も書かない。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00094}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_GetVersionInfo(Std_VersionInfoType* versioninfo);

/**
 * \brief   CanTSyn の送信機能をオン/オフする。
 *
 * \details オフにすると、途中の SYNC/FUP の送信シーケンスも破棄して待機に戻る。
 *          オンに戻すと、次の CanTSyn_MainFunction() で、前回の SYNC から送信周期が
 *          経過していれば SYNC を送る（オフの間に周期が過ぎていればすぐに送る）。
 *
 * \param[in]  CtrlIdx  CAN チャネル番号（単一チャネル構成のため 0 のみ有効）。
 *                      それ以外は CANTSYN_E_INV_CTRL_IDX を DET へ報告し何もしない。
 * \param[in]  Mode     CANTSYN_TX_OFF / CANTSYN_TX_ON。それ以外は
 *                      CANTSYN_E_PARAM を DET へ報告し何もしない。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00095}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_SetTransmissionMode(uint8 CtrlIdx, CanTSyn_TransmissionModeType Mode);

/**
 * \brief   時刻同期メッセージの周期送信と、送信完了待ちの監視を行う。
 *
 * \details 待機中に CANTSYN_TX_PERIOD_MS が経過していれば SYNC を送信する。
 *          SYNC/FUP の送信完了待ちが CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS を
 *          超えたら、送信を取り消して待機に戻る（[SWS_CanTSyn_00033]）。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00102}
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_MainFunction(void);

/**
 * \brief   SYNC/FUP の送信完了を通知する（CanIf から呼ばれる）。
 *
 * \details SYNC の送信完了なら、送信遅延を含む T4 を計算して FUP を送信する
 *          （[SWS_CanTSyn_00045]）。ただし SYNC を渡してから CANTSYN_MASTER_
 *          CONFIRMATION_TIMEOUT_MS 以上経っていたら、T4 が信用できないため FUP は
 *          送らず待機に戻る。FUP の送信完了なら待機に戻る。
 *          result が E_NOT_OK の場合は、その送信シーケンスを破棄して待機に戻る。
 *
 * \param[in]  TxPduId  送信完了した PDU の ID（CANTSYN_TX_PDU_ID 以外は
 *                      CANTSYN_E_INVALID_PDUID を DET へ報告し何もしない）。
 * \param[in]  result   E_OK=送信成功。
 *
 * \AUTOSARReq     {SWS_CanTSyn_00099}
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTSyn_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

#ifdef __cplusplus
}
#endif

#endif /* CANTSYN_H */
