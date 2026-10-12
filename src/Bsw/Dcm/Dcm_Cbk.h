/**
 * \file    Dcm_Cbk.h
 * \brief   DCM コールバック 公開インタフェース (AUTOSAR SWS_DCM 簡易実装)
 * \details PduR から呼び出される DCM 受信コールバックを宣言する。
 *          CanTp との受け渡しは、仕様の TP の API（`Dcm_StartOfReception`/`Dcm_CopyRxData`/
 *          `Dcm_TpRxIndication`（受信）、`Dcm_CopyTxData`/`Dcm_TpTxConfirmation`（送信））で行う。
 *          CanTp は組み立て用のバッファを持たず、フレームごとに、Dcm の受信バッファへの
 *          コピーと、Dcm の送信バッファからのコピーを行う。仕様では PduR を経由する
 *          （PduR_CanTpStartOfReception など）が、本プロジェクトは PduR の TP の経路を
 *          持たないので、CanTp が Dcm を直接呼ぶ。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DCM_CBK_H
#define DCM_CBK_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "ComStack_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

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
 * \brief   TP 受信の開始を Dcm へ伝える（SF の受信、または FF の受信）。
 *
 * \details CanTp が、SF または FF を受信した時点で呼ぶ。Dcm は受信を受け入れるか、拒否するかを返す。
 *          TpSduLength が 0 のときは、受信バッファの大きさを bufferSizePtr で返すだけである。
 *          本プロジェクトは MetaData を持たないので、CanTp は info に NULL を渡す。データは、続く
 *          Dcm_CopyRxData() で受け取る。
 *          次の場合は、BUFREQ_E_NOT_OK で拒否する: 診断応答の送信が進行中（[SWS_Dcm_00557]）、未初期化。
 *          TpSduLength が受信バッファ（DCM_RX_BUF_SIZE）より大きければ BUFREQ_E_OVFL を返す。
 *
 * \param[in]   id             I-PDU の識別子。
 * \param[in]   info           FF/SF のペイロードと MetaData。本プロジェクトでは NULL。
 * \param[in]   TpSduLength    受信する N-SDU 全体の長さ。
 * \param[out]  bufferSizePtr  Dcm の受信バッファの空き。
 *
 * \retval  BUFREQ_OK        受信を受け入れた。
 * \retval  BUFREQ_E_NOT_OK  受信を拒否した。受信は中断する。
 * \retval  BUFREQ_E_OVFL    必要な長さのバッファを用意できない。受信は中断する。
 *
 * \AUTOSARReq     {SWS_Dcm_00094}
 * \ServiceID      {0x46}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType* info, PduLengthType TpSduLength, PduLengthType* bufferSizePtr);

/**
 * \brief   受信した N-SDU の一部（N-PDU のデータ）を、Dcm の受信バッファへコピーする。
 *
 * \details 呼ぶたびに、N-SDU の次の部分を渡す。SduLength が 0 のときは、バッファの空きを問い合わせるだけで、
 *          SduDataPtr は NULL でもよい。
 *
 * \param[in]   id             受信した I-PDU の識別子。
 * \param[in]   info           コピー元（SduDataPtr）とバイト数（SduLength）。
 * \param[out]  bufferSizePtr  コピー後の、Dcm の受信バッファの空き。
 *
 * \retval  BUFREQ_OK        コピーした。
 * \retval  BUFREQ_E_NOT_OK  コピーしなかった（受信中でない、通知した長さを超える、ポインタが NULL）。
 *
 * \AUTOSARReq     {SWS_Dcm_00556}
 * \ServiceID      {0x44}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
BufReq_ReturnType Dcm_CopyRxData(PduIdType id, const PduInfoType* info, PduLengthType* bufferSizePtr);

/**
 * \brief   TP 受信の終了を Dcm へ伝える。
 *
 * \details result が E_OK で、通知された長さのデータが揃っていれば、Dcm は受信した UDS 要求を処理する。
 *          E_NOT_OK（N_Cr タイムアウトやシーケンス番号の誤りなどによる中断）なら、受信したデータを捨てる。
 *
 * \param[in]  id      受信した I-PDU の識別子。
 * \param[in]  result  受信の結果。
 *
 * \AUTOSARReq     {SWS_Dcm_00093}
 * \ServiceID      {0x45}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result);

/**
 * \brief   送信する N-SDU の一部（N-PDU のデータ）を、CanTp の送信バッファへコピーする。
 *
 * \details CanTp が、SF・FF・各 CF を組み立てる時に呼ぶ。呼ぶたびに、応答の次の部分を渡す。
 *          retry->TpDataState が TP_DATARETRY のときは、TxTpDataCnt バイト前から再度コピーする。
 *          SduLength が 0 のときは、状態の指示や、残りのデータ量の問い合わせだけを行う。
 *
 * \param[in]   id                 送信する I-PDU の識別子。
 * \param[in]   info               コピー先（SduDataPtr）とバイト数（SduLength）。
 * \param[in]   retry              送信済みデータの確認・再送の指示。NULL なら、コピーしたデータはすぐ捨ててよい。
 * \param[out]  availableDataPtr   コピー後の、Dcm の送信バッファの残りのバイト数。
 *
 * \retval  BUFREQ_OK        要求どおりにコピーした。
 * \retval  BUFREQ_E_BUSY    要求したバイト数の送信データがまだ無い。何もコピーしていない。
 * \retval  BUFREQ_E_NOT_OK  コピーしなかった（送信中でない、ポインタが NULL、再送の位置が不正）。
 *
 * \AUTOSARReq     {SWS_Dcm_00092}
 * \ServiceID      {0x43}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
BufReq_ReturnType Dcm_CopyTxData(PduIdType id, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr);

/**
 * \brief   TP 送信の終了を Dcm へ伝える。
 *
 * \details 応答の最後のフレームを送った時（E_OK）、または送信を中断した時（E_NOT_OK）に、CanTp が呼ぶ。
 *          Dcm は、送信が進行中である状態を解除する。
 *
 * \param[in]  id      送信した I-PDU の識別子。
 * \param[in]  result  送信の結果。
 *
 * \AUTOSARReq     {SWS_Dcm_00351}
 * \ServiceID      {0x48}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/**
 * \brief   ComM のチャネルが COMM_NO_COMMUNICATION へ遷移したことを Dcm へ伝える。
 *
 * \details 応答の送信を禁止する。Dcm が扱わない ComM チャネルの通知は何もせず戻る。
 *          ComM_BusSM_ModeIndication() が、チャネルのモードが通知されるたびに呼ぶ。
 *
 * \param[in]  NetworkId  モードが変わった ComM チャネルの識別子。
 *
 * \AUTOSARReq     {SWS_Dcm_00356}
 * \ServiceID      {0x21}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_ComM_NoComModeEntered(uint8 NetworkId);

/**
 * \brief   ComM のチャネルが COMM_SILENT_COMMUNICATION へ遷移したことを Dcm へ伝える。
 *
 * \details 応答の送信を禁止する。Dcm が扱わない ComM チャネルの通知は何もせず戻る。
 *          ComM_BusSM_ModeIndication() が、チャネルのモードが通知されるたびに呼ぶ。
 *
 * \param[in]  NetworkId  モードが変わった ComM チャネルの識別子。
 *
 * \AUTOSARReq     {SWS_Dcm_00358}
 * \ServiceID      {0x22}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_ComM_SilentComModeEntered(uint8 NetworkId);

/**
 * \brief   ComM のチャネルが COMM_FULL_COMMUNICATION へ遷移したことを Dcm へ伝える。
 *
 * \details 応答の送信を許可する。Dcm が扱わない ComM チャネルの通知は何もせず戻る。
 *          ComM_BusSM_ModeIndication() が、チャネルのモードが通知されるたびに呼ぶ。
 *
 * \param[in]  NetworkId  モードが変わった ComM チャネルの識別子。
 *
 * \AUTOSARReq     {SWS_Dcm_00360}
 * \ServiceID      {0x23}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_ComM_FullComModeEntered(uint8 NetworkId);

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* DCM_CBK_H */
