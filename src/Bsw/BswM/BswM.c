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
 *            - 複合条件は AND/OR の2条件まで（BswMArgumentRef [ECUC_BswM_00820]
 *              が表す BswMLogicalExpression の簡略版。NAND/NOT/XOR、3条件以上、
 *              LogicalExpression の入れ子は対応除外）
 *            - BswM_MainFunction なし (即時評価モードのみ)
 *            - ActionList は TaskActivation（本プロジェクト独自拡張）と
 *              PduGroupSwitch（[SWS_BswM_00273] 相当、Com_IpduGroupStart/Stop
 *              呼び出し）の 2 種類のみ (ModeSwitch / Timer 未実装)。
 *              1 ルール = 1 アクションの簡略化のため、実 AUTOSAR のように
 *              1 つのモード遷移で複数種のアクションを同時実行したい場合は
 *              同じ条件を持つルールを複数登録する
 *              (BswM_ExecuteRules() は一致する全ルールを実行するため)。
 *
 *          複合条件ルールの評価方式:
 *            各モードソース (ECUM/COMM) の現在値を BswM_ModeSrcCache[] に
 *            キャッシュしておき、いずれかのソースが変化するたびに全ルールを
 *            再評価する（複合条件の場合、他方の条件は BswM_ModeSrcCache[] の
 *            キャッシュ値を参照するため、2つの条件がどちらの通知で最後に
 *            真になっても正しく発火する。変化していないソースしか条件に
 *            含まないルールは評価結果も変化しないため、下記の
 *            BswM_RuleLastResult[] 比較で自然に Action 実行がスキップされる）。
 *            各ルールの直近の評価結果を BswM_RuleLastResult[] に保持し、
 *            Action は結果が false→true へ遷移したときのみ実行する
 *            （true が続く間の重複実行や true→false への遷移では実行しない）。
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
#include "Nm.h"
#include "Det.h"

#define TAG "BswM"

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */

static const BswM_ConfigType* BswM_Cfg;

/** 各モードソースの現在値キャッシュ（BswM_ModeSrcType の列挙値でインデックス）。
 *  型が異なる複数のモードソース（EcuM_StateType/ComM_ModeType）を共通の
 *  uint8 配列で一様に扱う。「今回変化していない方」の値を複合条件ルールが
 *  参照する用途と、変化検出（早期リターン）の用途を兼ねる。 */
static uint8 BswM_ModeSrcCache[BSWM_MODE_SRC_COUNT];

/** 各ルールの直近の評価結果 (0/1)。Action は結果が false→true へ遷移した
 *  ときのみ実行する。 */
static uint8 BswM_RuleLastResult[BSWM_RULE_COUNT];

/* -----------------------------------------------------------------------
 * 内部関数
 * ----------------------------------------------------------------------- */

/** 単一条件（ModeSrc の現在キャッシュ値が ModeValue と一致するか）を評価する。 */
static uint8 BswM_EvaluateCondition(const BswM_ConditionType* cond)
{
    DET_LOGT(TAG, "called");
    return (BswM_ModeSrcCache[cond->ModeSrc] == cond->ModeValue) ? 1U : 0U;
}

/**
 * \brief   ルールの Condition[] を Operator で組み合わせて評価する。
 *
 * \details [SWS_BswM_00245]/[SWS_BswM_00247] のとおり、AND は全条件が真の
 *          ときのみ真、OR はいずれか1つでも真なら真。ConditionCount=1 の
 *          場合は Operator の値に関わらず単一条件の評価結果と等価になる
 *          （BswMLogicalOperator [ECUC_BswM_00814] に「論理演算子が BSWM_NOT
 *          以外で条件が1つしかない場合、この演算子は無効」と明記されている
 *          とおり）。OR は最初の true で、AND は最初の false で短絡する
 *          対称性を利用し、単一ループで両方を表す。
 */
static uint8 BswM_EvaluateRule(const BswM_RuleType* rule)
{
    DET_LOGT(TAG, "called");
    const uint8 isOr = (rule->Operator == BSWM_OP_OR) ? 1U : 0U;

    for (uint8 c = 0U; c < rule->ConditionCount; c++)
    {
        if (BswM_EvaluateCondition(&rule->Condition[c]) == isOr)
            return isOr;
    }
    return !isOr;
}

/**
 * \brief   `BSWM_MODE_SRC_DCM_COMM` の現在キャッシュ値を Com/Nm への具体的な
 *          有効/無効呼び出しへ変換して適用する（[SWS_BswM_00048]、
 *          `BswMDcmComModeRequest` コンフィグ相当）。
 *
 * \details `Dcm_CommunicationModeType`（0x00〜0x0B）は
 *          `controlType + (communicationType-1)*4` で構成されるため、
 *          4 で割った商が対象範囲（0=normal/1=NM/2=両方）、余りが
 *          Rx/Tx の有効/無効パターン（0=enableRxTx/1=enableRxDisableTx/
 *          2=disableRxEnableTx/3=disableRxTx）を表す（Dcm_Types.h 参照）。
 *          12 通りの `BSWM_ACTION_DCM_COMM_APPLY` ルール（`BswM_PBCfg.c`）は
 *          いずれも本関数を呼ぶだけで、発火時点で `BswM_ModeSrcCache[]` は
 *          既に新値へ更新済み（`BswM_ExecuteRules()` 冒頭）のため、
 *          どのルールが発火したかに関わらず正しい値を読み直せる。
 */
static void BswM_ApplyDcmCommMode(void)
{
    const uint8 mode  = BswM_ModeSrcCache[BSWM_MODE_SRC_DCM_COMM];
    const uint8 group = mode / 4U;  /* 0=normal, 1=NM, 2=両方 */
    const uint8 op    = mode % 4U;  /* 0=enableRxTx/1=enableRxDisableTx/2=disableRxEnableTx/3=disableRxTx */
    const uint8 rxEnabled = (op == 0U || op == 1U) ? 1U : 0U;
    const uint8 txEnabled = (op == 0U || op == 2U) ? 1U : 0U;

    if (group == 0U || group == 2U)
        Com_SetCommunicationEnabled(rxEnabled, txEnabled);

    if (group == 1U || group == 2U)
    {
        (void)(txEnabled
            ? Nm_EnableCommunication(NM_MAIN_NETWORK_HANDLE)
            : Nm_DisableCommunication(NM_MAIN_NETWORK_HANDLE));
    }
}

/**
 * \brief   モード変化をトリガとしてルールテーブルを評価し、アクションを実行する。
 *
 * \param[in]  src       通知元モジュールの種別 (BSWM_MODE_SRC_*)。
 * \param[in]  newValue  新しいモード値。
 */
static void BswM_ExecuteRules(BswM_ModeSrcType src, uint8 newValue)
{
    DET_LOGT(TAG, "called");
    if (BswM_Cfg == NULL)
        return;  /* BswM_Init() 未実行 (呼び出し順序の誤りに対する保険) */

    /* 複合条件ルールが他方のソースの最新値を参照できるよう、ルール評価前に
     * 必ずキャッシュを更新する。 */
    BswM_ModeSrcCache[src] = newValue;

    for (uint8 i = 0U; i < BswM_Cfg->RuleCount; i++)
    {
        const BswM_RuleType* rule = &BswM_Cfg->Rules[i];

        /* src を条件に含まないルールは結果が変わらないため、下の
         * result == BswM_RuleLastResult[i] で自然にスキップされる。 */
        uint8 result = BswM_EvaluateRule(rule);

        if (result == BswM_RuleLastResult[i])
            continue;  /* 結果が変化していなければ Action を再実行しない */
        BswM_RuleLastResult[i] = result;

        if (!result)
            continue;  /* false→true へ遷移したときのみ Action を実行する */

        DET_LOGI(TAG, "Rule%u fired op=%u conds=%u act=%u mask=0x%05lX",
                 (unsigned)i, (unsigned)rule->Operator, (unsigned)rule->ConditionCount,
                 (unsigned)rule->Action, (unsigned long)rule->TaskMask);

        if (rule->Action == BSWM_ACTION_ACTIVATE || rule->Action == BSWM_ACTION_DEACTIVATE)
        {
            for (uint8 t = 0U; t < OS_TASK_COUNT; t++)
            {
                if ((rule->TaskMask & (uint32)(1UL << t)) == 0U)
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
        else if (rule->Action == BSWM_ACTION_DCM_COMM_APPLY)
        {
            BswM_ApplyDcmCommMode();
        }
    }
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

void BswM_Init(const BswM_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init: NULL ConfigPtr");
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_INIT, BSWM_E_PARAM_CONFIG);
        return;
    }

    BswM_Cfg = ConfigPtr;

    BswM_ModeSrcCache[BSWM_MODE_SRC_ECUM]     = (uint8)ECUM_STATE_STARTUP;
    BswM_ModeSrcCache[BSWM_MODE_SRC_COMM]     = (uint8)COMM_NO_COMMUNICATION;
    /* 起動直後は UDS 0x28 が一度も要求されていない状態 = Rx/Tx とも
     * 通常通信・NM通信ともに有効（Com_SetCommunicationEnabled()/
     * Nm_EnableCommunication() の既定値と一致させる）。 */
    BswM_ModeSrcCache[BSWM_MODE_SRC_DCM_COMM] = (uint8)DCM_ENABLE_RX_TX_NORM_NM;
    for (uint8 i = 0U; i < ConfigPtr->RuleCount; i++)
        BswM_RuleLastResult[i] = 0U;

    DET_LOGI(TAG, "Init ok rules=%u", (unsigned)ConfigPtr->RuleCount);
}

void BswM_Deinit(void)
{
    DET_LOGT(TAG, "called");
    if (BswM_Cfg == NULL)
    {
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_DEINIT, BSWM_E_NO_INIT);
        return;
    }

    /* [SWS_BswM_00120]: 以降 BswM_EcuM_CurrentState()/BswM_ComM_CurrentMode()
     * が呼ばれても BswM_Cfg==NULL チェックで弾かれ、モード処理は行われない。 */
    BswM_Cfg = NULL;

    DET_LOGI(TAG, "DeInit ok");
}

void BswM_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    DET_LOGT(TAG, "called");
    if (VersionInfo == NULL)
    {
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_GET_VERSION_INFO, BSWM_E_PARAM_POINTER);
        return;
    }

    VersionInfo->vendorID         = BSWM_VENDOR_ID;
    VersionInfo->moduleID         = BSWM_MODULE_ID;
    VersionInfo->sw_major_version = BSWM_SW_MAJOR_VERSION;
    VersionInfo->sw_minor_version = BSWM_SW_MINOR_VERSION;
    VersionInfo->sw_patch_version = BSWM_SW_PATCH_VERSION;
}

void BswM_EcuM_CurrentState(EcuM_StateType state)
{
    DET_LOGT(TAG, "called");
    if (BswM_Cfg == NULL)
    {
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_ECUM_CURRENT_STATE, BSWM_E_NO_INIT);
        return;
    }

    if (state != ECUM_STATE_STARTUP && state != ECUM_STATE_RUN
        && state != ECUM_STATE_POST_RUN && state != ECUM_STATE_SHUTDOWN)
    {
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_ECUM_CURRENT_STATE, BSWM_E_REQ_MODE_OUT_OF_RANGE);
        return;
    }

    if (BswM_ModeSrcCache[BSWM_MODE_SRC_ECUM] == (uint8)state)
        return;

    BswM_ExecuteRules(BSWM_MODE_SRC_ECUM, (uint8)state);
}

void BswM_ComM_CurrentMode(NetworkHandleType channel, ComM_ModeType mode)
{
    DET_LOGT(TAG, "called");
    (void)channel;

    if (BswM_Cfg == NULL)
    {
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_COMM_CURRENT_MODE, BSWM_E_NO_INIT);
        return;
    }

    if (mode > COMM_FULL_COMMUNICATION)
    {
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_COMM_CURRENT_MODE, BSWM_E_REQ_MODE_OUT_OF_RANGE);
        return;
    }

    if (BswM_ModeSrcCache[BSWM_MODE_SRC_COMM] == (uint8)mode)
        return;

    BswM_ExecuteRules(BSWM_MODE_SRC_COMM, (uint8)mode);
}

/**
 * \brief   Dcm からの UDS 0x28 CommunicationControl 通知コールバック。
 *
 * \details Dcm_HandleCommunicationControl()/Dcm_CommControlReset() が呼ぶ。
 *          BswM は受け取った Dcm_CommunicationModeType 値に一致する
 *          `BSWM_ACTION_DCM_COMM_APPLY` ルール（`BswM_PBCfg.c`）を発火させ、
 *          `BswM_ApplyDcmCommMode()` 経由で実際に Com/Nm へ反映する。
 *
 * \param[in]  Network        通信チャネル（本プロジェクトは単一ネットワーク
 *                            構成のため受け取るだけで検証・使用しない）。
 * \param[in]  RequestedMode  要求された通信モード。
 *
 * \note    BswM 未初期化時（`BswM_Cfg == NULL`）は DET 報告のみで無視するが、
 *          本関数は仕様どおり戻り値を持たないため呼び出し元（Dcm）へは
 *          伝わらない。実際にはこの呼び出し経路（`Dcm_HandleCommunicationControl()`/
 *          `Dcm_CommControlReset()`）は EcuM_Init() の起動シーケンスで
 *          BswM_Init() 完了後にしか到達しない（診断セッション確立後の
 *          UDS 要求経由のため）ため、実運用上は問題にならない。
 *
 * \AUTOSARReq     {SWS_BswM_00048, SWS_BswM_00079, SWS_BswM_00093}
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_Dcm_CommunicationMode_CurrentState(NetworkHandleType Network, Dcm_CommunicationModeType RequestedMode)
{
    DET_LOGT(TAG, "called");
    (void)Network;

    if (BswM_Cfg == NULL)
    {
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_DCM_COMMUNICATION_MODE_CURRENT_STATE, BSWM_E_NO_INIT);
        return;
    }

    if (RequestedMode > DCM_DISABLE_RX_TX_NORM_NM)
    {
        Det_ReportError(BSWM_MODULE_ID, 0U, BSWM_API_ID_DCM_COMMUNICATION_MODE_CURRENT_STATE, BSWM_E_REQ_MODE_OUT_OF_RANGE);
        return;
    }

    if (BswM_ModeSrcCache[BSWM_MODE_SRC_DCM_COMM] == (uint8)RequestedMode)
        return;

    BswM_ExecuteRules(BSWM_MODE_SRC_DCM_COMM, (uint8)RequestedMode);
}
