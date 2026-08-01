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
 *                                      NvM_MainFunction・MemIf_MainFunction・
 *                                      Nm_MainFunction 以外を無効化
 *            Rule 3: EcuM==RUN AND ComM==FULL_COMMUNICATION
 *                                    → I-PDU Group「テレメトリ」(E2EHealthStatus) を起動
 *            Rule 4: EcuM → POST_RUN → I-PDU Group「テレメトリ」(E2EHealthStatus) を停止
 *            Rule 5: ComM==SILENT_COMMUNICATION OR ComM==NO_COMMUNICATION
 *                                    → I-PDU Group「テレメトリ」(E2EHealthStatus) を停止
 *
 *          Rule 3/4/5 の狙い: E2EHealthStatus は診断監視用のネットワーク健全性
 *          テレメトリであり、車両の基本動作には不要な「非重要」通信である。
 *          POST_RUN（シャットダウン前の後処理フェーズ）中はこの非重要テレメトリ
 *          の送信を止め、バス負荷・ログ出力量を削減する（実 AUTOSAR で
 *          Com_IpduGroupStart/Stop の典型的な呼び出し元として BswM が
 *          明記されている、[7.3.5.1] "it is expected that the complete state
 *          handling of I-PDU groups is done ... within the Basic Software Mode
 *          Manager" のとおりの構成）。EngineInfo/AbsInfo/MeterStatus/
 *          WarningStatus/ImmobilizerCmd はどの I-PDU Group にも属さないため
 *          （Com_PBCfg.c 参照）、POST_RUN 中も引き続き送受信される
 *          （Rule 1 で BSW タスク自体は継続する設計と整合）。
 *
 *          Rule 3 が単一条件（EcuM==RUN のみ）ではなく AND 複合条件になった
 *          理由: Nm（CanNm 状態機械）導入後、ComM のチャネルモードは EcuM の
 *          RUN/POST_RUN とは独立して変化しうる（Bus-Off 中の SILENT_COMMUNICATION
 *          等）。EcuM が RUN のままでも CAN チャネルが実際には FULL_COMMUNICATION
 *          でなければ、E2EHealthStatus を送信してもバスに届かない。Rule 5 の OR
 *          条件と対になっており、「ComM がチャネルを離脱したら（SILENT/NO_COM の
 *          いずれでも）即座にテレメトリを止める」を1本のルールで表現する
 *          （AND 版の Rule3 だけでは、CAN チャネル側の離脱を検知して止める
 *          経路が無かった）。
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
#include "ComM.h"
#include "Com_Cfg.h"

/* -----------------------------------------------------------------------
 * ルールテーブル
 * ----------------------------------------------------------------------- */

static const BswM_RuleType BswM_Rules[BSWM_RULE_COUNT] =
{
    /* Rule 0: EcuM → RUN: 全タスクを有効化 */
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_ECUM, (uint8)ECUM_STATE_RUN }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_ACTIVATE,
        .TaskMask       = BSWM_TASK_MASK_ALL
    },
    /* Rule 1: EcuM → POST_RUN: アプリ Runnable のみ無効化 (BSW は継続) */
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_ECUM, (uint8)ECUM_STATE_POST_RUN }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DEACTIVATE,
        .TaskMask       = BSWM_TASK_MASK_APP
    },
    /* Rule 2: EcuM → SHUTDOWN: WdgM_TriggerHwWatchdog・Can_MainFunction_Read・
     * Can_MainFunction_Wakeup・CanSM_MainFunction・NvM_MainFunction・
     * MemIf_MainFunction・Nm_MainFunction 以外を無効化 */
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_ECUM, (uint8)ECUM_STATE_SHUTDOWN }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DEACTIVATE,
        .TaskMask       = BSWM_TASK_MASK_SHUTDOWN
    },
    /* Rule 3: EcuM==RUN AND ComM==FULL_COMMUNICATION: I-PDU Group「テレメトリ」を起動
     * （initialize=false: POST_RUN/SILENT_COM 等で停止する前の直近の値・
     * カウンタをそのまま引き継いで再開する。起動直後の初回だけは Com_Init()
     * 自体が既に初期値でゼロクリア済みのため、initialize=false でも実害はない） */
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_ECUM, (uint8)ECUM_STATE_RUN },
                             { BSWM_MODE_SRC_COMM, (uint8)COMM_FULL_COMMUNICATION }},
        .ConditionCount = 2U,
        .Action         = BSWM_ACTION_PDU_GROUP_START,
        .IpduGroupId    = COM_IPDU_GROUP_TELEMETRY,
        .Initialize     = 0U
    },
    /* Rule 4: EcuM → POST_RUN: I-PDU Group「テレメトリ」を停止 */
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_ECUM, (uint8)ECUM_STATE_POST_RUN }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_PDU_GROUP_STOP,
        .IpduGroupId    = COM_IPDU_GROUP_TELEMETRY
    },
    /* Rule 5: ComM==SILENT_COMMUNICATION OR ComM==NO_COMMUNICATION:
     * I-PDU Group「テレメトリ」を停止（Rule3 の AND 条件と対になる OR 条件。
     * EcuM が RUN のままでも CAN チャネルが FULL_COMMUNICATION から離脱したら
     * （Bus-Off 中の SILENT_COM、ボランタリスリープの NO_COM いずれでも）
     * 即座にテレメトリ送信を止める） */
    {
        .Operator       = BSWM_OP_OR,
        .Condition       = {{ BSWM_MODE_SRC_COMM, (uint8)COMM_SILENT_COMMUNICATION },
                             { BSWM_MODE_SRC_COMM, (uint8)COMM_NO_COMMUNICATION }},
        .ConditionCount = 2U,
        .Action         = BSWM_ACTION_PDU_GROUP_STOP,
        .IpduGroupId    = COM_IPDU_GROUP_TELEMETRY
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
