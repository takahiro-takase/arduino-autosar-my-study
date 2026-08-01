/**
 * \file    Fee.h
 * \brief   Flash EEPROM Emulation 公開インタフェース (AUTOSAR SWS_Fee 準拠)
 * \details Renesas RA (Arduino UNO R4) の EEPROM.h（フラッシュエミュレーション、
 *          内部でセクタ消去・書き込みサイクルを伴う）を、MemIf 経由で NvM から
 *          利用可能にする下位ドライバ。本プロジェクトが対応する MCU は
 *          Renesas RA のみ（AVR/UNO 無印は初代のプログラムサイズ制限により
 *          UNO R4 へ移行済み。対になる Ea モジュールは削除済み）。
 *
 *          本実装の設計方針:
 *            - Fee_Write() は「ジョブを受け付けて即座に返る」非同期 API
 *              (AUTOSAR SWS_Fee の Fee_Write 相当)。実際の物理書き込みは
 *              Fee_MainFunction() が 1 回の呼び出しにつき 1 バイトだけ進める。
 *              これは、RA の EEPROM ライブラリがバイト単位の書き込みでも
 *              消去・書き込みサイクルを伴うため、複数バイトを同期的に
 *              書くと協調スケジューラが長時間停止し、WdgM の Deadline
 *              Supervision を巻き込んで実機で HW ウォッチドッグリセットを
 *              引き起こした（旧 NvM.c で判明した不具合、詳細は NvM.c の
 *              git 履歴参照）ことに対する、Fee 自身の責務としての対策。
 *            - Fee_Read() は同期 API。EcuM_Init() が Os スケジューラ開始前
 *              (Fee_MainFunction() を誰も呼べない期間) に NvM_Init() 経由で
 *              使うことしか想定していないため、ブロッキングしても無害。
 *            - Fee_WriteImmediate() も同期 API（AUTOSAR の
 *              FeeImmediateData ブロック属性に相当する「即時書き込み」）。
 *              NvM の CRC 不一致時の自動デフォルト復元など、Fee_Read() と
 *              同じく Os スケジューラ開始前の文脈でのみ使う。
 *            - 本実装は「論理ブロック番号 → 物理アドレス」を変換する
 *              FeeBlockConfiguration テーブルを持たない（学習用簡略化。
 *              呼び出し元が NvM 1 個のみで、かつ NvM_PBCfg.c が既に
 *              ブロックごとに一意な物理アドレスを静的に割り当てている
 *              ため、追加の間接テーブルを設けても学習効果が薄いと判断した）。
 *              かわりに呼び出し元が物理アドレスを直接指定する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef FEE_H
#define FEE_H

#include "Std_Types.h"
#include "MemIf_Types.h"
#include "Fee_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Fee モジュールを初期化する。
 * \details ジョブ状態を IDLE にリセットする。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Fee_Init(void);

/**
 * \brief   Fee モジュールのバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x08}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Fee_GetVersionInfo(Std_VersionInfoType* versioninfo);

/**
 * \brief   指定アドレスから Length バイトを同期的に読み込む。
 *
 * \details Os スケジューラ開始前（Fee_MainFunction() を誰も呼べない期間）の
 *          利用のみを想定した同期 API。ブロッキングする。
 *
 * \param[in]   Address        読み込み開始アドレス。
 * \param[out]  DataBufferPtr  読み込み先バッファ。NULL 禁止。
 * \param[in]   Length         読み込むバイト数。0 禁止。
 *
 * \retval  E_OK      正常完了。
 * \retval  E_NOT_OK  未初期化、NULL、または Length=0。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Fee_Read(uint16 Address, uint8* DataBufferPtr, uint16 Length);

/**
 * \brief   指定アドレスへ Length バイトを非同期に書き込むジョブを開始する。
 *
 * \details 本関数はジョブ状態を記録して即座に返る（物理書き込みは行わない）。
 *          実際の書き込みは Fee_MainFunction() が 1 バイトずつ進める。
 *          DataBufferPtr が指す先は、ジョブ完了（Fee_GetJobResult() が
 *          MEMIF_JOB_OK/MEMIF_JOB_CANCELED を返す）まで有効でなければならない
 *          （呼び出し元がスタック変数を渡してはならない。NvM.c が static 変数
 *          経由で渡している理由も参照）。
 *          既に別のジョブが処理中（Fee_GetStatus()==MEMIF_BUSY）の場合は
 *          失敗する（呼び出し元は先に Fee_Cancel() で中断すること）。
 *
 * \param[in]  Address        書き込み開始アドレス。
 * \param[in]  DataBufferPtr  書き込み元データ。NULL 禁止。
 * \param[in]  Length         書き込むバイト数。0 禁止。
 *
 * \retval  E_OK      ジョブを受け付けた（書き込み完了を意味しない）。
 * \retval  E_NOT_OK  未初期化、NULL、Length=0、または既にジョブ処理中。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Fee_Write(uint16 Address, const uint8* DataBufferPtr, uint16 Length);

/**
 * \brief   指定アドレスへ Length バイトを同期的に（即時）書き込む。
 *
 * \details AUTOSAR の FeeImmediateData ブロック属性に相当。Os スケジューラ
 *          開始前の利用のみを想定した同期 API。ブロッキングする。
 *          実 AUTOSAR には独立した関数としては存在しない（FeeImmediateData=
 *          TRUE のブロックに対する Fee_Write() が内部的に同期実行される
 *          という仕様のため）本プロジェクト独自の API（Fee_Cfg.h 参照）。
 *
 * \param[in]  Address        書き込み開始アドレス。
 * \param[in]  DataBufferPtr  書き込み元データ。NULL 禁止。
 * \param[in]  Length         書き込むバイト数。0 禁止。
 *
 * \retval  E_OK      正常完了。
 * \retval  E_NOT_OK  未初期化、NULL、または Length=0。
 *
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Fee_WriteImmediate(uint16 Address, const uint8* DataBufferPtr, uint16 Length);

/**
 * \brief   処理中の非同期ジョブを中断する。
 *
 * \details 呼び出し元（NvM）が処理中のブロックへ新たな書き込みを要求した際、
 *          書きかけの古いデータと新しいデータが混在した「ちぎれ書き」を
 *          防ぐために使う。中断後、ジョブ状態は IDLE に戻り
 *          Fee_GetJobResult() は MEMIF_JOB_CANCELED を返す。
 *          ジョブが無い状態で呼んでも副作用はない。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Fee_Cancel(void);

/**
 * \brief   現在のビジー状態を取得する。
 *
 * \retval  MEMIF_UNINIT  Fee_Init() 未実行。
 * \retval  MEMIF_IDLE    ジョブなし。次の Fee_Write() を受付可能。
 * \retval  MEMIF_BUSY    Fee_Write() のジョブ処理中。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
MemIf_StatusType Fee_GetStatus(void);

/**
 * \brief   直近のジョブ結果を取得する。
 *
 * \retval  MEMIF_JOB_OK       直近のジョブが正常完了した。
 * \retval  MEMIF_JOB_PENDING  ジョブが処理中。
 * \retval  MEMIF_JOB_CANCELED 直近のジョブが Fee_Cancel() で中断された。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
MemIf_JobResultType Fee_GetJobResult(void);

/**
 * \brief   Fee 周期処理。処理中の非同期ジョブを 1 バイトだけ進める。
 *
 * \details ジョブが無ければ何もしない。MemIf_MainFunction() 経由で
 *          Os スケジューラから周期的に呼び出すこと。
 *
 * \ServiceID      {0x12}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Fee_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* FEE_H */
