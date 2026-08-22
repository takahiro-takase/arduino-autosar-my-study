/**
 * \file    Com.h
 * \brief   通信マネージャ 公開インタフェース (AUTOSAR SWS_COM 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COM_H
#define COM_H

#include "Com_Types.h"
#include "Com_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWS_Com_00432 */
void Com_Init(const Com_ConfigType* config);

/**
 * \brief   COM モジュールを未初期化状態に戻す。
 *
 * \details 起動中の全 I-PDU（RX/TX 双方、`COM_IPDU_GROUP_NONE` 所属の
 *          常時有効 I-PDU も含む）を停止済み状態にした上で、内部設定
 *          ポインタを NULL に戻す（[SWS_Com_00129]）。以降 `Com_GetStatus()`
 *          は `COM_UNINIT` を返し、`Com_Init()` を除く他の全 API は
 *          `COM_E_UNINIT` を報告するようになる（[SWS_Com_00804]）。
 *          再度通信するには `Com_Init()` を呼び直す必要がある。
 *
 * \pre        他の AUTOSAR COM モジュールの API がプリエンプトしていないこと
 *             （[SWS_Com_00130] の Caveats。呼び出し元が保証する責務）。
 *
 * \AUTOSARReq     {SWS_Com_00129, SWS_Com_00130, SWS_Com_00804}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_DeInit(void);

/**
 * \brief   COM モジュールの初期化状態を返す。
 *
 * \details `Com_Init()` を除く他の全 API が未初期化時に `COM_E_UNINIT` を
 *          報告する（[SWS_Com_00804]）のに対し、本 API のみがその例外として
 *          未初期化状態でも安全に呼び出せる（開発エラー報告の対象外）。
 *
 * \retval  COM_INIT    COM は初期化済みで使用可能。
 * \retval  COM_UNINIT  COM は未初期化（Com_Init 前、または Com_DeInit 後）。
 *
 * \AUTOSARReq     {SWS_Com_00194}
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Com_StatusType Com_GetStatus(void);

/**
 * \brief   COM モジュールのバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_Com_00426}
 * \ServiceID      {0x09}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_GetVersionInfo(Std_VersionInfoType* versioninfo);

/* SWS_Com_00123 */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
/* SWS_Com_00198 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);

/**
 * \brief   RX Signal Group を I-PDU バッファから RX シャドウバッファへ確定コピーする。
 *
 * \details Com_SendSignalGroup() の RX 側対称版（詳細は Com.c の実装コメント、
 *          および README.md の「RX Signal Group」節を参照）。所属 I-PDU が
 *          IsSignalGroup=1 の場合、この呼び出し以降 Com_ReceiveSignal() は
 *          当該 I-PDU のメンバーシグナルを RX シャドウバッファから読む
 *          （呼び出し前は Com_Init() 直後の初期値のまま、または前回コミット
 *          時点の値のままとなる）。これにより、同じグループの複数メンバーを
 *          複数回の Com_ReceiveSignal() 呼び出しにまたがって読む間に、
 *          途中で新しいフレームが届いても（Com_RxIndication() が
 *          Com_RxBuffer を上書きしても）読み取り側は常にこの呼び出し時点の
 *          一貫したスナップショットを見る。
 *
 * \param[in]  GroupId  確定コピーする RX Signal Group（RX I-PDU）の ID。
 *
 * \retval  E_OK      GroupId が見つかり、コピーを行った（かつ現在タイムアウト中でない）。
 * \retval  E_NOT_OK  COM 未初期化、GroupId が RX I-PDU 設定テーブルに
 *                    存在しない、IsSignalGroup=0 の I-PDU を指定した、
 *                    またはコピー自体は行ったが当該 I-PDU が現在デッドライン
 *                    監視タイムアウト中である。
 *
 * \AUTOSARReq     {SWS_Com_00201, SWS_Com_00051, SWS_Com_00638, SWS_Com_00461}
 * \ServiceID      {0x19}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_ReceiveSignalGroup(Com_IPduIdType GroupId);

/**
 * \brief   RX I-PDU の生バイト列をそのままコピーする（E2E Transformer 等向け）。
 *
 * \details Com_ReceiveSignal() のビット単位アンパックを経由せず、I-PDU
 *          バッファの先頭 DLC バイトをそのまま DataPtr へコピーする。
 *          Com_RxTimedOut は見ない（RxIndicationCbk から、フレーム受信直後・
 *          タイムアウト判定より前に呼ばれる用途を想定しているため）。
 *
 * \param[in]  IPduId   読み取る RX I-PDU の ID。
 * \param[out] DataPtr  コピー先バッファへのポインタ。ipdu->DLC バイト以上必要。
 *
 * \retval  E_OK      IPduId が見つかり、DataPtr へコピーした。
 * \retval  E_NOT_OK  COM 未初期化、DataPtr が NULL、または IPduId が存在しない。
 *
 * \ServiceID      {0x1A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_ReceiveSignalGroupArray(Com_IPduIdType IPduId, uint8* DataPtr);

/**
 * \brief   RX I-PDU が現在タイムアウト中かどうかを返す軽量アクセサ。
 *
 * \details Rte 層が Com_ReceiveSignal() を介さずに E_NOT_OK 判定のゲートとして
 *          直接参照する用途を想定している（E2E Transformer 方式で Rte が
 *          ミラーから値を読む際に使用）。
 *
 * \param[in]  IPduId  確認する RX I-PDU の ID。
 *
 * \retval  1  タイムアウト中（IPduId が範囲外の場合も安全側でこちらを返す）。
 * \retval  0  タイムアウトしていない。
 *
 * \ServiceID      {0x1B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_IsRxTimedOut(Com_IPduIdType IPduId);

/* SWS_Com_00197 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
/* SWS_Com_00200: Signal Group メンバーをシャドウバッファへまとめて確定コミットする */
Std_ReturnType Com_SendSignalGroup(Com_IPduIdType GroupId);
/* SWS_Com_00124 */
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
/* SWS_Com_00398: 受信デッドライン監視タイムアウト検出。Os の 100ms タスクから呼び出す */
void Com_MainFunction(void);

/**
 * \brief   診断 CommunicationControl (UDS SID 0x28) からの通信有効/無効要求を反映する。
 *
 * \details RxEnabled=0 の間、Com_RxIndication() は受信フレームを無視する
 *          （バッファ・タイムアウトタイマとも更新しない）。あわせて
 *          Com_MainFunction() の受信デッドライン監視自体も評価を止める
 *          （SWS_Com_00684/SWS_Com_00685 相当。意図的に止めているだけの
 *          通信を「通信異常」として誤って上位層へ伝えないため）。
 *          RxEnabled が 0→1 へ遷移した際は、全 RX I-PDU の
 *          最終受信時刻・タイムアウトフラグをリセットしてデッドライン監視
 *          タイマを再始動する（SWS_Com_00787 相当。リセットしないと、
 *          TimeoutMs 以上の時間受信を抑制していた場合に再開直後で
 *          即座にタイムアウト判定されてしまう）。
 *          TxEnabled=0 の間、DIRECT/MIXED/PERIODIC いずれの I-PDU も
 *          Com_MainFunction() での実送信を抑制する（DIRECT/MIXED の変化検知
 *          自体は Com_RequestTxOnChange() が Com_TxPending[] へ記録するが、
 *          実際に PduR_Transmit() を呼ぶかどうかは Com_MainFunction() が
 *          Com_TxEnabled を見て判断する）。SWS_Com_00777「停止中の I-PDU の
 *          送信要求はキャンセルしなければならない」・SWS_Com_00334 の説明文
 *          の要求は、Com_MainFunction() が抑制中でも Com_TxPending[] を
 *          破棄する（保留させない）ことで満たす。TX バッファの値自体は
 *          Com_SendSignal() が既に更新済みのため失われないが、再度有効化
 *          された「だけ」で即座に送信されることはなく、再開後に実際に値が
 *          変化した時、または通常の周期フロアに新たに達した時に初めて
 *          送信される。
 *
 *          Com_IpduGroupStart()/Com_IpduGroupStop()（下記）とは独立した、
 *          直交する抑制機構である点に注意: こちらは全 I-PDU 一括の診断用
 *          スイッチ、I-PDU Group は個別の I-PDU 単位の起動/停止状態。
 *          実際に送受信処理が行われるのは両方が有効な場合のみ（AND 条件）。
 *
 * \param[in]  RxEnabled  0=受信を無視する、1=通常どおり受信する。
 * \param[in]  TxEnabled  0=送信を抑制する、1=通常どおり送信する。
 *
 * \ServiceID      {0x30}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_SetCommunicationEnabled(uint8 RxEnabled, uint8 TxEnabled);

/**
 * \brief   I-PDU Group を起動する（所属する I-PDU の送受信処理を許可する）。
 *
 * \details IpduGroupId に一致する `Com_IPduConfigType.IpduGroupId` を持つ
 *          全 I-PDU（RX/TX 双方）を起動済み状態にする。RX I-PDU は受信処理・
 *          デッドライン監視タイマを再始動する（[SWS_Com_00787]）。TX I-PDU は
 *          MDT/周期タイマの基準時刻を再始動し、update-bit をクリアし、
 *          現在のデータ内容から TMS を再評価する。
 *
 * \param[in]  IpduGroupId  起動する I-PDU Group の ID
 *                          （Com_Cfg.h の COM_IPDU_GROUP_* 参照）。
 * \param[in]  initialize   非 0 の場合、追加で I-PDU のデータ・Signal Group の
 *                          シャドウバッファ・フィルタ old_value を
 *                          ComSignalInitValue で初期化する（[SWS_Com_00222]）。
 *                          0 の場合は直近の値を保持したまま起動する。
 *
 * \AUTOSARReq     {SWS_Com_91001, SWS_Com_00114, SWS_Com_00787, SWS_Com_00222,
 *                  SWS_Com_00223, SWS_Com_00840}
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant for different I-PDU groups. Non reentrant for the same I-PDU group.}
 * \Synchronicity  {Synchronous}
 */
void Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, uint8 initialize);

/**
 * \brief   I-PDU Group を停止する（所属する I-PDU の送受信処理を禁止する）。
 *
 * \details IpduGroupId に一致する `Com_IPduConfigType.IpduGroupId` を持つ
 *          全 I-PDU（RX/TX 双方）を停止済み状態にする。RX I-PDU は受信処理・
 *          デッドライン監視を無効化する（[SWS_Com_00684]/[SWS_Com_00685]）。
 *          TX I-PDU は保留中の送信要求をキャンセルする（[SWS_Com_00777]）。
 *          さらに、PduR へは引き渡し済み（実送信済み）だが対応する
 *          `Com_TxConfirmation()` がまだ届いていない（未確認の）TX I-PDU が
 *          あれば、その `TxErrCbk`（Com_CbkTxErr 相当）を即座に呼ぶ
 *          （[SWS_Com_00479]/[SWS_Com_00491]）。実 AUTOSAR は signal 単位/
 *          signal group 単位で別々のコールバック名（Rte_COMCbkTErr_<sn>/<sg>）
 *          を持てるため（SWS_Com_00491 "corresponds to Rte_COMCbkTErr_<sn>
 *          or Rte_COMCbkTErr_<sg> respectively"）、Signal Group なら
 *          `Com_IPduConfigType.TxErrCbk` をグループ単位で 1 回、非 Signal
 *          Group なら従来どおり `Com_SignalConfigType.TxErrCbk` をシグナル
 *          単位で呼ぶ（TxAckCbk/Com_TxConfirmation() と同じ区別。詳細は
 *          Com.c の Com_IpduGroupStop() 実装コメント参照）。
 *          `Com_SendSignal()`/`Com_ReceiveSignal()` 自体は停止中でも内部
 *          バッファを更新・参照できる（[SWS_Com_00334]）。
 *
 * \param[in]  IpduGroupId  停止する I-PDU Group の ID。
 *
 * \AUTOSARReq     {SWS_Com_91002, SWS_Com_00684, SWS_Com_00685, SWS_Com_00777,
 *                  SWS_Com_00800, SWS_Com_00334, SWS_Com_00479, SWS_Com_00491}
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant for different I-PDU groups. Non reentrant for the same I-PDU group.}
 * \Synchronicity  {Synchronous}
 */
void Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId);

#ifdef COM_UNIT_TEST
/**
 * \brief   [テスト専用] TX I-PDU の Com_TxPending（変化時送信の保留フラグ）を取得する。
 *
 * \details `test/test_chain/` のコールチェーンテストからのみ使用するアクセサ。
 *          `COM_UNIT_TEST` は `[env:native_chain]` の `build_flags` でのみ
 *          定義され、実機ビルド（`uno_r4`）や単一モジュールテスト
 *          （`[env:native]`）では定義されないため、実機の `Com.h`/`Com.c` には
 *          一切含まれない（AUTOSAR 標準外の関数、`Can.h` の
 *          `CAN_UNIT_TEST`/`Can_Test_GetControllerState()` と同じ運用）。
 *          範囲外の `ipduId` は 0 を返す。
 */
uint8 Com_Test_GetTxPending(Com_IPduIdType ipduId);

/** [テスト専用] TX I-PDU の内部バッファ（Com_TxBuffer[ipduId]）先頭ポインタを
 *  取得する。範囲外の `ipduId` は NULL を返す。呼び出し側は当該 I-PDU の
 *  DLC バイト分のみ読むこと（バッファ自体は COM_IPDU_MAX_DLC バイト確保）。 */
const uint8* Com_Test_GetTxBuffer(Com_IPduIdType ipduId);

/** [テスト専用] RX シグナルの Com_SigTimedOut（デッドライン監視のタイムアウト
 *  検知フラグ）を取得する。`Com_MainFunction()` がしきい値超過を検知した後、
 *  `Com_ReceiveSignal()` が `RxDataTimeoutAction` を適用して消費するまでの間
 *  立っている（[「Rx 処理」の「デッドライン監視」](../../README.md#rx-processing-timeout)
 *  参照）。範囲外の `SignalId` は 0 を返す。 */
uint8 Com_Test_GetSigTimedOut(Com_SignalIdType SignalId);

/** [テスト専用] TX I-PDU の「送信済み・未確認」フラグ（Com_TxConfPending
 *  [ipduId]）を直接セットする。Com_IpduGroupStop() の TxErrCbk（グループ単位
 *  是正、SWS_Com_00491）を検証するには、実際に PduR_Transmit() 成功後・
 *  Com_TxConfirmation() 到達前という狭い区間を作り出す必要があるが、
 *  Signal Group の PduR/CanIf ルーティングをテスト専用に用意するのは
 *  過剰なため、この内部フラグを直接操作できるようにする。範囲外の
 *  `ipduId` は無視する。 */
void Com_Test_SetTxConfPending(Com_IPduIdType ipduId, uint8 value);
#endif

#ifdef __cplusplus
}
#endif

#endif
