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
 *          ROM デフォルト値は CRC 不一致時に NvM が自動的に復元する内容
 *          （AUTOSAR NvMBlockDescriptor の NvMRomBlockDataAddress に相当）。
 *          DEM_STATUS のデフォルトは Dem_Init() の初回起動時と同じ値にするため
 *          Dem_Cfg.h の定数を使用する（PBCfg はモジュール間の配線を行う
 *          コンフィグ層であり、複数モジュールの定義を参照してよい）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "NvM_PBCfg.h"
#include "NvM_Cfg.h"
#include "Dem_Cfg.h"

/* -----------------------------------------------------------------------
 * RAM ミラーバッファ (NvM が管理; 上位モジュールは直接参照しない)
 * NvM_Init() で EEPROM 内容がここへ展開される。
 * ----------------------------------------------------------------------- */
static uint8 NvM_Ram_DemMagic[NVM_BLOCK_DEM_MAGIC_LENGTH];
static uint8 NvM_Ram_DemStatus[NVM_BLOCK_DEM_STATUS_LENGTH];
static uint8 NvM_Ram_DemAging[NVM_BLOCK_DEM_AGING_LENGTH];

/* -----------------------------------------------------------------------
 * ROM デフォルト値 (CRC 不一致時に NvM_Init()/NvM_RestoreBlockDefaults() が
 * RAM ミラーへ適用し、CRC を付け直して EEPROM へ書き戻す内容)
 * ----------------------------------------------------------------------- */

/** マジックバイトのデフォルトは DEM_NVM_MAGIC_BYTE と異なる値（無効）にする。
 *  MAGIC ブロックが破損から復元された場合でも、Dem_Init() 側の既存ロジックが
 *  そのまま「初回起動扱い」として STATUS/AGING を含め一貫した初期化を行う。 */
static const uint8 NvM_Default_DemMagic = 0x00U;

/** DEM_STATUS のデフォルトは Dem_Init() の初回起動時と同じ値
 *  (DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE)。
 *  MAGIC は無事だが STATUS だけ破損した場合でも、Dem が想定する
 *  「正常な初期状態」と一致させるため。 */
static const uint8 NvM_Default_DemStatus[NVM_BLOCK_DEM_STATUS_LENGTH] =
{
    DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE,
    DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE,
    DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE,
    DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE,
    DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE,
    DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE,
    DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE,
    DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE
};

/* DEM_AGING はデフォルトを定義しない (NULL)。経年回復カウンタの初回起動値は
 * 全イベント 0 であり、これは NvM が NULL の場合に行う全 0 フィルそのものと
 * 一致するため、専用の ROM テーブルを用意する必要がない。 */

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
        NvM_Ram_DemMagic,                  /* RamBlockDataAddress  */
        &NvM_Default_DemMagic              /* RomBlockDataAddress  */
    },
    /* NVM_BLOCK_ID_DEM_STATUS */
    {
        NVM_BLOCK_DEM_STATUS_EEPROM_ADDR,  /* NvMNvBlockBaseNumber */
        NVM_BLOCK_DEM_STATUS_LENGTH,       /* NvMNvBlockLength     */
        NvM_Ram_DemStatus,                 /* RamBlockDataAddress  */
        NvM_Default_DemStatus              /* RomBlockDataAddress  */
    },
    /* NVM_BLOCK_ID_DEM_AGING */
    {
        NVM_BLOCK_DEM_AGING_EEPROM_ADDR,   /* NvMNvBlockBaseNumber */
        NVM_BLOCK_DEM_AGING_LENGTH,        /* NvMNvBlockLength     */
        NvM_Ram_DemAging,                  /* RamBlockDataAddress  */
        NULL                               /* RomBlockDataAddress: 未設定→全0で代替 */
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
