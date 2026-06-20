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
 *              NVM_BLOCK_ID_DEM_MAGIC  — 有効データマーカー (1 byte)
 *              NVM_BLOCK_ID_DEM_STATUS — イベントステータステーブル (7 bytes)
 *
 *          デバウンス (counter-based debouncing):
 *            各イベントは -DEM_DEBOUNCE_LIMIT 〜 +DEM_DEBOUNCE_LIMIT のカウンタを持つ。
 *            FAILED 報告でカウンタ+1、PASSED 報告でカウンタ-1 (上下限でクランプ)。
 *            カウンタが +LIMIT に達した瞬間にのみ DTC ステータスを確定 FAILED にし、
 *            -LIMIT に達した瞬間にのみ確定 PASSED にする。
 *            その間 (PRE-FAILED / PRE-PASSED) は DTC ステータスビットを変更しない。
 *            1 回の報告では確定しないため、単発の事象は DTC を確定させない
 *            （実車の「数サイクル/数回の再現で確定」という挙動の簡易再現）。
 *
 *          DTC ライフサイクル:
 *            報告なし → TNCLC=1, TNCTOC=1 (未テスト状態)
 *            PASSED デバウンス確定 → TF クリア (CDTC は保持)
 *            FAILED デバウンス確定 → TF=1, PDTC=1, CDTC=1, TFSLC=1 (NvM 経由で保存)
 *            ClearDTC 後 → 全ビットリセット, TNCLC=1
 *
 *          AUTOSAR 実装との主な違い (学習用簡略化):
 *            - 全イベント共通の単一デバウンス閾値 (実車は DemDebounceAlgorithmClass で
 *              イベントごとに個別設定可能)
 *            - デバウンスカウンタは RAM のみ保持 (電源 OFF でリセット)
 *            - 操作サイクル管理なし
 *            - FreezeFrame は RAM のみに保持 (EEPROM 非永続化、電源 OFF で消去)
 *            - ExtendedData 未対応
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
    DEM_DTC_ADC_VOLT_LOW             /* event 6 */
};

/** イベントごとの FreezeFrame (故障時スナップショット)。RAM のみ保持 */
static Dem_FreezeFrameType Dem_FreezeFrameTable[DEM_EVENT_COUNT];

/** Dem_FreezeFrameTable[i] が有効か (0=未記録, 1=記録あり) */
static uint8 Dem_FreezeFrameValid[DEM_EVENT_COUNT];

/** SW-C が毎周期更新する「現在値」。FAILED 遷移時にこの値をコピーする */
static Dem_FreezeFrameType Dem_CurrentContext;

/** イベントごとのデバウンスカウンタ (-DEM_DEBOUNCE_LIMIT〜+DEM_DEBOUNCE_LIMIT, 0=中立) */
static sint8 Dem_DebounceCounter[DEM_EVENT_COUNT];

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   DEM を初期化する。NvM から前回起動の DTC 状態を復元する。
 *
 * \details NvM_Init() 完了後に呼び出すこと。
 *          NVM_BLOCK_ID_DEM_MAGIC のマジックバイトが有効 (0xDE) なら
 *          NVM_BLOCK_ID_DEM_STATUS からイベントステータスを復元する。
 *          電源サイクル後なので TF / TFTOC は常にクリアし、
 *          TNCTOC (今サイクル未テスト) を設定する。
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
        /* NvM から前回のステータスを復元。TF / TFTOC はリセット時に必ずクリア */
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);
        for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
        {
            Dem_StatusTable[i] = (Dem_StatusTable[i]
                                   & (uint8)(~DEM_STATUS_TEST_FAILED)
                                   & (uint8)(~DEM_STATUS_TF_THIS_OP_CYCLE))
                                  | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
        }
        DET_LOGI(TAG, "Init NvM restored ev=%u", (unsigned)DEM_EVENT_COUNT);
    }
    else
    {
        /* 初回起動または EEPROM 破損: 全イベントを初期状態に設定 */
        for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
        {
            Dem_StatusTable[i] = DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR
                               | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
        }
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);
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
 *          ±1 する。カウンタが ±DEM_DEBOUNCE_LIMIT に達した瞬間にのみ、
 *          DTC ステータスの TF/TFTOC/PDTC/CDTC/TFSLC を確定し
 *          NvM_WriteBlock() でステータステーブル全体を永続化する
 *          (FAILED 確定時は併せて FreezeFrame も更新する)。
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

    const sint8 prevCounter = Dem_DebounceCounter[EventId];
    sint8 counter = prevCounter;

    if (EventStatus == DEM_EVENT_STATUS_FAILED)
    {
        if (counter < DEM_DEBOUNCE_LIMIT)
            counter++;
    }
    else /* DEM_EVENT_STATUS_PASSED */
    {
        if (counter > -DEM_DEBOUNCE_LIMIT)
            counter--;
    }

    if (counter == prevCounter)
    {
        /* カウンタが上下限で飽和済み（既に確定済みの状態が継続）: 変化なしのため何もしない。
         * これにより、確定後も毎サイクル報告され続けるイベント（BUTTON_STUCK 等）で
         * 同じデバウンスログが繰り返し出力されることを防ぐ。 */
        return;
    }
    Dem_DebounceCounter[EventId] = counter;

    const uint8 wasFailedConfirmed = (prevCounter >= DEM_DEBOUNCE_LIMIT)  ? 1U : 0U;
    const uint8 wasPassedConfirmed = (prevCounter <= -DEM_DEBOUNCE_LIMIT) ? 1U : 0U;
    const uint8 nowFailedConfirmed = (counter     >= DEM_DEBOUNCE_LIMIT)  ? 1U : 0U;
    const uint8 nowPassedConfirmed = (counter     <= -DEM_DEBOUNCE_LIMIT) ? 1U : 0U;

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
 * \brief   全 DTC をクリアし、NvM (EEPROM) を初期状態へ戻す。
 *
 * \details 全イベントのステータスを TNCLC | TNCTOC にリセットし、
 *          NvM_WriteBlock() でテーブル全体を永続化する。
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
        Dem_StatusTable[i] = DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR
                           | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
    }
    (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, Dem_StatusTable);
    DET_LOGI(TAG, "ClearAll ok");
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
