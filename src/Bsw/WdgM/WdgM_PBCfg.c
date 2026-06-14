/**
 * \file    WdgM_PBCfg.c
 * \brief   ウォッチドッグマネージャ ポストビルドコンフィグ 定義
 * \details プロジェクトで監視する Supervised Entity テーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイルに相当する。
 *
 *          エンティティ一覧:
 *            Entity 0: App_EngineManager_Run
 *              周期  : 3000 ms (Os_PBCfg.c Task 2)
 *              監視  : 6000 ms サイクルで CheckpointReached が 1 回以上来ること
 *              失敗時: WdgM_LocalStatus → FAILED / WARN ログ出力
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "WdgM_PBCfg.h"

/* -----------------------------------------------------------------------
 * Supervised Entity テーブル
 * ----------------------------------------------------------------------- */

static const WdgM_EntityCfgType WdgM_Entities[WDGM_SUPERVISED_ENTITY_COUNT] =
{
    /* Entity 0: App_EngineManager_Run
     * 3000ms 周期タスクを 6000ms サイクルで監視。1 回以上の報告を期待する。 */
    {
        WDGM_ENTITY_ENGINE,
        WDGM_SUPERVISION_CYCLE_MS,
        WDGM_EXPECTED_ALIVE_INDICATIONS
    }
};

/* -----------------------------------------------------------------------
 * ポストビルドコンフィグインスタンス (EcuM が WdgM_Init に渡す)
 * ----------------------------------------------------------------------- */

const WdgM_ConfigType WdgM_Config =
{
    WdgM_Entities,
    WDGM_SUPERVISED_ENTITY_COUNT
};
