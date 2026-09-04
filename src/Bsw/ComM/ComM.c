/**
 * \file    ComM.c
 * \brief   通信マネージャ 実装 (AUTOSAR SWS_ComM 準拠)
 * \details CAN 通信スタックの通信モードを状態機械で管理する。
 *          ユーザからの要求を調停し、チャネルを
 *          NO_COM / SILENT_COM / FULL_COM の 3 状態間で遷移させる。
 *
 *          状態遷移図:
 *
 *            ┌──────────────────────────────────────────────────┐
 *            │           COMM_NO_COMMUNICATION                  │
 *            │  (CAN バス停止: Can_SetControllerMode(CAN_T_STOP)) │
 *            └────────────┬──────────────────────┬─────────────┘
 *             FULL_COM 要求↓                    ↑NO_COM 要求
 *            ┌────────────▼──────────────────────▼─────────────┐
 *            │          COMM_FULL_COMMUNICATION                 │
 *            │  (CAN バス起動: Can_SetControllerMode(CAN_T_START))│
 *            └────────────┬──────────────────────┬─────────────┘
 *           SILENT_COM 要求↓                    ↑FULL_COM 要求
 *            ┌────────────▼──────────────────────▼─────────────┐
 *            │        COMM_SILENT_COMMUNICATION                 │
 *            │  (受信専用: Can_SetControllerMode(CAN_T_STOP))    │
 *            └──────────────────────────────────────────────────┘
 *
 *          複数ユーザの調停 (SWS_ComM_00686: 複数ユーザの要求のうち最も高い
 *          Communication Mode を採用する "highest wins" 戦略。SWS_ComM_00500:
 *          要求はキューイングせず、同じユーザの新しい要求が古い要求を上書きする):
 *            ComM_UserRequest[] にユーザごとの要求を記録し、
 *            COMM_FULL_COMMUNICATION(2) > COMM_SILENT_COMMUNICATION(1) >
 *            COMM_NO_COMMUNICATION(0) の優先順位で最大値を集約してチャネルへ反映する。
 *            現在のユーザ: User0=App_EngineManager（エンジン運転中は FULL_COM、
 *            OFF 継続時は NO_COM を要求。ボランタリスリープの判断主体）、
 *            User1=Dcm（extendedSession の間だけ FULL_COM 要求）。
 *
 *          User0 要求のバス状態変化への追従（ComM_BusSM_ModeIndication 参照）:
 *            CanSM がユーザの要求とは独立に（ウェイクアップ検証成功や Bus-Off
 *            回復等）チャネルを FULL_COM/NO_COM へ変化させたとき、
 *            ComM_UserRequest[COMM_USER_0] もその値へ同期させる。これを怠ると、
 *            ウェイクアップ直後（App_EngineManager がまだ 1 周期も再評価して
 *            いない間）に他ユーザ（Dcm）が要求を変化させただけで、User0 の
 *            古い要求と誤って再集約され、意図せず即座に再スリープしてしまう
 *            （実機で確認された不具合。ウェイクアップの契機フレームが
 *            defaultSession への遷移だった場合に発生した）。
 *
 *          Nm（CanNm）との連携（2026-08、[SWS_ComM_00133]/[SWS_ComM_00392]/
 *          [SWS_ComM_00637] に準拠する形へ設計変更。旧版は ComM_RequestComMode()
 *          が NO_COM 集約と同時に CanSM へも通知し、EcuM の RUN 解放が Nm の
 *          協調スリープ完了を待たず即座に起きていた）:
 *            FULL_COM → NO_COM の要求（ComM_RequestComMode 参照）では CanSM を
 *            一切呼ばず、Nm_NetworkRelease() のみを直接呼ぶ。ComM_ChannelMode は
 *            この時点ではまだ FULL_COM のまま変えない（実 AUTOSAR の
 *            COMM_FULL_COM_READY_SLEEP サブステートに相当する期間）。
 *            Nm の状態機械（Repeat Message → Ready Sleep → Prepare Bus-Sleep →
 *            Bus-Sleep Mode、Nm.c 参照）が自律的に協調スリープを進める。
 *
 *            2026-08 の追加分（[SWS_ComM_00826]/[SWS_ComM_00296]）: Nm が
 *            Prepare Bus-Sleep Mode へ入ると ComM_Nm_PrepareBusSleepMode() が
 *            呼ばれ、CanSM_RequestComMode(SILENT_COM) でチャネルを受信専用へ
 *            切り替える（FULL_COM → SILENT_COM）。この間に他ノードの NM
 *            フレーム受信で Nm が自律的にスリープを取りやめた場合
 *            （[SWS_CanNm_00124]）は ComM_Nm_NetworkMode() が呼ばれ、
 *            CanSM_RequestComMode(FULL_COM) でチャネルを元に戻す
 *            （SILENT_COM/NO_COM → FULL_COM）。Nm がそのまま
 *            Bus-Sleep Mode へ到達すれば ComM_Nm_BusSleepMode() を呼ぶ。ここで
 *            初めて CanSM_RequestComMode(NO_COM) を呼び、物理スリープと
 *            ComM_ChannelMode の更新（ComM_BusSM_ModeIndication 経由）が起きる。
 *            状態遷移まとめ: FULL_COM →(Prepare Bus-Sleep 到達)→ SILENT_COM
 *            →(Bus-Sleep Mode 到達)→ NO_COM、および SILENT_COM/NO_COM
 *            →(Network Mode へ復帰)→ FULL_COM（Nm.c 冒頭の ASCII 状態図も参照）。
 *            この待機期間中に誰かが FULL_COM を再要求した場合の扱いは
 *            ComM_NmReleasePending[] のコメント（下記）を参照。
 *
 *            2026-09 追加: ComM_Nm_NetworkStartIndication()（[SWS_ComM_00383]）。
 *            この待機期間中の物理ウェイクアップ（CanSM_ControllerWakeup() →
 *            ウェイクアップ検証 → CanSM_RxIndication() →
 *            ComM_BusSM_ModeIndication(FULL_COM)）とは別に、Nm 自身がまだ
 *            Bus-Sleep Mode のまま NM フレームを受信した場合（レース条件、
 *            [SWS_CanNm_00127]）は Nm_RxIndication() の NM_STATE_BUS_SLEEP
 *            ケースから本関数が呼ばれ、チャネルを FULL_COM へ、Nm 自身も
 *            Bus-Sleep Mode から起こす（詳細は同関数の Doxygen 参照）。
 *
 *          本実装の簡略化:
 *            - 1 チャネル固定
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "ComM.h"
#include "CanSM.h"
#include "EcuM.h"
#include "BswM.h"
#include "Nm.h"
#include "Det.h"

#define TAG "ComM"

/* チャネルごとの現在の通信モード */
static ComM_ModeType ComM_ChannelMode[COMM_CHANNEL_COUNT];

/* ユーザごとの要求モード（調停のために保持）*/
static ComM_ModeType ComM_UserRequest[COMM_USER_COUNT];

/** 1 = FULL_COM → NO_COM の要求を Nm_NetworkRelease() で Nm へ伝達済みだが、
 *  Nm がまだ Bus-Sleep Mode へ到達した通知（ComM_Nm_BusSleepMode()）を
 *  返してきていない（協調スリープ待ち）。この間 ComM_ChannelMode は
 *  FULL_COM のまま据え置く。ComM_RequestComMode()/ComM_BusSM_ModeIndication()/
 *  ComM_Nm_BusSleepMode() 参照。
 *  ComM_Nm_NetworkMode() 経由でも（CanSM_RequestComMode() 呼び出しの成否に
 *  関わらず無条件に）クリアされうる。理由は同関数の doc コメント参照。 */
static uint8 ComM_NmReleasePending[COMM_CHANNEL_COUNT];

/** EcuM へ最後に伝えた RUN 要求状態（COMM_FULL_COMMUNICATION または
 *  COMM_NO_COMMUNICATION のみを取る。COMM_SILENT_COMMUNICATION は EcuM の
 *  RUN 状態に影響しないため対象外）。
 *  ComM_ChannelMode（生のチャネルモード）とは別に持つ理由: Bus-Off 検出時に
 *  CanSM が本関数を一時的に COMM_SILENT_COMMUNICATION で呼ぶ（ComM_ChannelMode
 *  もこの値に変わる）ため、Bus-Off 前後で ComM_ChannelMode だけを見て
 *  「モードが変化したか」を判定すると、SILENT_COM を経由しただけで実際には
 *  EcuM の RUN 要求状態が変わっていないのに EcuM_RequestRUN()/EcuM_ReleaseRUN()
 *  を再度呼んでしまい、SWS_EcuM_04125/04127 の重複要求/不整合解放エラーを
 *  誤検知する（2026-08 のスペック監査で発見・修正）。 */
static ComM_ModeType ComM_EcuMRunMode;

/** [SWS_ComM_00612/00858] 用の初期化済みフラグ。ComM_ChannelMode/
 *  ComM_UserRequest の既定値 (COMM_NO_COMMUNICATION=0) は「未初期化」と
 *  区別が付かないため別途持つ。 */
static uint8 ComM_Initialized = 0U;

/** 1 = 対象チャネルで Dcm が ComM_DCM_ActiveDiagnostic() により診断セッション
 *  進行中であることを通知済み（[SWS_ComM_00876]）。1 の間、ComM_UserRequest[]
 *  の集約結果に関わらず常に COMM_FULL_COMMUNICATION を要求する仮想ユーザとして
 *  扱う（ComM_ComputeAggregatedMode() 参照）。 */
static uint8 ComM_DcmActiveDiagnostic[COMM_CHANNEL_COUNT];

/**
 * \brief   ComM モジュールを初期化する。
 *
 * \details 全チャネルを COMM_NO_COMMUNICATION 状態に設定する。
 *          CAN バスは本関数では起動しない。
 *          ComM_RequestComMode() で COMM_FULL_COMMUNICATION を要求することで
 *          初めて CAN バスがアクティブになる。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Init(const ComM_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    (void)ConfigPtr; /* 本プロジェクトは post-build 設定を持たない（ComM.h 参照） */
    uint8 i;
    for (i = 0U; i < COMM_CHANNEL_COUNT; i++)
    {
        ComM_ChannelMode[i]         = COMM_NO_COMMUNICATION;
        ComM_NmReleasePending[i]    = 0U;
        ComM_DcmActiveDiagnostic[i] = 0U;
    }
    for (i = 0U; i < COMM_USER_COUNT; i++)
        ComM_UserRequest[i] = COMM_NO_COMMUNICATION;
    ComM_EcuMRunMode = COMM_NO_COMMUNICATION;
    ComM_Initialized = 1U;
    DET_LOGI(TAG, "Init ch=%u", (unsigned)COMM_CHANNEL_COUNT);
}

/**
 * \brief   ComM モジュールを未初期化状態に戻す。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_DeInit(void)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_DEINIT, COMM_E_UNINIT);
        return;
    }

    ComM_Initialized = 0U;
    DET_LOGI(TAG, "DeInit ok");
}

/**
 * \brief   ComM モジュールの初期化状態を取得する。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_GetStatus(ComM_InitStatusType* Status)
{
    DET_LOGT(TAG, "called");
    if (Status == NULL)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_GET_STATUS, COMM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *Status = ComM_Initialized ? COMM_INIT : COMM_UNINIT;
    return E_OK;
}

/**
 * \brief   ComM_UserRequest[] と ComM_DcmActiveDiagnostic[] から、チャネル0の
 *          集約要求モードを計算する。
 *
 * \details 全ユーザの要求のうち最も通信レベルの高いモード（FULL_COM >
 *          SILENT_COM > NO_COM）へ集約する。COMM_FULL_COMMUNICATION(2) >
 *          COMM_SILENT_COMMUNICATION(1) > COMM_NO_COMMUNICATION(0) と値
 *          そのものが優先順位になっているため、単純な最大値計算で集約できる。
 *          Dcm が ComM_DCM_ActiveDiagnostic() で診断セッション進行中を通知
 *          している間は、[SWS_ComM_00876] のとおり実ユーザの要求に関わらず
 *          常に COMM_FULL_COMMUNICATION を要求する仮想ユーザとして扱う。
 */
static ComM_ModeType ComM_ComputeAggregatedMode(void)
{
    ComM_ModeType aggregated = COMM_NO_COMMUNICATION;
    uint8 i;
    for (i = 0U; i < COMM_USER_COUNT; i++)
    {
        if (ComM_UserRequest[i] > aggregated)
            aggregated = ComM_UserRequest[i];
    }
    if (ComM_DcmActiveDiagnostic[0U])
        aggregated = COMM_FULL_COMMUNICATION;
    return aggregated;
}

/**
 * \brief   集約済み要求モード aggregated をチャネル0へ適用する。
 *
 * \details ComM_RequestComMode()/ComM_DCM_ActiveDiagnostic()/
 *          ComM_DCM_InactiveDiagnostic() いずれが要求元でも同じ Nm 協調
 *          スリープ絡みのレースコンディション処理を通す（挙動は要求元を
 *          区別しない。「最新の集約結果が何か」だけを見る）。
 */
static Std_ReturnType ComM_ApplyAggregatedRequest(ComM_ModeType aggregated)
{
    /* FULL_COM への（再）要求が Nm 協調スリープ待ち（ComM_NmReleasePending）の
     * 最中に来た場合は、他の何よりも先にこれを処理する（[SWS_ComM_00882] 相当）。
     * ComM_ChannelMode が現在何であるか（FULL_COM のまま据え置き中の通常ケース、
     * Nm が既に Prepare Bus-Sleep Mode へ到達し Stage 1（ComM_Nm_
     * PrepareBusSleepMode()）で SILENT_COM になっているケース、Bus-Off で
     * 一時的に SILENT_COM になっているケースのいずれであっても）に関わらず、
     * 要求元の最新の意思は「もう眠らなくていい」なので、まず解放を取り消す。
     * 本関数自身はここで CanSM を直接呼ばないが、直後の Nm_NetworkRequest() の
     * 呼び出し結果として間接的に CanSM へ届く場合がある: Nm が既に Prepare
     * Bus-Sleep Mode へ到達済みであれば、Nm_NetworkRequest() は
     * Nm_EnterRepeatMessage() を呼び、そこから同期的に ComM_Nm_NetworkMode()
     * → CanSM_RequestComMode(FULL_COM) まで到達し、この関数へ戻ってくる時点
     * には ComM_ChannelMode は既に FULL_COM へ更新済みになっている（下記
     * channel==aggregated のチェックで捕捉される）。CanSM へ届かないのは
     * Bus-Off 回復待ち中のケースのみで、この場合 CanSM_RequestComMode() は
     * どのみち拒否するため待つほかなく、CanSM が後で FULL_COM を通知して
     * きたときに ComM_BusSM_ModeIndication() の FULL_COM 分岐が
     * ComM_NmReleasePending の解除を確認して通常どおり処理する（下記
     * ComM_BusSM_ModeIndication() 参照）。
     * これを channel==aggregated の場合だけに限定してはいけない: Bus-Off 中は
     * ComM_ChannelMode が SILENT_COMMUNICATION になっており、FULL_COM の
     * 再要求は「変化あり」に見えて CanSM_RequestComMode(FULL_COM) まで
     * 落ちてしまう（Bus-Off 回復中のため拒否される）。その結果
     * ComM_NmReleasePending が解除されないまま Bus-Off が回復すると、
     * 誰も望んでいないのに CanSM_RequestComMode(NO_COM) を再送してコントローラを
     * 眠らせてしまい、CAN_CS_SLEEP からの CAN_T_START は Can.c が拒否する
     * （SWS_Can_00200/00409-00412）ため、外部からの CAN ウェイクアップ割り込みが
     * 来るまで ECU が誤って眠り続ける（/code-review で指摘・検証済み）。 */
    if (aggregated == COMM_FULL_COMMUNICATION && ComM_NmReleasePending[0U])
    {
        ComM_NmReleasePending[0U] = 0U;
        (void)Nm_NetworkRequest(NM_MAIN_NETWORK_HANDLE);
        DET_LOGI(TAG, "FULL_COM re-requested during Nm release wait -> cancelled");
        /* ComM_ChannelMode が既に FULL_COM へ更新済み（上記コメントの通常
         * ケース）か、まだ Bus-Off 回復待ち中で FULL_COM でないか、いずれの
         * 場合も本関数がこれ以上すべきことはない（後者は CanSM が後で
         * FULL_COM を通知してきたときに ComM_BusSM_ModeIndication() 側が処理する）。 */
        return E_OK;
    }

    /* 集約結果が現在のチャネル状態と同じなら何もしない
     * （要求元の要求変化が他の要求元の要求に埋もれて無効化されたケースを含む）。 */
    if (ComM_ChannelMode[0U] == aggregated)
        return E_OK;

    /* NO_COM が（再）要求され、かつ既に Nm 協調スリープ待ち
     * （ComM_NmReleasePending）が進行中なら、チャネルが現在 FULL_COM
     * （通常の待機中、[SWS_ComM_00133] 準拠で CanSM へは何も伝えない）・
     * SILENT_COM（Stage 1: Nm が Prepare Bus-Sleep Mode へ到達し
     * ComM_Nm_PrepareBusSleepMode() 経由でチャネルが切り替わっている）
     * いずれであっても何もしない。App_EngineManager が OFF 継続中毎周期
     * NO_COM を要求し続けるため、この分岐に何度も来うる（Nm_NetworkRelease()
     * の再送や DET_LOGI の連呼は無意味、/code-review で指摘・検証済み）。
     * SILENT_COM 中にここを素通りして下の CanSM_RequestComMode(NO_COM) まで
     * 落としてしまうと（2026-08、Stage 1/2 導入直後の実機ログで発見した
     * 重大な回帰）、Nm が Bus-Sleep Mode へまだ到達していない（Prepare
     * Bus-Sleep Mode で他ノードの NM フレームによるキャンセルを待っている
     * 最中）にも関わらず CanSM_RequestComMode(NO_COM) が直接呼ばれてしまう。
     * CAN_T_SLEEP は CanSM_State に関わらず常に成功する（CanSM.c 参照）ため、
     * この呼び出しは黙って成功し、コントローラを物理的に即座にスリープさせて
     * しまう。結果、Nm の内部状態（Prepare Bus-Sleep Mode、まだキャンセル
     * されうる）と物理コントローラの状態（CAN_CS_SLEEP、受信も停止）が
     * 食い違い、Nm 協調スリープの核心である「他ノードがまだ通信中の間は
     * 実際には寝ない」という猶予期間が実質的に消え、ComM_Nm_NetworkMode()
     * 経由の RX キャンセル経路も機能しなくなる。
     * ComM_NmReleasePending が立っていない場合（Bus-Off で SILENT_COM に
     * なっているだけで Stage 1 の巻き戻り待ちではない場合を含む）はここに
     * 該当せず、下の各分岐へ進む。 */
    if (aggregated == COMM_NO_COMMUNICATION && ComM_NmReleasePending[0U])
        return E_OK;

    /* FULL_COM -> NO_COM（初回）: CanSM へは何も伝えない。[SWS_ComM_00133]
     * のとおり Nm_NetworkRelease() のみを直接呼び、ComM_ChannelMode は
     * FULL_COM のまま据え置く（Nm の協調スリープが完了するまでの間、実
     * AUTOSAR の COMM_FULL_COM_READY_SLEEP サブステートに相当）。実際の
     * 物理スリープと ComM_ChannelMode の更新は、Nm が Bus-Sleep Mode へ
     * 到達して呼ぶ ComM_Nm_BusSleepMode() まで遅延する（詳細はファイル
     * 冒頭コメント参照）。2 回目以降の冗長な再要求は上の
     * ComM_NmReleasePending チェックで既に無視されているため、ここへ来るのは
     * 常に初回のみ。 */
    if (ComM_ChannelMode[0U] == COMM_FULL_COMMUNICATION && aggregated == COMM_NO_COMMUNICATION)
    {
        ComM_NmReleasePending[0U] = 1U;
        (void)Nm_NetworkRelease(NM_MAIN_NETWORK_HANDLE);
        DET_LOGI(TAG, "FULL_COM -> NO_COM requested: Nm_NetworkRelease() sent, awaiting Bus-Sleep Mode");
        return E_OK;
    }

    /* それ以外（NO_COM -> FULL_COM 等）は従来どおり即座に CanSM へ転送する。
     * 成功すれば CanSM が ComM_BusSM_ModeIndication を呼んでチャネル状態と EcuM を
     * 更新する。Bus-Off 回復中は E_NOT_OK が返る。 */
    return CanSM_RequestComMode(0U, aggregated);
}

/**
 * \brief   ユーザが通信モードを要求する。
 *
 * \details ユーザの要求を記録した後、全ユーザの要求と Dcm の診断アクティブ
 *          通知（ComM_DCM_ActiveDiagnostic()、ComM_ComputeAggregatedMode()
 *          参照）のうち最も通信レベルの高いモード（FULL_COM > SILENT_COM >
 *          NO_COM）へ集約し、集約結果がチャネルの現状と異なる場合のみ CanSM
 *          へ転送する。1 ユーザだけが FULL_COM を要求していても、他のユーザが
 *          NO_COM を要求している間はチャネルは FULL_COM のまま維持される
 *          （「誰か一人でも通信を必要としていればバスは落とさない」）。
 *
 * \param[in]  User     要求するユーザ ID (COMM_USER_0)。
 * \param[in]  ComMode  要求する通信モード。
 *
 * \retval  E_OK      要求を受理した（チャネルが実際に遷移したとは限らない）。
 * \retval  E_NOT_OK  User が範囲外、ComMode が不正、または CanSM への転送が失敗した
 *                    （Bus-Off 回復中等）。
 *
 * \AUTOSARReq     {SWS_ComM_00686, SWS_ComM_00500, SWS_ComM_00069}
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_REQUEST_COM_MODE, COMM_E_UNINIT);
        return E_NOT_OK;
    }

    if (User >= COMM_USER_COUNT || ComMode > COMM_FULL_COMMUNICATION)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_REQUEST_COM_MODE, COMM_E_WRONG_PARAMETERS);
        return E_NOT_OK;
    }

    ComM_UserRequest[User] = ComMode;

    ComM_ModeType aggregated = ComM_ComputeAggregatedMode();

    DET_LOGI(TAG, "User%u req=%u -> aggregated=%u (channel=%u)",
             (unsigned)User, (unsigned)ComMode,
             (unsigned)aggregated, (unsigned)ComM_ChannelMode[0U]);

    return ComM_ApplyAggregatedRequest(aggregated);
}

/**
 * \brief   ユーザが現在要求している通信モードを取得する（[SWS_ComM_00079]）。
 *
 * \AUTOSARReq     {SWS_ComM_00079}
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_GetRequestedComMode(ComM_UserHandleType User, ComM_ModeType* ComMode)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_GET_REQUESTED_COM_MODE, COMM_E_UNINIT);
        return E_NOT_OK;
    }

    if (User >= COMM_USER_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_GET_REQUESTED_COM_MODE, COMM_E_WRONG_PARAMETERS);
        return E_NOT_OK;
    }

    if (ComMode == NULL)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_GET_REQUESTED_COM_MODE, COMM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *ComMode = ComM_UserRequest[User];
    return E_OK;
}

/**
 * \brief   ユーザの現在の通信モードを取得する。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_GET_CURRENT_COM_MODE, COMM_E_UNINIT);
        return E_NOT_OK;
    }

    if (User >= COMM_USER_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_GET_CURRENT_COM_MODE, COMM_E_WRONG_PARAMETERS);
        return E_NOT_OK;
    }

    if (ComMode == NULL)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_GET_CURRENT_COM_MODE, COMM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* ユーザ 0 → チャネル 0 の現在モードを返す */
    *ComMode = ComM_ChannelMode[0U];
    return E_OK;
}

/**
 * \brief   Dcm から、対象チャネルで診断セッションが進行中であることを通知する。
 *
 * \details [SWS_ComM_00876]。実ユーザの要求に関わらず、以降
 *          ComM_DCM_InactiveDiagnostic() が呼ばれるまで常に
 *          COMM_FULL_COMMUNICATION を要求する仮想ユーザとして扱う
 *          （ComM_ComputeAggregatedMode() 参照）。
 *
 * \param[in]  Channel  診断通信が必要になったチャネル。
 *
 * \AUTOSARReq     {SWS_ComM_00873}
 * \ServiceID      {0x1F}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_DCM_ActiveDiagnostic(uint8 Channel)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_DCM_ACTIVE_DIAGNOSTIC, COMM_E_UNINIT);
        return;
    }

    if (Channel >= COMM_CHANNEL_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_DCM_ACTIVE_DIAGNOSTIC, COMM_E_WRONG_PARAMETERS);
        return;
    }

    ComM_DcmActiveDiagnostic[Channel] = 1U;

    ComM_ModeType aggregated = ComM_ComputeAggregatedMode();
    DET_LOGI(TAG, "DCM ActiveDiagnostic ch=%u -> aggregated=%u", (unsigned)Channel, (unsigned)aggregated);
    (void)ComM_ApplyAggregatedRequest(aggregated);
}

/**
 * \brief   Dcm から、対象チャネルで診断セッションが終了したことを通知する。
 *
 * \details [SWS_ComM_00876]。ComM_DCM_ActiveDiagnostic() が課していた仮想
 *          COMM_FULL_COMMUNICATION 要求を解除する。他の実ユーザがまだ
 *          COMM_FULL_COMMUNICATION を要求していれば、チャネルは維持される。
 *
 * \param[in]  Channel  診断通信が不要になったチャネル。
 *
 * \AUTOSARReq     {SWS_ComM_00874}
 * \ServiceID      {0x20}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_DCM_InactiveDiagnostic(uint8 Channel)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_DCM_INACTIVE_DIAGNOSTIC, COMM_E_UNINIT);
        return;
    }

    if (Channel >= COMM_CHANNEL_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_DCM_INACTIVE_DIAGNOSTIC, COMM_E_WRONG_PARAMETERS);
        return;
    }

    ComM_DcmActiveDiagnostic[Channel] = 0U;

    ComM_ModeType aggregated = ComM_ComputeAggregatedMode();
    DET_LOGI(TAG, "DCM InactiveDiagnostic ch=%u -> aggregated=%u", (unsigned)Channel, (unsigned)aggregated);
    (void)ComM_ApplyAggregatedRequest(aggregated);
}

/**
 * \brief   Bus-Off 回復エコー時に Nm 協調スリープの解放要求を仕切り直す。
 *
 * \details Bus-Off が Nm 協調スリープ待ち（ComM_NmReleasePending が立っている）
 *          の最中に発生すると、Nm は Bus-Off とは無関係に独立したタイマで
 *          動作し続け、既に Bus-Sleep Mode へ到達して
 *          ComM_Nm_BusSleepMode() 経由の CanSM_RequestComMode(NO_COM) を
 *          試みている可能性があるが、その時点では CanSM が Bus-Off 回復中の
 *          ため拒否されている（一発勝負の通知であり、Nm 側は再送しない）。
 *          放置すると ComM_NmReleasePending が永久に残り、チャネルは
 *          二度と NO_COM（物理スリープ）へ収束しない。CanSM が Bus-Off から
 *          回復し、以前のモード（FULL_COM または SILENT_COM）を改めて通知
 *          してきたこのタイミングでなら CanSM_RequestComMode(NO_COM) は
 *          受理されるはずなので、解放を仕切り直す。この再帰呼び出しが
 *          ComM_BusSM_ModeIndication(NO_COM) を呼び返し、ComM_ChannelMode/
 *          ComM_NmReleasePending は最終的にそちらの分岐で正しい値へ収束する
 *          （BswM への通知もそちらに任せるため、呼び出し元は本関数の後
 *          return すること）。
 *
 *          ComM_BusSM_ModeIndication() の COMM_FULL_COMMUNICATION 分岐
 *          （Bus-Off が FULL_COM から発生したケース）と
 *          COMM_SILENT_COMMUNICATION 分岐（2026-08 追加。CanSM の
 *          SILENT_COMMUNICATION が CanIf_SetPduMode(CANIF_TX_OFFLINE)
 *          ベースへ移行しコントローラが CAN_CS_STARTED のまま維持される
 *          ようになったことで、SILENT_COM 中にも本当に Bus-Off しうるように
 *          なった経路。CanSM_ControllerBusOff()/CanSM_PreBusOffState 参照）
 *          の両方から共有する。
 *
 * \param[in]  Network    対象ネットワークハンドル。
 * \param[in]  modeLabel  DET ログに残す、再通知された通信モードの名前
 *                         （例: "FULL_COM"）。
 */
static void ComM_RetryNmReleaseAfterBusOff(uint8 Network, const char* modeLabel)
{
    DET_LOGW(TAG, "%s notified while Nm release still pending "
                  "(likely BusOff recovery mid-winddown) -> retry release", modeLabel);
    (void)CanSM_RequestComMode(Network, COMM_NO_COMMUNICATION);
}

/**
 * \brief   CanSM からの通信モード変化通知コールバック（下位層 → 上位層）。
 *
 * \details CanSM が実際の CAN バス状態を変化させた後に呼ぶ。
 *          ComM はチャネル状態を更新し EcuM の RUN 要求を操作する。
 *
 *          ComM_UserRequest[COMM_USER_0] の再同期:
 *          この通知は CanSM がユーザの要求とは独立に（ウェイクアップ検証成功や
 *          Bus-Off 回復等）チャネルを変化させた場合にも呼ばれる。
 *          これは「どのユーザの要求でもない」変化のため、放置すると
 *          ComM_UserRequest[COMM_USER_0] が古い値のまま残り、次に別ユーザが
 *          ComM_RequestComMode() を呼んだ瞬間に古い値と誤って再集約されてしまう
 *          （実機で確認された不具合: ウェイクアップ直後、App_EngineManager が
 *          まだ 1 周期も再評価していない間に Dcm が defaultSession へ戻ると、
 *          User0 の古い NO_COM 要求と集約されて即座に再スリープしていた）。
 *          COMM_USER_0（App_EngineManager）はチャネルの実状態が「暫定的な自分の
 *          要求」であるとみなし、次回 App_EngineManager_Run() が実際の
 *          エンジン状態に基づいて改めて要求し直すまではこの値を使う。
 *          Dcm の診断アクティブ通知（ComM_DCM_ActiveDiagnostic/
 *          InactiveDiagnostic、ComM_DcmActiveDiagnostic[]）はセッション状態に
 *          基づく独立した判断のため、ここでは同期しない（そもそも
 *          ComM_UserRequest[] とは別の配列のため対象外）。
 *          注意: この再同期は COMM_SILENT_COMMUNICATION を対象にしていない
 *          （CanSM_ControllerBusOff() が Bus-Off 検出時に本関数を SILENT_COM で
 *          呼ぶ経路があるが、Bus-Off 回復中は CanSM_RequestComMode() 自体が
 *          全ユーザ要求を拒否するため現状は無害。将来 SILENT_COM を能動的に
 *          要求するユーザを追加する場合はこの非対称性に注意すること）。
 *
 * \ServiceID      {0x33}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_BusSM_ModeIndication(uint8 Network, ComM_ModeType Mode)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_BUS_SM_MODE_INDICATION, COMM_E_UNINIT);
        return;
    }

    if (Network >= COMM_CHANNEL_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_BUS_SM_MODE_INDICATION, COMM_E_WRONG_PARAMETERS);
        return;
    }

    const ComM_ModeType prevMode = ComM_ChannelMode[Network];
    ComM_ChannelMode[Network] = Mode;
    DET_LOGI(TAG, "ch%u ->mode=%u", (unsigned)Network, (unsigned)Mode);

    if (Mode == COMM_FULL_COMMUNICATION || Mode == COMM_NO_COMMUNICATION)
    {
        ComM_UserRequest[COMM_USER_0] = Mode;
    }

    /* EcuM_RequestRUN()/EcuM_ReleaseRUN() は冪等呼び出しを避けるため、
     * 実際に EcuM の RUN 要求状態（ComM_EcuMRunMode）が変化する時のみ呼ぶ。
     * CanSM の Bus-Off 回復（L1/L2 バックオフ）はリトライ成功のたびに本関数を
     * COMM_FULL_COMMUNICATION で呼ぶため、変化を見ずに毎回呼ぶと EcuM 側で
     * 「同一ユーザからの重複要求」(SWS_EcuM_04125) が不必要に頻発してしまう。
     * ここで生の prevMode（ComM_ChannelMode）ではなく専用の ComM_EcuMRunMode
     * を比較対象にしているのは、Bus-Off 中に挟まる COMM_SILENT_COMMUNICATION
     * （EcuM の RUN 状態には影響しない、下記コメント参照）を挟んだ前後で
     * FULL⇔NO_COM が実際には変化していないのに変化したと誤判定するのを防ぐ
     * ため（2026-08 のスペック監査で発見・修正。以前は SILENT_COM を経由した
     * だけで EcuM_RequestRUN()/EcuM_ReleaseRUN() が二重に呼ばれ、
     * SWS_EcuM_04125/04127 の誤検知を起こしていた）。 */
    if (Mode != prevMode)
    {
        if (Mode == COMM_FULL_COMMUNICATION)
        {
            if (ComM_NmReleasePending[Network])
            {
                /* Nm 協調スリープ待ちの最中に Bus-Off が発生し、回復した CanSM が
                 * 改めて FULL_COMMUNICATION を通知してきたケース（誰かが
                 * FULL_COM を能動的に再要求したのであれば、その再要求は
                 * ComM_RequestComMode() の「集約結果が現状と同じ」早期return
                 * パスが既に処理し ComM_NmReleasePending をクリア済みのはず
                 * なので、ここへ到達するのは Bus-Off 由来のみ）。詳細は
                 * ComM_RetryNmReleaseAfterBusOff() 参照。 */
                ComM_RetryNmReleaseAfterBusOff(Network, "FULL_COM");
                return;
            }
            if (ComM_EcuMRunMode != COMM_FULL_COMMUNICATION)
            {
                (void)EcuM_RequestRUN(ECUM_USER_COMM);
                ComM_EcuMRunMode = COMM_FULL_COMMUNICATION;
            }
            (void)Nm_NetworkRequest(NM_MAIN_NETWORK_HANDLE);  /* 通信が必要になったことを Nm へ伝える */
        }
        else if (Mode == COMM_NO_COMMUNICATION)
        {
            /* Nm_NetworkRelease() はここでは呼ばない。ComM_RequestComMode() が
             * FULL_COM -> NO_COM 要求の時点で既に呼んでおり（Nm 協調スリープの
             * 起点、ファイル冒頭コメント参照）、本関数が呼ばれる頃には Nm は
             * 既に Bus-Sleep Mode へ到達済みのため呼んでも無意味である。
             * ComM_NmReleasePending は、物理スリープが実際に完了したことが
             * 確定するこのタイミングでクリアする。 */
            ComM_NmReleasePending[Network] = 0U;
            if (ComM_EcuMRunMode != COMM_NO_COMMUNICATION)
            {
                (void)EcuM_ReleaseRUN(ECUM_USER_COMM);
                ComM_EcuMRunMode = COMM_NO_COMMUNICATION;
            }
        }
        else if (Mode == COMM_SILENT_COMMUNICATION)
        {
            /* Bus-Off 検出時に CanSM_ControllerBusOff() が CanSM_RequestComMode()
             * を経由せず直接呼ぶ経路（CanSM.c 参照）。EcuM の RUN 状態は維持
             * するが（下記コメント参照）、CAN コントローラは Can_T_STOP されて
             * おり送信できないため、Nm にも通信不要を伝えて送信試行を止める。
             * これを怠ると、Nm が NM-Timeout Timer 満了のたびに送信を再試行
             * しては Can_Write() に拒否され、Bus-Off 回復完了まで（L2 バックオフ
             * のため無期限になり得る）NM_E_NETWORK_TIMEOUT を報告し続ける
             * （実機で確認された不具合）。回復成功時は CanSM が改めて
             * ComM_BusSM_ModeIndication(FULL_COMMUNICATION) を呼ぶため、その際に
             * 上の分岐（ComM_NmReleasePending が立っていなければ）で
             * Nm_NetworkRequest() が呼ばれ自動的に再開する。 */
            (void)Nm_NetworkRelease(NM_MAIN_NETWORK_HANDLE);
        }
    }
    else if (Mode == COMM_SILENT_COMMUNICATION && ComM_NmReleasePending[Network])
    {
        /* 2026-08 追加: Mode==prevMode==COMM_SILENT_COMMUNICATION（上の
         * `if (Mode != prevMode)` では捕捉できない再通知）で、かつ Nm 協調
         * スリープ待ちが残っているケース。SILENT_COM 中にも本当に Bus-Off
         * しうるようになったことで生まれた、上の FULL_COM 分岐と対称の経路
         * （詳細は ComM_RetryNmReleaseAfterBusOff() 参照）。 */
        ComM_RetryNmReleaseAfterBusOff(Network, "SILENT_COM");
        return;
    }
    /* SILENT_COM: EcuM の RUN 状態は維持（受信専用でも ECU は動作継続）。 */

    BswM_ComM_CurrentMode(Network, Mode);  /* BswM へ ComM モード変化を通知 */
}

/**
 * \brief   Nm が Prepare Bus-Sleep Mode へ入ったことの通知（Nm から呼び出される）。
 *
 * \details [SWS_ComM_00826]。COMM_FULL_COMMUNICATION 中に Nm が Prepare
 *          Bus-Sleep Mode へ入ると、CanSM_RequestComMode(Network,
 *          COMM_SILENT_COMMUNICATION) を呼びチャネルを受信専用へ切り替える。
 *          `ComM_ChannelMode[Network]` が既に COMM_FULL_COMMUNICATION でない
 *          場合（Bus-Off で既に SILENT_COM のケース等）は no-op とし、
 *          Bus-Off 回復シーケンスと衝突させない。
 *
 * \param[in]  Network  ネットワークハンドル（0 〜 COMM_CHANNEL_COUNT-1）。
 *
 * \AUTOSARReq     {SWS_ComM_00391, SWS_ComM_00826}
 * \ServiceID      {0x19}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Nm_PrepareBusSleepMode(uint8 Network)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_NM_PREPARE_BUS_SLEEP_MODE, COMM_E_UNINIT);
        return;
    }

    if (Network >= COMM_CHANNEL_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_NM_PREPARE_BUS_SLEEP_MODE, COMM_E_WRONG_PARAMETERS);
        return;
    }

    if (ComM_ChannelMode[Network] == COMM_FULL_COMMUNICATION)
    {
        (void)CanSM_RequestComMode(Network, COMM_SILENT_COMMUNICATION);
    }
}

/**
 * \brief   Bus-Sleep Mode 中に NM PDU を受信したことの通知（Nm から呼び出される、
 *          [SWS_ComM_00383]）。
 *
 * \details [SWS_CanNm_00127]: Nm は Bus-Sleep Mode 中に NM PDU を受信しても
 *          自動的に Network Mode へ遷移せず、上位層（ComM）へ通知するのみで
 *          判断を委ねる。これは「他ノードは既に Network Mode にいる」ことを
 *          示す、レース条件由来のシグナルである（[SWS_ComM_00583] のユース
 *          ケース参照）。
 *
 *          本プロジェクトの同期的な設計では、Nm が `NM_STATE_BUS_SLEEP` へ
 *          到達する時点で通常は `ComM_Nm_BusSleepMode()` 経由の
 *          `CanSM_RequestComMode(NO_COM)` が既に成功しており、物理コントローラ
 *          も `CAN_CS_SLEEP` へ落ちている（＝`Can_MainFunction_Read()` 自体が
 *          停止し `Nm_RxIndication()` は物理的に呼ばれ得ない）。そのため本関数
 *          が実際に到達しうるのは、Bus-Off 回復待ち中に `CanSM_RequestComMode
 *          (NO_COM)` が拒否されて Nm だけが独立したタイマで先に Bus-Sleep
 *          Mode へ到達してしまうケース（コントローラは Bus-Off により
 *          Listen-Only のまま受信は継続、`ComM_Nm_BusSleepMode()` の Doxygen
 *          `BusOffDuringNmWinddown_OK_DoesNotResurrectNm` 相当）にほぼ限られる。
 *
 *          [SWS_ComM_00583]: `ComM_ChannelMode[Network]` が既に
 *          COMM_FULL_COMMUNICATION でなければ `CanSM_RequestComMode(Network,
 *          COMM_FULL_COMMUNICATION)` を試みる（実仕様の `CommunicationAllowed`
 *          フラグは本プロジェクトが対応するゲート機構自体を持たないため常に
 *          許可とみなす簡略化）。**`ComM_Nm_NetworkMode()` とは異なり
 *          `ComM_NmReleasePending[Network]` は呼び出しが実際に成功した場合
 *          のみクリアする**（`ComM_Nm_BusSleepMode()` の NO_COM 分岐と同じ
 *          「まだ解放が完了していない事実を保持し続ける」方針）。上記の
 *          Bus-Off 回復待ちシナリオでは `CanSM_RequestComMode()` は
 *          `CanSM_State==CANSM_STATE_BUS_OFF` により必ず拒否される（モード
 *          変更は Bus-Off 回復ステートマシンに一本化する設計、CanSM.c
 *          参照）ため、ここで無条件にクリアしてしまうと、後の Bus-Off 回復時に
 *          `ComM_BusSM_ModeIndication()` の SILENT_COM 分岐にある既存の
 *          リトライガード（`ComM_NmReleasePending` が立っていれば
 *          `CanSM_RequestComMode(NO_COM)` を再送する）を迂回させ、ユーザーが
 *          望んでいないのに FULL_COM へ「復活」させてしまう
 *          （`BusOffDuringNmWinddown_OK_DoesNotResurrectNm` と同種の回帰）。
 *
 *          実仕様は `ComMNmVariant=FULL` 構成で `Nm_PassiveStartup()` を要求
 *          するが（[SWS_ComM_00903]）、本 ECU は能動送信ノードでありこの API
 *          自体を実装しない（`CanNm_PassiveStartUp` は既知の対応除外）。
 *          `CanSM_RequestComMode(FULL_COM)` が成功する経路では
 *          `ComM_BusSM_ModeIndication()` が内部で `Nm_NetworkRequest()` を
 *          呼び Nm 自身を起こす（同関数の FULL_COM 分岐参照）ため、本関数は
 *          追加のアクションを取らない。
 *
 * \param[in]  Network  ネットワークハンドル（0 〜 COMM_CHANNEL_COUNT-1）。
 *
 * \AUTOSARReq     {SWS_ComM_00383, SWS_ComM_00583}
 * \ServiceID      {0x15}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Asynchronous}
 */
void ComM_Nm_NetworkStartIndication(uint8 Network)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_NM_NETWORK_START_INDICATION, COMM_E_UNINIT);
        return;
    }

    if (Network >= COMM_CHANNEL_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_NM_NETWORK_START_INDICATION, COMM_E_WRONG_PARAMETERS);
        return;
    }

    DET_LOGW(TAG, "ch%u NetworkStartIndication (NM PDU seen while Nm Bus-Sleep, race condition)",
             (unsigned)Network);

    if (ComM_ChannelMode[Network] != COMM_FULL_COMMUNICATION)
    {
        /* 呼び出しが実際に成功した場合のみクリアする（理由は本関数の
         * \details 参照。ComM_Nm_NetworkMode() と同じ無条件クリアに
         * 変更しないこと）。 */
        if (CanSM_RequestComMode(Network, COMM_FULL_COMMUNICATION) == E_OK)
        {
            ComM_NmReleasePending[Network] = 0U;
        }
    }
    /* else: チャネルは既に FULL_COM。本プロジェクトの同期的な設計では
     * ComM_ChannelMode を FULL_COM にする経路（ComM_BusSM_ModeIndication()の
     * FULL_COM分岐、ComM_Nm_NetworkMode()）がいずれも呼び出しの中で
     * Nm_NetworkRequest() を既に呼んでいるため、Nm がこの時点でなお
     * NM_STATE_BUS_SLEEP のままという状況は本プロジェクトの呼び出し経路
     * からは到達しない（本関数のDoxygen参照）。 */
}

/**
 * \brief   Nm が Network Mode へ（再）入ったことの通知（Nm から呼び出される）。
 *
 * \details [SWS_ComM_00296]。典型的には、Prepare Bus-Sleep Mode 中に他ノードの
 *          NM フレームを受信して Nm が自律的にスリープを取りやめたケース
 *          （[SWS_CanNm_00124]）。`ComM_ChannelMode[Network] !=
 *          COMM_FULL_COMMUNICATION` の場合、CanSM_RequestComMode(Network,
 *          COMM_FULL_COMMUNICATION) を呼ぶ前に ComM_NmReleasePending[Network]
 *          を無条件で（この呼び出しが成功するか Bus-Off 回復中で拒否される
 *          かに関わらず）クリアする。
 *
 *          この無条件クリアが安全性の根拠そのものである点に注意（「2 経路が
 *          物理的に排他だから安全」ではない）: CANSM_STATE_BUS_OFF 中も
 *          コントローラは受信を継続する（Can_MainFunction_Read() が RX
 *          ドレインをスキップするのは CanState==CAN_CS_SLEEP のときのみで、
 *          BUS_OFF は CAN_CS_STOPPED）ため、Bus-Off の
 *          最中でも Nm_RxIndication() は普通に発火しうる。つまり本関数が
 *          CanSM_State==CANSM_STATE_BUS_OFF の最中に呼ばれ、
 *          CanSM_RequestComMode() が拒否されるケースは実在する。もしここで
 *          「成功したときだけクリア」としていたら、ComM_NmReleasePending が
 *          立ったまま残り、後の Bus-Off 回復時に ComM_BusSM_ModeIndication() の
 *          FULL_COM 分岐にある既存のリトライガード（ComM_NmReleasePending が
 *          立っていれば CanSM_RequestComMode(NO_COM) を再送する）を誤って
 *          発火させ、Nm が既に Repeat Message State へ復帰して送信中の
 *          コントローラを強制的に再スリープさせてしまう（Nm の内部状態と
 *          物理コントローラの状態が食い違う）。無条件クリアにより、この
 *          リトライガードは本当に「Bus-Off がまだ解放未確認のまま回復した」
 *          ケースにのみ発火する。
 *
 * \param[in]  Network  ネットワークハンドル（0 〜 COMM_CHANNEL_COUNT-1）。
 *
 * \AUTOSARReq     {SWS_ComM_00390, SWS_ComM_00296}
 * \ServiceID      {0x18}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Nm_NetworkMode(uint8 Network)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_NM_NETWORK_MODE, COMM_E_UNINIT);
        return;
    }

    if (Network >= COMM_CHANNEL_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_NM_NETWORK_MODE, COMM_E_WRONG_PARAMETERS);
        return;
    }

    if (ComM_ChannelMode[Network] != COMM_FULL_COMMUNICATION)
    {
        /* CanSM_RequestComMode() 呼び出しの成否に関わらず無条件でクリアする
         * （理由は上記 \details 参照。ここを条件付きクリアに変更しないこと）。 */
        ComM_NmReleasePending[Network] = 0U;
        (void)CanSM_RequestComMode(Network, COMM_FULL_COMMUNICATION);
    }
}

/**
 * \brief   Nm が Bus-Sleep Mode へ到達したことの通知（Nm から呼び出される）。
 *
 * \details [SWS_ComM_00392]。ComM_RequestComMode() が FULL_COM -> NO_COM の
 *          要求時に Nm_NetworkRelease() のみを送って以降、Nm の協調スリープが
 *          完了する（[SWS_ComM_00637]）まで ComM_ChannelMode は FULL_COM の
 *          まま据え置かれている（ファイル冒頭コメント参照）。本関数はその
 *          完了通知であり、ここで初めて CanSM_RequestComMode(NO_COM) を呼び、
 *          物理スリープと ComM_ChannelMode の更新（CanSM が呼び返す
 *          ComM_BusSM_ModeIndication 経由）を行う。
 *
 *          Bus-Off 回復中は CanSM_RequestComMode() が E_NOT_OK を返しうる
 *          （CanSM.c 参照）。この場合 ComM_NmReleasePending はクリアしない
 *          （まだ解放が完了していないという事実を保持し続ける必要がある。
 *          クリアするタイミングは対称性のため ComM_BusSM_ModeIndication() の
 *          NO_COM 分岐に一本化している。同分岐の FULL_COM 側コメントも参照）。
 *
 * \param[in]  Network  ネットワークハンドル（0 〜 COMM_CHANNEL_COUNT-1）。
 *
 * \AUTOSARReq     {SWS_ComM_00392, SWS_ComM_00637}
 * \ServiceID      {0x1a}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Nm_BusSleepMode(uint8 Network)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_NM_BUS_SLEEP_MODE, COMM_E_UNINIT);
        return;
    }

    if (Network >= COMM_CHANNEL_COUNT)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_NM_BUS_SLEEP_MODE, COMM_E_WRONG_PARAMETERS);
        return;
    }

    (void)CanSM_RequestComMode(Network, COMM_NO_COMMUNICATION);
}

/**
 * \brief   ComM 周期処理。
 *
 * \details 本実装は意図的な NOP。ComMTMinFullComModeDuration（ECUC_ComM_00557、
 *          FULL_COM 要求解放を一定時間遅らせて要求のチャタリングを防ぐ
 *          ヒステリシスタイマ）は、[SWS_ComM_00888] のとおり
 *          `ComMNmVariant=FULL` の構成では不要（Rationale 原文: "No timer
 *          needed if AUTOSAR NM is used. This avoids redundant functionality
 *          because AUTOSAR NM also ensures this functionality."）。本プロジェクトは
 *          `Nm_NetworkRequest()`/`Nm_NetworkRelease()` を能動的に呼び、CanNm の
 *          協調スリープ（Repeat Message Time → Ready Sleep Time → Prepare
 *          Bus-Sleep、Nm.c 参照）が同じチャタリング防止の役目を既に果たして
 *          いるため、この `ComMNmVariant=FULL` に該当する。そのため本関数で
 *          追加のタイマ処理は行わない（2026-08 のスペック監査で確認・
 *          Os_PBCfg.c への MainFunction 登録漏れも同時に修正済み）。
 *          `ComM_MainFunction()` 自体は AUTOSAR が要求する周期関数のため、
 *          将来 DCM ActiveDiagnostic 監視等の別用途が生じた際に流用できるよう
 *          スケジューラへの登録（Task 19、Os_PBCfg.c 参照）だけは行っている。
 *
 * \AUTOSARReq     {SWS_ComM_00888}
 * \ServiceID      {0x60}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_MainFunction(void)
{
    DET_LOGT(TAG, "called");
    if (!ComM_Initialized)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_MAIN_FUNCTION, COMM_E_UNINIT);
        return;
    }
    /* NOP（理由は上記 \details 参照）。 */
}

void ComM_GetVersionInfo(Std_VersionInfoType* Versioninfo)
{
    DET_LOGT(TAG, "called");
    if (Versioninfo == NULL)
    {
        Det_ReportError(COMM_MODULE_ID, 0U, COMM_API_ID_GET_VERSION_INFO, COMM_E_PARAM_POINTER);
        return;
    }

    Versioninfo->vendorID         = COMM_VENDOR_ID;
    Versioninfo->moduleID         = COMM_MODULE_ID;
    Versioninfo->sw_major_version = COMM_SW_MAJOR_VERSION;
    Versioninfo->sw_minor_version = COMM_SW_MINOR_VERSION;
    Versioninfo->sw_patch_version = COMM_SW_PATCH_VERSION;
}
