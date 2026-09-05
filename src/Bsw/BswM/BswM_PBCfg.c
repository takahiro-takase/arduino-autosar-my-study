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
 *            Rule 6: EcuM==RUN AND ComM==FULL_COMMUNICATION
 *                                    → I-PDU Group「センサーRX」(EngineInfo/AbsInfo) を起動
 *            Rule 7: ComM==NO_COMMUNICATION（真の物理スリープのみ）
 *                                    → I-PDU Group「センサーRX」(EngineInfo/AbsInfo) を停止
 *            Rule 8-19: Dcm_CommunicationModeType（UDS 0x28 CommunicationControl）の
 *                                    全 12 通りそれぞれに対応し、BswM_ApplyDcmCommMode()
 *                                    経由で Com_SetCommunicationEnabled()/
 *                                    Nm_EnableCommunication()/DisableCommunication() へ
 *                                    反映する（2026-09-05 追加、[SWS_BswM_00048]。
 *                                    Dcm_HandleCommunicationControl()/
 *                                    Dcm_CommControlReset() が
 *                                    BswM_Dcm_CommunicationMode_CurrentState() 経由で通知）。
 *
 *          Rule 3/4/5 の狙い: E2EHealthStatus は診断監視用のネットワーク健全性
 *          テレメトリであり、車両の基本動作には不要な「非重要」通信である。
 *          POST_RUN（シャットダウン前の後処理フェーズ）中はこの非重要テレメトリ
 *          の送信を止め、バス負荷・ログ出力量を削減する（実 AUTOSAR で
 *          Com_IpduGroupStart/Stop の典型的な呼び出し元として BswM が
 *          明記されている、[7.3.5.1] "it is expected that the complete state
 *          handling of I-PDU groups is done ... within the Basic Software Mode
 *          Manager" のとおりの構成）。MeterStatus/WarningStatus/
 *          ImmobilizerCmd/ImmobilizerStatus はどの I-PDU Group にも属さない
 *          ため（Com_PBCfg.c 参照）、POST_RUN 中も引き続き送受信される
 *          （Rule 1 で BSW タスク自体は継続する設計と整合）。EngineInfo/
 *          AbsInfo（RX）も COM_IPDU_GROUP_SENSOR_RX に属してはいるが、
 *          POST_RUN で止める Rule（Rule 4 相当）はあえて追加していない
 *          （Rule 6/7 のコメント参照）ため、この2本も同様に POST_RUN 中
 *          引き続き受信・デッドライン監視される。
 *
 *          Rule 6/7 の狙い（2026-08 追加）: EngineInfo/AbsInfo（RX）は
 *          元々 COM_IPDU_GROUP_NONE（常に有効）だったため、Bus-Sleep
 *          （ComM が NO_COMMUNICATION へ離脱、真の物理スリープ）中も受信
 *          デッドライン監視が止まらず、意図的な通信断のたびに「RX
 *          timeout」警告 + Dem FAILED DTC が誤って記録される問題があった
 *          （相手も送信を止めるスリープ中は 5000ms 以上の無通信が珍しく
 *          ない）。Rule 3/4/5（テレメトリ、TX）とは条件が異なる点に注意
 *          （/code-review で指摘・是正）: TX は SILENT_COMMUNICATION
 *          （Bus-Off 等による受信専用モード）でも送信できないため Rule 5
 *          の OR 条件で正しいが、RX は SILENT_COMMUNICATION 中も受信自体は
 *          生きているため、これを含めると実際に届いているフレームを
 *          誤って捨ててしまう。そのため Rule 7 の停止条件は
 *          NO_COMMUNICATION のみに絞り、POST_RUN 相当のルール（Rule 4）も
 *          追加しない（「POST_RUN 中も COM デッドライン監視を最後まで
 *          実行する」という既存の設計意図——下記参照——を踏襲するため）。
 *          詳細は Com_Cfg.h の COM_IPDU_GROUP_SENSOR_RX コメント・
 *          docs/modules/Com_Notes.md 参照。
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
 *            - COM デッドライン監視を最後まで実行する (Com_MainFunctionRx/Tx)
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
#include "Dcm_Types.h"

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
        .Initialize     = FALSE
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
    },
    /* Rule 6/7: I-PDU Group「センサーRX」(EngineInfo/AbsInfo) の起動/停止
     * （2026-08 追加）。狙い・Rule 3/4/5（テレメトリ、TX）との条件の違い
     * （SILENT_COMMUNICATION/POST_RUN を対象外にした理由）は、本ファイル
     * 冒頭の「Rule 6/7 の狙い」コメント参照。 */
    {
        /* Rule 6: EcuM==RUN AND ComM==FULL_COMMUNICATION: I-PDU Group「センサーRX」を起動 */
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_ECUM, (uint8)ECUM_STATE_RUN },
                             { BSWM_MODE_SRC_COMM, (uint8)COMM_FULL_COMMUNICATION }},
        .ConditionCount = 2U,
        .Action         = BSWM_ACTION_PDU_GROUP_START,
        .IpduGroupId    = COM_IPDU_GROUP_SENSOR_RX,
        .Initialize     = FALSE
    },
    {
        /* Rule 7: ComM==NO_COMMUNICATION（真の物理スリープのみ。
         * SILENT_COMMUNICATION は対象外——上のコメント参照）:
         * I-PDU Group「センサーRX」を停止 */
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_COMM, (uint8)COMM_NO_COMMUNICATION }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_PDU_GROUP_STOP,
        .IpduGroupId    = COM_IPDU_GROUP_SENSOR_RX
    },
    /* Rule 8-19: Dcm_CommunicationModeType（[SWS_Dcm_00981]）の全 12 通り
     * それぞれに対して BSWM_ACTION_DCM_COMM_APPLY を発火させる
     * （BswM_Dcm_CommunicationMode_CurrentState()/BswM_ApplyDcmCommMode()
     * 参照。12 条件は互いに排他的なため、値が変化するたびに旧値のルールが
     * false へ、新値のルールが true へ遷移し、常にちょうど1本だけ発火する）。 */
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_ENABLE_RX_TX_NORM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_ENABLE_RX_DISABLE_TX_NORM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_DISABLE_RX_ENABLE_TX_NORM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_DISABLE_RX_TX_NORM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_ENABLE_RX_TX_NM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_ENABLE_RX_DISABLE_TX_NM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_DISABLE_RX_ENABLE_TX_NM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_DISABLE_RX_TX_NM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_ENABLE_RX_TX_NORM_NM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_ENABLE_RX_DISABLE_TX_NORM_NM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_DISABLE_RX_ENABLE_TX_NORM_NM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
    },
    {
        .Operator       = BSWM_OP_AND,
        .Condition       = {{ BSWM_MODE_SRC_DCM_COMM, DCM_DISABLE_RX_TX_NORM_NM }},
        .ConditionCount = 1U,
        .Action         = BSWM_ACTION_DCM_COMM_APPLY
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
