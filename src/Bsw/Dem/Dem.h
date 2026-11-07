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
 *          ExtendedData（累積故障確定回数。Dem_SelectExtendedDataRecord() 系）
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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "Dem_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** [SWS_Dem_00198] `Dem_GetDTCOfEvent()` の拡張戻り値（要求フォーマットに
 *  対応する DTC が構成されていない場合）。実仕様の Service Interface
 *  DiagnosticInfo（値表）に基づく数値。Dem_Cfg.h の `DEM_E_*`（Det_ReportError
 *  に渡す開発エラー ID）とは別の値域（本関数の戻り値そのもの）である点に注意。 */
#define DEM_E_NO_DTC_AVAILABLE  0x0AU

/** `Dem_ClearDTC()` の拡張戻り値（実仕様の Service Interface ClearDTC（値表、Table 8.175）に基づく数値。
 *  Dcm は、これを UDS 0x14 の否定応答コードへ対応付ける [SWS_Dcm_00705]〜[00708]/[00966]/[01060]/[01408]）。
 *  実仕様は、非同期（`DEM_PENDING`）・複数クライアント（`DEM_CLEAR_BUSY`）を前提とするが、本プロジェクトは単一クライアント・
 *  常に同期処理なので、`DEM_PENDING` は定義せず、`DEM_CLEAR_BUSY` は返さない（値だけを定義する）。 */
#define DEM_CLEAR_BUSY          5U   /**< 別のクライアントがクリア中（本実装は返さない） */
#define DEM_CLEAR_MEMORY_ERROR  6U   /**< 不揮発メモリへの書き込みを積めなかった（NvM_WriteBlock() の失敗） */
#define DEM_CLEAR_FAILED        7U   /**< クリアできなかった（本実装は返さない: DemClearEventAllowed を持たない） */
#define DEM_WRONG_DTC           8U   /**< 指定した DTC（または、その形式）が存在しない */
#define DEM_WRONG_DTCORIGIN     9U   /**< 指定した DTCOrigin が存在しない */

/** [SWS_Dem_00215] Dem_GetNextFilteredDTC() 系の戻り値: フィルタ条件に一致する
 *  次の要素が無い。実仕様の Service Interface DiagnosticInfo（値表、
 *  DEM_NO_SUCH_ELEMENT = 48）に基づく数値。 */
#define DEM_NO_SUCH_ELEMENT  48U

/** [SWS_Dem_00239] Dem_GetNextExtendedDataRecord() の戻り値: 渡されたバッファが、レコードより小さい。
 *  実仕様の Service Interface に数値の記載が無いため、既存の拡張戻り値（5〜9、48）と重ならない値を、独自に割り当てた。 */
#define DEM_BUFFER_TOO_SMALL  21U

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * 型定義
 * ----------------------------------------------------------------------- */
/** イベント ID 型 (DemConf_DemEventParameter_* 定数を渡す) */
typedef uint8 Dem_EventIdType;

/** UDS DTC ステータスバイト型（[SWS_Dem_00928]、AUTOSAR では Bitfield/uint8
 *  として定義される。本実装の `Dem_StatusTable[]` の要素と同じビット配置）。 */
typedef uint8 Dem_UdsStatusByteType;

/** イベントの監視ステータス型（[SWS_Dem_91005]、Bitfield/uint8）。FiM などが、テストの結果を見るために使う。 */
typedef uint8 Dem_MonitorStatusType;
#define DEM_MONITOR_STATUS_TF     0x01U  /**< bit0: TestFailed                            */
#define DEM_MONITOR_STATUS_TNCTOC 0x02U  /**< bit1: TestNotCompletedThisOperationCycle    */

/**
 * \brief   DTC 値のフォーマットを選択する型（[SWS_Dem_00933]）。
 * \details 本プロジェクトの `Dem_DtcTable[]`（Dem_PBCfg.c）は UDS 3-byte 形式の
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

/**
 * \brief   イベントステータス型 (Dem_EventStatusType は AUTOSAR SWS_Dem_00926 で定義)
 *
 * \details counter-based debouncing（仕様どおり）。
 *          - PREFAILED / PREPASSED: モニタが、1 回分の判定を、段階的に報告する。報告のたびに、
 *            デバウンスカウンタを、1 つ（ステップ幅 1）だけ、失敗側 / 成功側へ進める
 *            （SWS_Dem_00418/00419）。しきい値（Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*）に達した
 *            瞬間に、DTC のステータスを確定する。反対向きの報告で、カウンタが反対側にあるときは、
 *            先に 0 へ戻す（jump-up / jump-down の値が 0。SWS_Dem_00422〜00425）。
 *          - FAILED / PASSED: モニタが、既に確定した結果を報告する。カウンタを、失敗側 / 成功側の
 *            しきい値へ、直接、ジャンプさせて、すぐに確定する（SWS_Dem_00420/00421）。
 *          しきい値が 1 のイベントは、PREFAILED と FAILED（PREPASSED と PASSED）の効果が同じ。
 *          以前は、FAILED/PASSED を、毎回の判定として、報告のたびに数える独自の方式で、
 *          PREFAILED/PREPASSED を受け付けなかった（2026-11 に仕様どおりへ変更）。
 */
typedef enum
{
    DEM_EVENT_STATUS_PASSED    = 0U,  /**< テスト合格 (モニタが報告する生の結果)      */
    DEM_EVENT_STATUS_FAILED    = 1U,  /**< テスト失敗 (モニタが報告する生の結果)      */
    DEM_EVENT_STATUS_PREPASSED = 2U,  /**< デバウンス中 (PASSED 方向、未確定)         */
    DEM_EVENT_STATUS_PREFAILED = 3U   /**< デバウンス中 (FAILED 方向、未確定)         */
} Dem_EventStatusType;

/**
 * \brief   デバウンスの状態のビット集合（[SWS_Dem_01000]、`Dem_GetDebouncingOfEvent()` の出力）。
 */
typedef uint8 Dem_DebouncingStateType;
#define DEM_TEMPORARILY_DEFECTIVE 0x01U  /**< bit0: 0 < FDC < 127（失敗側へ進行中）                   */
#define DEM_FINALLY_DEFECTIVE     0x02U  /**< bit1: FDC = 127（失敗が確定）                           */
#define DEM_TEMPORARILY_HEALED    0x04U  /**< bit2: -128 < FDC < 0（成功側へ進行中）                  */
#define DEM_TEST_COMPLETE         0x08U  /**< bit3: FDC = -128 または 127（テスト完了）               */
#define DEM_DTR_UPDATE            0x10U  /**< bit4: テスト完了かつデバウンス完了かつ有効/保存条件が成立 */

/**
 * \brief   デバウンスを止める / 戻す操作の型（[SWS_Dem_00927]、`Dem_ResetEventDebounceStatus()` に渡す）。
 */
typedef uint8 Dem_DebounceResetStatusType;
#define DEM_DEBOUNCE_STATUS_FREEZE 0x00U  /**< 内部のデバウンスカウンタ/タイマを止める */
#define DEM_DEBOUNCE_STATUS_RESET  0x01U  /**< 内部のデバウンスカウンタ/タイマを戻す   */

/**
 * \brief   操作サイクルの状態の型（[SWS_Dem_00194]、`Dem_SetOperationCycleState()` に渡す新しい状態）。
 */
typedef enum
{
    DEM_CYCLE_STATE_START = 0U,  /**< 操作サイクルを（再）開始する */
    DEM_CYCLE_STATE_END   = 1U   /**< 操作サイクルを終了する       */
} Dem_OperationCycleStateType;

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

/** [SWS_Dem_00057] Dem_SetDTCFilter() の DTCSeverityMask の型（uint8）。
 *  本プロジェクトは DTC の Severity を持たないため、FilterWithSeverity=TRUE は
 *  受け付けない。 */
typedef uint8 Dem_DTCSeverityType;

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
 * \param[in]  EventId      イベント ID (DemConf_DemEventParameter_* 定数)。
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
 * \brief   イベントの失敗ステータスを、PASSED を報告せずに戻す（[SWS_Dem_00187]、[SWS_Dem_00331]）。
 *
 * \details TestFailed（UDS ステータスの bit0）を 0 にし、デバウンスカウンタを初期値（0）へ戻す。監視ステータスと、
 *          UDS ステータスの bit6（TestNotCompletedThisOperationCycle）は変えない。モニタが、限定動作（limp home）を
 *          解除して通常動作へ戻るときに、PASSED を報告できない場合に使う。
 *          DTC の設定が無効（`Dem_DisableDTCSetting()`）の間は、UDS ステータスを更新しないので、何もせず E_OK を返す
 *          （[SWS_Dem_00079]）。そうでなければ、既にこの操作サイクルでテスト済み（bit6 = 0）のイベントは、E_NOT_OK で
 *          拒否する（[SWS_Dem_00638]）。操作サイクルが終了していても、動作する（報告ではないため、[SWS_Dem_00482] の対象外）。
 *
 * \param[in]  EventId  イベント ID（`DemConf_DemEventParameter_*`）。
 *
 * \retval  E_OK      受理した（DTC の設定が無効で、何もしなかった場合を含む）。
 * \retval  E_NOT_OK  EventId が構成にない、またはこの操作サイクルでテスト済み。
 *
 * \note    Dem_SetEventStatus() と同じく、未初期化チェック（DEM_E_UNINIT）の対象外（[SWS_Dem_00124]）。
 *          仕様は、非同期の処理（次のメイン関数で実行）と規定するが、本実装は、同期で処理する。ステータス変更の
 *          コールバック（[SWS_Dem_00187]）は持たない。仕様は RTE 経由のみで Dem.h には公開しないが、本プロジェクトは
 *          RTE を持たないので、ここで宣言する。
 *
 * \AUTOSARReq     {SWS_Dem_00187, SWS_Dem_00331, SWS_Dem_00638}
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ResetEventStatus(Dem_EventIdType EventId);

/**
 * \brief   イベントのデバウンスを、止める / 戻す（[SWS_Dem_00683]）。
 *
 * \details RESET: デバウンスカウンタを 0 へ戻す（FDC も 0 になる、[SWS_Dem_00684]）。FREEZE: 止める対象のタイマが、
 *          カウンタ式のデバウンスには無いので、何もしない（[SWS_Dem_00685] は、タイマ式のデバウンスの規定）。
 *          どちらも、イベントの UDS ステータスには影響しない（デバウンスの状態だけが変わる）。
 *          Dem_PreInit のあと、すぐに使える API のため、Dem_SetEventStatus() と同じく、未初期化チェックの対象外。
 *
 * \param[in]  EventId              イベント ID（`DemConf_DemEventParameter_*`）。
 * \param[in]  DebounceResetStatus  DEM_DEBOUNCE_STATUS_FREEZE / DEM_DEBOUNCE_STATUS_RESET。
 *
 * \retval  E_OK      受理した。
 * \retval  E_NOT_OK  EventId が構成にない（DEM_E_WRONG_CONFIGURATION）、または操作が FREEZE / RESET 以外（DEM_E_PARAM_DATA）。
 *
 * \AUTOSARReq     {SWS_Dem_00683, SWS_Dem_00684, SWS_Dem_00685}
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ResetEventDebounceStatus(Dem_EventIdType EventId, Dem_DebounceResetStatusType DebounceResetStatus);

/**
 * \brief   イベントのデバウンスの状態を取得する（[SWS_Dem_00730]、[SWS_Dem_00844]）。
 *
 * \details 内部のデバウンスカウンタから、`Dem_DebouncingStateType` のビットを導く（FDC と同じ区分）。
 *          DEM_DTR_UPDATE（bit4）は、テスト完了のとき立てる（有効条件・保存条件を持たず、常に成立しているため）。
 *
 * \param[in]   EventId         イベント ID（`DemConf_DemEventParameter_*`）。
 * \param[out]  DebouncingState  デバウンスの状態（DEM_TEMPORARILY_DEFECTIVE 等のビットの組）。
 *
 * \retval  E_OK      取得した。
 * \retval  E_NOT_OK  未初期化、NULL ポインタ、または EventId が構成にない。
 *
 * \AUTOSARReq     {SWS_Dem_00730, SWS_Dem_00844, SWS_Dem_01000}
 * \ServiceID      {0x9F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetDebouncingOfEvent(Dem_EventIdType EventId, Dem_DebouncingStateType* DebouncingState);

/**
 * \brief   操作サイクルの状態を設定する（[SWS_Dem_00194]）。
 *
 * \details START: 操作サイクルを（再）開始する。既に開始済みなら、再開始する（[SWS_Dem_00483]）。直前の操作サイクルの
 *          最終ステータスで、経年回復（Aging）と PendingDTC の自動クリアを評価し（END で評価済みなら、しない）、
 *          イベントごとに TestFailed・
 *          TestFailedThisOperationCycle をクリア、TestNotCompletedThisOperationCycle を立て、デバウンスカウンタを
 *          0 へ戻す。開始してから、`Dem_SetEventStatus()` の報告を、再び受け付ける（[SWS_Dem_00481]）。
 *          END: 操作サイクルを終了する。そのサイクルの最終ステータスで、経年回復（Aging）と PendingDTC の
 *          自動クリアを評価する（[SWS_Dem_00390]、[SWS_Dem_00493]）。終了している間、`Dem_SetEventStatus()` の報告は無視され、その報告による
 *          UDS ステータスの変化は起きない（[SWS_Dem_00482]。`Dem_ClearDTC()` のような、明示的な操作は、サイクルの
 *          状態に関係なく働く）。既に終了済みなら、何もしない（[SWS_Dem_00484]）。
 *          `Dem_Init()` が、電源サイクルを開始する（[SWS_Dem_00853]）。
 *
 * \param[in]  OperationCycleId  操作サイクルの ID（`DemConf_DemOperationCycle_*`）。
 * \param[in]  CycleState        新しい状態（START / END）。
 *
 * \retval  E_OK      受理した。
 * \retval  E_NOT_OK  未初期化、操作サイクルが構成にない（DEM_E_WRONG_CONFIGURATION、[SWS_Dem_01223]）、
 *                    または状態が START / END 以外。
 *
 * \note    仕様は、非同期の処理（モニタの `InitMonitorForEvent` コールバック。[SWS_Dem_00679]）と規定するが、
 *          本実装は、同期で処理し、コールバックは呼ばない。従属サイクル（DemLeadingCycleRef）は持たない。
 *          仕様は RTE 経由のみで、Dem.h には公開しないが、本プロジェクトは RTE を持たないので、ここで宣言する。
 *
 * \AUTOSARReq     {SWS_Dem_00194, SWS_Dem_00338, SWS_Dem_00481, SWS_Dem_00482, SWS_Dem_00483, SWS_Dem_00484, SWS_Dem_00853}
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SetOperationCycleState(uint8 OperationCycleId, Dem_OperationCycleStateType CycleState);

/**
 * \brief   操作サイクルの状態を取得する（[SWS_Dem_00729]）。
 *
 * \param[in]   OperationCycleId  操作サイクルの ID（`DemConf_DemOperationCycle_*`）。
 * \param[out]  CycleState        現在の状態（START = 開始済み、END = 終了済み）。
 *
 * \retval  E_OK      取得した。
 * \retval  E_NOT_OK  未初期化、NULL ポインタ、または操作サイクルが構成にない。
 *
 * \AUTOSARReq     {SWS_Dem_00729}
 * \ServiceID      {0x9E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetOperationCycleState(uint8 OperationCycleId, Dem_OperationCycleStateType* CycleState);

/**
 * \brief   DTC ステータス availability mask（サポートするステータスビット）を取得する。
 *
 * \par 仕様
 *          DCM SID 0x19 の各種応答([SWS_Dem_00060]: statusAvailabilityMask
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
 * \param[in]   EventId        イベント ID (DemConf_DemEventParameter_* 定数)。
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
 * \brief   指定イベントの監視ステータスを取得する（[SWS_Dem_91007]）。
 *
 * \details 実仕様の FiM は、抑止の条件（FIM_LAST_FAILED など）の判定に、UDS ステータスではなく、本関数の
 *          監視ステータスを使う。UDS ステータスの bit6（TestNotCompletedThisOperationCycle）は、
 *          `Dem_GetEventUdsStatus()` では statusAvailabilityMask で隠れるが、本関数では見える。
 *          簡略化: 監視ステータスを別に持たず、内部のステータスの bit0（TestFailed）と
 *          bit6（TestNotCompletedThisOperationCycle）から導く。
 *
 * \param[in]   EventId        イベント ID (DemConf_DemEventParameter_* 定数)。
 * \param[out]  MonitorStatus  監視ステータス（DEM_MONITOR_STATUS_TF / DEM_MONITOR_STATUS_TNCTOC）の格納先。
 *                             NULL 禁止。戻り値が E_NOT_OK の場合は不定。
 *
 * \retval  E_OK      正常取得 [SWS_Dem_01287]。
 * \retval  E_NOT_OK  未初期化、EventId が範囲外 [SWS_Dem_01288]、または MonitorStatus が NULL。
 *
 * \AUTOSARReq     {SWS_Dem_91007, SWS_Dem_01287, SWS_Dem_01288}
 * \ServiceID      {0xb5}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetMonitorStatus(Dem_EventIdType EventId, Dem_MonitorStatusType* MonitorStatus);

/**
 * \brief   イベント ID から DTC コードを取得する（[SWS_Dem_00198]/[SWS_Dem_00269]）。
 *
 * \param[in]   EventId     イベント ID (DemConf_DemEventParameter_* 定数)。
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
 * \brief   Dem_SelectDTC() で選んだ DTC（または DTC グループ）をクリアし、NvM (EEPROM) を初期状態へ戻す
 *          （[SWS_Dem_00665]）。DCM SID 0x14 (ClearDiagnosticInformation) から呼び出す。
 *
 * \par 仕様
 *          実仕様どおりの 2 段階: 先に `Dem_SelectDTC(ClientId, DTC, DTCFormat, DTCOrigin)` で対象を選び、
 *          （任意で `Dem_GetDTCSelectionResultForClearDTC()` で選択の結果を確かめ、）本関数を呼ぶ。
 *          選択は、次に `Dem_SelectDTC()` を呼ぶまで、クライアントごとに残る（[SWS_Dem_01253]）。
 *
 *          選んだのが `DEM_GROUP_ALL_DTCS`（0xFFFFFF）なら、[SWS_Dem_01203] のとおり、選んだ DTCOrigin の
 *          全イベントをクリアする。それ以外は、一致するイベント 1 件だけをクリアする。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 *
 * \retval  E_OK                    正常クリア（RAM の状態を戻し、不揮発メモリへの書き込みも積んだ）。
 * \retval  E_NOT_OK                未初期化、ClientId が範囲外、または Dem_SelectDTC() が呼ばれていない
 *                                  （DEM_E_WRONG_CONDITION、[SWS_Dem_01255]）。
 * \retval  DEM_WRONG_DTC           選んだ DTC に一致するイベントが無い、または DTCFormat が DEM_DTC_FORMAT_UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN     選んだ DTCOrigin が非対応。
 * \retval  DEM_CLEAR_MEMORY_ERROR  不揮発メモリへの書き込みを積めなかった（NvM_WriteBlock() が失敗。RAM 上は
 *                                  クリア済み（巻き戻しはしない）だが、再起動すると、クリアした内容が復活しうる）。
 *
 * \AUTOSARReq     {SWS_Dem_00665, SWS_Dem_01203, SWS_Dem_01253, SWS_Dem_01255}
 * \ServiceID      {0x23}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ClearDTC(uint8 ClientId);

/**
 * \brief   直近の Dem_SelectDTC() が、有効な DTC（または DTC グループ）を選んだかを返す（[SWS_Dem_91023]）。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 *
 * \retval  E_OK                 選んだ DTC（または DTC グループ）が有効。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、または Dem_SelectDTC() が呼ばれていない
 *                               （DEM_E_WRONG_CONDITION、[SWS_Dem_01255]）。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、または DTCFormat が DEM_DTC_FORMAT_UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_91023, SWS_Dem_01253}
 * \ServiceID      {0xb8}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING・DEM_BUSY は返さない）}
 */
Std_ReturnType Dem_GetDTCSelectionResult(uint8 ClientId);

/**
 * \brief   Dem_GetDTCSelectionResult() と同じだが、DTC 消去（Dem_ClearDTC()）の設定を考慮した結果を返す
 *          （[SWS_Dem_91020]）。Dcm が、0x14 の否定応答コードを決めるために、消去の前に呼ぶ。
 *
 * \par 仕様
 *          単一の DTC の消去だけを許す設定（DemClearDTCLimitation）は持たないため、結果は
 *          Dem_GetDTCSelectionResult() と同じ。ただし、Dem_SelectDTC() が呼ばれていないときは、
 *          E_NOT_OK ではなく DEM_WRONG_DTC を返す（本関数の戻り値表に E_NOT_OK が無いため。[SWS_Dem_01261]）。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 *
 * \retval  E_OK                 選んだ DTC（または DTC グループ）が有効。
 * \retval  E_NOT_OK             未初期化、または ClientId が範囲外。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、DTCFormat が DEM_DTC_FORMAT_UDS 以外、
 *                               または Dem_SelectDTC() が呼ばれていない（DEM_E_WRONG_CONDITION）。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_91020, SWS_Dem_01253, SWS_Dem_01261}
 * \ServiceID      {0xbb}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING・DEM_BUSY は返さない）}
 */
Std_ReturnType Dem_GetDTCSelectionResultForClearDTC(uint8 ClientId);

/**
 * \brief   以降の Dem_ClearDTC() などの対象にする、DTC（または DTC グループ）を選ぶ（[SWS_Dem_91016]）。
 *
 * \par 仕様
 *          選択は、クライアントごとに、次に本関数を呼ぶまで残る（[SWS_Dem_01253]）。選択が有効かどうかは、
 *          本関数の戻り値ではなく、Dem_GetDTCSelectionResult() など、選択に続く API の戻り値で分かる
 *          （[SWS_Dem_01254]）。`DTC` に `DEM_GROUP_ALL_DTCS`（0xFFFFFF）を渡すと、全 DTC のグループを選ぶ。
 *
 * \param[in]  ClientId   呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 * \param[in]  DTC        選ぶ DTC（24-bit UDS 形式）、または `DEM_GROUP_ALL_DTCS`。
 * \param[in]  DTCFormat  DTC のフォーマット。本プロジェクトは `DEM_DTC_FORMAT_UDS` のみ対応。
 * \param[in]  DTCOrigin  イベントメモリ。本プロジェクトは `DEM_DTC_ORIGIN_PRIMARY_MEMORY` のみ構成する
 *                        （対応外の値は DEM_E_WRONG_CONFIGURATION を報告する）。
 *
 * \retval  E_OK      選択の処理を受け付けた（選択の結果は、Dem_GetDTCSelectionResult() で分かる）。
 * \retval  E_NOT_OK  未初期化、または ClientId が範囲外。
 *
 * \AUTOSARReq     {SWS_Dem_91016, SWS_Dem_01253, SWS_Dem_01254}
 * \ServiceID      {0xb7}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SelectDTC(uint8 ClientId, uint32 DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin);

/**
 * \brief   Dem_SelectDTC() で選んだ DTC のステータスバイトを取得する（[SWS_Dem_00212]）。
 *
 * \par 仕様
 *          Dcm は、イベント ID ではなく、本関数で DTC のステータスを取得する（`Dem_GetEventUdsStatus()` は、
 *          SW-C や BSW モジュールが、イベント単位で使う）。ステータスは、`DemStatusAvailabilityMask` で
 *          マスクして返す。DTC グループを選んでいるときは、DTC が 1 件に決まらないため、DEM_WRONG_DTC を返す。
 *
 * \param[in]   ClientId   呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 * \param[out]  DTCStatus  ステータスバイト（ISO 14229-1）の格納先。NULL 禁止。E_OK 以外のときは不定。
 *
 * \retval  E_OK                 DTCStatus へ格納した。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、DTCStatus が NULL、または Dem_SelectDTC() が
 *                               呼ばれていない（DEM_E_WRONG_CONDITION）。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、DTCFormat が DEM_DTC_FORMAT_UDS 以外、
 *                               または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_00212, SWS_Dem_01253}
 * \ServiceID      {0x15}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING・DEM_BUSY は返さない）}
 */
Std_ReturnType Dem_GetStatusOfDTC(uint8 ClientId, Dem_UdsStatusByteType* DTCStatus);

/**
 * \brief   選んだ DTC のレコード（FreezeFrame・ExtendedData）の更新と削除を止める（[SWS_Dem_00233]）。
 *
 * \par 仕様
 *          Dcm が、Dem_SelectFreezeFrameData() / Dem_SelectExtendedDataRecord() で読む前に呼び、読み終えてから
 *          Dem_EnableDTCRecordUpdate() で戻す（[SWS_Dem_00270]）。止めている間、その DTC のイベントが
 *          確定 FAILED になっても、FreezeFrame の取り直しと故障確定回数の加算は、止めている間は行わず、
 *          Dem_EnableDTCRecordUpdate() が戻したときに行う（捨てない）。Dem_ClearDTC() が消去しても、そのレコードは
 *          消さず、戻したときに消す（消去のあとに確定していれば、消してから、更新する）。
 *          UDS ステータスの更新には影響しない。他のイベントのレコードも、影響を受けない。
 *          同じクライアントが、1 つしか止められない（[SWS_Dem_00648]）。DTC グループは止められない。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 *
 * \retval  E_OK                 止めた。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、Dem_SelectDTC() が呼ばれていない、または、すでに
 *                               同じクライアントが別の DTC を止めている（DEM_E_WRONG_CONDITION）。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_00233, SWS_Dem_00270, SWS_Dem_00648}
 * \ServiceID      {0x1a}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING・DEM_BUSY は返さない）}
 */
Std_ReturnType Dem_DisableDTCRecordUpdate(uint8 ClientId);

/**
 * \brief   Dem_DisableDTCRecordUpdate() で止めたレコードの更新と削除を、元に戻す（[SWS_Dem_00234]）。
 *
 * \par 仕様
 *          止めている間に Dem_ClearDTC() が消去していたレコードは消し、止めている間に確定 FAILED になっていたときは、
 *          FreezeFrame と故障確定回数を更新する（消去と更新は、保留した順に、NvM への書き込みを積む）。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 *
 * \retval  E_OK      戻した。
 * \retval  E_NOT_OK  未初期化、ClientId が範囲外、Dem_SelectDTC() が呼ばれていない、または、止めている DTC が
 *                    無い（DEM_E_WRONG_CONDITION）。
 *
 * \AUTOSARReq     {SWS_Dem_00234, SWS_Dem_00271}
 * \ServiceID      {0x1b}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_BUSY は返さない）}
 */
Std_ReturnType Dem_EnableDTCRecordUpdate(uint8 ClientId);

/**
 * \brief   ExtendedData のレコードを選ぶ（[SWS_Dem_91017]）。Dem_SelectDTC() で選んだ DTC が、対象になる。
 *
 * \par 仕様
 *          以降の Dem_GetSizeOfExtendedDataRecordSelection() と Dem_GetNextExtendedDataRecord() が、この選択を使う
 *          （[SWS_Dem_01272]）。Dem_SelectDTC() を呼ぶと、この選択は破棄される。Dem_SelectDTC() が呼ばれていない
 *          ときは、DEM_E_WRONG_CONDITION を報告する（[SWS_Dem_01274]）。
 *          選ぶ DTC が有効かどうか（DEM_WRONG_DTC / DEM_WRONG_DTCORIGIN）は、本関数が返す。レコード番号が
 *          構成に無いことは、本関数ではなく、Get 系が DEM_NO_SUCH_ELEMENT で返す（本関数の戻り値表に無いため）。
 *
 * \param[in]  ClientId            呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 * \param[in]  ExtendedDataNumber  レコード番号。0xFF は全レコード、0xFE は OBD のレコード全て（本プロジェクトは持たない）。
 *
 * \retval  E_OK                 選んだ。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、または Dem_SelectDTC() が呼ばれていない。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、または DTCFormat が DEM_DTC_FORMAT_UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_91017, SWS_Dem_01272, SWS_Dem_01273, SWS_Dem_01274}
 * \ServiceID      {0xba}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SelectExtendedDataRecord(uint8 ClientId, uint8 ExtendedDataNumber);

/**
 * \brief   Dem_SelectExtendedDataRecord() で選んだレコードの大きさを取得する（[SWS_Dem_00240]）。
 *
 * \par 仕様
 *          大きさは、レコード番号を含む（形式は {レコード番号, データ...}、[SWS_Dem_00076]）。レコードが
 *          記録されていない（一度も確定 FAILED していない）ときは、E_OK で 0 を返す（[SWS_Dem_00631]）。
 *          DTC グループを選んでいるときは、DTC が 1 件に決まらないため、DEM_WRONG_DTC を返す。
 *
 * \param[in]   ClientId                 呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 * \param[out]  SizeOfExtendedDataRecord  大きさ（バイト）の格納先。NULL 禁止。E_OK 以外のときは不定。
 *
 * \retval  E_OK                 大きさを格納した。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、格納先が NULL、または選択（Dem_SelectDTC() /
 *                               Dem_SelectExtendedDataRecord()）が呼ばれていない（DEM_E_WRONG_CONDITION）。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が、構成に無い。
 *
 * \AUTOSARReq     {SWS_Dem_00240, SWS_Dem_00076, SWS_Dem_00631, SWS_Dem_01274}
 * \ServiceID      {0x21}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetSizeOfExtendedDataRecordSelection(uint8 ClientId, uint16* SizeOfExtendedDataRecord);

/**
 * \brief   Dem_SelectExtendedDataRecord() で選んだレコードを、バッファへ書き出す（[SWS_Dem_00239]）。
 *
 * \par 仕様
 *          書き出す形式は {レコード番号, データ...}（[SWS_Dem_00075]）。要求したレコード番号が 0xFF でも、
 *          先頭の 1 バイトは、実際のレコード番号（0x01）になる。レコードが記録されていないときは、E_OK で
 *          BufSize を 0 にする（[SWS_Dem_00631]）。1 回の選択で得られるレコードは、1 回の呼び出しで全て
 *          渡すため、続けて呼ぶと DEM_NO_SUCH_ELEMENT（これ以上無い）を返す。
 *
 * \param[in]     ClientId    呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 * \param[out]    DestBuffer  レコードの書き出し先。NULL 禁止。
 * \param[in,out] BufSize     入力: DestBuffer に書ける最大バイト数。出力: 実際に書いたバイト数。NULL 禁止。
 *
 * \retval  E_OK                 書き出した（記録が無いときは、BufSize が 0）。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、ポインタが NULL、または選択が呼ばれていない。
 * \retval  DEM_BUFFER_TOO_SMALL BufSize が、レコードより小さい（DestBuffer は書き換えない。選択は進めない）。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が構成に無い、または、これ以上渡すレコードが無い。
 *
 * \AUTOSARReq     {SWS_Dem_00239, SWS_Dem_00075, SWS_Dem_00631, SWS_Dem_01274}
 * \ServiceID      {0x20}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetNextExtendedDataRecord(uint8 ClientId, uint8* DestBuffer, uint16* BufSize);

/**
 * \brief   FreezeFrame のレコードを選ぶ（[SWS_Dem_91015]）。Dem_SelectDTC() で選んだ DTC が、対象になる。
 *
 * \par 仕様
 *          以降の Dem_GetSizeOfFreezeFrameSelection() と Dem_GetNextFreezeFrameData() が、この選択を使う
 *          （[SWS_Dem_01268]）。Dem_SelectDTC() を呼ぶと、この選択は破棄される。Dem_SelectDTC() が呼ばれていない
 *          ときは、DEM_E_WRONG_CONDITION を報告する（[SWS_Dem_01271]）。
 *
 * \param[in]  ClientId      呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 * \param[in]  RecordNumber  レコード番号。
 *
 * \retval  E_OK                 選んだ。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、または Dem_SelectDTC() が呼ばれていない。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、または DTCFormat が DEM_DTC_FORMAT_UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_91015, SWS_Dem_01268, SWS_Dem_01269, SWS_Dem_01270, SWS_Dem_01271}
 * \ServiceID      {0xb9}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SelectFreezeFrameData(uint8 ClientId, uint8 RecordNumber);

/**
 * \brief   Dem_SelectFreezeFrameData() で選んだレコードの大きさを取得する（[SWS_Dem_00238]）。
 *
 * \par 仕様
 *          大きさは、レコード番号と DID の数を含む（形式は {レコード番号, DID 数, DID, データ, ...}、
 *          [SWS_Dem_00074]）。レコードが記録されていないときは、E_OK で 0 を返す（[SWS_Dem_00630]）。
 *          DTC グループを選んでいるときは、DTC が 1 件に決まらないため、DEM_WRONG_DTC を返す。
 *
 * \param[in]   ClientId           呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 * \param[out]  SizeOfFreezeFrame  大きさ（バイト）の格納先。NULL 禁止。E_OK 以外のときは不定。
 *
 * \retval  E_OK                 大きさを格納した。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、格納先が NULL、または選択（Dem_SelectDTC() /
 *                               Dem_SelectFreezeFrameData()）が呼ばれていない（DEM_E_WRONG_CONDITION）。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が、構成に無い。
 *
 * \AUTOSARReq     {SWS_Dem_00238, SWS_Dem_00074, SWS_Dem_00630, SWS_Dem_01270, SWS_Dem_01271}
 * \ServiceID      {0x1f}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetSizeOfFreezeFrameSelection(uint8 ClientId, uint16* SizeOfFreezeFrame);

/**
 * \brief   Dem_SelectFreezeFrameData() で選んだレコードを、バッファへ書き出す（[SWS_Dem_00236]）。
 *
 * \par 仕様
 *          書き出す形式は {レコード番号, DID 数, DID, データ, ...}（[SWS_Dem_00071]）。要求したレコード番号が
 *          0xFF でも、先頭の 1 バイトは、実際のレコード番号（0x01）になる。DID の構成と、データ要素の読み出しは
 *          Dem_PBCfg.c の `Dem_DidClassTable[]` による（データは、確定 FAILED の瞬間に取り、RAM に保持する）。
 *          レコードが記録されていないときは、E_OK で BufSize を 0 にする（[SWS_Dem_00630]）。
 *          1 回の選択で得られるレコードは、1 回の呼び出しで全て渡すため、続けて呼ぶと DEM_NO_SUCH_ELEMENT
 *          （これ以上無い）を返す。
 *
 * \param[in]     ClientId    呼び出し元のクライアント識別子（`DEM_CLIENT_COUNT` 未満）。
 * \param[out]    DestBuffer  レコードの書き出し先。NULL 禁止。
 * \param[in,out] BufSize     入力: DestBuffer に書ける最大バイト数。出力: 実際に書いたバイト数。NULL 禁止。
 *
 * \retval  E_OK                 書き出した（記録が無いときは、BufSize が 0）。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、ポインタが NULL、または選択が呼ばれていない。
 * \retval  DEM_BUFFER_TOO_SMALL BufSize が、レコードより小さい（DestBuffer は書き換えない。選択は進めない）。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が構成に無い、または、これ以上渡すレコードが無い。
 *
 * \AUTOSARReq     {SWS_Dem_00236, SWS_Dem_00071, SWS_Dem_00630, SWS_Dem_01270, SWS_Dem_01271}
 * \ServiceID      {0x1d}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetNextFreezeFrameData(uint8 ClientId, uint8* DestBuffer, uint16* BufSize);

/**
 * \brief   DTC フィルタ条件を設定する（[SWS_Dem_00208]）。
 *
 * \par 仕様
 *          以降の Dem_GetNumberOfFilteredDTC() / Dem_GetNextFilteredDTC() /
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
 *                    E_OK 以外のときは、前に設定したフィルタも無効になり、後続の API は E_NOT_OK を返す（[SWS_Dem_01266]）。
 *
 * \AUTOSARReq     {SWS_Dem_00208, SWS_Dem_00057, SWS_Dem_01266}
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
 * \par 仕様
 *          「次に返す DTC」の位置は進めない（件数を数えるだけ）。常に同期的に
 *          完了する（DEM_PENDING は返さない）。
 *
 * \param[in]   ClientId             クライアント識別子。本実装では未使用。
 * \param[out]  NumberOfFilteredDTC  一致した DTC の件数。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  未初期化、NumberOfFilteredDTC が NULL、または Dem_SetDTCFilter()
 *                    でフィルタが設定されていない（DEM_E_WRONG_CONDITION、[SWS_Dem_01300]）。
 *
 * \AUTOSARReq     {SWS_Dem_00214, SWS_Dem_01300}
 * \ServiceID      {0x17}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetNumberOfFilteredDTC(uint8 ClientId, uint16* NumberOfFilteredDTC);

/**
 * \brief   フィルタ条件に一致する次の DTC とそのステータスを取得する（[SWS_Dem_00215]）。
 *
 * \par 仕様
 *          呼ぶたびに一致する DTC を 1 件ずつ返し、全件返し終えたら
 *          DEM_NO_SUCH_ELEMENT を返す。常に同期的に完了する（DEM_PENDING は返さない）。
 *
 * \param[in]   ClientId   クライアント識別子。本実装では未使用。
 * \param[out]  DTC        DTC 値（UDS 形式、24-bit）。NULL 禁止。E_OK 以外のときは不定。
 * \param[out]  DTCStatus  DTC のステータスバイト。NULL 禁止。E_OK 以外のときは不定。
 *
 * \retval  E_OK                次の DTC を返した。
 * \retval  E_NOT_OK            未初期化、ポインタが NULL、またはフィルタが設定されていない（DEM_E_WRONG_CONDITION、[SWS_Dem_01300]）。
 * \retval  DEM_NO_SUCH_ELEMENT 一致する DTC をすべて返し終えた。
 *
 * \AUTOSARReq     {SWS_Dem_00215, SWS_Dem_01300}
 * \ServiceID      {0x18}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Synchronous（本実装は DEM_PENDING を返さない）}
 */
Std_ReturnType Dem_GetNextFilteredDTC(uint8 ClientId, uint32* DTC, uint8* DTCStatus);

/**
 * \brief   フィルタ条件に一致する次の DTC と Fault Detection Counter を取得する
 *          （[SWS_Dem_00227]）。
 *
 * \par 仕様
 *          Dem_GetNextFilteredDTC() と同じ要領で 1 件ずつ返す。FDC は
 *          Dem_GetFaultDetectionCounter() と同じ写像（-128〜127）。
 *          UDS 0x19/0x14 では Dem_SetDTCFilter() に
 *          FilterForFaultDetectionCounter=TRUE を渡して prefailed のみに絞る。
 *
 * \param[in]   ClientId                 クライアント識別子。本実装では未使用。
 * \param[out]  DTC                      DTC 値（UDS 形式、24-bit）。NULL 禁止。
 * \param[out]  DTCFaultDetectionCounter Fault Detection Counter。NULL 禁止。
 *
 * \retval  E_OK                次の DTC を返した。
 * \retval  E_NOT_OK            未初期化、ポインタが NULL、またはフィルタが設定されていない（DEM_E_WRONG_CONDITION、[SWS_Dem_01300]）。
 * \retval  DEM_NO_SUCH_ELEMENT 一致する DTC をすべて返し終えた。
 *
 * \AUTOSARReq     {SWS_Dem_00227, SWS_Dem_01300}
 * \ServiceID      {0x3b}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetNextFilteredDTCAndFDC(uint8 ClientId, uint32* DTC, sint8* DTCFaultDetectionCounter);

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
 * \par 仕様
 *          DCM SID 0x19 subFunc 0x14 (reportDTCFaultDetectionCounter) から
 *          呼び出す。内部の `Dem_DebounceCounter[]`（-limit〜+limit、limit は
 *          イベントごとの `Dem_DebounceLimitTable[]`）を、ISO 14229-1 に従い
 *          -128(PASSED 側に最も振れた状態)〜127(FAILED 側に最も振れた状態)の
 *          範囲へ線形写像して返す（[SWS_Dem_00415]、`Dem_MapDebounceCounterToFdc()`
 *          参照）。
 *
 * \param[in]   EventId               イベント ID (DemConf_DemEventParameter_* 定数)。
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
 *          実現可能な範囲に簡略化）。
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

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

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
