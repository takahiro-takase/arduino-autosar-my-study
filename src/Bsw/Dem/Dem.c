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
 *          DTC ライフサイクル:
 *            報告なし → TNCLC=1, TNCTOC=1 (未テスト状態)
 *            PASSED 報告 → TF クリア (CDTC は保持)
 *            FAILED 報告 → TF=1, PDTC=1, CDTC=1, TFSLC=1 (NvM 経由で保存)
 *            ClearDTC 後 → 全ビットリセット, TNCLC=1
 *
 *          AUTOSAR 実装との主な違い (学習用簡略化):
 *            - デバウンスカウンタなし (FAILED 即確定)
 *            - 操作サイクル管理なし
 *            - FreezeFrame / ExtendedData 未対応
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
}

/**
 * \brief   イベントの発生/消滅を DEM に通知する。
 *
 * \details FAILED 通知で DTC ステータスの TF/TFTOC/PDTC/CDTC/TFSLC を設定し
 *          NvM_WriteBlock() でステータステーブル全体を永続化する。
 *          PASSED 通知で TF/TFTOC/TNCTOC をクリアするが CDTC/TFSLC は保持する。
 *          ステータスが変化した場合のみ NvM 書き込みを行う。
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

    uint8 prev   = Dem_StatusTable[EventId];
    uint8 status = prev;

    if (EventStatus == DEM_EVENT_STATUS_FAILED)
    {
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
    else if (EventStatus == DEM_EVENT_STATUS_PASSED)
    {
        status &= (uint8)(~DEM_STATUS_TEST_FAILED);
        status &= (uint8)(~DEM_STATUS_TF_THIS_OP_CYCLE);
        status &= (uint8)(~DEM_STATUS_NOT_COMPLETED_THIS_CYCLE);
        /* CDTC / PDTC / TFSLC は保持 — SID 0x14 でのみクリア可能 */
    }

    /* ステータスが変化した場合のみ NvM (EEPROM) を更新 */
    if (status != prev)
    {
        Dem_StatusTable[EventId] = status;
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
