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
 *              Deadline: START→END は 500ms 以内、END→START は 2500〜3500ms
 *            Entity 1: App_WarningIndicator_Run
 *              周期    : 500 ms (Os_PBCfg.c Task 3)
 *              Alive   : 6000 ms サイクルで CheckpointReached が 6 回以上来ること
 *              Logical : START → END → START → ... の順序のみを許可
 *              Deadline: START→END は 200ms 以内、END→START は 300〜700ms
 *              失敗時  : WdgM_AliveStatus / WdgM_LogicalStatus / WdgM_DeadlineStatus
 *                        のいずれかが FAILED / WARN ログ出力
 *                        (WdgM_GetLocalStatus() はどれか一方でも FAILED なら FAILED)
 *
 *          Entity 0 と Entity 1 は周期の異なる独立したタスクを別々に監視し、
 *          WdgM_TriggerHwWatchdog() がその両方の WdgM_GetLocalStatus() を
 *          集約する（1 つでも FAILED なら HW ウォッチドッグの refresh を止める）。
 *          「エンティティごとのローカル判定 → 全エンティティを見たグローバル判定」
 *          という WdgM 本来の構成を、単一エンティティでは確認できなかった形で
 *          実機検証できる。
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
 * 時間監視 (Deadline Supervision) 許容テーブル — Entity 0 用
 * ----------------------------------------------------------------------- */

/**
 * Entity 0 (App_EngineManager_Run) の 2 つのチェックポイント間隔を監視する:
 *   START→END : Run() 1 回分の処理時間（無限ループ・ブロッキング処理の検出）
 *   END→START : 次サイクルの Run() 呼び出しまでの間隔（タスク周期 3000ms の遵守確認）
 */
static const WdgM_DeadlineCfgType WdgM_EngineDeadlines[] =
{
    { WDGM_CP_ENGINE_START, WDGM_CP_ENGINE_END,
      WDGM_ENGINE_DEADLINE_START_TO_END_MIN_MS, WDGM_ENGINE_DEADLINE_START_TO_END_MAX_MS },
    { WDGM_CP_ENGINE_END, WDGM_CP_ENGINE_START,
      WDGM_ENGINE_DEADLINE_END_TO_START_MIN_MS, WDGM_ENGINE_DEADLINE_END_TO_START_MAX_MS }
};

/* -----------------------------------------------------------------------
 * 論理監視 (Logical Supervision) 許可遷移テーブル — Entity 1 用
 * ----------------------------------------------------------------------- */

/**
 * Entity 1 (App_WarningIndicator_Run) のプログラムフロー:
 *   (起動直後) → START → END → START → END → ...
 * Entity 0 と同じ「START/END の交互反復」パターンだが、周期は 500ms と
 * ENGINE (3000ms) より短い。
 */
static const WdgM_TransitionCfgType WdgM_WarningTransitions[] =
{
    { WDGM_CP_INITIAL,       WDGM_CP_WARNING_START },
    { WDGM_CP_WARNING_START, WDGM_CP_WARNING_END   },
    { WDGM_CP_WARNING_END,   WDGM_CP_WARNING_START }
};

/* -----------------------------------------------------------------------
 * 時間監視 (Deadline Supervision) 許容テーブル — Entity 1 用
 * ----------------------------------------------------------------------- */

static const WdgM_DeadlineCfgType WdgM_WarningDeadlines[] =
{
    { WDGM_CP_WARNING_START, WDGM_CP_WARNING_END,
      WDGM_WARNING_DEADLINE_START_TO_END_MIN_MS, WDGM_WARNING_DEADLINE_START_TO_END_MAX_MS },
    { WDGM_CP_WARNING_END, WDGM_CP_WARNING_START,
      WDGM_WARNING_DEADLINE_END_TO_START_MIN_MS, WDGM_WARNING_DEADLINE_END_TO_START_MAX_MS }
};

/* -----------------------------------------------------------------------
 * Supervised Entity テーブル
 * ----------------------------------------------------------------------- */

static const WdgM_EntityCfgType WdgM_Entities[WDGM_SUPERVISED_ENTITY_COUNT] =
{
    /* Entity 0: App_EngineManager_Run
     * 3000ms 周期タスクを 6000ms サイクルで監視。1 回以上の報告を期待する。
     * 論理監視は WdgM_EngineTransitions の遷移グラフのみを許可する。
     * 時間監視は WdgM_EngineDeadlines の 2 区間（START→END, END→START）を監視する。 */
    {
        WDGM_ENTITY_ENGINE,
        WDGM_SUPERVISION_CYCLE_MS,
        WDGM_ENGINE_EXPECTED_ALIVE_INDICATIONS,
        WdgM_EngineTransitions,
        (uint8)(sizeof(WdgM_EngineTransitions) / sizeof(WdgM_EngineTransitions[0])),
        WdgM_EngineDeadlines,
        (uint8)(sizeof(WdgM_EngineDeadlines) / sizeof(WdgM_EngineDeadlines[0]))
    },
    /* Entity 1: App_WarningIndicator_Run
     * 500ms 周期タスクを、Entity 0 と同じ 6000ms サイクルで監視。6 回以上の
     * 報告を期待する（期待値 約12回の半分）。論理監視は WdgM_WarningTransitions、
     * 時間監視は WdgM_WarningDeadlines をそれぞれ独立に適用する。 */
    {
        WDGM_ENTITY_WARNING,
        WDGM_SUPERVISION_CYCLE_MS,
        WDGM_WARNING_EXPECTED_ALIVE_INDICATIONS,
        WdgM_WarningTransitions,
        (uint8)(sizeof(WdgM_WarningTransitions) / sizeof(WdgM_WarningTransitions[0])),
        WdgM_WarningDeadlines,
        (uint8)(sizeof(WdgM_WarningDeadlines) / sizeof(WdgM_WarningDeadlines[0]))
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
