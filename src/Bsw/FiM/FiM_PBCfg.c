/**
 * \file    FiM_PBCfg.c
 * \brief   機能抑止マネージャ ポストビルドコンフィグ 定義
 * \details FID と監視対象 Dem イベントの対応テーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイル
 *          に相当する。
 *
 *          機能一覧:
 *            FID 0 (RUNNING_LED): DEM_EVENT_CAN_BUSOFF が CONFIRMED の間抑止
 *            FID 1 (BUTTON_ACK) : DEM_EVENT_BUTTON_STUCK が CONFIRMED の間抑止
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "FiM_PBCfg.h"
#include "Dem_Cfg.h"

/* -----------------------------------------------------------------------
 * FID × Dem イベント対応テーブル
 * ----------------------------------------------------------------------- */

static const FiM_FunctionCfgType FiM_Functions[FIM_FUNCTION_COUNT] =
{
    /* FID 0: RUNNING LED — CAN Bus-Off 確定中は EngineState (CAN 受信由来) が
     * 信頼できないため、点灯機能そのものを抑止する。 */
    { FIM_FID_RUNNING_LED, DEM_EVENT_CAN_BUSOFF,   DEM_STATUS_CONFIRMED },

    /* FID 1: 警告確認ボタンによる FAULT 解除 — ボタン固着確定中の押下は
     * 物理的固着による偽信号の可能性があるため、確認操作を抑止する。 */
    { FIM_FID_BUTTON_ACK,  DEM_EVENT_BUTTON_STUCK, DEM_STATUS_CONFIRMED }
};

/* -----------------------------------------------------------------------
 * ポストビルドコンフィグインスタンス (EcuM が FiM_Init に渡す)
 * ----------------------------------------------------------------------- */

const FiM_ConfigType FiM_Config =
{
    FiM_Functions,
    FIM_FUNCTION_COUNT
};
