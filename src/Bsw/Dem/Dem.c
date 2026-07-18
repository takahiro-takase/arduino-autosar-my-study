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
 *            SWS_Dem_00390 (Figure 7.19) 準拠。Dem_Init() が起動ごとに
 *            「直前の操作サイクル」の最終ステータスを評価し (TF/TFTOC/TNCTC を
 *            新サイクル用にリセットする直前の値を見る)、PDTC=1 かつ
 *            TFTOC=0（このサイクル中に FAILED 確定なし）かつ TNCTC=0（テスト済み）
 *            であれば即座に PDTC をクリアする。CDTC のような多サイクルの
 *            エージングカウンタは介さず、クリーンなサイクルが 1 回来た時点で
 *            即クリアする点が CDTC との違い（Dem_EvaluatePendingClear()）。
 *
 *          経年回復 (Aging) — 操作サイクルベースの CDTC 自動回復:
 *            Dem_Init() が起動ごとに「直前の操作サイクル」の最終ステータスを評価する
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
 *            - 操作サイクルの境界判定は Dem_Init() (起動時) のみで行う
 *              (実車は明示的な OperationCycle Start/End API を持つ)
 *            - FreezeFrame は RAM のみに保持 (EEPROM 非永続化、電源 OFF で消去)
 *            - ExtendedData はカウンタ 1 種類のみ（実車は OperationCycle 情報等も持てる）
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Dem.h"
#include "Det.h"
#include "NvM.h"

#define TAG "Dem"

/* -----------------------------------------------------------------------
 * モジュール内部状態
 * ----------------------------------------------------------------------- */

/** 各イベントの DTC ステータスバイト (NvM RAM ミラーから復元した作業コピー) */
static uint8 Dem_StatusTable[DEM_EVENT_COUNT];

/** イベント ID → DTC コード (24-bit) 変換テーブル */
static const uint32 Dem_DtcTable[DEM_EVENT_COUNT] = {
    DEM_DTC_ENGINE_OVERHEAT,         /* event 0 */
    DEM_DTC_ENGINE_STALL,            /* event 1 */
    DEM_DTC_ENGINE_SPEED_NO_FLAG,    /* event 2 */
    DEM_DTC_STARTING_TIMEOUT,        /* event 3 */
    DEM_DTC_COMM_TIMEOUT,            /* event 4 */
    DEM_DTC_BUTTON_STUCK,            /* event 5 */
    DEM_DTC_ADC_VOLT_LOW,            /* event 6 */
    DEM_DTC_CAN_BUSOFF,              /* event 7 */
    DEM_DTC_E2E_ABSINFO,             /* event 8 */
    DEM_DTC_E2E_ENGINEINFO           /* event 9 */
};

/** イベント ID → デバウンス確定閾値 変換テーブル (Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*) */
static const sint8 Dem_DebounceLimitTable[DEM_EVENT_COUNT] = {
    DEM_DEBOUNCE_LIMIT_ENGINE_OVERHEAT,       /* event 0 */
    DEM_DEBOUNCE_LIMIT_ENGINE_STALL,          /* event 1 */
    DEM_DEBOUNCE_LIMIT_ENGINE_SPEED_NO_FLAG,  /* event 2 */
    DEM_DEBOUNCE_LIMIT_STARTING_TIMEOUT,      /* event 3 */
    DEM_DEBOUNCE_LIMIT_COMM_TIMEOUT,          /* event 4 */
    DEM_DEBOUNCE_LIMIT_BUTTON_STUCK,          /* event 5 */
    DEM_DEBOUNCE_LIMIT_ADC_VOLT_LOW,          /* event 6 */
    DEM_DEBOUNCE_LIMIT_CAN_BUSOFF,            /* event 7 */
    DEM_DEBOUNCE_LIMIT_E2E_ABSINFO,           /* event 8 */
    DEM_DEBOUNCE_LIMIT_E2E_ENGINEINFO         /* event 9 */
};

/** イベント ID → 経年回復(Aging)閾値 変換テーブル (Dem_Cfg.h の DEM_AGING_THRESHOLD_*) */
static const uint8 Dem_AgingThresholdTable[DEM_EVENT_COUNT] = {
    DEM_AGING_THRESHOLD_ENGINE_OVERHEAT,       /* event 0 */
    DEM_AGING_THRESHOLD_ENGINE_STALL,          /* event 1 */
    DEM_AGING_THRESHOLD_ENGINE_SPEED_NO_FLAG,  /* event 2 */
    DEM_AGING_THRESHOLD_STARTING_TIMEOUT,      /* event 3 */
    DEM_AGING_THRESHOLD_COMM_TIMEOUT,          /* event 4 */
    DEM_AGING_THRESHOLD_BUTTON_STUCK,          /* event 5 */
    DEM_AGING_THRESHOLD_ADC_VOLT_LOW,          /* event 6 */
    DEM_AGING_THRESHOLD_CAN_BUSOFF,            /* event 7 */
    DEM_AGING_THRESHOLD_E2E_ABSINFO,           /* event 8 */
    DEM_AGING_THRESHOLD_E2E_ENGINEINFO         /* event 9 */
};

/** イベントごとの FreezeFrame (故障時スナップショット)。RAM のみ保持 */
static Dem_FreezeFrameType Dem_FreezeFrameTable[DEM_EVENT_COUNT];

/** Dem_FreezeFrameTable[i] が有効か (0=未記録, 1=記録あり) */
static uint8 Dem_FreezeFrameValid[DEM_EVENT_COUNT];

/** SW-C が毎周期更新する「現在値」。FAILED 遷移時にこの値をコピーする */
static Dem_FreezeFrameType Dem_CurrentContext;

/** イベントごとのデバウンスカウンタ (-limit〜+limit, 0=中立。limit は Dem_DebounceLimitTable[]) */
static sint8 Dem_DebounceCounter[DEM_EVENT_COUNT];

/** イベントごとの経年回復 (Aging) カウンタ。NvM 経由で操作サイクルをまたいで永続化する */
static uint8 Dem_AgingCounter[DEM_EVENT_COUNT];

/** イベントごとの ExtendedData（確定 FAILED の累積回数、0xFF で飽和）。NvM 経由で永続化する */
static uint8 Dem_OccurrenceCounter[DEM_EVENT_COUNT];

/**
 * \brief   経年回復 (Aging) を判定する。
 *
 * \details 直前の操作サイクルの最終ステータス（TF/TFTOC/TNCTC が新サイクル用に
 *          リセットされる前の値）を評価し、Dem_AgingCounter[EventId] を更新する。
 *          Dem_AgingThresholdTable[EventId]（イベントごとの閾値）に達したら
 *          CONFIRMED を自動解除する。
 *          Dem_Init() からのみ呼び出すこと（呼び出し順序に依存するため非公開）。
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

/**
 * \brief   PendingDTC (UDS status bit2) を操作サイクル境界で判定する。
 *
 * \details SWS_Dem_00390 (Figure 7.19): PendingDTC は
 *          「NOT TestFailedThisOperationCycle AND NOT TestNotCompletedThisOperationCycle
 *          かつ操作サイクルの END/RESTART」または ClearDTC でクリアされる。
 *          CONFIRMED (bit3) のような多サイクルのエージングは行わず、直前サイクルが
 *          1 回でもクリーン（そのサイクル中に FAILED 確定が無く、かつテスト済み）
 *          であれば即座にクリアする点が Dem_EvaluateAging() との違い。
 *          Dem_EvaluateAging() と同じく、TF/TFTOC/TNCTC が新サイクル用に
 *          リセットされる前の値を見る必要があるため、Dem_Init() からのみ、
 *          かつそのリセットより前に呼び出すこと（呼び出し順序に依存するため非公開）。
 *
 * \param[in]  EventId  イベント ID。範囲チェックは呼び出し元の責務。
 */
static void Dem_EvaluatePendingClear(Dem_EventIdType EventId)
{
    const uint8 status = Dem_StatusTable[EventId];

    if ((status & DEM_STATUS_PENDING) == 0U)
        return; /* 既にクリア済み: 対象外 */

    const uint8 cleanCycle = ((status & DEM_STATUS_TF_THIS_OP_CYCLE) == 0U)
                           && ((status & DEM_STATUS_NOT_COMPLETED_THIS_CYCLE) == 0U);

    if (cleanCycle)
    {
        Dem_StatusTable[EventId] &= (uint8)(~DEM_STATUS_PENDING);
        DET_LOGI(TAG, "ev=%u pendingDTC cleared (clean operation cycle)", (unsigned)EventId);
    }
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

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
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_Init(void)
{
    uint8 magic = 0U;
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_MAGIC, &magic);

    if (magic == DEM_NVM_MAGIC_BYTE)
    {
        /* NvM から前回の最終状態を復元する */
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS,   Dem_StatusTable);
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_AGING,    Dem_AgingCounter);
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);

        /* 経年回復・PendingDTC の判定は「前サイクルの最終結果」を見る必要があるため、
         * TF/TFTOC/TNCTC を新サイクル用にリセットする前に行う */
        for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
        {
            Dem_EvaluateAging(i);
            Dem_EvaluatePendingClear(i);
        }

        /* 新しい操作サイクルの開始: TF / TFTOC はリセット時に必ずクリア */
        for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
        {
            Dem_StatusTable[i] = (Dem_StatusTable[i]
                                   & (uint8)(~DEM_STATUS_TEST_FAILED)
                                   & (uint8)(~DEM_STATUS_TF_THIS_OP_CYCLE))
                                  | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
        }

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
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS,   Dem_StatusTable);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING,    Dem_AgingCounter);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);
        uint8 magicVal = DEM_NVM_MAGIC_BYTE;
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_MAGIC, &magicVal);
        DET_LOGI(TAG, "Init first-run ev=%u", (unsigned)DEM_EVENT_COUNT);
    }

    /* FreezeFrame・デバウンスカウンタは RAM のみで管理するため EEPROM 復元はなく、
     * 常に未記録/中立 (0) から開始する */
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        Dem_FreezeFrameValid[i]  = 0U;
        Dem_DebounceCounter[i]   = 0;
    }
    Dem_CurrentContext.EngineSpeed = 0U;
    Dem_CurrentContext.CoolantTemp = 0U;
    Dem_CurrentContext.EngineState = 0U;
}

/**
 * \brief   イベントの発生/消滅を DEM に通知する (モニタからの生のテスト結果)。
 *
 * \details FAILED/PASSED の生の報告をそのまま確定はせず、まずデバウンスカウンタを
 *          ±1 する。カウンタがイベントごとの確定閾値 (Dem_DebounceLimitTable[EventId])
 *          に達した瞬間にのみ、DTC ステータスの TF/TFTOC/PDTC/CDTC/TFSLC を確定し
 *          NvM_WriteBlock() でステータステーブル全体を永続化する
 *          (FAILED 確定時は併せて FreezeFrame も更新し、ExtendedData の
 *          故障確定回数カウンタを +1 する)。
 *          まだ閾値に達していない間 (PRE-FAILED / PRE-PASSED) は
 *          DTC ステータスビットを変更しない。
 *          DEM_EVENT_STATUS_PREPASSED / PREFAILED はモニタからの入力としては
 *          受け付けない（Dem が内部カウンタから導出する値のため）。
 *
 * \param[in]  EventId      イベント ID (DEM_EVENT_* 定数)。
 * \param[in]  EventStatus  DEM_EVENT_STATUS_FAILED または DEM_EVENT_STATUS_PASSED。
 *
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_ReportErrorStatus(Dem_EventIdType EventId,
                            Dem_EventStatusType EventStatus)
{
    if (EventId >= DEM_EVENT_COUNT)
        return;
    if (EventStatus != DEM_EVENT_STATUS_FAILED && EventStatus != DEM_EVENT_STATUS_PASSED)
        return;

    const sint8 limit       = Dem_DebounceLimitTable[EventId];
    const sint8 prevCounter = Dem_DebounceCounter[EventId];
    sint8 counter = prevCounter;

    if (EventStatus == DEM_EVENT_STATUS_FAILED)
    {
        /* 確定 PASSED 側 (負) からの遷移は中立 (0) からやり直す。
         * そうしないと、反対側の確定状態から数え始めるせいで
         * 反転に 2*limit 回分の報告が必要になってしまう
         * (limit=1 の「1 回で即確定」が実質機能しなくなる)。 */
        if (counter < 0)
            counter = 0;
        if (counter < limit)
            counter++;
    }
    else /* DEM_EVENT_STATUS_PASSED */
    {
        /* 確定 FAILED 側 (正) からの遷移も同様に中立からやり直す */
        if (counter > 0)
            counter = 0;
        if (counter > -limit)
            counter--;
    }

    if (counter == prevCounter)
    {
        /* カウンタが上下限で飽和済み（既に確定済みの状態が継続）: 変化なしのため何もしない。
         * これにより、確定後も毎サイクル報告され続けるイベント（ADC_VOLT_LOW 等）で
         * 同じデバウンスログが繰り返し出力されることを防ぐ。 */
        return;
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
        status &= (uint8)(~DEM_STATUS_TF_THIS_OP_CYCLE);
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
        DET_LOGD(TAG, "ev=%u debounce=%d (%s)",
                 (unsigned)EventId, (int)counter,
                 (counter > 0) ? "PREFAILED" : (counter < 0) ? "PREPASSED" : "neutral");
        return;
    }

    /* ステータスが変化した場合のみ NvM (EEPROM) を更新 */
    if (status != prev)
    {
        Dem_StatusTable[EventId] = status;

        if (nowFailedConfirmed)
        {
            /* デバウンス確定 FAILED の瞬間にのみ FreezeFrame を更新する */
            Dem_FreezeFrameTable[EventId] = Dem_CurrentContext;
            Dem_FreezeFrameValid[EventId] = 1U;
            DET_LOGI(TAG, "FreezeFrame ev=%u spd=%u tmp=%u st=%u",
                     (unsigned)EventId,
                     (unsigned)Dem_CurrentContext.EngineSpeed,
                     (unsigned)Dem_CurrentContext.CoolantTemp,
                     (unsigned)Dem_CurrentContext.EngineState);

            /* ExtendedData: 確定 FAILED の累積回数を +1 (0xFF で飽和) */
            if (Dem_OccurrenceCounter[EventId] < 0xFFU)
                Dem_OccurrenceCounter[EventId]++;
            (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);
            DET_LOGI(TAG, "ExtendedData ev=%u occurrence=%u",
                     (unsigned)EventId, (unsigned)Dem_OccurrenceCounter[EventId]);
        }

        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);
    }
}

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
uint8 Dem_GetStatusOfEvent(Dem_EventIdType EventId)
{
    if (EventId >= DEM_EVENT_COUNT)
        return 0U;
    return Dem_StatusTable[EventId] & DEM_STATUS_AVAILABILITY_MASK;
}

/**
 * \brief   イベント ID から DTC コード (24-bit) を取得する。
 *
 * \param[in]   EventId  イベント ID (DEM_EVENT_* 定数)。
 * \param[out]  DTC      DTC コードの格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  EventId が範囲外、または DTC が NULL。
 *
 * \ServiceID      {0x1A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetDTCOfEvent(Dem_EventIdType EventId, uint32* DTC)
{
    if (EventId >= DEM_EVENT_COUNT || DTC == NULL)
        return E_NOT_OK;
    *DTC = Dem_DtcTable[EventId];
    return E_OK;
}

/**
 * \brief   指定イベントの DTC ステータス・デバウンスカウンタ・経年回復カウンタ・
 *          ExtendedData・FreezeFrame を初期状態に戻す（NvM 書き込みは呼び出し元が行う）。
 *
 * \param[in]  EventId  イベント ID (DEM_EVENT_* 定数)。範囲チェックは呼び出し元の責務。
 */
static void Dem_ClearOne(Dem_EventIdType EventId)
{
    Dem_StatusTable[EventId] = DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR
                             | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
    Dem_DebounceCounter[EventId]    = 0;
    Dem_AgingCounter[EventId]       = 0U;
    Dem_OccurrenceCounter[EventId]  = 0U;
    Dem_FreezeFrameValid[EventId]   = 0U;
}

/**
 * \brief   全 DTC をクリアし、NvM (EEPROM) を初期状態へ戻す。
 *
 * \details 全イベントのステータスを TNCLC | TNCTOC にリセットし、
 *          経年回復カウンタ・ExtendedData も 0 に戻して NvM_WriteBlock() で永続化する。
 *          マジックバイトは保持する（再初期化は不要）。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0x23}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_ClearAllDTCs(void)
{
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        Dem_ClearOne(i);
    }
    (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS,   Dem_StatusTable);
    (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING,    Dem_AgingCounter);
    (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);
    DET_LOGI(TAG, "ClearAll ok");
    return E_OK;
}

/**
 * \brief   指定イベントの DTC のみをクリアし、NvM (EEPROM) へ反映する。
 *
 * \details SID 0x14 ClearDiagnosticInformation のグループ指定クリア
 *          (特定の DTC コードのみを指定するケース) から呼び出す。
 *          ステータスを TNCLC | TNCTOC にリセットし、デバウンスカウンタ・
 *          経年回復カウンタ・ExtendedData・FreezeFrame も未記録状態に戻す。
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
Std_ReturnType Dem_ClearDTC(Dem_EventIdType EventId)
{
    if (EventId >= DEM_EVENT_COUNT)
        return E_NOT_OK;

    Dem_ClearOne(EventId);
    (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS,   Dem_StatusTable);
    (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING,    Dem_AgingCounter);
    (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, Dem_OccurrenceCounter);
    DET_LOGI(TAG, "Clear ev=%u ok", (unsigned)EventId);
    return E_OK;
}

/**
 * \brief   ステータスマスクに一致する全 DTC を列挙する。
 *
 * \details 各イベントのステータスバイトと statusMask の AND が非ゼロなら
 *          dtcBuf / statusBuf に追加して count をインクリメントする。
 *          DCM SID 0x19 サブ機能 0x01 / 0x02 から呼び出す。
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
void Dem_GetAllDTCs(uint32* dtcBuf, uint8* statusBuf,
                     uint8* count, uint8 statusMask)
{
    *count = 0U;
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        uint8 status = Dem_StatusTable[i] & DEM_STATUS_AVAILABILITY_MASK;
        if ((status & statusMask) != 0U)
        {
            dtcBuf[*count]    = Dem_DtcTable[i];
            statusBuf[*count] = status;
            (*count)++;
        }
    }
}

/**
 * \brief   FreezeFrame として保存する現在値を更新する。
 *
 * \details Dem_ReportErrorStatus() が参照する「現在値」を上書きするだけで、
 *          この時点では FreezeFrameTable へのコピーは行わない。
 *
 * \ServiceID      {0x25}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_SetFreezeFrameContext(uint16 EngineSpeed, uint8 CoolantTemp, uint8 EngineState)
{
    Dem_CurrentContext.EngineSpeed = EngineSpeed;
    Dem_CurrentContext.CoolantTemp = CoolantTemp;
    Dem_CurrentContext.EngineState = EngineState;
}

/**
 * \brief   指定イベントに保存された FreezeFrame を取得する。
 *
 * \ServiceID      {0x26}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetFreezeFrameOfEvent(Dem_EventIdType EventId, Dem_FreezeFrameType* Frame)
{
    if (EventId >= DEM_EVENT_COUNT || Frame == NULL || Dem_FreezeFrameValid[EventId] == 0U)
        return E_NOT_OK;
    *Frame = Dem_FreezeFrameTable[EventId];
    return E_OK;
}

/**
 * \brief   DTC コード (24-bit) から EventId を逆引きする。
 *
 * \ServiceID      {0x27}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetEventIdOfDTC(uint32 DTC, Dem_EventIdType* EventId)
{
    if (EventId == NULL)
        return E_NOT_OK;

    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        if (Dem_DtcTable[i] == DTC)
        {
            *EventId = i;
            return E_OK;
        }
    }
    return E_NOT_OK;
}

/**
 * \brief   指定イベントの ExtendedData（故障確定回数）を取得する。
 *
 * \details FreezeFrame と異なり「記録なし」という状態を持たない
 *          （一度も確定 FAILED していなければ単に 0）。
 *
 * \ServiceID      {0x29}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dem_GetOccurrenceCounterOfEvent(Dem_EventIdType EventId, uint8* Counter)
{
    if (EventId >= DEM_EVENT_COUNT || Counter == NULL)
        return E_NOT_OK;
    *Counter = Dem_OccurrenceCounter[EventId];
    return E_OK;
}
