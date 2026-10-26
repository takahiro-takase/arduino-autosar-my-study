/**
 * \file    FiM_PBCfg.c
 * \brief   機能抑止マネージャ ポストビルドコンフィグ 定義
 * \details FID と監視対象 Dem イベントの対応テーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイル
 *          に相当する。
 *
 *          機能の一覧（FID と、監視する Dem イベント・抑止するステータス）は
 *          config/data/FiM.json から生成する（下の生成領域）。
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

#include "FiM_PBCfg.h"
#include "Dem_Cfg.h"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* @@GEN-BEGIN fim-table  生成元: config/data/FiM.json  （自動生成: 手編集禁止） */
/* -----------------------------------------------------------------------
 * FID × Dem イベント対応テーブル（抑止の条件 1 つが 1 行。同じ FID の行は、どれか 1 つが成り立てば抑止する）
 * ----------------------------------------------------------------------- */
static const FiM_FunctionCfgType FiM_Functions[FIM_INHIBITION_COUNT] =
{
    /* FID 0: RUNNING LED (D6) 点灯機能
     * CAN Bus-Off 確定中は EngineState (CAN 受信由来) が信頼できないため、
     * 点灯機能そのものを抑止する。 */
    { FIM_FID_RUNNING_LED, DemConf_DemEventParameter_CAN_BUSOFF, DEM_STATUS_CONFIRMED, FIM_INH_STATUS_MASK },

    /* FID 1: 警告確認ボタンによる FAULT 解除
     * ボタン固着確定中の押下は物理的固着による偽信号の可能性があるため、
     * 確認操作を抑止する。 */
    { FIM_FID_BUTTON_ACK, DemConf_DemEventParameter_BUTTON_STUCK, DEM_STATUS_CONFIRMED, FIM_INH_STATUS_MASK }
};
/* @@GEN-END fim-table */

/* -----------------------------------------------------------------------
 * ポストビルドコンフィグインスタンス (EcuM が FiM_Init に渡す)
 * ----------------------------------------------------------------------- */
const FiM_ConfigType FiM_Config =
{
    FiM_Functions,
    FIM_INHIBITION_COUNT,
    FIM_FUNCTION_COUNT
};
