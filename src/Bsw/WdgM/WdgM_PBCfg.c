/**
 * \file    WdgM_PBCfg.c
 * \brief   ウォッチドッグマネージャ ポストビルドコンフィグ 定義
 * \details プロジェクトで監視する Supervised Entity テーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイルに相当する。
 *
 *          エンティティ一覧:
 *            Entity 0: App_EngineManager_Run
 *              周期    : 3000 ms (Os_PBCfg.c Task 2)
 *              Alive   : 6000 ms サイクルで CheckpointReached が 1 回以上来ること
 *              Logical : START → END → START → ... の順序のみを許可
 *              失敗時  : WdgM_AliveStatus または WdgM_LogicalStatus → FAILED / WARN ログ出力
 *                        (WdgM_GetLocalStatus() はどちらか一方でも FAILED なら FAILED)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "WdgM_PBCfg.h"

/* -----------------------------------------------------------------------
 * 論理監視 (Logical Supervision) 許可遷移テーブル — Entity 0 用
 * ----------------------------------------------------------------------- */

/**
 * Entity 0 (App_EngineManager_Run) のプログラムフロー:
 *   (起動直後) → START → END → START → END → ...
 * 上記以外の遷移（START の連続呼び出し、END で始まる等）は順序違反として検出する。
 */
static const WdgM_TransitionCfgType WdgM_EngineTransitions[] =
{
    { WDGM_CP_INITIAL,      WDGM_CP_ENGINE_START },  /* 起動後、最初に許可されるのは START のみ */
    { WDGM_CP_ENGINE_START, WDGM_CP_ENGINE_END   },  /* START の次に許可されるのは END          */
    { WDGM_CP_ENGINE_END,   WDGM_CP_ENGINE_START }   /* END の次に許可されるのは次サイクルの START */
};

/* -----------------------------------------------------------------------
 * Supervised Entity テーブル
 * ----------------------------------------------------------------------- */

static const WdgM_EntityCfgType WdgM_Entities[WDGM_SUPERVISED_ENTITY_COUNT] =
{
    /* Entity 0: App_EngineManager_Run
     * 3000ms 周期タスクを 6000ms サイクルで監視。1 回以上の報告を期待する。
     * 論理監視は WdgM_EngineTransitions の遷移グラフのみを許可する。 */
    {
        WDGM_ENTITY_ENGINE,
        WDGM_SUPERVISION_CYCLE_MS,
        WDGM_EXPECTED_ALIVE_INDICATIONS,
        WdgM_EngineTransitions,
        (uint8)(sizeof(WdgM_EngineTransitions) / sizeof(WdgM_EngineTransitions[0]))
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
