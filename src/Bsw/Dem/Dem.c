/**
 * \file    Dem.c
 * \brief   診断イベントマネージャ (AUTOSAR SWS_DEM 準拠)
 * \details SW-C / BSW モジュールから報告されるイベントを受け取り、
 *          DTC (Diagnostic Trouble Code) のライフサイクルを管理する。
 *
 *          NvM 連携:
 *            本モジュールは avr/eeprom.h を直接参照しない。
 *            EEPROM への永続化はすべて NvM_ReadBlock() / NvM_WriteBlock() 経由で行う。
 *            NvM が管理するブロック:
 *              NVM_BLOCK_ID_DEM_MAGIC    — 有効データマーカー (1 byte)
 *              NVM_BLOCK_ID_DEM_STATUS   — イベントステータステーブル (DEM_EVENT_COUNT bytes)
 *              NVM_BLOCK_ID_DEM_AGING    — 経年回復(Aging)カウンタ (DEM_EVENT_COUNT bytes)
 *              NVM_BLOCK_ID_DEM_EXTENDED — ExtendedData 故障確定回数 (DEM_EVENT_COUNT bytes)
 *
 *          デバウンス (counter-based debouncing):
 *            各イベントは -limit 〜 +limit のカウンタを持つ (limit はイベントごとに
 *            Dem_DebounceLimitTable[] で個別設定。Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*)。
 *            FAILED 報告でカウンタ+1、PASSED 報告でカウンタ-1 (上下限でクランプ)。
 *            カウンタが +limit に達した瞬間にのみ DTC ステータスを確定 FAILED にし、
 *            -limit に達した瞬間にのみ確定 PASSED にする。
 *            その間 (PRE-FAILED / PRE-PASSED) は DTC ステータスビットを変更しない。
 *            limit=1 のイベント（モニタ側が既に十分な持続性チェックをしてから
 *            報告する BUTTON_STUCK / CAN_BUSOFF）は 1 回の報告で即確定する。
 *            報告の方向がカウンタの現在の符号と逆の場合は、まず中立 (0) に
 *            リセットしてから数え始める（IoHwAb のボタンデバウンスと同じ
 *            「割り込まれたら最初からやり直す」方式）。これにより、反対側の
 *            確定状態から数え始めて反転に 2*limit 回必要になる問題を防ぐ。
 *
 *          DTC ライフサイクル:
 *            報告なし → TNCLC=1, TNCTOC=1 (未テスト状態)
 *            PASSED デバウンス確定 → TF クリア, TNCLC クリア (CDTC/PDTC は保持)
 *            FAILED デバウンス確定 → TF=1, PDTC=1, CDTC=1, TFSLC=1, TNCLC クリア (NvM 経由で保存)
 *            ClearDTC 後 → 全ビットリセット, TNCLC=1
 *            クリーンな操作サイクル 1 回 → PDTC を自動クリア (詳細は下記)
 *            経年回復 (Aging) 完了 → CDTC を自動クリア (詳細は下記)
 *
 *          PendingDTC (PDTC) の操作サイクル境界での自動クリア:
 *            SWS_Dem_00390 (Figure 7.19) 準拠。操作サイクルの終了
 *            （Dem_SetOperationCycleState(END)、Dem_EndOperationCycle()）で、
 *            そのサイクルの最終ステータスを評価し、PDTC=1 かつ
 *            TFTOC=0（このサイクル中に FAILED 確定なし）かつ TNCTC=0（テスト済み）
 *            であれば即座に PDTC をクリアする。電源 OFF で終わる電源サイクルは END が呼ばれないので、
 *            次の起動の Dem_Init() が、前回の最終ステータスで評価する（再開始も同じ）。
 *            CDTC のような多サイクルのエージングカウンタは介さず、クリーンなサイクルが
 *            1 回来た時点で即クリアする点が CDTC との違い（Dem_EvaluatePendingClear()）。
 *
 *          経年回復 (Aging) — 操作サイクルベースの CDTC 自動回復:
 *            操作サイクルの終了（Dem_SetOperationCycleState(END)、Dem_EndOperationCycle()）で、
 *            そのサイクルの最終ステータスを評価する（電源 OFF で終わるサイクルは END が呼ばれないので、
 *            次の起動の Dem_Init() が、前回の最終ステータスで評価する。再開始も同じ）。
 *            (TF/TFTOC/TNCTC を新サイクル用にリセットする直前の値を見る)。
 *              CDTC=0                      → エージング対象外、カウンタ=0
 *              CDTC=1 かつ TF=1（再故障）    → 連続性が途切れたためカウンタ=0
 *              CDTC=1 かつ TNCTC=1（未テスト）→ このサイクルは数えない（カウンタ維持）
 *              CDTC=1 かつ TF=0 かつ TNCTC=0 → 「クリーンな操作サイクル」としてカウンタ+1
 *                → Dem_AgingThresholdTable[EventId]（イベントごとの閾値、
 *                  Dem_Cfg.h の DEM_AGING_THRESHOLD_*）に達したら CDTC を自動クリア
 *            カウンタは NvM_BLOCK_ID_DEM_AGING で永続化する（電源サイクルをまたいで
 *            連続性を判定するため、RAM のみでは意味がない）。
 *
 *          ExtendedData (故障確定回数) — FreezeFrame との違い:
 *            FreezeFrame は「故障した瞬間の車両状態のスナップショット」（1 件のみ、
 *            上書きされる）であるのに対し、ExtendedData (Dem_OccurrenceCounter) は
 *            「これまでに何回確定 FAILED したか」を表す累積カウンタである。
 *            デバウンス確定 FAILED の瞬間（FreezeFrame 更新と同じ箇所）でのみ
 *            +1 し、0xFF で飽和する。SID 0x14 でのクリア時は経年回復カウンタと
 *            同様に 0 へ戻る（CDTC 自体のライフサイクルとは独立）。
 *            NVM_BLOCK_ID_DEM_EXTENDED で永続化し、UDS SID 0x19 subFunc 0x06
 *            (reportExtendedDataRecordByDTCNumber) で読み出せる。
 *
 *          AUTOSAR 実装との主な違い (学習用簡略化):
 *            - デバウンスカウンタは RAM のみ保持 (電源 OFF でリセット)
 *            - 操作サイクルは、全イベントが属する電源サイクル 1 つだけ（Dem_Cfg.h の DemConf_DemOperationCycle_POWER）。
 *              Dem_Init()（起動時、[SWS_Dem_00853]）と Dem_SetOperationCycleState(START) が、サイクルの境界を処理する。
 *              従属サイクル（DemLeadingCycleRef）・状態の不揮発保存・InitMonitorForEvent コールバックは持たない
 *            - FreezeFrame は RAM のみに保持 (EEPROM 非永続化、電源 OFF で消去)
 *            - ExtendedData はカウンタ 1 種類のみ（実車は OperationCycle 情報等も持てる）
 *            - CONFIRMED (bit3) はデバウンス確定 FAILED の瞬間に即座に立てる。
 *              仕様の Figure 7.20 (SWS_Dem_00391) は本来これとは別に、複数の操作
 *              サイクルにまたがる "failure counter" と DemEventFailureCycleCounter
 *              Threshold（PENDING が何サイクル連続で FAILED になったら CONFIRMED に
 *              昇格させるか）を持つが、本実装はこの多サイクル成熟過程を持たない。
 *              挙動としては同閾値を 1（=最初の確定 FAILED で即昇格）に設定した場合と
 *              等価であり、仕様上有効な設定値の一つなので不整合ではない
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Dem.h"
#include "Dem_PBCfg.h"
#include "Det.h"
#include "NvM.h"
#include "FiM.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Dem"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * モジュール内部状態
 * ----------------------------------------------------------------------- */

/** 各イベントの DTC ステータスバイト (NvM RAM ミラーから復元した作業コピー) */
static uint8 Dem_StatusTable[DEM_EVENT_COUNT];

/* イベント ID → DTC / デバウンス閾値 / Aging 閾値 の各テーブル (Dem_DtcTable[] 等) は
 * Dem_PBCfg.c（config/data/Dem.json から生成）で定義する。 */

/** イベントごとの FreezeFrame (故障時スナップショット) のデータ要素。Dem_DidClassTable[] の順に、DID を除いて並べる。RAM のみ保持 */
static uint8 Dem_FreezeFrameData[DEM_EVENT_COUNT][DEM_FREEZE_FRAME_DATA_SIZE];

/** Dem_FreezeFrameData[i] が有効か (0=未記録, 1=記録あり) */
static uint8 Dem_FreezeFrameValid[DEM_EVENT_COUNT];

/** イベントごとのデバウンスカウンタ (-limit〜+limit, 0=中立。limit は Dem_DebounceLimitTable[]) */
static sint8 Dem_DebounceCounter[DEM_EVENT_COUNT];

/** イベントごとの経年回復 (Aging) カウンタ。NvM 経由で操作サイクルをまたいで永続化する */
static uint8 Dem_AgingCounter[DEM_EVENT_COUNT];

/** イベントごとの ExtendedData（確定 FAILED の累積回数、0xFF で飽和）。NvM 経由で永続化する */
static uint8 Dem_OccurrenceCounter[DEM_EVENT_COUNT];

/** [SWS_Dem_00124] 用の初期化済みフラグ。Dem_SetEventStatus() は
 *  明示的に未初期化チェック対象外のため参照しない。 */
static uint8 Dem_Initialized = 0U;

/** 操作サイクル（電源サイクル 1 つ）の状態。Dem_Init() が START にする（[SWS_Dem_00853]）。
 *  END の間、Dem_SetEventStatus() の報告は無視される（[SWS_Dem_00482]）。不揮発には保存しない。 */
static Dem_OperationCycleStateType Dem_OperationCycleState[DEM_OPERATION_CYCLE_COUNT];

/** DTC の記録が有効か（1=有効、0=無効）。Dem_EnableDTCSetting()/
 *  Dem_DisableDTCSetting()（Dcm UDS SID 0x85 ControlDTCSetting 用）で操作する。
 *  既定で有効。無効化中は Dem_SetEventStatus() を丸ごと無視する。 */
static uint8 Dem_DTCSettingEnabled = 1U;

/** Dem_SetDTCFilter() で設定された DTC フィルタ条件（[SWS_Dem_00057]）。
 *  単一ECU・単一診断クライアント構成のため 1 組だけ保持する。
 *  Dem_FilterActive==0 は「フィルタ未設定」で、Dem_GetNumberOfFilteredDTC() 等は
 *  DEM_E_WRONG_CONDITION を報告して E_NOT_OK を返す（[SWS_Dem_01300]）。Dem_Init() で未設定へ戻す。
 *  Dem_SetDTCFilter() が E_OK 以外を返したときも、未設定へ戻す（[SWS_Dem_01266]）。 */
static uint8 Dem_FilterActive     = 0U;
static uint8 Dem_FilterStatusMask = 0U;  /**< 0x00 はステータスによる絞り込みなし */
static uint8 Dem_FilterForFdc     = 0U;  /**< 1 = FDC が 1〜0x7E の DTC のみ */
static uint8 Dem_FilterNextEvent  = 0U;  /**< 次に調べるイベント ID（Dem_GetNextFilteredDTC 系の走査位置） */

/** レコード（ExtendedData / FreezeFrame）の種類。選択と読み出しの処理を、種類で切り替えて共有する。 */
typedef enum
{
    DEM_RECORD_EXTENDED_DATA = 0,   /**< Dem_SelectExtendedDataRecord() 系 */
    DEM_RECORD_FREEZE_FRAME  = 1,   /**< Dem_SelectFreezeFrameData() 系 */
    DEM_RECORD_KIND_COUNT    = 2
} Dem_RecordKindType;

/** レコードの選択（Dem_Select*Record/FreezeFrameData() の結果）。 */
typedef struct
{
    uint8 selected;    /**< 1 = Dem_Select*() が呼ばれた */
    uint8 number;      /**< 選んだレコード番号 */
    uint8 delivered;   /**< 1 = Dem_GetNext*() が、レコードを渡し済み */
} Dem_RecordSelectionType;

/** Dem_SelectDTC() の選択（クライアントごと、[SWS_Dem_01253]）。選択は、次に Dem_SelectDTC() を呼ぶまで残る。 */
typedef struct
{
    uint8           selected;   /**< 1 = Dem_SelectDTC() が呼ばれた */
    uint32          dtc;        /**< 選んだ DTC、または DEM_GROUP_ALL_DTCS */
    Dem_EventIdType eventId;    /**< 単一の DTC を選んだときの、一致したイベント（result が E_OK のときだけ有効） */
    Std_ReturnType  result;     /**< 選択の検査の結果: E_OK / DEM_WRONG_DTC / DEM_WRONG_DTCORIGIN */
    Dem_RecordSelectionType record[DEM_RECORD_KIND_COUNT];   /**< レコードの選択（ExtendedData / FreezeFrame。Dem_SelectDTC() で破棄） */
} Dem_SelectionType;

static Dem_SelectionType Dem_Selection[DEM_CLIENT_COUNT];

/** Dem_DisableDTCRecordUpdate() で止めている DTC（クライアントごとに 1 つ、[SWS_Dem_00648]）。 */
typedef struct
{
    uint8           locked;    /**< 1 = 止めている */
    Dem_EventIdType eventId;   /**< 止めているイベント（locked が 1 のときだけ有効） */
} Dem_RecordLockType;

static Dem_RecordLockType Dem_RecordLock[DEM_CLIENT_COUNT];

/** 1 = 止めている間に Dem_ClearDTC() が消去したため、止めを戻すときに、レコードを消す */
static uint8 Dem_RecordClearPending[DEM_EVENT_COUNT];

/** 1 = 止めている間に確定 FAILED になったため、止めを戻すときに、FreezeFrame と故障確定回数を更新する（[SWS_Dem_00270]: 更新は、止めるだけで、捨てない） */
static uint8 Dem_RecordCapturePending[DEM_EVENT_COUNT];

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static uint8 Dem_ClearOne(Dem_EventIdType EventId);
static boolean Dem_FindEventOfDtc(uint32 DTC, Dem_EventIdType* EventId);
static boolean Dem_IsRecordLocked(Dem_EventIdType EventId);
static void Dem_StoreRecords(Dem_EventIdType EventId);
static void Dem_DiscardRecords(Dem_EventIdType EventId);
static Std_ReturnType Dem_CheckClient(uint8 ClientId, uint8 apiId);
static Std_ReturnType Dem_CheckSelected(uint8 ClientId, uint8 apiId);
static Std_ReturnType Dem_CheckRecordSelection(uint8 ClientId, uint8 apiId, Dem_RecordKindType kind, uint16* RecordSize);
static Std_ReturnType Dem_SelectRecord(uint8 ClientId, uint8 apiId, Dem_RecordKindType kind, uint8 RecordNumber);
static Std_ReturnType Dem_GetSizeOfRecord(uint8 ClientId, uint8 apiId, Dem_RecordKindType kind, uint16* Size);
static Std_ReturnType Dem_GetNextRecord(uint8 ClientId, uint8 apiId, Dem_RecordKindType kind, uint8* DestBuffer, uint16* BufSize);
static void Dem_CaptureFreezeFrame(Dem_EventIdType EventId);
static sint8 Dem_MapDebounceCounterToFdc(sint8 counter, sint8 limit);
static uint8 Dem_FilterMatches(Dem_EventIdType EventId);
static void Dem_EvaluateAging(Dem_EventIdType EventId);
static void Dem_EvaluatePendingClear(Dem_EventIdType EventId);
static uint8 Dem_BeginOperationCycle(uint8 notifyFim);
static uint8 Dem_EndOperationCycle(uint8 notifyFim);
static void Dem_EvaluateCycleEnd(Dem_EventIdType EventId);
static void Dem_NotifyFimIfStatusChanged(Dem_EventIdType EventId, uint8 prevStatus);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_GetVersionInfo
 * ---------------------------------------------------------------------- */

void Dem_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_VERSION_INFO, DEM_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = DEM_VENDOR_ID;
    versioninfo->moduleID         = DEM_MODULE_ID;
    versioninfo->sw_major_version = DEM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = DEM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = DEM_SW_PATCH_VERSION;
}

/* ======================================================================
 * Interface ECU State Manager <=> Dem
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_PreInit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_Init
 * ---------------------------------------------------------------------- */

/**
 * \brief   DEM を初期化する。NvM から前回起動の DTC 状態を復元する。
 *
 * \details NvM_Init() 完了後に呼び出すこと。
 *          NVM_BLOCK_ID_DEM_MAGIC のマジックバイトが有効 (0xDE) なら
 *          NVM_BLOCK_ID_DEM_STATUS / _DEM_AGING / _DEM_EXTENDED から前回の状態を
 *          復元し、経年回復 (Dem_EvaluateAging) と PendingDTC 自動クリア
 *          (Dem_EvaluatePendingClear) を評価したうえで、
 *          TF / TFTOC をクリアし TNCTOC (今サイクル未テスト) を立てる
 *          （新しい操作サイクルの開始）。ExtendedData (Dem_OccurrenceCounter) は
 *          サイクル境界での評価対象ではないため、そのまま復元するだけでよい。
 *          マジックバイトが無効なら全イベントを初期状態にして NvM へ書き込む。
 *
 * \note    ServiceID 0x01 は実仕様では未実装の `Dem_PreInit`（[SWS_Dem_00180]）
 *          に割り当てられており、本関数の正しい ServiceID は 0x02 である。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                        持たないため）。
 *
 * \pre        EcuM_Init() から、Com_Init() の後に呼び出すこと。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_Init(const Dem_ConfigType* ConfigPtr)
{
    (void)ConfigPtr; /* 本プロジェクトは post-build 設定を持たない（Dem.h 参照） */
    uint8 magic = 0U;
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_MAGIC, &magic);

    if (magic == DEM_NVM_MAGIC_BYTE)
    {
        /* NvM から前回の最終状態を復元する */
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS,   Dem_StatusTable);
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_AGING,    Dem_AgingCounter);
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);

        /* 新しい操作サイクルの開始（前サイクルの最終結果での、経年回復・PendingDTC の判定を含む。END で評価済みなら、しない） */
        (void)Dem_BeginOperationCycle(0U);   /* FiM は、Dem より後に初期化されるので、通知しない */

        /* 経年回復判定とサイクル開始処理で変化した内容を反映する */
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING,  Dem_AgingCounter);

        DET_LOGI(TAG, "Init NvM restored ev=%u", (unsigned)DEM_EVENT_COUNT);
    }
    else
    {
        /* 初回起動または EEPROM 破損: 全イベントを初期状態に設定 */
        for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
        {
            Dem_StatusTable[i]      = DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR
                                     | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
            Dem_AgingCounter[i]     = 0U;
            Dem_OccurrenceCounter[i] = 0U;
        }
        (void)Dem_BeginOperationCycle(0U);   /* 初期状態のまま変わらない。デバウンスの初期化を、復元時と共通にする */
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS,   Dem_StatusTable);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING,    Dem_AgingCounter);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);
        uint8 magicVal = DEM_NVM_MAGIC_BYTE;
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_MAGIC, &magicVal);
        DET_LOGI(TAG, "Init first-run ev=%u", (unsigned)DEM_EVENT_COUNT);
    }

    /* FreezeFrame は RAM のみで管理するため EEPROM 復元はなく、常に未記録から開始する
     * （デバウンスカウンタは、Dem_BeginOperationCycle() が 0 へ戻す） */
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        Dem_FreezeFrameValid[i]    = 0U;
        Dem_RecordClearPending[i]  = 0U;
        Dem_RecordCapturePending[i] = 0U;
    }

    Dem_DTCSettingEnabled = 1U;
    Dem_FilterActive      = 0U;
    for (uint8 c = 0U; c < DEM_CLIENT_COUNT; c++)
    {
        Dem_Selection[c].selected = 0U;
        Dem_Selection[c].dtc      = 0U;
        Dem_Selection[c].eventId  = 0U;
        Dem_Selection[c].result   = E_NOT_OK;
        Dem_RecordLock[c].locked  = 0U;
        Dem_RecordLock[c].eventId = 0U;
        for (uint8 k = 0U; k < (uint8)DEM_RECORD_KIND_COUNT; k++)
        {
            Dem_Selection[c].record[k].selected  = 0U;
            Dem_Selection[c].record[k].number    = 0U;
            Dem_Selection[c].record[k].delivered = 0U;
        }
    }

    /* [SWS_Dem_00853]: DemOperationCycleAutostart の操作サイクルは、Dem_Init() が開始する */
    for (uint8 c = 0U; c < DEM_OPERATION_CYCLE_COUNT; c++)
    {
        Dem_OperationCycleState[c] = DEM_CYCLE_STATE_START;
    }

    Dem_Initialized = 1U;
}

/* ----------------------------------------------------------------------
 * Dem_Shutdown
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Interface BSW modules / SW-Components <=> Dem
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_ClearDTC
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem_SelectDTC() で選んだ DTC（または DTC グループ）をクリアし、NvM (EEPROM) を初期状態へ戻す
 *          （[SWS_Dem_00665]、Dem.h 参照）。
 *
 * \par 仕様
 *          実仕様どおりの 2 段階: 先に `Dem_SelectDTC(ClientId, DTC, DTCFormat, DTCOrigin)` で対象を選び、
 *          （任意で `Dem_GetDTCSelectionResultForClearDTC()` で選択の結果を確かめ、）本関数を呼ぶ。
 *          選択は、次に `Dem_SelectDTC()` を呼ぶまで、クライアントごとに残る（[SWS_Dem_01253]）。
 *
 *          選んだのが `DEM_GROUP_ALL_DTCS`（0xFFFFFF）なら、[SWS_Dem_01203] のとおり、選んだ DTCOrigin の
 *          全イベントをクリアする。それ以外は、一致するイベント 1 件だけをクリアする。
 *
 * \par 実装
 *          実仕様は、非同期（`DEM_PENDING`）・複数クライアント（`DEM_CLEAR_BUSY`、`DEM_BUSY`）を前提とするが、
 *          本プロジェクトは常に同期的に完了するため、`DEM_PENDING`・`DEM_BUSY` は返さない。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 *
 * \retval  E_OK                    正常クリア。
 * \retval  E_NOT_OK                未初期化、ClientId が範囲外、または Dem_SelectDTC() が呼ばれていない。
 * \retval  DEM_WRONG_DTC           選んだ DTC に一致するイベントが無い、または DTCFormat が UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN     選んだ DTCOrigin が非対応。
 * \retval  DEM_CLEAR_MEMORY_ERROR  不揮発メモリへの書き込みを積めなかった。
 *
 * \AUTOSARReq     {SWS_Dem_00665, SWS_Dem_01203, SWS_Dem_01253, SWS_Dem_01255}
 * \ServiceID      {0x23}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ClearDTC(uint8 ClientId)
{
    if (Dem_CheckSelected(ClientId, DEM_API_ID_CLEAR_DTC) != E_OK)
    {
        return E_NOT_OK;
    }

    const Dem_SelectionType* sel = &Dem_Selection[ClientId];

    if (sel->result != E_OK)
    {
        return sel->result;   /* DEM_WRONG_DTC / DEM_WRONG_DTCORIGIN */
    }

    if (sel->dtc == DEM_GROUP_ALL_DTCS)
    {
        uint8 changed = 0U;
        for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
        {
            changed |= Dem_ClearOne(i);
        }
        if (changed != 0U)
        {
            /* 全イベントをクリアし終えてから、まとめて通知する（途中の、中途半端な状態で、FiM に評価させない） */
            for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
            {
                FiM_DemTriggerOnMonitorStatus(i);
            }
        }
        DET_LOGI(TAG, "ClearDTC all ok");
    }
    else
    {
        if (Dem_ClearOne(sel->eventId) != 0U)
        {
            FiM_DemTriggerOnMonitorStatus(sel->eventId);
        }
        DET_LOGI(TAG, "ClearDTC dtc=0x%06lX ok", (unsigned long)sel->dtc);
    }

    /* 不揮発メモリへの書き込みを積む（DemClearDTCBehavior = DEM_CLRRESP_NONVOLATILE_TRIGGER 相当: 書き込みの完了は待たない、
     * [SWS_Dem_00571]）。積めなかったとき（NvM が未初期化、ブロックが書き込み保護中など）は、RAM 上はクリア済み（巻き戻しはしない）
     * だが、再起動すると復活しうるので、成功とは返さない（[SWS_Dem_01057] は完了を待つ設定の話だが、同じ値で失敗を知らせる）。
     * 3 ブロックとも積むのは、失敗を見つけても、残りの積み忘れを作らないため。 */
    const Std_ReturnType writeStatus   = NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS,   Dem_StatusTable);
    const Std_ReturnType writeAging    = NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING,    Dem_AgingCounter);
    const Std_ReturnType writeExtended = NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);
    if ((writeStatus != E_OK) || (writeAging != E_OK) || (writeExtended != E_OK))
    {
        DET_LOGE(TAG, "ClearDTC: NvM_WriteBlock failed (status=%u aging=%u ext=%u)",
                 (unsigned)writeStatus, (unsigned)writeAging, (unsigned)writeExtended);
        return DEM_CLEAR_MEMORY_ERROR;
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_ClearPrestoredFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetComponentFailed
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDTCSelectionResult
 * ---------------------------------------------------------------------- */

/**
 * \brief   直近の Dem_SelectDTC() が、有効な DTC（または DTC グループ）を選んだかを返す（[SWS_Dem_91023]、Dem.h 参照）。
 *
 * \param[in]  ClientId  呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 *
 * \retval  E_OK                 選んだ DTC（または DTC グループ）が有効。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、または Dem_SelectDTC() が呼ばれていない。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、または DTCFormat が UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_91023, SWS_Dem_01253}
 * \ServiceID      {0xb8}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_GetDTCSelectionResult(uint8 ClientId)
{
    if (Dem_CheckSelected(ClientId, DEM_API_ID_GET_DTC_SELECTION_RESULT) != E_OK)
    {
        return E_NOT_OK;
    }
    return Dem_Selection[ClientId].result;
}

/* ----------------------------------------------------------------------
 * Dem_GetDTCSelectionResultForClearDTC
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem_GetDTCSelectionResult() と同じだが、DTC 消去の設定を考慮した結果を返す（[SWS_Dem_91020]、Dem.h 参照）。
 *
 * \par 仕様
 *          単一の DTC の消去だけを許す設定（DemClearDTCLimitation）は持たないため、結果は
 *          Dem_GetDTCSelectionResult() と同じ。ただし、Dem_SelectDTC() が呼ばれていないときは、
 *          E_NOT_OK ではなく DEM_WRONG_DTC を返す（本関数の戻り値表に E_NOT_OK が無いため。[SWS_Dem_01261]）。
 * \param[in]  ClientId  呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 *
 * \retval  E_OK                 選んだ DTC（または DTC グループ）が有効。
 * \retval  E_NOT_OK             未初期化、または ClientId が範囲外。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または Dem_SelectDTC() が呼ばれていない。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_91020, SWS_Dem_01253, SWS_Dem_01261}
 * \ServiceID      {0xbb}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_GetDTCSelectionResultForClearDTC(uint8 ClientId)
{
    if (Dem_CheckClient(ClientId, DEM_API_ID_GET_DTC_SELECTION_RESULT_FOR_CLEAR_DTC) != E_OK)
    {
        return E_NOT_OK;
    }

    if (Dem_Selection[ClientId].selected == 0U)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DTC_SELECTION_RESULT_FOR_CLEAR_DTC, DEM_E_WRONG_CONDITION);
        return DEM_WRONG_DTC;   /* [SWS_Dem_01261]: 選択が無い＝無効な選択 */
    }
    return Dem_Selection[ClientId].result;
}

/* ----------------------------------------------------------------------
 * Dem_GetEventUdsStatus
 * ---------------------------------------------------------------------- */

/**
 * \brief   指定イベントの UDS DTC ステータスバイトを取得する（[SWS_Dem_91008]）。
 *
 * \par 実装
 *          実仕様は本関数を SW-C や FiM 等の BSW モジュールがイベント単位で
 *          使うためのものと位置づけ、Dcm は DTC 単位の `Dem_GetStatusOfDTC`
 *          を使うと規定する（同関数の Note 参照）。本プロジェクトは
 *          `Dem_GetStatusOfDTC`（DTC→EventId 変換を内蔵する別 API）を持たず、
 *          Dcm 側は既に解決済みの EventId で直接本関数を呼ぶ既存の簡略化を
 *          そのまま踏襲する（本関数の改名以前から変わらない設計）。
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
Std_ReturnType Dem_GetEventUdsStatus(Dem_EventIdType EventId, Dem_UdsStatusByteType* UDSStatusByte)
{
    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_EVENT_UDS_STATUS, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if (EventId >= DEM_EVENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_EVENT_UDS_STATUS, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }

    if (UDSStatusByte == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_EVENT_UDS_STATUS, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *UDSStatusByte = Dem_StatusTable[EventId] & DEM_STATUS_AVAILABILITY_MASK;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetMonitorStatus
 * ---------------------------------------------------------------------- */

Std_ReturnType Dem_GetMonitorStatus(Dem_EventIdType EventId, Dem_MonitorStatusType* MonitorStatus)
{
    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_MONITOR_STATUS, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if (EventId >= DEM_EVENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_MONITOR_STATUS, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;   /* [SWS_Dem_01288] */
    }

    if (MonitorStatus == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_MONITOR_STATUS, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const uint8 status = Dem_StatusTable[EventId];   /* statusAvailabilityMask では隠さない（bit6 を見せる） */
    Dem_MonitorStatusType monitor = 0U;

    if ((status & DEM_STATUS_TEST_FAILED) != 0U)
    {
        monitor |= DEM_MONITOR_STATUS_TF;
    }
    if ((status & DEM_STATUS_NOT_COMPLETED_THIS_CYCLE) != 0U)
    {
        monitor |= DEM_MONITOR_STATUS_TNCTOC;
    }

    *MonitorStatus = monitor;   /* [SWS_Dem_01287] */
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetDebouncingOfEvent
 * ---------------------------------------------------------------------- */

/**
 * \brief   イベントのデバウンスの状態を取得する（[SWS_Dem_00730]、Dem.h 参照）。
 *
 * \details 内部のデバウンスカウンタから、`Dem_DebouncingStateType` のビットを導く（FDC と同じ区分）。
 *          DEM_DTR_UPDATE（bit4）は、テスト完了のとき立てる（有効条件・保存条件を持たず、常に成立しているため）。
 *
 * \param[in]   EventId          イベント ID（`DemConf_DemEventParameter_*`）。
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
Std_ReturnType Dem_GetDebouncingOfEvent(Dem_EventIdType EventId, Dem_DebouncingStateType* DebouncingState)
{
    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DEBOUNCING_OF_EVENT, DEM_E_UNINIT);
        return E_NOT_OK;
    }
    if (DebouncingState == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DEBOUNCING_OF_EVENT, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (EventId >= DEM_EVENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DEBOUNCING_OF_EVENT, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }

    const sint8 counter = Dem_DebounceCounter[EventId];
    const sint8 limit   = Dem_DebounceLimitTable[EventId];
    Dem_DebouncingStateType state = 0U;

    if (counter >= limit)
    {
        state |= DEM_FINALLY_DEFECTIVE;
    }
    else if (counter > 0)
    {
        state |= DEM_TEMPORARILY_DEFECTIVE;
    }
    else if ((counter < 0) && (counter > -limit))
    {
        state |= DEM_TEMPORARILY_HEALED;
    }
    else
    {
        /* 中立（FDC = 0）、または成功が確定（FDC = -128）: Defective / Healed のビットは立てない */
    }

    if ((counter >= limit) || (counter <= -limit))
    {
        state |= (Dem_DebouncingStateType)(DEM_TEST_COMPLETE | DEM_DTR_UPDATE);   /* テスト完了（両端） */
    }

    *DebouncingState = state;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetDTCOfEvent
 * ---------------------------------------------------------------------- */

/**
 * \brief   イベント ID から DTC コードを取得する（[SWS_Dem_00198]/[SWS_Dem_00269]）。
 *
 * \par 実装
 *          本プロジェクトの `Dem_DtcTable[]`（Dem_PBCfg.c）は UDS 3-byte 形式の
 *          DTC 値のみを構成しており、OBD/J1939 形式は一切保持しないため、
 *          `DTCFormat` に `DEM_DTC_FORMAT_UDS` 以外を渡された場合は
 *          `DEM_E_NO_DTC_AVAILABLE`（要求フォーマットの DTC が構成されていない）
 *          を返す。
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
Std_ReturnType Dem_GetDTCOfEvent(Dem_EventIdType EventId, Dem_DTCFormatType DTCFormat, uint32* DTCOfEvent)
{
    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DTC_OF_EVENT, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if (EventId >= DEM_EVENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DTC_OF_EVENT, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }

    if (DTCOfEvent == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DTC_OF_EVENT, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (DTCFormat != DEM_DTC_FORMAT_UDS)
    {
        return DEM_E_NO_DTC_AVAILABLE;
    }

    *DTCOfEvent = Dem_DtcTable[EventId];
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetDTCSuppression
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetFaultDetectionCounter
 * ---------------------------------------------------------------------- */

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
 * \par 実装
 *          デバウンスカウンタ生値を [SWS_Dem_00415] 通り -128〜127 へ
 *          線形写像して返す (`Dem_MapDebounceCounterToFdc()`参照)。
 *
 * \AUTOSARReq     {SWS_Dem_00203}
 * \AUTOSARReq     {SWS_Dem_00415}
 *
 * \param[in]   EventId               イベント ID (DemConf_DemEventParameter_* 定数)。
 * \param[out]  FaultDetectionCounter 写像後の Fault Detection Counter の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  未初期化、EventId が範囲外、または FaultDetectionCounter が NULL。
 *
 * \ServiceID      {0x3e}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetFaultDetectionCounter(Dem_EventIdType EventId, sint8* FaultDetectionCounter)
{
    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_FAULT_DETECTION_COUNTER, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if (EventId >= DEM_EVENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_FAULT_DETECTION_COUNTER, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }

    if (FaultDetectionCounter == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_FAULT_DETECTION_COUNTER, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *FaultDetectionCounter = Dem_MapDebounceCounterToFdc(Dem_DebounceCounter[EventId], Dem_DebounceLimitTable[EventId]);
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetIndicatorStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetEventFreezeFrameDataEx
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetEventExtendedDataRecordEx
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetEventMemoryOverflow
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNumberOfEventMemoryEntries
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_ResetEventDebounceStatus
 * ---------------------------------------------------------------------- */

/**
 * \brief   イベントのデバウンスを、止める / 戻す（[SWS_Dem_00683]、Dem.h 参照）。
 *
 * \details RESET: デバウンスカウンタを 0 へ戻す（FDC も 0 になる、[SWS_Dem_00684]）。FREEZE: 止める対象のタイマが、
 *          カウンタ式のデバウンスには無いので、何もしない（[SWS_Dem_00685] は、タイマ式のデバウンスの規定）。
 *          どちらも、イベントの UDS ステータスには影響しない。
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
Std_ReturnType Dem_ResetEventDebounceStatus(Dem_EventIdType EventId, Dem_DebounceResetStatusType DebounceResetStatus)
{
    if (EventId >= DEM_EVENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_RESET_EVENT_DEBOUNCE_STATUS, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }
    if ((DebounceResetStatus != DEM_DEBOUNCE_STATUS_FREEZE) && (DebounceResetStatus != DEM_DEBOUNCE_STATUS_RESET))
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_RESET_EVENT_DEBOUNCE_STATUS, DEM_E_PARAM_DATA);
        return E_NOT_OK;
    }

    if (DebounceResetStatus == DEM_DEBOUNCE_STATUS_RESET)
    {
        Dem_DebounceCounter[EventId] = 0;
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_ResetEventStatus
 * ---------------------------------------------------------------------- */

/**
 * \brief   イベントの失敗ステータスを、PASSED を報告せずに戻す（[SWS_Dem_00187]、Dem.h 参照）。
 *
 * \details TestFailed（UDS ステータスの bit0）を 0 にし、デバウンスカウンタを 0 へ戻す。監視ステータスと、
 *          UDS ステータスの bit6 は変えない。DTC の設定が無効の間は、何もせず E_OK を返す（[SWS_Dem_00079]）。
 *          そうでなければ、この操作サイクルでテスト済み（bit6 = 0）のイベントは、E_NOT_OK で拒否する（[SWS_Dem_00638]）。
 *          TestFailed が変わったときだけ、NvM へ書き込み、FiM へ通知する。
 *
 * \param[in]  EventId  イベント ID（`DemConf_DemEventParameter_*`）。
 *
 * \retval  E_OK      受理した（DTC の設定が無効で、何もしなかった場合を含む）。
 * \retval  E_NOT_OK  EventId が構成にない、またはこの操作サイクルでテスト済み。
 *
 * \note    Dem_SetEventStatus() と同じく、未初期化チェック（DEM_E_UNINIT）の対象外（[SWS_Dem_00124]）。
 *
 * \AUTOSARReq     {SWS_Dem_00187, SWS_Dem_00331, SWS_Dem_00638}
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ResetEventStatus(Dem_EventIdType EventId)
{
    /* [SWS_Dem_00124]: Dem_ResetEventStatus は、未初期化チェック（DEM_E_UNINIT）の対象外。 */

    if (EventId >= DEM_EVENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_RESET_EVENT_STATUS, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }

    if (!Dem_DTCSettingEnabled)
    {
        /* [SWS_Dem_00079]: DTC の設定が無効の間は、UDS ステータスを更新しない（テスト済みかどうかに関係なく、E_OK） */
        DET_LOGD(TAG, "ev=%u reset ignored: DTC setting disabled", (unsigned)EventId);
        return E_OK;
    }

    const uint8 prev = Dem_StatusTable[EventId];
    if ((prev & DEM_STATUS_NOT_COMPLETED_THIS_CYCLE) == 0U)
    {
        /* [SWS_Dem_00638]: この操作サイクルで、既にテスト済み */
        DET_LOGD(TAG, "ev=%u reset rejected: already tested in this operation cycle", (unsigned)EventId);
        return E_NOT_OK;
    }

    Dem_DebounceCounter[EventId] = 0;
    Dem_StatusTable[EventId]     = (uint8)(prev & (uint8)(~DEM_STATUS_TEST_FAILED));

    if (Dem_StatusTable[EventId] != prev)
    {
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);
        FiM_DemTriggerOnMonitorStatus(EventId);
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_PrestoreFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SelectDTC
 * ---------------------------------------------------------------------- */

/**
 * \brief   以降の Dem_ClearDTC() などの対象にする、DTC（または DTC グループ）を選ぶ（[SWS_Dem_91016]、Dem.h 参照）。
 *
 * \par 仕様
 *          選択は、クライアントごとに、次に本関数を呼ぶまで残る（[SWS_Dem_01253]）。選択が有効かどうかは、
 *          本関数の戻り値ではなく、Dem_GetDTCSelectionResult() など、選択に続く API の戻り値で分かる
 *          （[SWS_Dem_01254]）。`DTC` に `DEM_GROUP_ALL_DTCS`（0xFFFFFF）を渡すと、全 DTC のグループを選ぶ。
 *
 * \par 実装
 *          本プロジェクトは、DTC グループを、これ 1 つだけ持つ。
 *
 * \param[in]  ClientId   呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 * \param[in]  DTC        選ぶ DTC（24-bit UDS 形式）、または DEM_GROUP_ALL_DTCS。
 * \param[in]  DTCFormat  DTC のフォーマット（DEM_DTC_FORMAT_UDS のみ対応）。
 * \param[in]  DTCOrigin  イベントメモリ（DEM_DTC_ORIGIN_PRIMARY_MEMORY のみ構成する）。
 *
 * \retval  E_OK      選択の処理を受け付けた（結果は、Dem_GetDTCSelectionResult() で分かる）。
 * \retval  E_NOT_OK  未初期化、または ClientId が範囲外。
 *
 * \AUTOSARReq     {SWS_Dem_91016, SWS_Dem_01253, SWS_Dem_01254}
 * \ServiceID      {0xb7}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SelectDTC(uint8 ClientId, uint32 DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin)
{
    if (Dem_CheckClient(ClientId, DEM_API_ID_SELECT_DTC) != E_OK)
    {
        return E_NOT_OK;
    }

    Dem_SelectionType* sel = &Dem_Selection[ClientId];

    sel->selected = 1U;
    sel->dtc      = DTC;
    sel->eventId  = 0U;
    sel->result   = E_OK;
    for (uint8 k = 0U; k < (uint8)DEM_RECORD_KIND_COUNT; k++)
    {
        sel->record[k].selected  = 0U;   /* レコードの選択は、DTC を選び直すと破棄する */
        sel->record[k].delivered = 0U;
    }

    if (DTCFormat != DEM_DTC_FORMAT_UDS)
    {
        sel->result = DEM_WRONG_DTC;   /* 要求した形式の DTC は構成されていない */
    }
    else if (DTCOrigin != DEM_DTC_ORIGIN_PRIMARY_MEMORY)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_SELECT_DTC, DEM_E_WRONG_CONFIGURATION);
        sel->result = DEM_WRONG_DTCORIGIN;
    }
    else if (DTC != DEM_GROUP_ALL_DTCS)
    {
        sel->result = Dem_FindEventOfDtc(DTC, &sel->eventId) ? E_OK : DEM_WRONG_DTC;
    }
    else
    {
        /* DEM_GROUP_ALL_DTCS: 全 DTC のグループ（イベントは決まらない） */
    }

    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_SetComponentAvailable
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDTCSuppression
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetEnableCondition
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetEventAvailable
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetEventFailureCycleCounterThreshold
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetEventStatus
 * ---------------------------------------------------------------------- */

/**
 * \brief   イベントの発生/消滅を DEM に通知する (モニタからの生のテスト結果、
 *          [SWS_Dem_00183])。
 *
 * \details counter-based debouncing（仕様どおり）。モニタの報告に応じて、デバウンスカウンタを動かす。
 *          - PREFAILED: カウンタを +1（失敗側）。PREPASSED: カウンタを -1（成功側）。ステップ幅は 1
 *            （SWS_Dem_00418/00419）。反対側にいるときは、先に 0 へ戻す（jump-up / jump-down の値が 0。
 *            SWS_Dem_00422〜00425）。
 *          - FAILED: カウンタを、失敗側のしきい値（+limit）へ、直接ジャンプさせる。PASSED: 成功側のしきい値
 *            （-limit）へ、直接ジャンプさせる（SWS_Dem_00420/00421）。
 *          カウンタがイベントごとの確定閾値 (Dem_DebounceLimitTable[EventId])
 *          に達した瞬間にのみ、DTC ステータスの TF/TFTOC/PDTC/CDTC/TFSLC を確定し
 *          NvM_WriteBlock() でステータステーブル全体を永続化する
 *          (FAILED 確定時は併せて FreezeFrame も更新し、ExtendedData の
 *          故障確定回数カウンタを +1 する)。
 *          まだ閾値に達していない間 (PRE-FAILED / PRE-PASSED) は
 *          DTC ステータスビットを変更しない。
 *          以前は、FAILED/PASSED を、毎回の判定として、報告のたびに ±1 で数える独自の方式で、
 *          PREPASSED/PREFAILED を受け付けなかった（2026-11 に仕様どおりへ変更）。
 *
 * \param[in]  EventId      イベント ID (DemConf_DemEventParameter_* 定数)。
 * \param[in]  EventStatus  DEM_EVENT_STATUS_FAILED / PASSED / PREFAILED / PREPASSED。
 *
 * \retval  E_OK      正常に受理した（デバウンス未確定・DTC設定無効化中の
 *                    無視も含む。実仕様も「呼び出し元のBSWモジュールは戻り値を
 *                    無視してよい」と明記しているため、実質的な失敗のみを
 *                    E_NOT_OK とする）。
 * \retval  E_NOT_OK  EventId が範囲外、または EventStatus が上の 4 値以外。
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
Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId,
                                   Dem_EventStatusType EventStatus)
{
    /* [SWS_Dem_00124]: Dem_SetEventStatus 相当のため未初期化チェック対象外。 */

    if (EventId >= DEM_EVENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_SET_EVENT_STATUS, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }
    if ((EventStatus != DEM_EVENT_STATUS_FAILED) && (EventStatus != DEM_EVENT_STATUS_PASSED)
        && (EventStatus != DEM_EVENT_STATUS_PREFAILED) && (EventStatus != DEM_EVENT_STATUS_PREPASSED))
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_SET_EVENT_STATUS, DEM_E_PARAM_DATA);
        return E_NOT_OK;
    }

    if (Dem_OperationCycleState[DemConf_DemOperationCycle_POWER] == DEM_CYCLE_STATE_END)
    {
        /* [SWS_Dem_00482]: 操作サイクルが終了している間の報告は、無視する（UDS ステータスもデバウンスも、変えない） */
        DET_LOGD(TAG, "ev=%u report ignored: operation cycle ended", (unsigned)EventId);
        return E_OK;
    }

    if (!Dem_DTCSettingEnabled)
    {
        /* Dem_DisableDTCSetting() 中（Dcm UDS SID 0x85 ControlDTCSetting off）:
         * 報告そのものを丸ごと無視する。デバウンスカウンタも進めない
         * （再有効化した瞬間に、無効化中に蓄積した中途半端なカウントで
         * 誤確定させないため）。 */
        DET_LOGD(TAG, "ev=%u report ignored: DTC setting disabled", (unsigned)EventId);
        return E_OK;
    }

    const sint8 limit       = Dem_DebounceLimitTable[EventId];
    const sint8 prevCounter = Dem_DebounceCounter[EventId];
    sint8 counter = prevCounter;

    if (EventStatus == DEM_EVENT_STATUS_FAILED)
    {
        /* [SWS_Dem_00420]: 失敗側のしきい値へ、直接ジャンプさせて、すぐに確定する。 */
        counter = limit;
    }
    else if (EventStatus == DEM_EVENT_STATUS_PASSED)
    {
        /* [SWS_Dem_00421]: 成功側のしきい値へ、直接ジャンプさせて、すぐに確定する。 */
        counter = (sint8)(-limit);
    }
    else if (EventStatus == DEM_EVENT_STATUS_PREFAILED)
    {
        /* [SWS_Dem_00418]: 失敗側へ 1 つ進める。成功側（負）にいたら、先に 0 へ戻す
         * （jump-up。[SWS_Dem_00424]/[SWS_Dem_00425]。値は 0）。そうしないと、反対側の確定状態から
         * 数え始めるせいで、反転に 2*limit 回分の報告が必要になってしまう。 */
        if (counter < 0)
        {
            counter = 0;
        }
        if (counter < limit)
        {
            counter++;
        }
    }
    else /* DEM_EVENT_STATUS_PREPASSED */
    {
        /* [SWS_Dem_00419]: 成功側へ 1 つ進める。失敗側（正）にいたら、先に 0 へ戻す
         * （jump-down。[SWS_Dem_00422]/[SWS_Dem_00423]。値は 0）。 */
        if (counter > 0)
        {
            counter = 0;
        }
        if (counter > -limit)
        {
            counter--;
        }
    }

    if (counter == prevCounter)
    {
        /* カウンタが上下限で飽和済み（既に確定済みの状態が継続）: 変化なしのため何もしない。
         * これにより、確定後も毎サイクル報告され続けるイベント（ADC_VOLT_LOW 等）で
         * 同じデバウンスログが繰り返し出力されることを防ぐ。 */
        return E_OK;
    }
    Dem_DebounceCounter[EventId] = counter;

    const uint8 wasFailedConfirmed = (prevCounter >= limit)  ? 1U : 0U;
    const uint8 wasPassedConfirmed = (prevCounter <= -limit) ? 1U : 0U;
    const uint8 nowFailedConfirmed = (counter     >= limit)  ? 1U : 0U;
    const uint8 nowPassedConfirmed = (counter     <= -limit) ? 1U : 0U;

    uint8 prev   = Dem_StatusTable[EventId];
    uint8 status = prev;

    if (nowFailedConfirmed && !wasFailedConfirmed)
    {
        /* デバウンス確定: FAILED */
        status |=  DEM_STATUS_TEST_FAILED;
        status |=  DEM_STATUS_TF_THIS_OP_CYCLE;
        status |=  DEM_STATUS_PENDING;
        status |=  DEM_STATUS_CONFIRMED;
        status |=  DEM_STATUS_FAILED_SINCE_CLEAR;
        status &= (uint8)(~DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR);
        status &= (uint8)(~DEM_STATUS_NOT_COMPLETED_THIS_CYCLE);

        DET_LOGW(TAG, "FAILED ev=%u dtc=0x%06lX",
                 (unsigned)EventId, (unsigned long)Dem_DtcTable[EventId]);
    }
    else if (nowPassedConfirmed && !wasPassedConfirmed)
    {
        /* デバウンス確定: PASSED */
        status &= (uint8)(~DEM_STATUS_TEST_FAILED);
        /* DEM_STATUS_TF_THIS_OP_CYCLE (bit1) は PASSED 確定でクリアしては
         * ならない（[SWS_Dem_00389] Figure 7.18。2026-09 是正）。仕様の状態
         * 遷移図は TFTOC を Dem_SetOperationCycleState(START) または
         * ClearDTC でのみ FALSE に戻すと定義しており、PASSED 確定はそもそも
         * 遷移条件に含まれない（同一操作サイクル内で「一度でも FAILED
         * 確定した」という事実は、その後 PASSED に回復しても消えない）。
         * 以前は TEST_FAILED と一緒に無条件でクリアしていたため、
         * Dem_EvaluatePendingClear() の cleanCycle 判定
         * （TFTOC==0 && NOT_COMPLETED_THIS_CYCLE==0 なら次回起動時に
         * PendingDTC を自動クリア）が誤って「このサイクルは健全だった」と
         * 判定し、本来保持すべき PendingDTC を不当に早期クリアしていた。
         * TFTOC 自体は Dem_Init() の新サイクル開始処理（本ファイル内、
         * サイクル開始時に必ずクリア）と ClearDTC 系関数（状態バイト全体を
         * リセット）で正しくクリアされるため、ここで削除しても副作用はない。 */
        status &= (uint8)(~DEM_STATUS_NOT_COMPLETED_THIS_CYCLE);
        /* SWS_Dem_00392 (Figure 7.21): testNotCompletedSinceLastClear は
         * FAILED・PASSED いずれの確定でもクリアされる（「一度もテストされて
         * いない」を表すビットのため、確定結果の方向は問わない）。FAILED 確定
         * 側では既にクリアしていたが、PASSED 確定側で対称的にクリアしていな
         * かった（ClearDTC 後に一度も FAILED 化せず PASSED のみが確定し続ける
         * イベントで、テスト済みにもかかわらず立ったままになる不具合）。 */
        status &= (uint8)(~DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR);
        /* CDTC / PDTC / TFSLC は保持 — SID 0x14 でのみクリア可能 */
    }
    else
    {
        /* PRE-FAILED / PRE-PASSED: まだ確定していないため DTC ステータスは変更しない */
        DET_LOGV(TAG, "ev=%u debounce=%d (%s)",
                 (unsigned)EventId, (int)counter,
                 (counter > 0) ? "PREFAILED" : (counter < 0) ? "PREPASSED" : "neutral");
        return E_OK;
    }

    /* ステータスが変化した場合のみ NvM (EEPROM) を更新 */
    if (status != prev)
    {
        Dem_StatusTable[EventId] = status;

        /* デバウンス確定 FAILED の瞬間にのみ FreezeFrame と ExtendedData を更新する。レコードの更新を止められている DTC は、
         * UDS ステータスだけを更新し、レコードの更新は、止めを戻すとき（Dem_EnableDTCRecordUpdate()）まで保留する（[SWS_Dem_00270]） */
        if ((nowFailedConfirmed) != 0U)
        {
            if (Dem_IsRecordLocked(EventId))
            {
                Dem_RecordCapturePending[EventId] = 1U;
            }
            else
            {
                Dem_StoreRecords(EventId);
            }
        }

        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);

        /* 監視ステータスが変わったことを FiM へ知らせ、そのイベントを使う機能の許可状態を、すぐに更新させる
         * （DemTriggerFiMReports 相当、[SWS_Fim_00021]）。FiM の初期化前は、FiM が黙って何もしない。 */
        FiM_DemTriggerOnMonitorStatus(EventId);
    }

    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_SetOperationCycleState
 * ---------------------------------------------------------------------- */

/**
 * \brief   操作サイクルの状態を設定する（[SWS_Dem_00194]、Dem.h 参照）。
 *
 * \details START: 操作サイクルを（再）開始する。既に開始済みなら、再開始する（[SWS_Dem_00483]）。直前の操作サイクルの
 *          最終ステータスで、経年回復（Aging）と PendingDTC の自動クリアを評価し（END で評価済みなら、しない）、
 *          イベントごとに TestFailed・TestFailedThisOperationCycle をクリア、TestNotCompletedThisOperationCycle を立て、デバウンスカウンタを
 *          0 へ戻す。開始してから、`Dem_SetEventStatus()` の報告を、再び受け付ける（[SWS_Dem_00481]）。
 *          END: 操作サイクルを終了する。そのサイクルの最終ステータスで、経年回復（Aging）と PendingDTC の
 *          自動クリアを評価する（[SWS_Dem_00390]）。終了している間、`Dem_SetEventStatus()` の報告は無視され、その報告による
 *          UDS ステータスの変化は起きない（[SWS_Dem_00482]。`Dem_ClearDTC()` のような、明示的な操作は、サイクルの
 *          状態に関係なく働く）。既に終了済みなら、何もしない（[SWS_Dem_00484]）。
 *
 * \param[in]  OperationCycleId  操作サイクルの ID（`DemConf_DemOperationCycle_*`）。
 * \param[in]  CycleState        新しい状態（START / END）。
 *
 * \retval  E_OK      受理した。
 * \retval  E_NOT_OK  未初期化、操作サイクルが構成にない（DEM_E_WRONG_CONFIGURATION、[SWS_Dem_01223]）、
 *                    または状態が START / END 以外。
 *
 * \AUTOSARReq     {SWS_Dem_00194, SWS_Dem_00338, SWS_Dem_00481, SWS_Dem_00482, SWS_Dem_00483, SWS_Dem_00484, SWS_Dem_00853}
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SetOperationCycleState(uint8 OperationCycleId, Dem_OperationCycleStateType CycleState)
{
    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_SET_OPERATION_CYCLE_STATE, DEM_E_UNINIT);
        return E_NOT_OK;
    }
    if (OperationCycleId >= DEM_OPERATION_CYCLE_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_SET_OPERATION_CYCLE_STATE, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }
    if ((CycleState != DEM_CYCLE_STATE_START) && (CycleState != DEM_CYCLE_STATE_END))
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_SET_OPERATION_CYCLE_STATE, DEM_E_PARAM_DATA);
        return E_NOT_OK;
    }

    if (CycleState == DEM_CYCLE_STATE_END)
    {
        /* 既に終了済みなら、何もしない（[SWS_Dem_00484]） */
        if (Dem_OperationCycleState[OperationCycleId] != DEM_CYCLE_STATE_END)
        {
            Dem_OperationCycleState[OperationCycleId] = DEM_CYCLE_STATE_END;
            if (Dem_EndOperationCycle(1U) != 0U)
            {
                (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);
                (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING,  Dem_AgingCounter);
            }
            DET_LOGI(TAG, "operation cycle %u ended", (unsigned)OperationCycleId);
        }
        return E_OK;
    }

    /* START: 開始済みでも再開始する（[SWS_Dem_00483]）。全イベントが、この 1 つのサイクルに属する */
    const uint8 changed = Dem_BeginOperationCycle(1U);
    Dem_OperationCycleState[OperationCycleId] = DEM_CYCLE_STATE_START;

    if (changed != 0U)
    {
        /* ステータス・経年回復カウンタが変わったときだけ、書き込む（変わらない再開始で、EEPROM を消耗させない） */
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING,  Dem_AgingCounter);
    }
    DET_LOGI(TAG, "operation cycle %u started", (unsigned)OperationCycleId);
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetOperationCycleState
 * ---------------------------------------------------------------------- */

/**
 * \brief   操作サイクルの状態を取得する（[SWS_Dem_00729]、Dem.h 参照）。
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
Std_ReturnType Dem_GetOperationCycleState(uint8 OperationCycleId, Dem_OperationCycleStateType* CycleState)
{
    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_OPERATION_CYCLE_STATE, DEM_E_UNINIT);
        return E_NOT_OK;
    }
    if (CycleState == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_OPERATION_CYCLE_STATE, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    if (OperationCycleId >= DEM_OPERATION_CYCLE_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_OPERATION_CYCLE_STATE, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }

    *CycleState = Dem_OperationCycleState[OperationCycleId];
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_SetStorageCondition
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetWIRStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Interface Dcm <=> Dem
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_GetTranslationType
 * ---------------------------------------------------------------------- */

/**
 * \brief   本 ECU が構成する DTC 翻訳フォーマットを取得する。
 *
 * \details エラー戻り値の定義が実仕様に存在しないため（[SWS_Dem_00231]）、
 *          ClientId・初期化状態に関わらず常に `DEM_DTC_TRANSLATION_ISO14229_1`
 *          を返す（本プロジェクトが `DemTypeOfDTCSupported` として構成する
 *          唯一の形式）。
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
Dem_DTCTranslationFormatType Dem_GetTranslationType(uint8 ClientId)
{
    (void)ClientId;  /* 単一ECU・単一診断クライアント構成のため未使用（Dem.h 冒頭コメント参照） */

    /* [SWS_Dem_00231]: DemTypeOfDTCSupported の構成値を返す。本プロジェクトは
     * ISO 14229-1 形式のみ構成するため常にこの値（エラー戻り値は実仕様に
     * 定義が無いため未初期化チェックは行わない）。 */
    return DEM_DTC_TRANSLATION_ISO14229_1;
}

/* ----------------------------------------------------------------------
 * Dem_GetDTCStatusAvailabilityMask
 * ---------------------------------------------------------------------- */

/**
 * \brief   DTC ステータス availability mask を取得する。
 *
 * \par 仕様
 *          DCM SID 0x19 の各種応答([SWS_Dem_00060]: statusAvailabilityMask
 *          フィールド)が使う値を返す。本 ECU は ISO 14229-1 Annex B の
 *          bit0(testFailed)/bit2(pendingDTC)/bit3(confirmedDTC)/
 *          bit4(testNotCompletedSinceLastClear)/bit5(testFailedSinceLastClear)
 *          をサポートする。
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
Std_ReturnType Dem_GetDTCStatusAvailabilityMask(uint8 ClientId, Dem_UdsStatusByteType* DTCStatusMask)
{
    (void)ClientId;  /* 単一ECU・単一診断クライアント構成のため未使用（Dem.h 冒頭コメント参照） */

    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DTC_STATUS_AVAILABILITY_MASK, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if (DTCStatusMask == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_DTC_STATUS_AVAILABILITY_MASK, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *DTCStatusMask = DEM_STATUS_AVAILABILITY_MASK;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetStatusOfDTC
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem_SelectDTC() で選んだ DTC のステータスバイトを取得する（[SWS_Dem_00212]、Dem.h 参照）。
 *
 * \par 仕様
 *          Dcm は、イベント ID ではなく、本関数で DTC のステータスを取得する（`Dem_GetEventUdsStatus()` は、
 *          SW-C や BSW モジュールが、イベント単位で使う）。ステータスは、`DemStatusAvailabilityMask` で
 *          マスクして返す。DTC グループを選んでいるときは、DTC が 1 件に決まらないため、DEM_WRONG_DTC を返す。
 * \param[in]   ClientId   呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 * \param[out]  DTCStatus  ステータスバイトの格納先。NULL 禁止。
 *
 * \retval  E_OK                 DTCStatus へ格納した。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、DTCStatus が NULL、または Dem_SelectDTC() が呼ばれていない。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_00212, SWS_Dem_01253}
 * \ServiceID      {0x15}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_GetStatusOfDTC(uint8 ClientId, Dem_UdsStatusByteType* DTCStatus)
{
    if (Dem_CheckSelected(ClientId, DEM_API_ID_GET_STATUS_OF_DTC) != E_OK)
    {
        return E_NOT_OK;
    }

    if (DTCStatus == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_STATUS_OF_DTC, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const Dem_SelectionType* sel = &Dem_Selection[ClientId];

    if (sel->result != E_OK)
    {
        return sel->result;
    }

    if (sel->dtc == DEM_GROUP_ALL_DTCS)
    {
        return DEM_WRONG_DTC;   /* グループは、DTC が 1 件に決まらない */
    }

    *DTCStatus = Dem_StatusTable[sel->eventId] & DEM_STATUS_AVAILABILITY_MASK;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetSeverityOfDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetFunctionalUnitOfDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDTCFilter
 * ---------------------------------------------------------------------- */

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
 * \par 実装
 *          仕様由来の挙動は Dem.h の宣言側コメント参照。
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
 * \retval  E_NOT_OK  未初期化、または DTCFormat / DTCOrigin / FilterWithSeverity が非対応の値。
 *                    非対応の値のときは、前に設定したフィルタも無効にする（[SWS_Dem_01266]）。
 *
 * \AUTOSARReq     {SWS_Dem_00208, SWS_Dem_00057, SWS_Dem_01266}
 * \ServiceID      {0x13}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SetDTCFilter(uint8 ClientId, uint8 DTCStatusMask, Dem_DTCFormatType DTCFormat,
                                Dem_DTCOriginType DTCOrigin, boolean FilterWithSeverity,
                                Dem_DTCSeverityType DTCSeverityMask, boolean FilterForFaultDetectionCounter)
{
    (void)ClientId;         /* 単一ECU・単一診断クライアント構成のため未使用（Dem.h 冒頭コメント参照） */
    (void)DTCSeverityMask;  /* FilterWithSeverity は常に FALSE のみ受け付けるため参照しない */

    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_SET_DTC_FILTER, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    /* [SWS_Dem_01266]: 設定に失敗したら、前のフィルタを残さず、後続の API を E_NOT_OK にする。 */
    Dem_FilterActive = 0U;

    /* [SWS_Dem_00208]: 構成に合わない DTCFormat / DTCOrigin は DEM_E_WRONG_CONFIGURATION を
     * 報告して E_NOT_OK を返す。Severity を持たない本プロジェクトでは
     * FilterWithSeverity=TRUE も同じ扱いとする（学習用簡略化）。 */
    if ((DTCFormat != DEM_DTC_FORMAT_UDS) || (DTCOrigin != DEM_DTC_ORIGIN_PRIMARY_MEMORY) ||
        (FilterWithSeverity != FALSE))
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_SET_DTC_FILTER, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }

    Dem_FilterStatusMask = DTCStatusMask;
    Dem_FilterForFdc     = (FilterForFaultDetectionCounter != FALSE) ? 1U : 0U;
    Dem_FilterNextEvent  = 0U;
    Dem_FilterActive     = 1U;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetNumberOfFilteredDTC
 * ---------------------------------------------------------------------- */

/**
 * \brief   現在のフィルタ条件に一致する DTC の件数を取得する（[SWS_Dem_00214]）。
 *
 * \par 仕様
 *          「次に返す DTC」の位置は進めない（件数を数えるだけ）。常に同期的に
 *          完了する（DEM_PENDING は返さない）。
 *
 * \par 実装
 *          詳細は Dem.h の宣言側コメント参照。走査位置は進めない。
 *
 * \param[in]   ClientId             クライアント識別子。本実装では未使用。
 * \param[out]  NumberOfFilteredDTC  一致した DTC の件数。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  未初期化、NumberOfFilteredDTC が NULL、またはフィルタ未設定（DEM_E_WRONG_CONDITION）。
 *
 * \AUTOSARReq     {SWS_Dem_00214, SWS_Dem_01300}
 * \ServiceID      {0x17}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetNumberOfFilteredDTC(uint8 ClientId, uint16* NumberOfFilteredDTC)
{
    (void)ClientId;  /* 単一ECU・単一診断クライアント構成のため未使用（Dem.h 冒頭コメント参照） */

    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NUMBER_OF_FILTERED_DTC, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if (NumberOfFilteredDTC == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NUMBER_OF_FILTERED_DTC, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (Dem_FilterActive == 0U)
    {
        /* [SWS_Dem_00214]: No DTC filter set。[SWS_Dem_01300]/[SWS_Dem_01267]: DEM_E_WRONG_CONDITION を報告する */
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NUMBER_OF_FILTERED_DTC, DEM_E_WRONG_CONDITION);
        return E_NOT_OK;
    }

    uint16 count = 0U;
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        if (Dem_FilterMatches(i) != 0U)
        {
            count++;
        }
    }
    *NumberOfFilteredDTC = count;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredDTC
 * ---------------------------------------------------------------------- */

/**
 * \brief   フィルタ条件に一致する次の DTC とそのステータスを取得する（[SWS_Dem_00215]）。
 *
 * \par 仕様
 *          呼ぶたびに一致する DTC を 1 件ずつ返し、全件返し終えたら
 *          DEM_NO_SUCH_ELEMENT を返す。常に同期的に完了する（DEM_PENDING は返さない）。
 *
 * \par 実装
 *          詳細は Dem.h の宣言側コメント参照。
 *
 * \param[in]   ClientId   クライアント識別子。本実装では未使用。
 * \param[out]  DTC        DTC 値（UDS 形式、24-bit）。NULL 禁止。
 * \param[out]  DTCStatus  DTC のステータスバイト。NULL 禁止。
 *
 * \retval  E_OK                次の DTC を返した。
 * \retval  E_NOT_OK            未初期化、ポインタが NULL、またはフィルタ未設定（DEM_E_WRONG_CONDITION）。
 * \retval  DEM_NO_SUCH_ELEMENT 一致する DTC をすべて返し終えた。
 *
 * \AUTOSARReq     {SWS_Dem_00215, SWS_Dem_01300}
 * \ServiceID      {0x18}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Synchronous（本実装は DEM_PENDING を返さない）}
 */
Std_ReturnType Dem_GetNextFilteredDTC(uint8 ClientId, uint32* DTC, uint8* DTCStatus)
{
    (void)ClientId;  /* 単一ECU・単一診断クライアント構成のため未使用（Dem.h 冒頭コメント参照） */

    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NEXT_FILTERED_DTC, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if ((DTC == NULL) || (DTCStatus == NULL))
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NEXT_FILTERED_DTC, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (Dem_FilterActive == 0U)
    {
        /* [SWS_Dem_00215]: No DTC filter set。[SWS_Dem_01300]/[SWS_Dem_01267]: DEM_E_WRONG_CONDITION を報告する */
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NEXT_FILTERED_DTC, DEM_E_WRONG_CONDITION);
        return E_NOT_OK;
    }

    while (Dem_FilterNextEvent < DEM_EVENT_COUNT)
    {
        const Dem_EventIdType id = Dem_FilterNextEvent;
        Dem_FilterNextEvent++;
        if (Dem_FilterMatches(id) != 0U)
        {
            *DTC       = Dem_DtcTable[id];
            *DTCStatus = Dem_StatusTable[id] & DEM_STATUS_AVAILABILITY_MASK;
            return E_OK;
        }
    }
    return DEM_NO_SUCH_ELEMENT;
}

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredDTCAndFDC
 * ---------------------------------------------------------------------- */

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
 * \par 実装
 *          詳細は Dem.h の宣言側コメント参照。
 *
 * \param[in]   ClientId                 クライアント識別子。本実装では未使用。
 * \param[out]  DTC                      DTC 値（UDS 形式、24-bit）。NULL 禁止。
 * \param[out]  DTCFaultDetectionCounter Fault Detection Counter。NULL 禁止。
 *
 * \retval  E_OK                次の DTC を返した。
 * \retval  E_NOT_OK            未初期化、ポインタが NULL、またはフィルタ未設定（DEM_E_WRONG_CONDITION）。
 * \retval  DEM_NO_SUCH_ELEMENT 一致する DTC をすべて返し終えた。
 *
 * \AUTOSARReq     {SWS_Dem_00227, SWS_Dem_01300}
 * \ServiceID      {0x3b}
 * \Reentrancy     {Reentrant for different ClientIds, Non Reentrant for the same ClientId}
 * \Synchronicity  {Asynchronous（本実装は同期的に完了するため DEM_PENDING は返さない）}
 */
Std_ReturnType Dem_GetNextFilteredDTCAndFDC(uint8 ClientId, uint32* DTC, sint8* DTCFaultDetectionCounter)
{
    (void)ClientId;  /* 単一ECU・単一診断クライアント構成のため未使用（Dem.h 冒頭コメント参照） */

    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NEXT_FILTERED_DTC_AND_FDC, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if ((DTC == NULL) || (DTCFaultDetectionCounter == NULL))
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NEXT_FILTERED_DTC_AND_FDC, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (Dem_FilterActive == 0U)
    {
        /* [SWS_Dem_00227]: No DTC filter set。[SWS_Dem_01300]/[SWS_Dem_01267]: DEM_E_WRONG_CONDITION を報告する */
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_GET_NEXT_FILTERED_DTC_AND_FDC, DEM_E_WRONG_CONDITION);
        return E_NOT_OK;
    }

    while (Dem_FilterNextEvent < DEM_EVENT_COUNT)
    {
        const Dem_EventIdType id = Dem_FilterNextEvent;
        Dem_FilterNextEvent++;
        if (Dem_FilterMatches(id) != 0U)
        {
            *DTC                      = Dem_DtcTable[id];
            *DTCFaultDetectionCounter = Dem_MapDebounceCounterToFdc(Dem_DebounceCounter[id], Dem_DebounceLimitTable[id]);
            return E_OK;
        }
    }
    return DEM_NO_SUCH_ELEMENT;
}

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredDTCAndSeverity
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetFreezeFrameRecordFilter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredRecord
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDTCByOccurrenceTime
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DisableDTCRecordUpdate
 * ---------------------------------------------------------------------- */

/**
 * \brief   選んだ DTC のレコードの更新と削除を止める（[SWS_Dem_00233]、Dem.h 参照）。
 *
 * \par 仕様
 *          Dcm が、Dem_SelectFreezeFrameData() / Dem_SelectExtendedDataRecord() で読む前に呼び、読み終えてから
 *          Dem_EnableDTCRecordUpdate() で戻す（[SWS_Dem_00270]）。止めている間、その DTC のイベントが
 *          確定 FAILED になっても、FreezeFrame の取り直しと故障確定回数の加算は、止めている間は行わず、
 *          Dem_EnableDTCRecordUpdate() が戻したときに行う（捨てない）。Dem_ClearDTC() が消去しても、そのレコードは
 *          消さず、戻したときに消す（消去のあとに確定していれば、消してから、更新する）。
 *          UDS ステータスの更新には影響しない。他のイベントのレコードも、影響を受けない。
 *          同じクライアントが、1 つしか止められない（[SWS_Dem_00648]）。DTC グループは止められない。
 * \param[in]  ClientId  呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 *
 * \retval  E_OK                 止めた。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、Dem_SelectDTC() が呼ばれていない、または、すでに別の DTC を止めている。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_00233, SWS_Dem_00270, SWS_Dem_00648}
 * \ServiceID      {0x1a}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_DisableDTCRecordUpdate(uint8 ClientId)
{
    if (Dem_CheckSelected(ClientId, DEM_API_ID_DISABLE_DTC_RECORD_UPDATE) != E_OK)
    {
        return E_NOT_OK;
    }

    const Dem_SelectionType* sel  = &Dem_Selection[ClientId];
    Dem_RecordLockType*      lock = &Dem_RecordLock[ClientId];

    if (lock->locked != 0U)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_DISABLE_DTC_RECORD_UPDATE, DEM_E_WRONG_CONDITION);   /* [SWS_Dem_00648] */
        return E_NOT_OK;
    }

    if (sel->result != E_OK)
    {
        return sel->result;
    }

    if (sel->dtc == DEM_GROUP_ALL_DTCS)
    {
        return DEM_WRONG_DTC;   /* グループは、止められない */
    }

    lock->locked  = 1U;
    lock->eventId = sel->eventId;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_EnableDTCRecordUpdate
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem_DisableDTCRecordUpdate() で止めたレコードの更新と削除を、元に戻す（[SWS_Dem_00234]、Dem.h 参照）。
 *
 * \par 仕様
 *          止めている間に Dem_ClearDTC() が消去していたレコードは消し、止めている間に確定 FAILED になっていたときは、
 *          FreezeFrame と故障確定回数を更新する（消去と更新は、保留した順に、NvM への書き込みを積む）。
 * \param[in]  ClientId  呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 *
 * \retval  E_OK      戻した。
 * \retval  E_NOT_OK  未初期化、ClientId が範囲外、Dem_SelectDTC() が呼ばれていない、または、止めている DTC が無い。
 *
 * \AUTOSARReq     {SWS_Dem_00234, SWS_Dem_00271}
 * \ServiceID      {0x1b}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_EnableDTCRecordUpdate(uint8 ClientId)
{
    if (Dem_CheckSelected(ClientId, DEM_API_ID_ENABLE_DTC_RECORD_UPDATE) != E_OK)
    {
        return E_NOT_OK;
    }

    Dem_RecordLockType* lock = &Dem_RecordLock[ClientId];

    if (lock->locked == 0U)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_ENABLE_DTC_RECORD_UPDATE, DEM_E_WRONG_CONDITION);
        return E_NOT_OK;
    }

    const Dem_EventIdType eventId = lock->eventId;
    lock->locked = 0U;

    /* 止めている間に消去されていたレコードを、ここで消す（別のクライアントが同じイベントを止めているときは、そちらが戻すまで待つ） */
    if (!Dem_IsRecordLocked(eventId))
    {
        /* 先に消去を、次に、消去のあとの確定の更新を、保留していた順に反映する */
        if (Dem_RecordClearPending[eventId] != 0U)
        {
            Dem_RecordClearPending[eventId] = 0U;
            Dem_DiscardRecords(eventId);
            (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);
        }
        if (Dem_RecordCapturePending[eventId] != 0U)
        {
            Dem_RecordCapturePending[eventId] = 0U;
            Dem_StoreRecords(eventId);
        }
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_GetSizeOfExtendedDataRecordSelection
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem_SelectExtendedDataRecord() で選んだレコードの大きさを取得する（[SWS_Dem_00240]、Dem.h 参照）。
 *
 * \par 仕様
 *          大きさは、レコード番号を含む（形式は {レコード番号, データ...}、[SWS_Dem_00076]）。レコードが
 *          記録されていない（一度も確定 FAILED していない）ときは、E_OK で 0 を返す（[SWS_Dem_00631]）。
 *          DTC グループを選んでいるときは、DTC が 1 件に決まらないため、DEM_WRONG_DTC を返す。
 * \param[in]   ClientId                 呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 * \param[out]  SizeOfExtendedDataRecord  大きさ（バイト）の格納先。NULL 禁止。
 *
 * \retval  E_OK                 大きさを格納した（記録が無いときは 0）。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、格納先が NULL、または選択が呼ばれていない。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が、構成に無い。
 *
 * \AUTOSARReq     {SWS_Dem_00240, SWS_Dem_00076, SWS_Dem_00631, SWS_Dem_01274}
 * \ServiceID      {0x21}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_GetSizeOfExtendedDataRecordSelection(uint8 ClientId, uint16* SizeOfExtendedDataRecord)
{
    return Dem_GetSizeOfRecord(ClientId, DEM_API_ID_GET_SIZE_OF_EXTENDED_DATA_RECORD_SELECTION, DEM_RECORD_EXTENDED_DATA, SizeOfExtendedDataRecord);
}

/* ----------------------------------------------------------------------
 * Dem_GetSizeOfFreezeFrameSelection
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem_SelectFreezeFrameData() で選んだレコードの大きさを取得する（[SWS_Dem_00238]、Dem.h 参照）。
 *
 * \par 仕様
 *          大きさは、レコード番号と DID の数を含む（形式は {レコード番号, DID 数, DID, データ, ...}、
 *          [SWS_Dem_00074]）。レコードが記録されていないときは、E_OK で 0 を返す（[SWS_Dem_00630]）。
 *          DTC グループを選んでいるときは、DTC が 1 件に決まらないため、DEM_WRONG_DTC を返す。
 * \param[in]   ClientId           呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 * \param[out]  SizeOfFreezeFrame  大きさ（バイト）の格納先。NULL 禁止。
 *
 * \retval  E_OK                 大きさを格納した（記録が無いときは 0）。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、格納先が NULL、または選択が呼ばれていない。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が、構成に無い。
 *
 * \AUTOSARReq     {SWS_Dem_00238, SWS_Dem_00074, SWS_Dem_00630, SWS_Dem_01270, SWS_Dem_01271}
 * \ServiceID      {0x1f}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_GetSizeOfFreezeFrameSelection(uint8 ClientId, uint16* SizeOfFreezeFrame)
{
    return Dem_GetSizeOfRecord(ClientId, DEM_API_ID_GET_SIZE_OF_FREEZE_FRAME_SELECTION, DEM_RECORD_FREEZE_FRAME, SizeOfFreezeFrame);
}

/* ----------------------------------------------------------------------
 * Dem_GetNextExtendedDataRecord
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem_SelectExtendedDataRecord() で選んだレコードを、バッファへ書き出す（[SWS_Dem_00239]、Dem.h 参照）。
 *
 * \par 仕様
 *          書き出す形式は {レコード番号, データ...}（[SWS_Dem_00075]）。要求したレコード番号が 0xFF でも、
 *          先頭の 1 バイトは、実際のレコード番号（0x01）になる。レコードが記録されていないときは、E_OK で
 *          BufSize を 0 にする（[SWS_Dem_00631]）。1 回の選択で得られるレコードは、1 回の呼び出しで全て
 *          渡すため、続けて呼ぶと DEM_NO_SUCH_ELEMENT（これ以上無い）を返す。
 * \param[in]     ClientId    呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 * \param[out]    DestBuffer  レコードの書き出し先。NULL 禁止。
 * \param[in,out] BufSize     入力: DestBuffer に書ける最大バイト数。出力: 実際に書いたバイト数。NULL 禁止。
 *
 * \retval  E_OK                 書き出した（記録が無いときは、BufSize が 0）。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、ポインタが NULL、または選択が呼ばれていない。
 * \retval  DEM_BUFFER_TOO_SMALL BufSize が、レコードより小さい。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が構成に無い、または、これ以上渡すレコードが無い。
 *
 * \AUTOSARReq     {SWS_Dem_00239, SWS_Dem_00075, SWS_Dem_00631, SWS_Dem_01274}
 * \ServiceID      {0x20}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_GetNextExtendedDataRecord(uint8 ClientId, uint8* DestBuffer, uint16* BufSize)
{
    return Dem_GetNextRecord(ClientId, DEM_API_ID_GET_NEXT_EXTENDED_DATA_RECORD, DEM_RECORD_EXTENDED_DATA, DestBuffer, BufSize);
}

/* ----------------------------------------------------------------------
 * Dem_GetNextFreezeFrameData
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem_SelectFreezeFrameData() で選んだレコードを、バッファへ書き出す（[SWS_Dem_00236]、Dem.h 参照）。
 *
 * \par 仕様
 *          書き出す形式は {レコード番号, DID 数, DID, データ, ...}（[SWS_Dem_00071]）。要求したレコード番号が
 *          0xFF でも、先頭の 1 バイトは、実際のレコード番号（0x01）になる。DID の構成と、データ要素の読み出しは
 *          Dem_PBCfg.c の `Dem_DidClassTable[]` による（データは、確定 FAILED の瞬間に取り、RAM に保持する）。
 *          レコードが記録されていないときは、E_OK で BufSize を 0 にする（[SWS_Dem_00630]）。
 *          1 回の選択で得られるレコードは、1 回の呼び出しで全て渡すため、続けて呼ぶと DEM_NO_SUCH_ELEMENT
 *          （これ以上無い）を返す。
 * \param[in]     ClientId    呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 * \param[out]    DestBuffer  レコードの書き出し先。NULL 禁止。
 * \param[in,out] BufSize     入力: DestBuffer に書ける最大バイト数。出力: 実際に書いたバイト数。NULL 禁止。
 *
 * \retval  E_OK                 書き出した（記録が無いときは、BufSize が 0）。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、ポインタが NULL、または選択が呼ばれていない。
 * \retval  DEM_BUFFER_TOO_SMALL BufSize が、レコードより小さい。
 * \retval  DEM_WRONG_DTC        一致するイベントが無い、DTCFormat が UDS 以外、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が構成に無い、または、これ以上渡すレコードが無い。
 *
 * \AUTOSARReq     {SWS_Dem_00236, SWS_Dem_00071, SWS_Dem_00630, SWS_Dem_01270, SWS_Dem_01271}
 * \ServiceID      {0x1d}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType Dem_GetNextFreezeFrameData(uint8 ClientId, uint8* DestBuffer, uint16* BufSize)
{
    return Dem_GetNextRecord(ClientId, DEM_API_ID_GET_NEXT_FREEZE_FRAME_DATA, DEM_RECORD_FREEZE_FRAME, DestBuffer, BufSize);
}

/* ----------------------------------------------------------------------
 * Dem_SelectExtendedDataRecord
 * ---------------------------------------------------------------------- */

/**
 * \brief   ExtendedData のレコードを選ぶ（[SWS_Dem_91017]、Dem.h 参照）。
 *
 * \par 仕様
 *          以降の Dem_GetSizeOfExtendedDataRecordSelection() と Dem_GetNextExtendedDataRecord() が、この選択を使う
 *          （[SWS_Dem_01272]）。Dem_SelectDTC() を呼ぶと、この選択は破棄される。Dem_SelectDTC() が呼ばれていない
 *          ときは、DEM_E_WRONG_CONDITION を報告する（[SWS_Dem_01274]）。
 *          選ぶ DTC が有効かどうか（DEM_WRONG_DTC / DEM_WRONG_DTCORIGIN）は、本関数が返す。レコード番号が
 *          構成に無いことは、本関数ではなく、Get 系が DEM_NO_SUCH_ELEMENT で返す（本関数の戻り値表に無いため）。
 *
 * \par 実装
 *          本プロジェクトのレコードは、DEM_EXTENDED_DATA_RECORD_NUMBER（0x01）の 1 件だけ。0xFF は、全レコード
 *          （= この 1 件）。
 *
 * \param[in]  ClientId            呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 * \param[in]  ExtendedDataNumber  レコード番号。0xFF は全レコード。
 *
 * \retval  E_OK                 選んだ。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、または Dem_SelectDTC() が呼ばれていない。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、または DTCFormat が UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_91017, SWS_Dem_01272, SWS_Dem_01273, SWS_Dem_01274}
 * \ServiceID      {0xba}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SelectExtendedDataRecord(uint8 ClientId, uint8 ExtendedDataNumber)
{
    return Dem_SelectRecord(ClientId, DEM_API_ID_SELECT_EXTENDED_DATA_RECORD, DEM_RECORD_EXTENDED_DATA, ExtendedDataNumber);
}

/* ----------------------------------------------------------------------
 * Dem_SelectFreezeFrameData
 * ---------------------------------------------------------------------- */

/**
 * \brief   FreezeFrame のレコードを選ぶ（[SWS_Dem_91015]、Dem.h 参照）。
 *
 * \par 仕様
 *          以降の Dem_GetSizeOfFreezeFrameSelection() と Dem_GetNextFreezeFrameData() が、この選択を使う
 *          （[SWS_Dem_01268]）。Dem_SelectDTC() を呼ぶと、この選択は破棄される。Dem_SelectDTC() が呼ばれていない
 *          ときは、DEM_E_WRONG_CONDITION を報告する（[SWS_Dem_01271]）。
 *
 * \par 実装
 *          本プロジェクトのレコードは、DEM_FREEZE_FRAME_RECORD_NUMBER（0x01）の 1 件だけ。0xFF は、全レコード
 *          （= この 1 件）。0x00（WWH-OBD のレコード）は持たない。
 *
 * \param[in]  ClientId      呼び出し元のクライアント識別子（DEM_CLIENT_COUNT 未満）。
 * \param[in]  RecordNumber  レコード番号。0xFF は全レコード。
 *
 * \retval  E_OK                 選んだ。
 * \retval  E_NOT_OK             未初期化、ClientId が範囲外、または Dem_SelectDTC() が呼ばれていない。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、または DTCFormat が UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 *
 * \AUTOSARReq     {SWS_Dem_91015, SWS_Dem_01268, SWS_Dem_01269, SWS_Dem_01270, SWS_Dem_01271}
 * \ServiceID      {0xb9}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_SelectFreezeFrameData(uint8 ClientId, uint8 RecordNumber)
{
    return Dem_SelectRecord(ClientId, DEM_API_ID_SELECT_FREEZE_FRAME_DATA, DEM_RECORD_FREEZE_FRAME, RecordNumber);
}

/* ----------------------------------------------------------------------
 * Dem_DisableDTCSetting
 * ---------------------------------------------------------------------- */

Std_ReturnType Dem_DisableDTCSetting(uint8 ClientId)
{
    (void)ClientId;  /* 単一ECU・単一診断クライアント構成のため未使用（Dem.h 冒頭コメント参照） */

    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_DISABLE_DTC_SETTING, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    Dem_DTCSettingEnabled = 0U;
    DET_LOGI(TAG, "DTC setting disabled");
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_EnableDTCSetting
 * ---------------------------------------------------------------------- */

Std_ReturnType Dem_EnableDTCSetting(uint8 ClientId)
{
    (void)ClientId;  /* 単一ECU・単一診断クライアント構成のため未使用（Dem.h 冒頭コメント参照） */

    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, DEM_API_ID_ENABLE_DTC_SETTING, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    Dem_DTCSettingEnabled = 1U;
    DET_LOGI(TAG, "DTC setting enabled");
    return E_OK;
}

/* ======================================================================
 * OBD-specific Dcm <=> Dem Interfaces
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_DcmGetInfoTypeValue08
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetInfoTypeValue0B
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID01
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID1C
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID21
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID30
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID31
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID41
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID4D
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID4E
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID91
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfOBDFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetDTCOfOBDFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetAvailableOBDMIDs
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetNumTIDsOfOBDMID
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetDTRData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Interface J1939Dcm <=> Dem
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmSetDTCFilter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmGetNumberOfFilteredDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmGetNextFilteredDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmFirstDTCwithLampStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmGetNextDTCwithLampStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmClearDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmSetFreezeFrameFilter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmGetNextFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmGetNextSPNInFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmSetRatioFilter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmGetNextFilteredRatio
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmReadDiagnosticReadiness1
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmReadDiagnosticReadiness2
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_J1939DcmReadDiagnosticReadiness3
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Interface Dlt <=> Dem
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_DltGetMostRecentFreezeFrameRecordData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DltGetAllExtendedDataRecords
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * OBD-specific Interfaces
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_SetEventDisabled
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_RepIUMPRFaultDetect
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetIUMPRDenCondition
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetIUMPRDenCondition
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_RepIUMPRDenLock
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_RepIUMPRDenRelease
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetPtoStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_ReadDataOfPID01
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDataOfPID21
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDataOfPID21
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDataOfPID31
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDataOfPID4D
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDataOfPID4E
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetCycleQualified
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetCycleQualified
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDTCSeverityAvailabilityMask
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetB1Counter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDTR
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_MainFunction
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * Dem_DiscardRecords
 * ---------------------------------------------------------------------- */

/**
 * \brief   イベントのレコード（故障確定回数と FreezeFrame）を捨てる（NvM への書き込みは、呼び出し元の責務）。
 *
 * \details Dem_ClearOne()（止められていないとき）と、Dem_EnableDTCRecordUpdate()（保留していた消去）が呼ぶ。
 *
 * \param[in]  EventId  イベント ID（範囲チェックは呼び出し元の責務）。
 */
static void Dem_DiscardRecords(Dem_EventIdType EventId)
{
    Dem_OccurrenceCounter[EventId] = 0U;
    Dem_FreezeFrameValid[EventId]  = 0U;
}

/* ----------------------------------------------------------------------
 * Dem_StoreRecords
 * ---------------------------------------------------------------------- */

/**
 * \brief   確定 FAILED のレコードを更新する: FreezeFrame を取り、故障確定回数を +1 する（0xFF で飽和）。
 *
 * \details Dem_SetEventStatus()（確定の瞬間）と、Dem_EnableDTCRecordUpdate()（止めている間に確定していたとき）が呼ぶ。
 *
 * \param[in]  EventId  イベント ID（範囲チェックは呼び出し元の責務）。
 */
static void Dem_StoreRecords(Dem_EventIdType EventId)
{
    Dem_CaptureFreezeFrame(EventId);
    DET_LOGI(TAG, "FreezeFrame ev=%u captured", (unsigned)EventId);

    if (Dem_OccurrenceCounter[EventId] < 0xFFU)
    {
        Dem_OccurrenceCounter[EventId]++;
    }
    (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);
    DET_LOGI(TAG, "ExtendedData ev=%u occurrence=%u",
             (unsigned)EventId, (unsigned)Dem_OccurrenceCounter[EventId]);
}

/* ----------------------------------------------------------------------
 * Dem_IsRecordLocked
 * ---------------------------------------------------------------------- */

/**
 * \brief   いずれかのクライアントが、そのイベントのレコードの更新を止めているか。
 *
 * \param[in]  EventId  イベント ID（範囲チェックは呼び出し元の責務）。
 *
 * \retval  TRUE   止められている（Dem_DisableDTCRecordUpdate()）。
 * \retval  FALSE  止められていない。
 */
static boolean Dem_IsRecordLocked(Dem_EventIdType EventId)
{
    boolean locked = FALSE;

    for (uint8 c = 0U; c < DEM_CLIENT_COUNT; c++)
    {
        if ((Dem_RecordLock[c].locked != 0U) && (Dem_RecordLock[c].eventId == EventId))
        {
            locked = TRUE;
        }
    }
    return locked;
}

/* ----------------------------------------------------------------------
 * Dem_FindEventOfDtc
 * ---------------------------------------------------------------------- */

/**
 * \brief   DTC コードに割り当てられたイベントを探す（Dem_SelectDTC() が使う）。
 *
 * \param[in]   DTC      探す DTC（24-bit UDS 形式）。
 * \param[out]  EventId  見つかったイベント ID の格納先。見つからないときは、書き換えない。
 *
 * \retval  TRUE   見つかった。
 * \retval  FALSE  どのイベントにも割り当てられていない。
 */
static boolean Dem_FindEventOfDtc(uint32 DTC, Dem_EventIdType* EventId)
{
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        if (Dem_DtcTable[i] == DTC)
        {
            *EventId = i;
            return TRUE;
        }
    }
    return FALSE;
}

/* ----------------------------------------------------------------------
 * Dem_CheckClient / Dem_CheckSelected
 * ---------------------------------------------------------------------- */

/**
 * \brief   Dem が初期化済みで、ClientId が構成にあるかを確認する（[SWS_Dem_01252]）。
 *
 * \param[in]  ClientId  確認する ClientId。
 * \param[in]  apiId     報告元の API の ID（DET 用）。
 *
 * \retval  E_OK      初期化済みで、ClientId は構成にある。
 * \retval  E_NOT_OK  未初期化（DEM_E_UNINIT）、または ClientId が範囲外（DEM_E_WRONG_CONFIGURATION）。
 */
static Std_ReturnType Dem_CheckClient(uint8 ClientId, uint8 apiId)
{
    if (!Dem_Initialized)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, apiId, DEM_E_UNINIT);
        return E_NOT_OK;
    }

    if (ClientId >= DEM_CLIENT_COUNT)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, apiId, DEM_E_WRONG_CONFIGURATION);
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * \brief   Dem_CheckClient() に加えて、Dem_SelectDTC() が呼ばれていることを確認する（[SWS_Dem_01255]）。
 *
 * \param[in]  ClientId  確認する ClientId。
 * \param[in]  apiId     報告元の API の ID（DET 用）。
 *
 * \retval  E_OK      確認に通った。
 * \retval  E_NOT_OK  Dem_CheckClient() の失敗、または Dem_SelectDTC() が呼ばれていない（DEM_E_WRONG_CONDITION）。
 */
static Std_ReturnType Dem_CheckSelected(uint8 ClientId, uint8 apiId)
{
    if (Dem_CheckClient(ClientId, apiId) != E_OK)
    {
        return E_NOT_OK;
    }

    if (Dem_Selection[ClientId].selected == 0U)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, apiId, DEM_E_WRONG_CONDITION);
        return E_NOT_OK;
    }

    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_CheckRecordSelection / Dem_SelectRecord / Dem_GetSizeOfRecord / Dem_GetNextRecord
 * ---------------------------------------------------------------------- */

/**
 * \brief   選んだ DTC に記録されている、種類 kind のレコードの大きさ（バイト）。記録が無ければ 0。
 *
 * \param[in]  kind     レコードの種類。
 * \param[in]  eventId  イベント ID（範囲チェックは呼び出し元の責務）。
 *
 * \return  レコードの大きさ（レコード番号などのヘッダを含む）。
 */
static uint16 Dem_StoredRecordSize(Dem_RecordKindType kind, Dem_EventIdType eventId)
{
    uint16 size;

    if (kind == DEM_RECORD_EXTENDED_DATA)
    {
        /* 故障確定回数が 0 = 一度も確定 FAILED していない = 記録されていない（[SWS_Dem_00631]） */
        size = (Dem_OccurrenceCounter[eventId] == 0U) ? 0U : DEM_EXTENDED_DATA_RECORD_SIZE;
    }
    else
    {
        size = (Dem_FreezeFrameValid[eventId] == 0U) ? 0U : DEM_FREEZE_FRAME_RECORD_SIZE;   /* [SWS_Dem_00630] */
    }
    return size;
}

/**
 * \brief   記録されているレコードを、{レコード番号, ...} の形式で書き出す（Dem_StoredRecordSize() が 0 でないときだけ呼ぶ）。
 *
 * \param[in]   kind        レコードの種類。
 * \param[in]   eventId     イベント ID。
 * \param[out]  DestBuffer  書き出し先（Dem_StoredRecordSize() バイト以上）。
 */
static void Dem_WriteStoredRecord(Dem_RecordKindType kind, Dem_EventIdType eventId, uint8* DestBuffer)
{
    if (kind == DEM_RECORD_EXTENDED_DATA)
    {
        DestBuffer[0] = DEM_EXTENDED_DATA_RECORD_NUMBER;   /* [SWS_Dem_00075] */
        DestBuffer[1] = Dem_OccurrenceCounter[eventId];
    }
    else
    {
        /* [SWS_Dem_00071]: {RecordNumber, NumOfDIDs, DID[1], data[1], ..., DID[N], data[N]} */
        uint8 pos = 0U;
        uint8 src = 0U;

        DestBuffer[0] = DEM_FREEZE_FRAME_RECORD_NUMBER;
        DestBuffer[1] = DEM_FREEZE_FRAME_DID_COUNT;
        pos           = 2U;
        for (uint8 i = 0U; i < DEM_FREEZE_FRAME_DID_COUNT; i++)
        {
            DestBuffer[pos]      = (uint8)(Dem_DidClassTable[i].Did >> 8U);
            DestBuffer[pos + 1U] = (uint8)(Dem_DidClassTable[i].Did & 0xFFU);
            pos                  = (uint8)(pos + 2U);
            for (uint8 j = 0U; j < Dem_DidClassTable[i].Size; j++)
            {
                DestBuffer[pos] = Dem_FreezeFrameData[eventId][src];
                pos             = (uint8)(pos + 1U);
                src             = (uint8)(src + 1U);
            }
        }
    }
}

/**
 * \brief   FreezeFrame を取る（確定 FAILED の瞬間に呼ぶ）。各 DID のデータ要素を、読み出し関数で読んで、記録する。
 *
 * \details 読み出し関数が失敗したデータ要素は、0xFF で埋める（無効なデータの印）。
 *
 * \param[in]  EventId  イベント ID（範囲チェックは呼び出し元の責務）。
 */
static void Dem_CaptureFreezeFrame(Dem_EventIdType EventId)
{
    uint8 offset = 0U;

    for (uint8 i = 0U; i < DEM_FREEZE_FRAME_DID_COUNT; i++)
    {
        const Dem_DidClassType* did = &Dem_DidClassTable[i];

        if (did->ReadFnc(&Dem_FreezeFrameData[EventId][offset]) != E_OK)
        {
            for (uint8 j = 0U; j < did->Size; j++)
            {
                Dem_FreezeFrameData[EventId][offset + j] = 0xFFU;
            }
        }
        offset = (uint8)(offset + did->Size);
    }
    Dem_FreezeFrameValid[EventId] = 1U;
}

/**
 * \brief   レコードの選択が有効か確かめ、選んだレコードの大きさを求める（Get 系が共有する）。
 *
 * \param[in]   ClientId    確認する ClientId。
 * \param[in]   apiId       報告元の API の ID（DET 用）。
 * \param[in]   kind        レコードの種類。
 * \param[out]  RecordSize  選んだレコードの大きさ（記録が無いときは 0）。E_OK のときだけ有効。
 *
 * \retval  E_OK                 選択が有効。
 * \retval  E_NOT_OK             Dem_CheckSelected() の失敗、またはレコードの選択が呼ばれていない
 *                               （DEM_E_WRONG_CONDITION、[SWS_Dem_01274]、[SWS_Dem_01270]）。
 * \retval  DEM_WRONG_DTC        選んだ DTC が無効、または DTC グループを選んでいる。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 * \retval  DEM_NO_SUCH_ELEMENT  選んだレコード番号が構成に無い。
 */
static Std_ReturnType Dem_CheckRecordSelection(uint8 ClientId, uint8 apiId, Dem_RecordKindType kind, uint16* RecordSize)
{
    if (Dem_CheckSelected(ClientId, apiId) != E_OK)
    {
        return E_NOT_OK;
    }

    const Dem_SelectionType*       sel = &Dem_Selection[ClientId];
    const Dem_RecordSelectionType* rec = &sel->record[kind];

    if (rec->selected == 0U)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, apiId, DEM_E_WRONG_CONDITION);
        return E_NOT_OK;
    }

    if (sel->result != E_OK)
    {
        return sel->result;
    }

    if (sel->dtc == DEM_GROUP_ALL_DTCS)
    {
        return DEM_WRONG_DTC;   /* グループは、DTC が 1 件に決まらない */
    }

    const uint8 ownNumber = (kind == DEM_RECORD_EXTENDED_DATA) ? DEM_EXTENDED_DATA_RECORD_NUMBER : DEM_FREEZE_FRAME_RECORD_NUMBER;
    if ((rec->number != ownNumber) && (rec->number != DEM_RECORD_NUMBER_ALL))
    {
        return DEM_NO_SUCH_ELEMENT;
    }

    *RecordSize = Dem_StoredRecordSize(kind, sel->eventId);
    return E_OK;
}

/**
 * \brief   種類 kind のレコードを選ぶ（Dem_SelectExtendedDataRecord() と Dem_SelectFreezeFrameData() が共有する）。
 *
 * \param[in]  ClientId      呼び出し元のクライアント識別子。
 * \param[in]  apiId         報告元の API の ID（DET 用）。
 * \param[in]  kind          レコードの種類。
 * \param[in]  RecordNumber  レコード番号。
 *
 * \retval  E_OK                 選んだ。
 * \retval  E_NOT_OK             Dem_CheckSelected() の失敗。
 * \retval  DEM_WRONG_DTC        選んだ DTC に一致するイベントが無い、または DTCFormat が UDS 以外。
 * \retval  DEM_WRONG_DTCORIGIN  選んだ DTCOrigin が非対応。
 */
static Std_ReturnType Dem_SelectRecord(uint8 ClientId, uint8 apiId, Dem_RecordKindType kind, uint8 RecordNumber)
{
    if (Dem_CheckSelected(ClientId, apiId) != E_OK)
    {
        return E_NOT_OK;
    }

    Dem_SelectionType* sel = &Dem_Selection[ClientId];

    if (sel->result != E_OK)
    {
        return sel->result;   /* DEM_WRONG_DTC / DEM_WRONG_DTCORIGIN */
    }

    sel->record[kind].selected  = 1U;
    sel->record[kind].number    = RecordNumber;
    sel->record[kind].delivered = 0U;
    return E_OK;
}

/**
 * \brief   選んだレコードの大きさを取得する（Dem_GetSizeOf*() が共有する）。
 *
 * \param[in]   ClientId  呼び出し元のクライアント識別子。
 * \param[in]   apiId     報告元の API の ID（DET 用）。
 * \param[in]   kind      レコードの種類。
 * \param[out]  Size      大きさ（バイト）の格納先。NULL 禁止。
 *
 * \retval  E_OK 以外  Dem_CheckRecordSelection() の戻り値、または Size が NULL（DEM_E_PARAM_POINTER、E_NOT_OK）。
 * \retval  E_OK       大きさを格納した。
 */
static Std_ReturnType Dem_GetSizeOfRecord(uint8 ClientId, uint8 apiId, Dem_RecordKindType kind, uint16* Size)
{
    uint16         recordSize = 0U;
    Std_ReturnType ret        = Dem_CheckRecordSelection(ClientId, apiId, kind, &recordSize);
    if (ret != E_OK)
    {
        return ret;
    }

    if (Size == NULL)
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, apiId, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *Size = recordSize;
    return E_OK;
}

/**
 * \brief   選んだレコードを、バッファへ書き出す（Dem_GetNext*() が共有する）。
 *
 * \param[in]     ClientId    呼び出し元のクライアント識別子。
 * \param[in]     apiId       報告元の API の ID（DET 用）。
 * \param[in]     kind        レコードの種類。
 * \param[out]    DestBuffer  書き出し先。NULL 禁止。
 * \param[in,out] BufSize     入力: 書ける最大バイト数。出力: 書いたバイト数。NULL 禁止。
 *
 * \retval  E_OK                  書き出した（記録が無いときは、BufSize が 0）。
 * \retval  DEM_BUFFER_TOO_SMALL  BufSize が、レコードより小さい。
 * \retval  DEM_NO_SUCH_ELEMENT   これ以上渡すレコードが無い。
 * \retval  その他                Dem_CheckRecordSelection() の戻り値、またはポインタが NULL（DEM_E_PARAM_POINTER、E_NOT_OK）。
 */
static Std_ReturnType Dem_GetNextRecord(uint8 ClientId, uint8 apiId, Dem_RecordKindType kind, uint8* DestBuffer, uint16* BufSize)
{
    uint16         recordSize = 0U;
    Std_ReturnType ret        = Dem_CheckRecordSelection(ClientId, apiId, kind, &recordSize);
    if (ret != E_OK)
    {
        return ret;
    }

    if ((DestBuffer == NULL) || (BufSize == NULL))
    {
        (void)Det_ReportError(DEM_MODULE_ID, 0U, apiId, DEM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    Dem_SelectionType* sel = &Dem_Selection[ClientId];

    if (sel->record[kind].delivered != 0U)
    {
        return DEM_NO_SUCH_ELEMENT;   /* 1 回の選択で得られるレコードは、渡し済み */
    }

    if (recordSize > *BufSize)
    {
        return DEM_BUFFER_TOO_SMALL;
    }

    if (recordSize != 0U)
    {
        Dem_WriteStoredRecord(kind, sel->eventId, DestBuffer);
    }

    *BufSize                    = recordSize;
    sel->record[kind].delivered = 1U;
    return E_OK;
}

/* ----------------------------------------------------------------------
 * Dem_ClearOne
 * ---------------------------------------------------------------------- */

/**
 * \brief   指定イベントの DTC ステータス・デバウンスカウンタ・経年回復カウンタ・
 *          ExtendedData・FreezeFrame を初期状態に戻す（NvM 書き込みは呼び出し元が行う）。
 *          FiM への通知（[SWS_Dem_01282]/[SWS_Dem_01284]: クリアで、モニタステータスも戻る）は、呼び出し元の責務。
 *
 * \param[in]  EventId  イベント ID (DemConf_DemEventParameter_* 定数)。範囲チェックは呼び出し元の責務。
 * \retval  1  ステータスが変わった。0  変わらなかった。
 */
static uint8 Dem_ClearOne(Dem_EventIdType EventId)
{
    const uint8 prevStatus = Dem_StatusTable[EventId];

    Dem_StatusTable[EventId] = DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR
                             | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
    Dem_DebounceCounter[EventId]    = 0;
    Dem_AgingCounter[EventId]       = 0U;
    if (Dem_IsRecordLocked(EventId))
    {
        Dem_RecordClearPending[EventId]   = 1U;   /* レコードは、Dem_EnableDTCRecordUpdate() が戻すときに消す */
        Dem_RecordCapturePending[EventId] = 0U;   /* 消去は、それより前の確定の更新より、あとに効く */
    }
    else
    {
        Dem_DiscardRecords(EventId);
    }

    return (Dem_StatusTable[EventId] != prevStatus) ? 1U : 0U;
}

/* ----------------------------------------------------------------------
 * Dem_FilterMatches
 * ---------------------------------------------------------------------- */

/**
 * \brief   指定イベントの DTC が、Dem_SetDTCFilter() で設定したフィルタ条件に一致するか。
 *
 * \details ステータスバイト（DEM_STATUS_AVAILABILITY_MASK でマスク済み）が
 *          `Dem_FilterStatusMask` と AND して非ゼロであること（マスクが 0x00 の
 *          ときはこの判定をしない）。さらに Dem_FilterForFdc が立っていれば、
 *          Fault Detection Counter が 1〜0x7E（prefailed）であること
 *          （[SWS_Dem_00208]）。
 *
 * \param[in]  EventId  イベント ID（0〜DEM_EVENT_COUNT-1）。範囲チェックは呼び出し元の責務。
 *
 * \retval  1  一致する。
 * \retval  0  一致しない。
 */
static uint8 Dem_FilterMatches(Dem_EventIdType EventId)
{
    if (Dem_FilterStatusMask != 0U)
    {
        const uint8 status = Dem_StatusTable[EventId] & DEM_STATUS_AVAILABILITY_MASK;
        if ((status & Dem_FilterStatusMask) == 0U)
        {
            return 0U;
        }
    }

    if (Dem_FilterForFdc != 0U)
    {
        const sint8 fdc = Dem_MapDebounceCounterToFdc(Dem_DebounceCounter[EventId], Dem_DebounceLimitTable[EventId]);
        if ((fdc < 1) || (fdc > 0x7E))
        {
            return 0U;
        }
    }

    return 1U;
}

/* ----------------------------------------------------------------------
 * Dem_MapDebounceCounterToFdc
 * ---------------------------------------------------------------------- */

/**
 * \brief   デバウンスカウンタ生値 (-limit〜+limit) を UDS の Fault Detection
 *          Counter 値域 (-128〜+127) へ線形写像する。
 *
 * \details [SWS_Dem_00415]: 内部デバウンスカウンタの値と範囲に基づき、
 *          線形にマッピングすること、との要求。`(counter * N) / limit` の
 *          Nを正側は127、負側は128に使い分ける（除数は共通のlimit、
 *          sint8 の値域が非対称(-128〜127、負側が1つ多い)なための調整）
 *          ことで、両端(counter=±limit)がちょうど±127/-128に一致する。
 *
 * \AUTOSARReq     {SWS_Dem_00415}
 */
static sint8 Dem_MapDebounceCounterToFdc(sint8 counter, sint8 limit)
{
    if (limit <= 0)
    {
        return 0;  /* 未到達コード想定 (Dem_DebounceLimitTable[] は全て1以上) */
    }

    if (counter >= 0)
    {
        return (sint8)(((sint16)counter * 127) / limit);
    }
    else
    {
        return (sint8)(((sint16)counter * 128) / limit);
    }
}

/* ----------------------------------------------------------------------
 * Dem_EvaluateAging
 * ---------------------------------------------------------------------- */

/**
 * \brief   経年回復 (Aging) を判定する。
 *
 * \details 直前の操作サイクルの最終ステータス（TF/TFTOC/TNCTC が新サイクル用に
 *          リセットされる前の値）を評価し、Dem_AgingCounter[EventId] を更新する。
 *          Dem_AgingThresholdTable[EventId]（イベントごとの閾値）に達したら
 *          CONFIRMED を自動解除する。
 *          Dem_EvaluateCycleEnd() からのみ呼び出すこと（呼び出し順序に依存するため非公開）。
 *          カウンタを進めるので、1 つのサイクルに対して、1 回だけ呼ぶこと（Dem_EvaluateCycleEnd() が保証する）。
 *
 * \param[in]  EventId  イベント ID。範囲チェックは呼び出し元の責務。
 */
static void Dem_EvaluateAging(Dem_EventIdType EventId)
{
    const uint8 status    = Dem_StatusTable[EventId];
    const uint8 threshold = Dem_AgingThresholdTable[EventId];

    if ((status & DEM_STATUS_CONFIRMED) == 0U)
    {
        /* 確定故障なし: エージング対象外 */
        Dem_AgingCounter[EventId] = 0U;
    }
    else if ((status & DEM_STATUS_TEST_FAILED) != 0U)
    {
        /* 直前サイクル中に再度 FAILED 確定: 連続性が途切れたためリセット */
        if (Dem_AgingCounter[EventId] != 0U)
        {
            DET_LOGI(TAG, "ev=%u aging reset (re-failed)", (unsigned)EventId);
        }
        Dem_AgingCounter[EventId] = 0U;
    }
    else if ((status & DEM_STATUS_NOT_COMPLETED_THIS_CYCLE) != 0U)
    {
        /* 直前サイクル中に一度もテストされなかった: このサイクルは数えない（カウンタ維持） */
    }
    else
    {
        /* 確定故障あり、かつ直前サイクルは故障なし・テスト済み: クリーンな操作サイクル */
        Dem_AgingCounter[EventId]++;

        if (Dem_AgingCounter[EventId] >= threshold)
        {
            Dem_StatusTable[EventId] &= (uint8)(~DEM_STATUS_CONFIRMED);
            Dem_AgingCounter[EventId] = 0U;
            DET_LOGI(TAG, "ev=%u healed (aging complete) dtc=0x%06lX",
                     (unsigned)EventId, (unsigned long)Dem_DtcTable[EventId]);
        }
        else
        {
            DET_LOGI(TAG, "ev=%u aging=%u/%u", (unsigned)EventId,
                     (unsigned)Dem_AgingCounter[EventId], (unsigned)threshold);
        }
    }
}

/* ----------------------------------------------------------------------
 * Dem_EvaluatePendingClear
 * ---------------------------------------------------------------------- */

/**
 * \brief   PendingDTC (UDS status bit2) を、操作サイクルの最終ステータスで判定する。
 *
 * \details SWS_Dem_00390 (Figure 7.19): PendingDTC は
 *          「NOT TestFailedThisOperationCycle AND NOT TestNotCompletedThisOperationCycle
 *          かつ操作サイクルの END/RESTART」または ClearDTC でクリアされる。
 *          CONFIRMED (bit3) のような多サイクルのエージングは行わず、直前サイクルが
 *          1 回でもクリーン（そのサイクル中に FAILED 確定が無く、かつテスト済み）
 *          であれば即座にクリアする点が Dem_EvaluateAging() との違い。
 *          TF/TFTOC/TNCTC が新サイクル用にリセットされる前の値を見る必要があるため、
 *          Dem_EvaluateCycleEnd() からのみ、かつそのリセットより前に呼び出すこと（呼び出し順序に依存するため非公開）。
 *
 * \param[in]  EventId  イベント ID。範囲チェックは呼び出し元の責務。
 */
static void Dem_EvaluatePendingClear(Dem_EventIdType EventId)
{
    const uint8 status = Dem_StatusTable[EventId];

    if ((status & DEM_STATUS_PENDING) == 0U)
    {
        return; /* 既にクリア済み: 対象外 */
    }

    const uint8 cleanCycle = (((status & DEM_STATUS_TF_THIS_OP_CYCLE) == 0U)
                            && ((status & DEM_STATUS_NOT_COMPLETED_THIS_CYCLE) == 0U)) ? 1U : 0U;

    if ((cleanCycle) != 0U)
    {
        Dem_StatusTable[EventId] &= (uint8)(~DEM_STATUS_PENDING);
        DET_LOGI(TAG, "ev=%u pendingDTC cleared (clean operation cycle)", (unsigned)EventId);
    }
}

/* ----------------------------------------------------------------------
 * Dem_NotifyFimIfStatusChanged
 * ---------------------------------------------------------------------- */

/**
 * \brief   イベントのステータスが、前の値から変わっていたら、FiM へ通知する。
 *
 * \details 評価済みの印（DEM_STATUS_CYCLE_EVALUATED）だけの変化は、内部用で、FiM の判定に関係しないので、通知しない。
 *
 * \param[in]  EventId     イベント ID。範囲チェックは呼び出し元の責務。
 * \param[in]  prevStatus  変化の前のステータス（Dem_StatusTable[EventId] の生の値）。
 */
static void Dem_NotifyFimIfStatusChanged(Dem_EventIdType EventId, uint8 prevStatus)
{
    if (((Dem_StatusTable[EventId] ^ prevStatus) & (uint8)(~DEM_STATUS_CYCLE_EVALUATED)) != 0U)
    {
        FiM_DemTriggerOnMonitorStatus(EventId);
    }
}

/* ----------------------------------------------------------------------
 * Dem_EvaluateCycleEnd
 * ---------------------------------------------------------------------- */

/**
 * \brief   イベントの、前サイクルの最終ステータスを、1 回だけ評価する（経年回復・PendingDTC の自動クリア）。
 *
 * \details DEM_STATUS_CYCLE_EVALUATED が立っていれば、何もしない。立っていなければ、Dem_EvaluateAging() と
 *          Dem_EvaluatePendingClear() を行い、印を立てる。印は NvM に保存されるので、END で評価したあと、電源 OFF を経た
 *          次の起動（Dem_Init()）や、続く START が、同じサイクルを、もう一度評価することはない（経年回復カウンタが、二重に
 *          進まない）。印は、Dem_BeginOperationCycle() が、新しいサイクルへ進むときに、落とす。
 *
 * \param[in]  EventId  イベント ID。範囲チェックは呼び出し元の責務。
 */
static void Dem_EvaluateCycleEnd(Dem_EventIdType EventId)
{
    if ((Dem_StatusTable[EventId] & DEM_STATUS_CYCLE_EVALUATED) == 0U)
    {
        Dem_EvaluateAging(EventId);
        Dem_EvaluatePendingClear(EventId);
        Dem_StatusTable[EventId] |= DEM_STATUS_CYCLE_EVALUATED;
    }
}

/* ----------------------------------------------------------------------
 * Dem_EndOperationCycle
 * ---------------------------------------------------------------------- */

/**
 * \brief   操作サイクルを終える（Dem_SetOperationCycleState(END) の処理）。
 *
 * \details イベントごとに、そのサイクルの最終ステータスで、経年回復と PendingDTC の自動クリアを評価する
 *          （[SWS_Dem_00390]、[SWS_Dem_00493]。Dem_EvaluateCycleEnd()）。NvM への書き込みは、呼び出し元の責務。
 *
 * \param[in]  notifyFim  1 のとき、ステータス（CONFIRMED の解除など）が変わったイベントを、FiM へ通知する。
 * \retval  1  ステータス（評価済みの印を含む）または経年回復カウンタが、1 つでも変わった。0  変わらなかった。
 */
static uint8 Dem_EndOperationCycle(uint8 notifyFim)
{
    uint8 changed = 0U;

    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        const uint8 prevStatus = Dem_StatusTable[i];
        const uint8 prevAging  = Dem_AgingCounter[i];

        Dem_EvaluateCycleEnd(i);

        if ((Dem_StatusTable[i] != prevStatus) || (Dem_AgingCounter[i] != prevAging))
        {
            changed = 1U;
        }
        if (notifyFim != 0U)
        {
            Dem_NotifyFimIfStatusChanged(i, prevStatus);
        }
    }

    return changed;
}

/* ----------------------------------------------------------------------
 * Dem_BeginOperationCycle
 * ---------------------------------------------------------------------- */

/**
 * \brief   新しい操作サイクルを始める（Dem_Init() と Dem_SetOperationCycleState(START) の共通処理）。
 *
 * \details イベントごとに、経年回復・PendingDTC の判定を、「前サイクルの最終結果」（TF / TFTOC / TNCTOC を新サイクル用に
 *          リセットする前の値）で行い（END で評価済みなら、しない。Dem_EvaluateCycleEnd()）、そのあと、TF・TFTOC をクリアし、
 *          TNCTOC を立て、評価済みの印を落とし、デバウンスカウンタを 0 へ戻す（[SWS_Dem_00344]/[SWS_Dem_00438]）。
 *          サイクルの途中での再開始（[SWS_Dem_00483]）も、その時点までを前サイクルとして評価する
 *          （仕様の、END と START を続けて行うのと同じ）。
 *          NvM への書き込みは、呼び出し元の責務。
 *
 * \param[in]  notifyFim  1 のとき、監視ステータス（TNCTOC）や CONFIRMED が変わったイベントを、FiM へ通知する
 *                        （Dem_Init() は FiM の初期化前のため 0）。
 * \retval  1  ステータスまたは経年回復カウンタが、1 つでも変わった。0  変わらなかった。
 */
static uint8 Dem_BeginOperationCycle(uint8 notifyFim)
{
    uint8 changed = 0U;

    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        const uint8 prevStatus = Dem_StatusTable[i];
        const uint8 prevAging  = Dem_AgingCounter[i];

        Dem_EvaluateCycleEnd(i);

        Dem_StatusTable[i] = (Dem_StatusTable[i]
                               & (uint8)(~DEM_STATUS_TEST_FAILED)
                               & (uint8)(~DEM_STATUS_TF_THIS_OP_CYCLE)
                               & (uint8)(~DEM_STATUS_CYCLE_EVALUATED))
                              | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
        Dem_DebounceCounter[i] = 0;

        if ((Dem_StatusTable[i] != prevStatus) || (Dem_AgingCounter[i] != prevAging))
        {
            changed = 1U;
        }
        if (notifyFim != 0U)
        {
            Dem_NotifyFimIfStatusChanged(i, prevStatus);
        }
    }

    return changed;
}

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef DEM_UNIT_TEST
void Dem_Test_ResetInitState(void)
{
    Dem_Initialized = 0U;
}
#endif
