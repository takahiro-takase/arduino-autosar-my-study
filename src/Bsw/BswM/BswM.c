/**
 * \file    BswM.c
 * \brief   BSW モードマネージャ 実装 (AUTOSAR SWS_BswM 準拠)
 * \details 他モジュールからのモード通知を受け取り、ルールテーブルを評価して
 *          Os タスクの有効・無効を切り替えるルールエンジン。
 *
 *          処理フロー:
 *            1. モード通知受信 (BswM_EcuM_CurrentState / BswM_ComM_CurrentMode)
 *            2. モードキャッシュを更新し変化がなければ早期リターン
 *            3. BswM_ExecuteRules() でルールテーブルを先頭から走査
 *            4. (ModeSrc == 通知元) AND (ModeValue == 新しいモード) のルールを実行
 *            5. TaskMask のビットが立っているタスクに対して Os_SetTaskActive() を呼ぶ
 *
 *          AUTOSAR との主な違い (学習用簡略化):
 *            - 単一条件ルールのみ (AND/OR の LogicalExpression なし)
 *            - BswM_MainFunction なし (即時評価モードのみ)
 *            - ActionList は TaskActivation（本プロジェクト独自拡張）と
 *              PduGroupSwitch（[SWS_BswM_00273] 相当、Com_IpduGroupStart/Stop
 *              呼び出し）の 2 種類のみ (ModeSwitch / Timer 未実装)。
 *              1 ルール = 1 アクションの簡略化のため、実 AUTOSAR のように
 *              1 つのモード遷移で複数種のアクションを同時実行したい場合は
 *              同じ ModeSrc/ModeValue を持つルールを複数登録する
 *              (BswM_ExecuteRules() は一致する全ルールを実行するため)。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "BswM.h"
#include "BswM_Cfg.h"
#include "Os.h"
#include "Os_Cfg.h"
#include "Com.h"
#include "Det.h"

#define TAG "BswM"

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */

static const BswM_ConfigType* BswM_Cfg;

/** BswM が認識している現在の EcuM フェーズ */
static EcuM_StateType BswM_EcuMState = ECUM_STATE_STARTUP;

/** BswM が認識している現在の ComM モード */
static ComM_ModeType  BswM_ComMMode  = COMM_NO_COMMUNICATION;

/* -----------------------------------------------------------------------
 * 内部関数
 * ----------------------------------------------------------------------- */

/**
 * \brief   モード変化をトリガとしてルールテーブルを評価し、アクションを実行する。
 *
 * \param[in]  src       通知元モジュールの種別 (BSWM_MODE_SRC_*)。
 * \param[in]  newValue  新しいモード値。
 */
static void BswM_ExecuteRules(BswM_ModeSrcType src, uint8 newValue)
{
    if (BswM_Cfg == NULL)
        return;  /* BswM_Init() 未実行 (呼び出し順序の誤りに対する保険) */

    for (uint8 i = 0U; i < BswM_Cfg->RuleCount; i++)
    {
        const BswM_RuleType* rule = &BswM_Cfg->Rules[i];

        if ((rule->ModeSrc != src) || (rule->ModeValue != newValue))
            continue;

        DET_LOGI(TAG, "Rule%u fired src=%u val=0x%02X act=%u mask=0x%03X",
                 (unsigned)i, (unsigned)src, (unsigned)newValue,
                 (unsigned)rule->Action, (unsigned)rule->TaskMask);

        if (rule->Action == BSWM_ACTION_ACTIVATE || rule->Action == BSWM_ACTION_DEACTIVATE)
        {
            for (uint8 t = 0U; t < OS_TASK_COUNT; t++)
            {
                if ((rule->TaskMask & (uint16)(1U << t)) == 0U)
                    continue;

                Os_SetTaskActive(t, (rule->Action == BSWM_ACTION_ACTIVATE) ? 1U : 0U);
            }
        }
        else if (rule->Action == BSWM_ACTION_PDU_GROUP_START)
        {
            /* [SWS_BswM_00273]: BswMEnabledPduGroupRef ごとに Com_IpduGroupStart
             * を呼ぶ（本実装は 1 ルール = 1 グループの簡略化）。 */
            Com_IpduGroupStart(rule->IpduGroupId, rule->Initialize);
        }
        else if (rule->Action == BSWM_ACTION_PDU_GROUP_STOP)
        {
            /* [SWS_BswM_00273]: BswMDisabledPduGroupRef ごとに Com_IpduGroupStop
             * を呼ぶ。 */
            Com_IpduGroupStop(rule->IpduGroupId);
        }
    }
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

void BswM_Init(const BswM_ConfigType* ConfigPtr)
{
    BswM_Cfg       = ConfigPtr;
    BswM_EcuMState = ECUM_STATE_STARTUP;
    BswM_ComMMode  = COMM_NO_COMMUNICATION;
    DET_LOGI(TAG, "Init ok rules=%u", (unsigned)ConfigPtr->RuleCount);
}

void BswM_EcuM_CurrentState(EcuM_StateType state)
{
    if (BswM_EcuMState == state)
        return;

    BswM_EcuMState = state;
    BswM_ExecuteRules(BSWM_MODE_SRC_ECUM, (uint8)state);
}

void BswM_ComM_CurrentMode(uint8 channel, ComM_ModeType mode)
{
    (void)channel;

    if (BswM_ComMMode == mode)
        return;

    BswM_ComMMode = mode;
    BswM_ExecuteRules(BSWM_MODE_SRC_COMM, (uint8)mode);
}
