/**
 * \file    Dem_PBCfg.h
 * \brief   DEM ポストビルドコンフィグ 外部宣言
 * \details イベントごとの設定テーブル（DTC コード、デバウンスの確定閾値、
 *          経年回復 (Aging) の閾値）の外部宣言を提供する。
 *          テーブルの定義は Dem_PBCfg.c にあり、config/data/Dem.json から生成する
 *          （実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する）。
 *          添字はイベント ID（DemConf_DemEventParameter_*）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DEM_PBCFG_H
#define DEM_PBCFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "Dem_Cfg.h"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/** データ要素の読み出し関数（DemDataElementReadFnc、[SWS_Dem_00464]）。Dem が、FreezeFrame を取る瞬間に呼ぶ。
 *  Buffer へ、データ要素を、DemDataElementClass の大きさ（バイト）だけ書く（MSB first）。 */
typedef Std_ReturnType (*Dem_DataElementReadFncType)(uint8* Buffer);

/** FreezeFrame の DID の構成（DemDidClass）。 */
typedef struct
{
    uint16                     Did;      /**< DID */
    uint8                      Size;     /**< データ要素の大きさ（バイト） */
    Dem_DataElementReadFncType ReadFnc;  /**< データ要素の読み出し関数 */
} Dem_DidClassType;

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/** イベント ID → DTC コード (24-bit) 変換テーブル */
extern const uint32 Dem_DtcTable[DEM_EVENT_COUNT];

/** イベント ID → デバウンス確定閾値 変換テーブル (Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*) */
extern const sint8 Dem_DebounceLimitTable[DEM_EVENT_COUNT];

/** イベント ID → 経年回復(Aging)閾値 変換テーブル (Dem_Cfg.h の DEM_AGING_THRESHOLD_*) */
extern const uint8 Dem_AgingThresholdTable[DEM_EVENT_COUNT];

/** FreezeFrame の DID の構成。この順に、{DID, データ} をレコードへ並べる（手書き。Dem.json の生成の対象外） */
extern const Dem_DidClassType Dem_DidClassTable[DEM_FREEZE_FRAME_DID_COUNT];

#endif /* DEM_PBCFG_H */
