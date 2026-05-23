/**
 * \file    Dem.c
 * \brief   診断イベントマネージャ (AUTOSAR SWS_DEM 準拠)
 * \details SW-C / BSW モジュールから報告されるイベントを受け取り、
 *          DTC (Diagnostic Trouble Code) のライフサイクルを管理する。
 *
 *          DTC ライフサイクル:
 *            報告なし → TNCLC=1, TNCTOC=1 (未テスト状態)
 *            PASSED 報告 → TF クリア (CDTC は保持)
 *            FAILED 報告 → TF=1, PDTC=1, CDTC=1, TFSLC=1 (EEPROM 保存)
 *            ClearDTC 後 → 全ビットリセット, TNCLC=1
 *
 *          AUTOSAR 実装との主な違い (学習用簡略化):
 *            - デバウンスカウンタなし (FAILED 即確定)
 *            - 操作サイクル管理なし (電源オフ=クリアではない)
 *            - NvM モジュールを経由せず EEPROM を直接アクセス
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
#include <avr/eeprom.h>

/* -----------------------------------------------------------------------
 * モジュール内部状態
 * ----------------------------------------------------------------------- */

/** 各イベントの DTC ステータスバイト (EEPROM の RAM ミラー) */
static uint8 Dem_StatusTable[DEM_EVENT_COUNT];

/** イベント ID → DTC コード (24-bit) 変換テーブル */
static const uint32 Dem_DtcTable[DEM_EVENT_COUNT] = {
    DEM_DTC_ENGINE_OVERHEAT,         /* event 0 */
    DEM_DTC_ENGINE_STALL,            /* event 1 */
    DEM_DTC_ENGINE_SPEED_NO_FLAG,    /* event 2 */
    DEM_DTC_STARTING_TIMEOUT         /* event 3 */
};

/* -----------------------------------------------------------------------
 * EEPROM アクセスヘルパー
 * avr/eeprom.h の eeprom_update_byte はデータが同じなら書き込みをスキップする。
 * ----------------------------------------------------------------------- */

static void Dem_NvmWrite(Dem_EventIdType id, uint8 status)
{
    eeprom_update_byte(
        (uint8_t*)(uintptr_t)(DEM_NVM_STATUS_BASE_ADDR + (uint8)id),
        status);
}

static uint8 Dem_NvmRead(Dem_EventIdType id)
{
    return eeprom_read_byte(
        (const uint8_t*)(uintptr_t)(DEM_NVM_STATUS_BASE_ADDR + (uint8)id));
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   DEM を初期化する。EEPROM から前回起動の DTC 状態を復元する。
 *
 * \details EEPROM のマジックバイトが有効 (0xDE) なら各イベントの
 *          DTC ステータスを復元する。電源サイクル後なので TF は常にクリアし、
 *          TNCTOC (今サイクル未テスト) を設定する。
 *          マジックバイトが無効 (初回起動 / EEPROM 破損) なら
 *          全イベントを TNCLC | TNCTOC の初期状態にしてマジックを書き込む。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dem_Init(void)
{
    uint8 magic = eeprom_read_byte(
        (const uint8_t*)(uintptr_t)DEM_NVM_MAGIC_ADDR);

    if (magic == DEM_NVM_MAGIC_BYTE)
    {
        /* EEPROM からステータスを復元。TF/TFTOC はリセット時に必ずクリア */
        for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
        {
            Dem_StatusTable[i] = (Dem_NvmRead(i)
                                   & (uint8)(~DEM_STATUS_TEST_FAILED)
                                   & (uint8)(~DEM_STATUS_TF_THIS_OP_CYCLE))
                                  | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
        }
        Det_LogP(PSTR("[Dem_Init] restored from NvM"));
    }
    else
    {
        /* 初回起動または EEPROM 破損: 全イベントを初期状態に設定 */
        for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
        {
            Dem_StatusTable[i] = DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR
                               | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE;
            Dem_NvmWrite(i, Dem_StatusTable[i]);
        }
        eeprom_update_byte(
            (uint8_t*)(uintptr_t)DEM_NVM_MAGIC_ADDR,
            DEM_NVM_MAGIC_BYTE);
        Det_LogP(PSTR("[Dem_Init] NvM init (first run)"));
    }

    Det_PrintP(PSTR("  events="));
    Det_PrintDec(DEM_EVENT_COUNT);
    Det_Newline();
}

/**
 * \brief   イベントの発生/消滅を DEM に通知する。
 *
 * \details FAILED 通知で DTC ステータスの TF/TFTOC/PDTC/CDTC/TFSLC を設定し
 *          EEPROM へ書き込む (AUTOSAR SWS_Dem_00036)。
 *          PASSED 通知で TF/TFTOC/TNCTOC をクリアするが CDTC/TFSLC は保持する。
 *          EEPROM への書き込みはステータスが変化した場合のみ行う。
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

        Det_PrintP(PSTR("[Dem] FAILED ev="));
        Det_PrintDec(EventId);
        Det_PrintP(PSTR(" DTC=0x"));
        Det_PrintHex((uint8)(Dem_DtcTable[EventId] >> 16U));
        Det_PrintHex((uint8)(Dem_DtcTable[EventId] >>  8U));
        Det_PrintHex((uint8)(Dem_DtcTable[EventId]));
        Det_Newline();
    }
    else if (EventStatus == DEM_EVENT_STATUS_PASSED)
    {
        status &= (uint8)(~DEM_STATUS_TEST_FAILED);
        status &= (uint8)(~DEM_STATUS_TF_THIS_OP_CYCLE);
        status &= (uint8)(~DEM_STATUS_NOT_COMPLETED_THIS_CYCLE);
        /* CDTC / PDTC / TFSLC は保持 — SID 0x14 でのみクリア可能 */
    }

    /* ステータスが変化した場合のみ EEPROM を更新 */
    if (status != prev)
    {
        Dem_StatusTable[EventId] = status;
        Dem_NvmWrite(EventId, status);
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
 * \brief   全 DTC をクリアし、EEPROM を初期状態へ戻す。
 *
 * \details 全イベントのステータスを TNCLC | TNCTOC にリセットし、
 *          EEPROM へ書き込む (AUTOSAR SWS_Dem_00570)。
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
        Dem_NvmWrite(i, Dem_StatusTable[i]);
    }
    Det_LogP(PSTR("[Dem] All DTCs cleared"));
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
