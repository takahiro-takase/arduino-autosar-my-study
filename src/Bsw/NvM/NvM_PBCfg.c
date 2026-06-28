/**
 * \file    NvM_PBCfg.c
 * \brief   NvM ポストビルドコンフィグ 定義
 * \details 各ブロックの RAM ミラーバッファとブロック記述子テーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイル
 *          に相当する。
 *
 *          RAM ミラーはここで宣言した静的バッファ。
 *          NvM_Init() が EEPROM → これらバッファへロードし、
 *          NvM_WriteBlock() がバッファ → EEPROM へライトバックする。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "NvM_PBCfg.h"
#include "NvM_Cfg.h"

/* -----------------------------------------------------------------------
 * RAM ミラーバッファ (NvM が管理; 上位モジュールは直接参照しない)
 * NvM_Init() で EEPROM 内容がここへ展開される。
 * ----------------------------------------------------------------------- */
static uint8 NvM_Ram_DemMagic[NVM_BLOCK_DEM_MAGIC_LENGTH];
static uint8 NvM_Ram_DemStatus[NVM_BLOCK_DEM_STATUS_LENGTH];
static uint8 NvM_Ram_DemAging[NVM_BLOCK_DEM_AGING_LENGTH];

/* -----------------------------------------------------------------------
 * ブロック記述子テーブル
 * インデックスがそのまま NvM_BlockIdType の値に対応する。
 * ----------------------------------------------------------------------- */
static const NvM_BlockDescriptorType NvM_BlockTable[NVM_BLOCK_COUNT] =
{
    /* NVM_BLOCK_ID_DEM_MAGIC */
    {
        NVM_BLOCK_DEM_MAGIC_EEPROM_ADDR,   /* NvMNvBlockBaseNumber */
        NVM_BLOCK_DEM_MAGIC_LENGTH,        /* NvMNvBlockLength     */
        NvM_Ram_DemMagic                   /* RamBlockDataAddress  */
    },
    /* NVM_BLOCK_ID_DEM_STATUS */
    {
        NVM_BLOCK_DEM_STATUS_EEPROM_ADDR,  /* NvMNvBlockBaseNumber */
        NVM_BLOCK_DEM_STATUS_LENGTH,       /* NvMNvBlockLength     */
        NvM_Ram_DemStatus                  /* RamBlockDataAddress  */
    },
    /* NVM_BLOCK_ID_DEM_AGING */
    {
        NVM_BLOCK_DEM_AGING_EEPROM_ADDR,   /* NvMNvBlockBaseNumber */
        NVM_BLOCK_DEM_AGING_LENGTH,        /* NvMNvBlockLength     */
        NvM_Ram_DemAging                   /* RamBlockDataAddress  */
    }
};

/* -----------------------------------------------------------------------
 * ポストビルドコンフィグインスタンス (EcuM が NvM_Init に渡す)
 * ----------------------------------------------------------------------- */
const NvM_ConfigType NvM_Config =
{
    NvM_BlockTable,
    NVM_BLOCK_COUNT
};
