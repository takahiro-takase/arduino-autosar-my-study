/**
 * \file    Dem.h
 * \brief   診断イベントマネージャ 公開インタフェース (AUTOSAR SWS_DEM 準拠)
 * \details アプリケーション SW-C および BSW モジュールが診断イベントを
 *          報告するための API を公開する。
 *
 *          エラー検出コンポーネント (App_EngineManager 等) は
 *          Dem_SetEventStatus() でイベントの発生・消滅を通知する。
 *          DEM は DTC ライフサイクル (PENDING → CONFIRMED → STORED) を
 *          管理し、EEPROM へ永続化する。
 *          CONFIRMED した DTC は、再故障せずに複数回の操作サイクル（起動〜次回
 *          起動）を経ると Dem_Init() が経年回復 (Aging) を判定し自動的に
 *          CONFIRMED を解除する（詳細は Dem.c / Dem_Cfg.h を参照）。
 *          DCM は Dem_SetDTCFilter() / Dem_GetNextFilteredDTC() / Dem_ClearDTC() 経由で UDS SID 0x19 / 0x14
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

/** UDS DTC ステータスバイト型（[SWS_Dem_00928]、AUTOSAR では Bitfield/uint8
 *  として定義される。本実装の `Dem_StatusTable[]` の要素と同じビット配置）。 */
typedef uint8 Dem_UdsStatusByteType;

/**
 * \brief   DTC 値のフォーマットを選択する型（[SWS_Dem_00933]）。
 * \details 本プロジェクトの `Dem_DtcTable[]`（Dem_Cfg.h）は UDS 3-byte 形式の
 *          DTC 値のみを保持し、OBD/J1939 形式は一切構成していないため、
 *          `Dem_GetDTCOfEvent()` は `DEM_DTC_FORMAT_UDS` 以外を
 *          `DEM_E_NO_DTC_AVAILABLE` で拒否する（実仕様の Return value 表が
 *          想定する「要求フォーマットの DTC が設定されていない」ケースそのもの）。
 */
typedef enum
{
    DEM_DTC_FORMAT_OBD   = 0U, /**< 2-byte OBD DTC 形式（本プロジェクトは非対応）        */
    DEM_DTC_FORMAT_UDS   = 1U, /**< 3-byte UDS DTC 形式（本プロジェクトが唯一対応する形式）*/
    DEM_DTC_FORMAT_J1939 = 2U  /**< SPN+FMI を合成した 3-byte J1939 形式（本プロジェクトは非対応）*/
} Dem_DTCFormatType;

/**
 * \brief   イベントメモリ（DTC の格納先）を指定する型（[SWS_Dem_00775]、
 *          `Dem_ClearDTC()` 等が対象メモリを選ぶために使う）。
 * \details 本プロジェクトはイベントメモリを1つ（プライマリメモリ相当、
 *          `Dem_StatusTable[]` 等）しか持たないため、`Dem_ClearDTC()` は
 *          `DEM_DTC_ORIGIN_PRIMARY_MEMORY` 以外を拒否する。値そのものは
 *          実仕様 [SWS_Dem_00775] の定義をそのまま使う。
 */
typedef enum
{
    DEM_DTC_ORIGIN_PRIMARY_MEMORY   = 0x0001U, /**< 本プロジェクトが唯一構成するイベントメモリ */
    DEM_DTC_ORIGIN_MIRROR_MEMORY    = 0x0002U, /**< ミラーメモリ（本プロジェクトは非対応）      */
    DEM_DTC_ORIGIN_PERMANENT_MEMORY = 0x0003U  /**< 永続メモリ（本プロジェクトは非対応）        */
} Dem_DTCOriginType;

/**
 * \brief   DTC 翻訳フォーマット型（[SWS_Dem_00936]、`Dem_GetTranslationType()`
 *          の戻り値）。DTC 値自体の表現形式を選ぶ `Dem_DTCFormatType` とは
 *          別の型・値域である点に注意。
 * \details 本プロジェクトは `DemTypeOfDTCSupported` として ISO 14229-1 形式
 *          のみを構成するため、`DEM_DTC_TRANSLATION_ISO14229_1` 以外が
 *          返ることはない。
 */
typedef enum
{
    DEM_DTC_TRANSLATION_ISO15031_6              = 0U, /**< ISO15031-6 / SAE J2012-DA_DTCFormat_00（本プロジェクトは非対応）*/
    DEM_DTC_TRANSLATION_ISO14229_1              = 1U, /**< ISO 14229-1 DTC 形式（本プロジェクトが唯一構成する形式）*/
    DEM_DTC_TRANSLATION_SAEJ1939_73             = 2U, /**< SAE J1939-73 DTC 形式（本プロジェクトは非対応）*/
    DEM_DTC_TRANSLATION_ISO11992_4              = 3U, /**< ISO 11992-4 DTC 形式（本プロジェクトは非対応）*/
    DEM_DTC_TRANSLATION_SAE_J2012_DA_DTCFORMAT_04 = 4U /**< SAE J2012-DA_DTCFormat_04（本プロジェクトは非対応）*/
} Dem_DTCTranslationFormatType;

/** [SWS_Dem_00198] `Dem_GetDTCOfEvent()` の拡張戻り値（要求フォーマットに
 *  対応する DTC が構成されていない場合）。実仕様の Service Interface
 *  DiagnosticInfo（値表）に基づく数値。Dem_Cfg.h の `DEM_E_*`（Det_ReportError
 *  に渡す開発エラー ID）とは別の値域（本関数の戻り値そのもの）である点に注意。 */
#define DEM_E_NO_DTC_AVAILABLE  0x0AU

/**
 * \brief   イベントステータス型 (Dem_EventStatusType は AUTOSAR SWS_Dem_00926 で定義)
 *
 * \details 本実装は counter-based debouncing の学習用簡略版。AUTOSAR 仕様では
 *          PREPASSED/PREFAILED もモニタが報告してよい正当な入力であり
 *          （SWS_Dem_00418/00419: 報告のたびにカウンタを step-size 分だけ増減させる、
 *          段階的な debounce 進行を表す値）、逆に本来の FAILED/PASSED
 *          （SWS_Dem_00420/00421）は「モニタ側で既に確定した単発の結果」を意味し、
 *          報告されるとカウンタを閾値へ直接ジャンプさせて即座に確定させる
 *          （繰り返し報告してカウントする値ではない）。
 *
 *          本実装はこれと異なり、モニタが毎回 FAILED/PASSED（1 周期分の生の判定）を
 *          報告し、Dem 側がその報告回数をカウントして閾値到達時に確定するという、
 *          仕様上は PREFAILED/PREPASSED に割り当てられた段階的カウント挙動に近い方式を
 *          FAILED/PASSED の名前のまま採用している。PREPASSED/PREFAILED 自体は
 *          Dem_SetEventStatus() への入力として受け付けない
 *          （AUTOSAR がこれらを入力不可と規定しているのではなく、本プロジェクト独自の
 *          簡略化。詳細は Dem.c の実装コメント参照）。
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
 * \brief   Dem_Init() の設定引数型（不透明型）。
 *
 * \details SWS_Dem_00181 は post-build 設定データへのポインタを要求するが、
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を持たない
 *          ため、中身を定義しない不透明型とし、ポインタとしてのみ扱う
 *          （`KeyM_ConfigType` と同じ簡略化パターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct Dem_ConfigType_Tag Dem_ConfigType;

/**
 * \brief   DEM を初期化する。
 * \details EEPROM のマジックバイトを確認し、有効なら前回の DTC ステータスを
 *          復元する。初回起動時は全イベントを初期状態にして EEPROM を書き込む。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                        持たないため）。
 *
 * \pre        EcuM_Init() から、Com_Init() の後に呼び出すこと。
 *
 * \note    ServiceID 0x01 は実仕様では未実装の `Dem_PreInit`（[SWS_Dem_00180]）
 *          に割り当てられており、本関数の正しい ServiceID は 0x02 である。
 *
 * \AUTOSARReq     {SWS_Dem_00181}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_Init(const Dem_ConfigType* ConfigPtr);

/**
 * \brief   イベントの発生/消滅を DEM に通知する (モニタからの生のテスト結果、
 *          [SWS_Dem_00183])。
 * \details FAILED/PASSED の報告でデバウンスカウンタを ±1 し、カウンタが
 *          イベントごとの確定閾値（Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*）に
 *          達した瞬間にのみ DTC ステータス (TF/PDTC/CDTC/TFSLC) を確定して
 *          EEPROM へ書き込む（学習用の counter-based debouncing）。
 *
 * \param[in]  EventId      イベント ID (DEM_EVENT_* 定数)。
 * \param[in]  EventStatus  DEM_EVENT_STATUS_FAILED または DEM_EVENT_STATUS_PASSED。
 *                          PREPASSED / PREFAILED は受け付けない。
 *
 * \retval  E_OK      正常に受理した（デバウンス未確定・DTC設定無効化中の
 *                    無視も含む。実仕様も「呼び出し元のBSWモジュールは戻り値を
 *                    無視してよい」と明記しているため、実質的な失敗のみを
 *                    E_NOT_OK とする）。
 * \retval  E_NOT_OK  EventId が範囲外、または EventStatus が
 *                    DEM_EVENT_STATUS_FAILED/PASSED 以外。
 *
 * \note    実仕様は「異なる EventId 間では Reentrant」と規定するが、本実装は
 *          確定 (デバウンス閾値到達) の都度 `Dem_StatusTable[]`/
 *          `Dem_OccurrenceCounter[]` を配列丸ごと `NvM_WriteBlock()` するため、
 *          異なる EventId への同時呼び出しでも書き込みが競合しうる
 *          （実仕様のようにイベント単位でキュー分離していないための制約）。
 *          そのため本実装は Non Reentrant のまま扱う。
 *
 * \AUTOSARReq     {SWS_Dem_00183}
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);

/**
 * \brief   DTC ステータス availability mask（サポートするステータスビット）を取得する。
 *
 * \details DCM SID 0x19 の各種応答([SWS_Dem_00060]: statusAvailabilityMask
 *          フィールド)が使う値を返す。本 ECU は ISO 14229-1 Annex B の
 *          bit0(testFailed)/bit2(pendingDTC)/bit3(confirmedDTC)/
 *          bit4(testNotCompletedSinceLastClear)/bit5(testFailedSinceLastClear)
 *          をサポートする。
 *
 * \note    シグネチャは実 AUTOSAR の `ClientId` 引数を含めて仕様準拠とする
 *          （方針: IF は仕様準拠、内部動作は Arduino で実現可能な範囲に
 *          簡略化）。本プロジェクトは単一 ECU・単一診断クライアント構成の
 *          ため、`ClientId` の値は内部で無視する（クライアントごとの
 *          個別状態は持たない）。
 *
 * \param[in]   ClientId       呼び出し元のクライアント識別子。本実装では未使用。
 * \param[out]  DTCStatusMask  取得したマスクの格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  DTCStatusMask が NULL。
 *
 * \AUTOSARReq     {SWS_Dem_00213}
 * \ServiceID      {0x16}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for same ClientId}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetDTCStatusAvailabilityMask(uint8 ClientId, Dem_UdsStatusByteType* DTCStatusMask);

/**
 * \brief   指定イベントの UDS DTC ステータスバイトを取得する（[SWS_Dem_91008]）。
 *
 * \details 実仕様は本関数を SW-C や FiM 等の BSW モジュールがイベント単位で
 *          使うためのものと位置づけ、Dcm は DTC 単位の `Dem_GetStatusOfDTC`
 *          を使うと規定する（同関数の Note 参照）。本プロジェクトは
 *          `Dem_GetStatusOfDTC`（DTC→EventId 変換を内蔵する別 API）を持たず、
 *          Dcm 側は既に解決済みの EventId で直接本関数を呼ぶ既存の簡略化を
 *          そのまま踏襲する（本関数の改名以前から変わらない設計）。
 *
 * \param[in]   EventId        イベント ID (DEM_EVENT_* 定数)。
 * \param[out]  UDSStatusByte  取得したステータスバイト（statusAvailabilityMask
 *                             でマスク済み）の格納先。NULL 禁止。
 *                             戻り値が E_NOT_OK の場合は不定。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  未初期化、EventId が範囲外、または UDSStatusByte が NULL。
 *
 * \AUTOSARReq     {SWS_Dem_91008, SWS_Dem_00051}
 * \ServiceID      {0xb6}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetEventUdsStatus(Dem_EventIdType EventId, Dem_UdsStatusByteType* UDSStatusByte);

/**
 * \brief   イベント ID から DTC コードを取得する（[SWS_Dem_00198]/[SWS_Dem_00269]）。
 *
 * \details 本プロジェクトの `Dem_DtcTable[]`（Dem_Cfg.h）は UDS 3-byte 形式の
 *          DTC 値のみを構成しており、OBD/J1939 形式は一切保持しないため、
 *          `DTCFormat` に `DEM_DTC_FORMAT_UDS` 以外を渡された場合は
 *          `DEM_E_NO_DTC_AVAILABLE`（要求フォーマットの DTC が構成されていない）
 *          を返す。
 *
 * \param[in]   EventId     イベント ID (DEM_EVENT_* 定数)。
 * \param[in]   DTCFormat   取得する DTC 値のフォーマット。本プロジェクトは
 *                          `DEM_DTC_FORMAT_UDS` のみ対応。
 * \param[out]  DTCOfEvent  24-bit DTC コードの格納先。NULL 禁止。
 *                          戻り値が E_OK 以外の場合は不定。
 *
 * \retval  E_OK                   正常取得。
 * \retval  E_NOT_OK               未初期化、EventId が範囲外、または
 *                                 DTCOfEvent が NULL。
 * \retval  DEM_E_NO_DTC_AVAILABLE DTCFormat が DEM_DTC_FORMAT_UDS 以外
 *                                 （要求フォーマットの DTC は構成されていない）。
 *
 * \AUTOSARReq     {SWS_Dem_00198, SWS_Dem_00269}
 * \ServiceID      {0x0D}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetDTCOfEvent(Dem_EventIdType EventId, Dem_DTCFormatType DTCFormat, uint32* DTCOfEvent);

/**
 * \brief   指定 DTC（または全 DTC）をクリアし、EEPROM を初期状態へ戻す
 *          （[SWS_Dem_00665]）。DCM SID 0x14 (ClearDiagnosticInformation) から
 *          呼び出す。
 *
 * \details 実仕様は `Dem_SelectDTC(ClientId, DTC, DTCFormat, DTCOrigin)`
 *          [SWS_Dem_91016] で対象 DTC を選択したうえで `Dem_ClearDTC(ClientId)`
 *          [SWS_Dem_00665] を呼ぶ、非同期対応の2段階プロトコル（複数クライアント・
 *          `DEM_PENDING`/`DEM_BUSY` によるポーリングを前提とする）だが、本プロジェクトは
 *          単一クライアント・常に同期処理のみのため、`Dem_SelectDTC()` 側の3引数
 *          （DTC/DTCFormat/DTCOrigin）を本関数に統合し、選択と同時にクリアまで
 *          一度に行う（2026-09-27 是正: 従来の独自関数 `Dem_ClearAllDTCs`/
 *          `Dem_ClearOneDtc` を本関数へ統合し、衝突回避のため避けていた実仕様の
 *          関数名 `Dem_ClearDTC` と ServiceID 0x23 を採用した。Dem_Cfg.h
 *          冒頭コメント参照）。
 *
 *          `DTC` に予約値 `DEM_GROUP_ALL_DTCS`（0xFFFFFF、ISO 14229-1 Annex D.1
 *          の "all DTCs" グループ）を渡すと、実仕様 [SWS_Dem_01203] のとおり
 *          `DTCOrigin` が示すメモリの全イベントをクリアする。それ以外の値は
 *          特定の DTC コードとみなし、`Dem_GetEventIdOfDTC()` で一致する
 *          イベントを探して 1 件だけクリアする（一致するイベントが無ければ
 *          E_NOT_OK）。
 *
 * \param[in]  ClientId   呼び出し元のクライアント識別子。本実装では未使用。
 * \param[in]  DTC        クリア対象の DTC（24-bit UDS 形式）、または
 *                        `DEM_GROUP_ALL_DTCS`（全 DTC 一括クリア）。
 * \param[in]  DTCFormat  DTC のフォーマット。本プロジェクトは
 *                        `DEM_DTC_FORMAT_UDS` のみ対応。
 * \param[in]  DTCOrigin  クリア対象のイベントメモリ。本プロジェクトは
 *                        `DEM_DTC_ORIGIN_PRIMARY_MEMORY` のみ構成する。
 *
 * \retval  E_OK                   正常クリア。
 * \retval  E_NOT_OK               未初期化、DTCOrigin が非対応、または DTC
 *                                 に一致するイベントが無い。
 * \retval  DEM_E_NO_DTC_AVAILABLE DTCFormat が DEM_DTC_FORMAT_UDS 以外
 *                                 （要求フォーマットの DTC は構成されていない）。
 *
 * \AUTOSARReq     {SWS_Dem_00665, SWS_Dem_91016, SWS_Dem_01203}
 * \ServiceID      {0x23}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ClearDTC(uint8 ClientId, uint32 DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin);

/** [SWS_Dem_00057] Dem_SetDTCFilter() の DTCSeverityMask の型（uint8）。
 *  本プロジェクトは DTC の Severity を持たないため、FilterWithSeverity=TRUE は
 *  受け付けない。 */
typedef uint8 Dem_DTCSeverityType;

/** [SWS_Dem_00215] Dem_GetNextFilteredDTC() 系の戻り値: フィルタ条件に一致する
 *  次の要素が無い。実仕様の Service Interface DiagnosticInfo（値表、
 *  DEM_NO_SUCH_ELEMENT = 48）に基づく数値。 */
#define DEM_NO_SUCH_ELEMENT  48U

/**
 * \brief   DTC フィルタ条件を設定する（[SWS_Dem_00208]）。
 *
 * \details 以降の Dem_GetNumberOfFilteredDTC() / Dem_GetNextFilteredDTC() /
 *          Dem_GetNextFilteredDTCAndFDC() が、この条件に一致する DTC だけを
 *          対象にする（[SWS_Dem_00057]）。条件は次回の本関数呼び出しまで有効で、
 *          呼び出すたびに「次に返す DTC」の位置が先頭へ戻る。
 *
 *          `DTCStatusMask` が 0x00 のときはステータスバイトによる絞り込みを
 *          しない（本 ECU が対応する全 DTC を対象にする。UDS 0x19/0x0A 用）。
 *          0x01〜0xFF のときは `(statusOfDTC & DTCStatusMask) != 0` の DTC のみ。
 *          `FilterForFaultDetectionCounter` が TRUE のときは、Fault Detection
 *          Counter が 1〜0x7E（prefailed）の DTC のみを対象にする
 *          （UDS 0x19/0x14 用。値は Dem_GetFaultDetectionCounter() と同じ写像）。
 *
 *          本プロジェクトは単一ECU・単一診断クライアント構成のため、フィルタ条件は
 *          1 組だけ保持し、`ClientId` は区別に使わない。また Severity を持たない
 *          ため `FilterWithSeverity` に TRUE を渡された場合は拒否する。
 *
 * \param[in]  ClientId                       クライアント識別子。本実装では未使用。
 * \param[in]  DTCStatusMask                  ステータスバイトのマスク（0x00=絞り込みなし）。
 * \param[in]  DTCFormat                      `DEM_DTC_FORMAT_UDS` のみ対応。
 * \param[in]  DTCOrigin                      `DEM_DTC_ORIGIN_PRIMARY_MEMORY` のみ構成する。
 * \param[in]  FilterWithSeverity             TRUE は非対応（Severity を持たない）。
 * \param[in]  DTCSeverityMask                FilterWithSeverity が FALSE のとき無視する。
 * \param[in]  FilterForFaultDetectionCounter TRUE で prefailed の DTC のみを対象にする。
 *
 * \retval  E_OK      フィルタを設定した。
 * \retval  E_NOT_OK  未初期化、または DTCFormat / DTCOrigin / FilterWithSeverity が
 *                    非対応の値（後 3 者は DEM_E_WRONG_CONFIGURATION も報告する）。
 *
 * \AUTOSARReq     {SWS_Dem_00208, SWS_Dem_00057}
 * \ServiceID      {0x13}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SetDTCFilter(uint8 ClientId, uint8 DTCStatusMask, Dem_DTCFormatType DTCFormat,
                                Dem_DTCOriginType DTCOrigin, boolean FilterWithSeverity,
                                Dem_DTCSeverityType DTCSeverityMask, boolean FilterForFaultDetectionCounter);

/**
 * \brief   現在のフィルタ条件に一致する DTC の件数を取得する（[SWS_Dem_00214]）。
 *
 * \details 「次に返す DTC」の位置は進めない（件数を数えるだけ）。常に同期的に
 *          完了する（DEM_PENDING は返さない）。
 *
 * \param[in]   ClientId             クライアント識別子。本実装では未使用。
 * \param[out]  NumberOfFilteredDTC  一致した DTC の件数。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  未初期化、NumberOfFilteredDTC が NULL、または Dem_SetDTCFilter()
 *                    でフィルタが設定されていない。
 *
 * \AUTOSARReq     {SWS_Dem_00214}
 * \ServiceID      {0x17}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetNumberOfFilteredDTC(uint8 ClientId, uint16* NumberOfFilteredDTC);

/**
 * \brief   フィルタ条件に一致する次の DTC とそのステータスを取得する（[SWS_Dem_00215]）。
 *
 * \details 呼ぶたびに一致する DTC を 1 件ずつ返し、全件返し終えたら
 *          DEM_NO_SUCH_ELEMENT を返す。常に同期的に完了する（DEM_PENDING は返さない）。
 *
 * \param[in]   ClientId   クライアント識別子。本実装では未使用。
 * \param[out]  DTC        DTC 値（UDS 形式、24-bit）。NULL 禁止。E_OK 以外のときは不定。
 * \param[out]  DTCStatus  DTC のステータスバイト。NULL 禁止。E_OK 以外のときは不定。
 *
 * \retval  E_OK                次の DTC を返した。
 * \retval  E_NOT_OK            未初期化、ポインタが NULL、またはフィルタが設定されていない。
 * \retval  DEM_NO_SUCH_ELEMENT 一致する DTC をすべて返し終えた。
 *
 * \AUTOSARReq     {SWS_Dem_00215}
 * \ServiceID      {0x18}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Synchronous（本実装は DEM_PENDING を返さない）}
 */
Std_ReturnType Dem_GetNextFilteredDTC(uint8 ClientId, uint32* DTC, uint8* DTCStatus);

/**
 * \brief   フィルタ条件に一致する次の DTC と Fault Detection Counter を取得する
 *          （[SWS_Dem_00227]）。
 *
 * \details Dem_GetNextFilteredDTC() と同じ要領で 1 件ずつ返す。FDC は
 *          Dem_GetFaultDetectionCounter() と同じ写像（-128〜127）。
 *          UDS 0x19/0x14 では Dem_SetDTCFilter() に
 *          FilterForFaultDetectionCounter=TRUE を渡して prefailed のみに絞る。
 *
 * \param[in]   ClientId                 クライアント識別子。本実装では未使用。
 * \param[out]  DTC                      DTC 値（UDS 形式、24-bit）。NULL 禁止。
 * \param[out]  DTCFaultDetectionCounter Fault Detection Counter。NULL 禁止。
 *
 * \retval  E_OK                次の DTC を返した。
 * \retval  E_NOT_OK            未初期化、ポインタが NULL、またはフィルタが設定されていない。
 * \retval  DEM_NO_SUCH_ELEMENT 一致する DTC をすべて返し終えた。
 *
 * \AUTOSARReq     {SWS_Dem_00227}
 * \ServiceID      {0x3b}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetNextFilteredDTCAndFDC(uint8 ClientId, uint32* DTC, sint8* DTCFaultDetectionCounter);

/**
 * \brief   FreezeFrame として保存する現在値を更新する。
 *
 * \details SW-C (App_EngineManager) が周期 Runnable の先頭で毎回呼び出し、
 *          「現在の車両状態」を Dem 内部に保持させる。
 *          Dem_SetEventStatus() が FAILED 遷移を検出した瞬間に、
 *          この値をイベントごとのスナップショットとしてコピーする
 *          (リアルタイムに値を読みに行くのではなく、毎周期の最新値を使う点が
 *          学習用簡略化。AUTOSAR では FreezeFrameClass の DataElement を
 *          Dem が都度読み出す)。
 *
 * \param[in]  EngineSpeed  現在のエンジン回転数 (DID 0x0101 相当)。
 * \param[in]  CoolantTemp  現在の冷却水温 (DID 0x0102 相当)。
 * \param[in]  EngineState  現在のエンジン状態 (DID 0x0103 相当)。
 *
 * \note    本プロジェクト独自の関数（実 AUTOSAR に対応する関数は無い）のため
 *          ApiId は任意の値。Dem_GetAllDTCs と同じ経緯で 0x25 から 0x2C へ
 *          移した（Dem_Cfg.h 冒頭コメント参照）。
 *
 * \ServiceID      {0x2C}
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
 *          一度も確定 FAILED していない（=ExtendedData 未記録の）イベントは
 *          E_NOT_OK を返す（[SWS_Dcm_01242]相当、2026-09 是正: 以前は常に
 *          E_OK・カウンタ 0 を返しており、唯一の呼び出し元 Dcm 側が
 *          誤って正応答を返してしまっていた）。
 *
 * \param[in]   EventId   イベント ID (DEM_EVENT_* 定数)。
 * \param[out]  Counter   故障確定回数の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得（1 回以上確定 FAILED 済み）。
 * \retval  E_NOT_OK  EventId が範囲外、Counter が NULL、または ExtendedData
 *                    未記録（一度も確定 FAILED していない）。
 *
 * \note    本プロジェクト独自の関数（実 AUTOSAR に対応する関数は無い）のため
 *          ApiId は任意の値のはずだったが、以前の 0x29 は実仕様の
 *          `Dem_GetIndicatorStatus`（[SWS_Dem_00205]）と衝突していたことが
 *          判明したため、実仕様のどの Dem 関数の Service ID とも一致しない
 *          ことを確認済みの 0x2E へ 2026-09-06 に付け替えた（Dem_Cfg.h
 *          冒頭コメント参照）。
 *
 * \ServiceID      {0x2E}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetOccurrenceCounterOfEvent(Dem_EventIdType EventId, uint8* Counter);

/**
 * \brief   本 ECU が構成する DTC 翻訳フォーマットを取得する。
 *
 * \details DCM SID 0x19 subFunc 0x01（reportNumberOfDTCByStatusMask）の
 *          応答に含まれる DTCFormatIdentifier バイトの取得元
 *          （[SWS_Dcm_00376]、Dcm.c 参照）。エラー戻り値の定義が
 *          実仕様に存在しないため
 *          （[SWS_Dem_00231]）、ClientId・初期化状態に関わらず常に
 *          `DEM_DTC_TRANSLATION_ISO14229_1` を返す（本プロジェクトが
 *          `DemTypeOfDTCSupported` として構成する唯一の形式）。
 *
 * \param[in]  ClientId  クライアント ID。単一診断クライアント構成のため未使用。
 *
 * \return  構成済み DTC 翻訳フォーマット。
 *
 * \AUTOSARReq     {SWS_Dem_00230}
 * \AUTOSARReq     {SWS_Dem_00231}
 * \ServiceID      {0x3c}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Dem_DTCTranslationFormatType Dem_GetTranslationType(uint8 ClientId);

/**
 * \brief   指定イベントの Fault Detection Counter を取得する。
 *
 * \details DCM SID 0x19 subFunc 0x14 (reportDTCFaultDetectionCounter) から
 *          呼び出す。内部の `Dem_DebounceCounter[]`（-limit〜+limit、limit は
 *          イベントごとの `Dem_DebounceLimitTable[]`）を、ISO 14229-1 に従い
 *          -128(PASSED 側に最も振れた状態)〜127(FAILED 側に最も振れた状態)の
 *          範囲へ線形写像して返す（[SWS_Dem_00415]、`Dem_MapDebounceCounterToFdc()`
 *          参照）。
 *
 * \param[in]   EventId               イベント ID (DEM_EVENT_* 定数)。
 * \param[out]  FaultDetectionCounter 写像後の Fault Detection Counter の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  未初期化、EventId が範囲外、または FaultDetectionCounter が NULL。
 *
 * \AUTOSARReq     {SWS_Dem_00203}
 * \AUTOSARReq     {SWS_Dem_00415}
 * \ServiceID      {0x3e}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetFaultDetectionCounter(Dem_EventIdType EventId, sint8* FaultDetectionCounter);

/**
 * \brief   DTC の記録（error memory への反映）を有効化する。
 *
 * \details Dcm UDS SID 0x85 ControlDTCSetting のサブ機能 0x01 (on) から呼び出す
 *          （[SWS_Dcm_01063]）。既定で有効。無効化中に Dem_SetEventStatus()
 *          が呼ばれても、本関数で再度有効化するまでデバウンス・DTC ステータス
 *          ともに一切変化しない（Dem_DisableDTCSetting() 参照）。
 *
 * \note    シグネチャは実 AUTOSAR の `ClientId` 引数・`Std_ReturnType` 戻り値を
 *          含めて仕様準拠とする（方針: IF は仕様準拠、内部動作は Arduino で
 *          実現可能な範囲に簡略化。以前は `void Dem_EnableDTCSetting(void)`
 *          という独自簡略シグネチャだったが、シグネチャ準拠方針のもと修正）。
 *          本プロジェクトは単一 ECU・単一診断クライアント構成のため
 *          `ClientId` は内部で無視し、非同期ジョブキューを持たないため
 *          常に同期的に完了する（`DEM_PENDING` を返すことはない）。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子。本実装では未使用。
 *
 * \retval  E_OK  常に成功（同期完了）。
 *
 * \AUTOSARReq     {SWS_Dem_00243}
 * \ServiceID      {0x25}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for same ClientId}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_EnableDTCSetting(uint8 ClientId);

/**
 * \brief   DTC の記録（error memory への反映）を無効化する。
 *
 * \details Dcm UDS SID 0x85 ControlDTCSetting のサブ機能 0x02 (off) から呼び出す
 *          （[SWS_Dcm_00406]）。無効化中は Dem_SetEventStatus() への報告を
 *          すべて無視する（デバウンスカウンタも進めない）。IOControl/
 *          RoutineControl でアクチュエータを意図的に操作するテスト中に、
 *          その副作用で誤った DTC が記録されるのを防ぐ用途を想定する。
 *
 * \note    シグネチャは実 AUTOSAR の `ClientId` 引数・`Std_ReturnType` 戻り値を
 *          含めて仕様準拠とする（`Dem_EnableDTCSetting()` と同じ経緯・理由）。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子。本実装では未使用。
 *
 * \retval  E_OK  常に成功（同期完了）。
 *
 * \AUTOSARReq     {SWS_Dem_00242}
 * \ServiceID      {0x24}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for same ClientId}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_DisableDTCSetting(uint8 ClientId);

/**
 * \brief   Dem モジュールのバージョン情報を取得する。
 *
 * \details [SWS_Dem_00124] により Dem_GetVersionInfo は明示的に未初期化
 *          チェック (DEM_E_UNINIT) の対象外と規定されているため、初期化状態は
 *          確認せず NULL ポインタチェックのみ行う（Dem_Cfg.h 冒頭コメント参照）。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef DEM_UNIT_TEST
/**
 * \brief   [テスト専用] Dem_Initialized を未初期化状態へ戻す。
 *
 * \details Dem には DeInit() に相当する API が無く、`Dem_Initialized` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          一度 `Dem_Init()` を呼ぶと他のテストファイルの実行順に関わらず
 *          「未初期化状態」を再現できなくなる。native 環境のホストテストから
 *          のみ使用する。`DEM_UNIT_TEST` は CMakeLists.txt の native_chain
 *          ターゲットでのみ定義され、実機ビルド（`uno_r4`）では定義されない
 *          ため、実機の `Dem.h`/`Dem.c` には一切含まれない（AUTOSAR
 *          標準外の関数。`Mcu_Test_ResetInitState()` と同じ設計方針）。
 */
void Dem_Test_ResetInitState(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DEM_H */
