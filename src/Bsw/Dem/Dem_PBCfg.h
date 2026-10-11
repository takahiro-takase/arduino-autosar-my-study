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
 * Global Variables
 * ====================================================================== */

/** イベント ID → DTC コード (24-bit) 変換テーブル */
extern const uint32 Dem_DtcTable[DEM_EVENT_COUNT];

/** イベント ID → デバウンス確定閾値 変換テーブル (Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*) */
extern const sint8 Dem_DebounceLimitTable[DEM_EVENT_COUNT];

/** イベント ID → 経年回復(Aging)閾値 変換テーブル (Dem_Cfg.h の DEM_AGING_THRESHOLD_*) */
extern const uint8 Dem_AgingThresholdTable[DEM_EVENT_COUNT];

#endif /* DEM_PBCFG_H */
