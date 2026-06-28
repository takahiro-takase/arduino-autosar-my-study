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
 *          CRC によるデータ破損検出:
 *            各ブロックのデータ本体直後の 1 バイトに AUTOSAR Crc8
 *            (SAE J1850: 多項式 0x1D, 初期値 0xFF, 最終 XOR 0xFF) を保存する。
 *            NvM_Init() は読み込み直後に CRC を再計算して検証し、
 *            不一致ならビット化けや書き込み中の電源断による破損とみなして
 *            NvM_RestoreBlockDefaults() と同等の処理を自動的に行う
 *            (ROM デフォルト値、未設定なら全 0 で復元し EEPROM へ書き戻す)。
 *            実際の AUTOSAR Crc モジュールへの委譲は行わず、本ファイル内に
 *            最小実装を持つ（学習用簡略化。Crc モジュール自体は本プロジェクトの
 *            対応範囲外）。
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

/** AUTOSAR Crc8 (SAE J1850) パラメータ */
#define NVM_CRC8_INITIAL  0xFFU
#define NVM_CRC8_POLY     0x1DU
#define NVM_CRC8_XOR_OUT  0xFFU

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

/**
 * \brief   AUTOSAR Crc8 (SAE J1850) アルゴリズムでブロックの CRC を計算する。
 *
 * \details 多項式 0x1D、初期値 0xFF、入力/出力の反転なし、最終 XOR 0xFF。
 *          ブロックは最大 8 バイトと小さいため、テーブルなしのビット単位
 *          計算で十分な速度が得られる。
 */
static uint8 NvM_CalcCrc8(const uint8* data, uint16 length)
{
    uint8 crc = NVM_CRC8_INITIAL;
    uint16 i;
    for (i = 0U; i < length; i++)
    {
        crc ^= data[i];
        uint8 bit;
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (uint8)(((crc & 0x80U) != 0U)
                           ? ((crc << 1U) ^ NVM_CRC8_POLY)
                           : (crc << 1U));
        }
    }
    return (uint8)(crc ^ NVM_CRC8_XOR_OUT);
}

/** ブロックの CRC 保存先 (データ本体直後の 1 バイト)。 */
static uint16 NvM_CrcAddress(const NvM_BlockDescriptorType* blk)
{
    return (uint16)(blk->NvMNvBlockBaseNumber + blk->NvMNvBlockLength);
}

/**
 * \brief   ブロックの RAM ミラーへデフォルト値を適用し、CRC とともに
 *          EEPROM へ書き戻す。
 *
 * \details NvM_Init() の CRC 不一致時と NvM_RestoreBlockDefaults() の
 *          両方から呼ばれる共通処理。
 */
static void NvM_ApplyDefault(NvM_BlockIdType id, const NvM_BlockDescriptorType* blk)
{
    if (blk->RomBlockDataAddress != NULL)
        memcpy(blk->RamBlockDataAddress, blk->RomBlockDataAddress, blk->NvMNvBlockLength);
    else
        memset(blk->RamBlockDataAddress, 0, blk->NvMNvBlockLength);

    eeprom_update_block(
        blk->RamBlockDataAddress,
        (void*)(uintptr_t)blk->NvMNvBlockBaseNumber,
        blk->NvMNvBlockLength);

    uint8 crc = NvM_CalcCrc8((const uint8*)blk->RamBlockDataAddress, blk->NvMNvBlockLength);
    eeprom_update_byte((uint8*)(uintptr_t)NvM_CrcAddress(blk), crc);

    DET_LOGW(TAG, "block=%u defaults restored (%s)", (unsigned)id,
             (blk->RomBlockDataAddress != NULL) ? "ROM default" : "zero-fill");
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   全ブロックの EEPROM 内容を RAM ミラーへ一括ロードする。
 *
 * \details eeprom_read_block は EEPROM アドレス → RAM へバイト列をコピーする
 *          AVR-libc 関数。引数順は (dst_RAM, src_EEPROM, size)。
 *          読み込み直後に各ブロックの CRC を検証し、不一致ならデフォルト値
 *          (NvM_ApplyDefault()) で復元する。
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
        if (blk->RamBlockDataAddress == NULL)
            continue;

        eeprom_read_block(
            blk->RamBlockDataAddress,
            (const void*)(uintptr_t)blk->NvMNvBlockBaseNumber,
            blk->NvMNvBlockLength);

        uint8 storedCrc = eeprom_read_byte((const uint8*)(uintptr_t)NvM_CrcAddress(blk));
        uint8 calcCrc    = NvM_CalcCrc8((const uint8*)blk->RamBlockDataAddress, blk->NvMNvBlockLength);

        if (storedCrc != calcCrc)
        {
            DET_LOGE(TAG, "block=%u CRC mismatch (stored=0x%02X calc=0x%02X)",
                     (unsigned)i, (unsigned)storedCrc, (unsigned)calcCrc);
            NvM_ApplyDefault(i, blk);
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
 *          書き込んだ内容から CRC を再計算し、データ本体直後の 1 バイトへ
 *          併せて保存する。
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

    uint8 crc = NvM_CalcCrc8((const uint8*)blk->RamBlockDataAddress, blk->NvMNvBlockLength);
    eeprom_update_byte((uint8*)(uintptr_t)NvM_CrcAddress(blk), crc);

    return E_OK;
}

/**
 * \brief   指定ブロックを ROM デフォルト値（未設定なら全 0）へ復元する。
 *
 * \details NvM_Init() が CRC 不一致時に内部的に行う処理
 *          (NvM_ApplyDefault()) を、明示的に呼び出せるようにする。
 *
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId)
{
    const NvM_BlockDescriptorType* blk = NvM_GetBlock(BlockId);
    if (blk == NULL || blk->RamBlockDataAddress == NULL)
        return E_NOT_OK;

    NvM_ApplyDefault(BlockId, blk);
    return E_OK;
}
