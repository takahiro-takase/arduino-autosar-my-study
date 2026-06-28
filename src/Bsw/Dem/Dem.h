/**
 * \file    Dem.h
 * \brief   診断イベントマネージャ 公開インタフェース (AUTOSAR SWS_DEM 準拠)
 * \details アプリケーション SW-C および BSW モジュールが診断イベントを
 *          報告するための API を公開する。
 *
 *          エラー検出コンポーネント (App_EngineManager 等) は
 *          Dem_ReportErrorStatus() でイベントの発生・消滅を通知する。
 *          DEM は DTC ライフサイクル (PENDING → CONFIRMED → STORED) を
 *          管理し、EEPROM へ永続化する。
 *          CONFIRMED した DTC は、再故障せずに複数回の操作サイクル（起動〜次回
 *          起動）を経ると Dem_Init() が経年回復 (Aging) を判定し自動的に
 *          CONFIRMED を解除する（詳細は Dem.c / Dem_Cfg.h を参照）。
 *          DCM は Dem_GetAllDTCs() / Dem_ClearAllDTCs() 経由で UDS SID 0x19 / 0x14
 *          に応答する。FreezeFrame（故障時点のスナップショット）に加え、
 *          ExtendedData（累積故障確定回数、Dem_GetOccurrenceCounterOfEvent()）
 *          も SID 0x19 subFunc 0x06 経由で提供する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DEM_H
#define DEM_H

#include "Std_Types.h"
#include "Dem_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * 型定義
 * ----------------------------------------------------------------------- */

/** イベント ID 型 (DEM_EVENT_* 定数を渡す) */
typedef uint8 Dem_EventIdType;

/**
 * \brief   イベントステータス型 (AUTOSAR SWS_Dem_00586)
 *
 * モニタ（呼び出し元）は PASSED / FAILED の生のテスト結果のみを報告する。
 * PREPASSED / PREFAILED は Dem 内部のデバウンスカウンタが未確定の間の状態を表す
 * 値であり、Dem_ReportErrorStatus() への入力としては受け付けない
 * （Dem が内部で導出し、ログ出力にのみ使用する）。
 */
typedef enum
{
    DEM_EVENT_STATUS_PASSED    = 0U,  /**< テスト合格 (モニタが報告する生の結果)      */
    DEM_EVENT_STATUS_FAILED    = 1U,  /**< テスト失敗 (モニタが報告する生の結果)      */
    DEM_EVENT_STATUS_PREPASSED = 2U,  /**< デバウンス中 (PASSED 方向、未確定)         */
    DEM_EVENT_STATUS_PREFAILED = 3U   /**< デバウンス中 (FAILED 方向、未確定)         */
} Dem_EventStatusType;

/**
 * \brief   FreezeFrame (故障時スナップショット) のデータ構造。
 *
 * \details DID 0x0101 (EngineSpeed) / 0x0102 (CoolantTemp) / 0x0103 (EngineState)
 *          に対応する 3 項目を固定フォーマットで保持する。
 *          AUTOSAR では FreezeFrameClass で項目を ARXML 設定するが、
 *          本実装は学習用簡略化のため固定 3 項目とする。
 *          RAM のみに保持し、EEPROM へは永続化しない（電源 OFF で消去）。
 */
typedef struct
{
    uint16 EngineSpeed;   /**< DID 0x0101 相当のスナップショット値 (rpm) */
    uint8  CoolantTemp;   /**< DID 0x0102 相当のスナップショット値 (℃)  */
    uint8  EngineState;   /**< DID 0x0103 相当のスナップショット値      */
} Dem_FreezeFrameType;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   DEM を初期化する。
 * \details EEPROM のマジックバイトを確認し、有効なら前回の DTC ステータスを
 *          復元する。初回起動時は全イベントを初期状態にして EEPROM を書き込む。
 *
 * \pre        EcuM_Init() から、Com_Init() の後に呼び出すこと。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_Init(void);

/**
 * \brief   イベントの発生/消滅を DEM に通知する (モニタからの生のテスト結果)。
 * \details FAILED/PASSED の報告でデバウンスカウンタを ±1 し、カウンタが
 *          イベントごとの確定閾値（Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*）に
 *          達した瞬間にのみ DTC ステータス (TF/PDTC/CDTC/TFSLC) を確定して
 *          EEPROM へ書き込む（学習用の counter-based debouncing）。
 *
 * \param[in]  EventId      イベント ID (DEM_EVENT_* 定数)。
 * \param[in]  EventStatus  DEM_EVENT_STATUS_FAILED または DEM_EVENT_STATUS_PASSED。
 *                          PREPASSED / PREFAILED は受け付けない。
 *
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_ReportErrorStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);

/**
 * \brief   指定イベントの DTC ステータスバイトを返す。
 *
 * \param[in]  EventId  イベント ID (DEM_EVENT_* 定数)。
 *
 * \return  statusAvailabilityMask でマスクされたステータスバイト。
 *          EventId が範囲外の場合は 0。
 *
 * \ServiceID      {0x19}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Dem_GetStatusOfEvent(Dem_EventIdType EventId);

/**
 * \brief   イベント ID から DTC コードを取得する。
 *
 * \param[in]   EventId  イベント ID (DEM_EVENT_* 定数)。
 * \param[out]  DTC      24-bit DTC コードの格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  EventId が範囲外、または DTC が NULL。
 *
 * \ServiceID      {0x1A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetDTCOfEvent(Dem_EventIdType EventId, uint32* DTC);

/**
 * \brief   全 DTC をクリアし、EEPROM を初期状態へ戻す。
 * \details DCM SID 0x14 (ClearDiagnosticInformation) から呼び出す。
 *          全イベントのステータスを TNCLC | TNCTOC にリセットする。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0x23}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ClearAllDTCs(void);

/**
 * \brief   指定イベントの DTC のみをクリアし、EEPROM へ反映する。
 * \details DCM SID 0x14 (ClearDiagnosticInformation) のグループ指定クリア
 *          (特定の DTC コードのみを指定するケース) から呼び出す。
 *          対象イベントのステータスを TNCLC | TNCTOC にリセットし、
 *          デバウンスカウンタと FreezeFrame も未記録状態に戻す。
 *
 * \param[in]  EventId  イベント ID (DEM_EVENT_* 定数)。
 *
 * \retval  E_OK      正常クリア。
 * \retval  E_NOT_OK  EventId が範囲外。
 *
 * \ServiceID      {0x28}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ClearDTC(Dem_EventIdType EventId);

/**
 * \brief   ステータスマスクに一致する全 DTC を列挙する。
 * \details DCM SID 0x19 サブ機能 0x01 / 0x02 から呼び出す。
 *
 * \param[out]  dtcBuf     DTC コード (24-bit) の格納先。DEM_EVENT_COUNT 要素以上。
 * \param[out]  statusBuf  DTC ステータスバイトの格納先。同サイズ。
 * \param[out]  count      マッチした DTC 数。
 * \param[in]   statusMask 絞り込みマスク。0xFF で全件取得。
 *
 * \ServiceID      {0x24}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_GetAllDTCs(uint32* dtcBuf, uint8* statusBuf, uint8* count, uint8 statusMask);

/**
 * \brief   FreezeFrame として保存する現在値を更新する。
 *
 * \details SW-C (App_EngineManager) が周期 Runnable の先頭で毎回呼び出し、
 *          「現在の車両状態」を Dem 内部に保持させる。
 *          Dem_ReportErrorStatus() が FAILED 遷移を検出した瞬間に、
 *          この値をイベントごとのスナップショットとしてコピーする
 *          (リアルタイムに値を読みに行くのではなく、毎周期の最新値を使う点が
 *          学習用簡略化。AUTOSAR では FreezeFrameClass の DataElement を
 *          Dem が都度読み出す)。
 *
 * \param[in]  EngineSpeed  現在のエンジン回転数 (DID 0x0101 相当)。
 * \param[in]  CoolantTemp  現在の冷却水温 (DID 0x0102 相当)。
 * \param[in]  EngineState  現在のエンジン状態 (DID 0x0103 相当)。
 *
 * \ServiceID      {0x25}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_SetFreezeFrameContext(uint16 EngineSpeed, uint8 CoolantTemp, uint8 EngineState);

/**
 * \brief   指定イベントに保存された FreezeFrame を取得する。
 *
 * \details DCM SID 0x19 subFunc 0x04 (reportDTCSnapshotRecordByDTCNumber) から
 *          呼び出す。一度も FAILED 報告されていないイベントには記録がない。
 *
 * \param[in]   EventId  イベント ID (DEM_EVENT_* 定数)。
 * \param[out]  Frame    取得した FreezeFrame の格納先。NULL 禁止。
 *
 * \retval  E_OK      記録あり。
 * \retval  E_NOT_OK  EventId が範囲外、Frame が NULL、または記録なし。
 *
 * \ServiceID      {0x26}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetFreezeFrameOfEvent(Dem_EventIdType EventId, Dem_FreezeFrameType* Frame);

/**
 * \brief   DTC コード (24-bit) から EventId を逆引きする。
 *
 * \details DCM SID 0x19 subFunc 0x04 はリクエストに DTC コードを含むため、
 *          内部処理用の EventId へ変換する必要がある。
 *
 * \param[in]   DTC      24-bit DTC コード。
 * \param[out]  EventId  対応するイベント ID の格納先。NULL 禁止。
 *
 * \retval  E_OK      一致する DTC が見つかった。
 * \retval  E_NOT_OK  一致する DTC がない、または EventId が NULL。
 *
 * \ServiceID      {0x27}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetEventIdOfDTC(uint32 DTC, Dem_EventIdType* EventId);

/**
 * \brief   指定イベントの ExtendedData（故障確定回数）を取得する。
 *
 * \details DCM SID 0x19 subFunc 0x06 (reportExtendedDataRecordByDTCNumber) から
 *          呼び出す。FreezeFrame（故障時点のスナップショット）とは異なり、
 *          これまでに確定 FAILED した累積回数を返す（0xFF で飽和）。
 *          一度も確定 FAILED していないイベントは 0 を返す（E_OK のまま）。
 *
 * \param[in]   EventId   イベント ID (DEM_EVENT_* 定数)。
 * \param[out]  Counter   故障確定回数の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  EventId が範囲外、または Counter が NULL。
 *
 * \ServiceID      {0x29}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetOccurrenceCounterOfEvent(Dem_EventIdType EventId, uint8* Counter);

#ifdef __cplusplus
}
#endif

#endif /* DEM_H */
