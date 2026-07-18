/**
 * \file    BswM_PBCfg.c
 * \brief   BSW モードマネージャ ポストビルドコンフィグ 定義
 * \details プロジェクト固有のルールテーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイル。
 *
 *          ルール一覧:
 *            Rule 0: EcuM → RUN      → 全タスクを有効化
 *            Rule 1: EcuM → POST_RUN → アプリ Runnable のみ無効化 (BSW は継続)
 *            Rule 2: EcuM → SHUTDOWN → WdgM_TriggerHwWatchdog・Can_MainFunction_Read・
 *                                      Can_MainFunction_Wakeup・CanSM_MainFunction・
 *                                      NvM_MainFunction 以外を無効化
 *
 *          POST_RUN で BSW タスク (Can_MainFunction_Read / CanTp / CanSM / Com /
 *          IoHwAb) を継続させる理由:
 *            - 受信中の診断フレームを正常に処理する (CanTp)
 *            - COM デッドライン監視を最後まで実行する (Com_MainFunction)
 *            - ボタン入力をデバウンス完了まで処理する (IoHwAb_MainFunction)
 *
 *          SHUTDOWN で WdgM_TriggerHwWatchdog だけ止めない理由:
 *            Renesas RA の IWDT は一度有効化すると無効化する手段がなく、
 *            SHUTDOWN 後に誰もリフレッシュしなければ HW タイムアウトで
 *            MCU がリセットされてしまう。WdgM_SupervisionSuppressed が
 *            立っているため、このタスクさえ動いていれば無条件にリフレッシュ
 *            を継続でき、SHUTDOWN を意図通りの終端状態（無限アイドル）に
 *            保てる（詳細は BswM_Cfg.h の BSWM_TASK_MASK_SHUTDOWN コメントを参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "BswM_PBCfg.h"
#include "EcuM.h"

/* -----------------------------------------------------------------------
 * ルールテーブル
 * ----------------------------------------------------------------------- */

static const BswM_RuleType BswM_Rules[BSWM_RULE_COUNT] =
{
    /* Rule 0: EcuM → RUN: 全タスクを有効化 */
    {
        BSWM_MODE_SRC_ECUM,
        (uint8)ECUM_STATE_RUN,
        BSWM_ACTION_ACTIVATE,
        BSWM_TASK_MASK_ALL
    },
    /* Rule 1: EcuM → POST_RUN: アプリ Runnable のみ無効化 (BSW は継続) */
    {
        BSWM_MODE_SRC_ECUM,
        (uint8)ECUM_STATE_POST_RUN,
        BSWM_ACTION_DEACTIVATE,
        BSWM_TASK_MASK_APP
    },
    /* Rule 2: EcuM → SHUTDOWN: WdgM_TriggerHwWatchdog・Can_MainFunction_Read・
     * Can_MainFunction_Wakeup・CanSM_MainFunction・NvM_MainFunction 以外を無効化 */
    {
        BSWM_MODE_SRC_ECUM,
        (uint8)ECUM_STATE_SHUTDOWN,
        BSWM_ACTION_DEACTIVATE,
        BSWM_TASK_MASK_SHUTDOWN
    }
};

/* -----------------------------------------------------------------------
 * ポストビルドコンフィグインスタンス (EcuM が BswM_Init に渡す)
 * ----------------------------------------------------------------------- */

const BswM_ConfigType BswM_Config =
{
    BswM_Rules,
    BSWM_RULE_COUNT
};
