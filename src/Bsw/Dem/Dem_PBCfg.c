/**
 * \file    Dem_PBCfg.c
 * \brief   DEM ポストビルドコンフィグ 定義
 * \details イベントごとの設定テーブル（DTC コード、デバウンスの確定閾値、
 *          経年回復 (Aging) の閾値）を定義する。
 *          値は config/data/Dem.json から生成する（下の生成領域）。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに相当する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Dem_PBCfg.h"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* @@GEN-BEGIN dem-tables  生成元: config/data/Dem.json  （自動生成: 手編集禁止） */
/** イベント ID → DTC コード (24-bit) 変換テーブル */
const uint32 Dem_DtcTable[DEM_EVENT_COUNT] = {
    DEM_DTC_ENGINE_OVERHEAT,        /* event 0 */
    DEM_DTC_ENGINE_STALL,           /* event 1 */
    DEM_DTC_ENGINE_SPEED_NO_FLAG,   /* event 2 */
    DEM_DTC_STARTING_TIMEOUT,       /* event 3 */
    DEM_DTC_COMM_TIMEOUT,           /* event 4 */
    DEM_DTC_BUTTON_STUCK,           /* event 5 */
    DEM_DTC_ADC_VOLT_LOW,           /* event 6 */
    DEM_DTC_CAN_BUSOFF,             /* event 7 */
    DEM_DTC_E2E_ABSINFO,            /* event 8 */
    DEM_DTC_E2E_ENGINEINFO,         /* event 9 */
    DEM_DTC_WDGM_SUPERVISION,       /* event 10 */
    DEM_DTC_WDG_DISABLE_REJECTED,   /* event 11 */
    DEM_DTC_NVM_INTEGRITY_FAILED,   /* event 12 */
    DEM_DTC_NVM_LOSS_OF_REDUNDANCY, /* event 13 */
    DEM_DTC_NVM_REQ_FAILED          /* event 14 */
};

/** イベント ID → デバウンス確定閾値 変換テーブル (Dem_Cfg.h の DEM_DEBOUNCE_LIMIT_*) */
const sint8 Dem_DebounceLimitTable[DEM_EVENT_COUNT] = {
    DEM_DEBOUNCE_LIMIT_ENGINE_OVERHEAT,        /* event 0 */
    DEM_DEBOUNCE_LIMIT_ENGINE_STALL,           /* event 1 */
    DEM_DEBOUNCE_LIMIT_ENGINE_SPEED_NO_FLAG,   /* event 2 */
    DEM_DEBOUNCE_LIMIT_STARTING_TIMEOUT,       /* event 3 */
    DEM_DEBOUNCE_LIMIT_COMM_TIMEOUT,           /* event 4 */
    DEM_DEBOUNCE_LIMIT_BUTTON_STUCK,           /* event 5 */
    DEM_DEBOUNCE_LIMIT_ADC_VOLT_LOW,           /* event 6 */
    DEM_DEBOUNCE_LIMIT_CAN_BUSOFF,             /* event 7 */
    DEM_DEBOUNCE_LIMIT_E2E_ABSINFO,            /* event 8 */
    DEM_DEBOUNCE_LIMIT_E2E_ENGINEINFO,         /* event 9 */
    DEM_DEBOUNCE_LIMIT_WDGM_SUPERVISION,       /* event 10 */
    DEM_DEBOUNCE_LIMIT_WDG_DISABLE_REJECTED,   /* event 11 */
    DEM_DEBOUNCE_LIMIT_NVM_INTEGRITY_FAILED,   /* event 12 */
    DEM_DEBOUNCE_LIMIT_NVM_LOSS_OF_REDUNDANCY, /* event 13 */
    DEM_DEBOUNCE_LIMIT_NVM_REQ_FAILED          /* event 14 */
};

/** イベント ID → 経年回復(Aging)閾値 変換テーブル (Dem_Cfg.h の DEM_AGING_THRESHOLD_*) */
const uint8 Dem_AgingThresholdTable[DEM_EVENT_COUNT] = {
    DEM_AGING_THRESHOLD_ENGINE_OVERHEAT,        /* event 0 */
    DEM_AGING_THRESHOLD_ENGINE_STALL,           /* event 1 */
    DEM_AGING_THRESHOLD_ENGINE_SPEED_NO_FLAG,   /* event 2 */
    DEM_AGING_THRESHOLD_STARTING_TIMEOUT,       /* event 3 */
    DEM_AGING_THRESHOLD_COMM_TIMEOUT,           /* event 4 */
    DEM_AGING_THRESHOLD_BUTTON_STUCK,           /* event 5 */
    DEM_AGING_THRESHOLD_ADC_VOLT_LOW,           /* event 6 */
    DEM_AGING_THRESHOLD_CAN_BUSOFF,             /* event 7 */
    DEM_AGING_THRESHOLD_E2E_ABSINFO,            /* event 8 */
    DEM_AGING_THRESHOLD_E2E_ENGINEINFO,         /* event 9 */
    DEM_AGING_THRESHOLD_WDGM_SUPERVISION,       /* event 10 */
    DEM_AGING_THRESHOLD_WDG_DISABLE_REJECTED,   /* event 11 */
    DEM_AGING_THRESHOLD_NVM_INTEGRITY_FAILED,   /* event 12 */
    DEM_AGING_THRESHOLD_NVM_LOSS_OF_REDUNDANCY, /* event 13 */
    DEM_AGING_THRESHOLD_NVM_REQ_FAILED          /* event 14 */
};
/* @@GEN-END dem-tables */
