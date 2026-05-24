/**
 * \file    NvM.c
 * \brief   Non-Volatile Memory Manager 実装 (AUTOSAR SWS_NvM 準拠)
 * \details avr/eeprom.h を直接利用するファイルはこのファイルだけ。
 *          上位モジュール (DEM など) は avr/eeprom.h を include せず、
 *          NvM の API 経由で EEPROM にアクセスする。
 *
 *          RAM ミラー設計:
 *            NvM_Init() が EEPROM → RAM ミラーを一括ロードする。
 *            NvM_ReadBlock() は RAM ミラーを参照するだけで EEPROM を読まない。
 *            NvM_WriteBlock() は src → RAM ミラーをコピーしたのち
 *            eeprom_update_block で差分バイトのみ EEPROM へ書く。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "NvM.h"
#include "Det.h"
#include <avr/eeprom.h>
#include <string.h>

#define TAG "NvM"

static const NvM_ConfigType* NvM_Cfg = NULL;

/* -----------------------------------------------------------------------
 * 内部ヘルパー
 * ----------------------------------------------------------------------- */

static const NvM_BlockDescriptorType* NvM_GetBlock(NvM_BlockIdType id)
{
    if (NvM_Cfg == NULL || id >= NvM_Cfg->NumBlocks)
        return NULL;
    return &NvM_Cfg->Blocks[id];
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   全ブロックの EEPROM 内容を RAM ミラーへ一括ロードする。
 *
 * \details eeprom_read_block は EEPROM アドレス → RAM へバイト列をコピーする
 *          AVR-libc 関数。引数順は (dst_RAM, src_EEPROM, size)。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void NvM_Init(const NvM_ConfigType* ConfigPtr)
{
    NvM_Cfg = ConfigPtr;

    for (uint8 i = 0U; i < ConfigPtr->NumBlocks; i++)
    {
        const NvM_BlockDescriptorType* blk = &ConfigPtr->Blocks[i];
        if (blk->RamBlockDataAddress != NULL)
        {
            eeprom_read_block(
                blk->RamBlockDataAddress,
                (const void*)(uintptr_t)blk->NvMNvBlockBaseNumber,
                blk->NvMNvBlockLength);
        }
    }

    DET_LOGI(TAG, "Init ok blocks=%u", (unsigned)ConfigPtr->NumBlocks);
}

/**
 * \brief   RAM ミラーの内容を NvM_DstPtr へコピーする。
 *
 * \details EEPROM アクセスは発生しない。NvM_Init() 後であれば RAM ミラーは
 *          常に最新の EEPROM 値を保持している。
 *
 * \ServiceID      {0x16}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void* NvM_DstPtr)
{
    const NvM_BlockDescriptorType* blk = NvM_GetBlock(BlockId);
    if (blk == NULL || NvM_DstPtr == NULL || blk->RamBlockDataAddress == NULL)
        return E_NOT_OK;

    memcpy(NvM_DstPtr, blk->RamBlockDataAddress, blk->NvMNvBlockLength);
    return E_OK;
}

/**
 * \brief   NvM_SrcPtr を RAM ミラーへコピーし、EEPROM を更新する。
 *
 * \details eeprom_update_block は EEPROM の各バイトと比較し、
 *          値が異なる場合のみ物理書き込みを行う (EEPROM 消耗低減)。
 *          引数順は (src_RAM, dst_EEPROM, size)。
 *
 * \ServiceID      {0x0D}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr)
{
    const NvM_BlockDescriptorType* blk = NvM_GetBlock(BlockId);
    if (blk == NULL || NvM_SrcPtr == NULL || blk->RamBlockDataAddress == NULL)
        return E_NOT_OK;

    /* RAM ミラーを最新値で更新 */
    memcpy(blk->RamBlockDataAddress, NvM_SrcPtr, blk->NvMNvBlockLength);

    /* EEPROM への差分書き込み (eeprom_update_block は比較してから書く) */
    eeprom_update_block(
        blk->RamBlockDataAddress,
        (void*)(uintptr_t)blk->NvMNvBlockBaseNumber,
        blk->NvMNvBlockLength);

    return E_OK;
}
