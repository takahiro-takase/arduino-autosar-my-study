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
 *          DCM は Dem_GetAllDTCs() / Dem_ClearAllDTCs() 経由で
 *          UDS SID 0x19 / 0x14 に応答する。
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
 * PASSED / FAILED のみ実装する。
 * PRE* はデバウンスカウンタが必要なため本実装の範囲外。
 */
typedef enum
{
    DEM_EVENT_STATUS_PASSED    = 0U,  /**< テスト合格 — TF ビットをクリア           */
    DEM_EVENT_STATUS_FAILED    = 1U,  /**< テスト失敗 — DTC を記録・EEPROM 保存     */
    DEM_EVENT_STATUS_PREPASSED = 2U,  /**< 合格中 (デバウンス) — 本実装では未対応   */
    DEM_EVENT_STATUS_PREFAILED = 3U   /**< 失敗中 (デバウンス) — 本実装では未対応   */
} Dem_EventStatusType;

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
 * \brief   イベントの発生/消滅を DEM に通知する。
 * \details FAILED 通知で DTC ステータスの TF/PDTC/CDTC/TFSLC を設定し
 *          EEPROM へ書き込む。PASSED 通知で TF をクリアするが CDTC は保持する。
 *          ステータスが変化した場合のみ EEPROM を更新する。
 *
 * \param[in]  EventId      イベント ID (DEM_EVENT_* 定数)。
 * \param[in]  EventStatus  DEM_EVENT_STATUS_FAILED または DEM_EVENT_STATUS_PASSED。
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

#ifdef __cplusplus
}
#endif

#endif /* DEM_H */
