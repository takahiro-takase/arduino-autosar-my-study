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
 *          User0 要求のバス状態変化への追従（ComM_BusSMIndication 参照）:
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
 *            Bus-Sleep Mode、Nm.c 参照）が自律的に協調スリープを進め、実際に
 *            Bus-Sleep Mode へ到達すると ComM_Nm_BusSleepMode() を呼ぶ。ここで
 *            初めて CanSM_RequestComMode(NO_COM) を呼び、物理スリープと
 *            ComM_ChannelMode の更新（ComM_BusSMIndication 経由）が起きる。
 *            この待機期間中に誰かが FULL_COM を再要求した場合の扱いは
 *            ComM_NmReleasePending[] のコメント（下記）を参照。
 *            本プロジェクトの Nm は ComM_Nm_NetworkMode()/
 *            ComM_Nm_NetworkStartIndication()/ComM_Nm_PrepareBusSleepMode() に
 *            相当するサブ遷移を区別して通知しない設計のため、これら 3 つの
 *            コールバックは実装しない（ComM_Nm_BusSleepMode() のみ）。
 *            ComMNmVariant=FULL の完全準拠ではなく、この意味での部分対応である。
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
 *  FULL_COM のまま据え置く。ComM_RequestComMode()/ComM_BusSMIndication()/
 *  ComM_Nm_BusSleepMode() 参照。 */
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
        ComM_ChannelMode[i]      = COMM_NO_COMMUNICATION;
        ComM_NmReleasePending[i] = 0U;
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
 * \brief   ユーザが通信モードを要求する。
 *
 * \details ユーザの要求を記録した後、全ユーザの要求のうち最も通信レベルの高い
 *          モード（FULL_COM > SILENT_COM > NO_COM）へ集約し、集約結果が
 *          チャネルの現状と異なる場合のみ CanSM へ転送する。
 *          1 ユーザだけが FULL_COM を要求していても、
 *          他のユーザが NO_COM を要求している間はチャネルは FULL_COM のまま
 *          維持される（「誰か一人でも通信を必要としていればバスは落とさない」）。
 *
 * \param[in]  User     要求するユーザ ID (COMM_USER_0 / COMM_USER_1)。
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

    /* 全ユーザの要求のうち最も通信レベルの高いモードへ集約する。
     * COMM_FULL_COMMUNICATION(2) > COMM_SILENT_COMMUNICATION(1) >
     * COMM_NO_COMMUNICATION(0) と値そのものが優先順位になっているため、
     * 単純な最大値計算で集約できる。 */
    ComM_ModeType aggregated = COMM_NO_COMMUNICATION;
    uint8 i;
    for (i = 0U; i < COMM_USER_COUNT; i++)
    {
        if (ComM_UserRequest[i] > aggregated)
            aggregated = ComM_UserRequest[i];
    }

    DET_LOGI(TAG, "User%u req=%u -> aggregated=%u (channel=%u)",
             (unsigned)User, (unsigned)ComMode,
             (unsigned)aggregated, (unsigned)ComM_ChannelMode[0U]);

    /* FULL_COM への（再）要求が Nm 協調スリープ待ち（ComM_NmReleasePending）の
     * 最中に来た場合は、他の何よりも先にこれを処理する（[SWS_ComM_00882] 相当）。
     * ComM_ChannelMode が現在何であるか（FULL_COM のまま据え置き中の通常ケースか、
     * Bus-Off で一時的に SILENT_COM になっているケースか）に関わらず、
     * ユーザーの最新の意思は「もう眠らなくていい」なので、まず解放を取り消す。
     * ここで CanSM へは触れない（Bus-Off 回復待ち中であれば CanSM は
     * CanSM_RequestComMode() をどのみち拒否するし、そうでなければ
     * ComM_ChannelMode は既に FULL_COM で何もすることがない）。CanSM が
     * FULL_COM へ復帰済み/復帰する際は、CanSM が呼び返す ComM_BusSMIndication()
     * の FULL_COM 分岐が ComM_NmReleasePending の解除を確認して通常どおり
     * 処理する（下記 ComM_BusSMIndication() 参照）。
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
        (void)Nm_NetworkRequest();
        DET_LOGI(TAG, "FULL_COM re-requested during Nm release wait -> cancelled");
        if (ComM_ChannelMode[0U] == aggregated)
            return E_OK;
        /* ComM_ChannelMode がまだ FULL_COM でない（Bus-Off で SILENT_COM 中）
         * 場合、CanSM は今操作しない。上記コメントのとおり、CanSM が後で
         * FULL_COM を通知してきたときに ComM_BusSMIndication() 側が処理する。 */
        return E_OK;
    }

    /* 集約結果が現在のチャネル状態と同じなら何もしない
     * （このユーザの要求変化が他ユーザの要求に埋もれて無効化されたケースを含む）。 */
    if (ComM_ChannelMode[0U] == aggregated)
        return E_OK;

    /* FULL_COM -> NO_COM: CanSM へは何も伝えない。[SWS_ComM_00133] のとおり
     * Nm_NetworkRelease() のみを直接呼び、ComM_ChannelMode は FULL_COM の
     * まま据え置く（Nm の協調スリープが完了するまでの間、実 AUTOSAR の
     * COMM_FULL_COM_READY_SLEEP サブステートに相当）。実際の物理スリープと
     * ComM_ChannelMode の更新は、Nm が Bus-Sleep Mode へ到達して呼ぶ
     * ComM_Nm_BusSleepMode() まで遅延する（詳細はファイル冒頭コメント参照）。
     * ComM_NmReleasePending が既に立っていれば（App_EngineManager が OFF 継続中
     * 毎周期 NO_COM を要求し続けるため、この分岐に何度も来うる）2 回目以降は
     * 何もしない。Nm_NetworkRelease() の再送は無意味な上、DET_LOGI が呼び出し
     * のたびに出てログを埋めてしまう（/code-review で指摘・検証済み）。 */
    if (ComM_ChannelMode[0U] == COMM_FULL_COMMUNICATION && aggregated == COMM_NO_COMMUNICATION)
    {
        if (ComM_NmReleasePending[0U])
            return E_OK;
        ComM_NmReleasePending[0U] = 1U;
        (void)Nm_NetworkRelease();
        DET_LOGI(TAG, "FULL_COM -> NO_COM requested: Nm_NetworkRelease() sent, awaiting Bus-Sleep Mode");
        return E_OK;
    }

    /* それ以外（NO_COM -> FULL_COM 等）は従来どおり即座に CanSM へ転送する。
     * 成功すれば CanSM が ComM_BusSMIndication を呼んでチャネル状態と EcuM を
     * 更新する。Bus-Off 回復中は E_NOT_OK が返る。 */
    return CanSM_RequestComMode(0U, aggregated);
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
 *          Dcm（COMM_USER_1）の要求はセッション状態に基づく独立した判断のため、
 *          ここでは同期しない。
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
void ComM_BusSMIndication(uint8 Network, ComM_ModeType Mode)
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
                /* Nm 協調スリープ待ち（ComM_RequestComMode() が Nm_NetworkRelease()
                 * を送信済み、Bus-Sleep Mode 到達待ち）の最中に、CanSM が
                 * COMM_FULL_COMMUNICATION を通知してきたケース。誰かが FULL_COM を
                 * 再要求したのであれば、その再要求は ComM_RequestComMode() の
                 * 「集約結果が現状と同じ」早期returnパスが既に処理し
                 * ComM_NmReleasePending をクリア済みのはず。したがってここへ
                 * 到達するのは、Bus-Off が協調スリープ待ちの最中に発生し
                 * （CanSM_ControllerBusOff() は ComM_BusSMIndication を
                 * SILENT_COMMUNICATION で呼ぶ）、回復した CanSM が改めて
                 * FULL_COMMUNICATION を通知してきた場合のみ。Nm は Bus-Off 中も
                 * 独立したタイマで動き続けており、既に Bus-Sleep Mode へ到達して
                 * いる可能性がある。ここで無条件に Nm_NetworkRequest() すると、
                 * 誰も再要求していないのに Nm を誤って再起床させてしまう
                 * （設計時レビューで発見）。CanSM が FULL_COM へ戻った今なら
                 * CanSM_RequestComMode(NO_COM) は受理されるはずなので、解放を
                 * 仕切り直す。この再帰呼び出しが ComM_BusSMIndication(NO_COM) を
                 * 呼び返し、ComM_ChannelMode/ComM_UserRequest[0]/
                 * ComM_NmReleasePending は最終的にそちらの分岐で正しい値へ
                 * 収束する（BswM への通知もそちらに任せるため、本分岐では
                 * ファイル末尾の BswM_ComM_CurrentMode() へは進まず return する）。 */
                DET_LOGW(TAG, "FULL_COM notified while Nm release still pending "
                              "(likely BusOff recovery mid-winddown) -> retry release");
                (void)CanSM_RequestComMode(Network, COMM_NO_COMMUNICATION);
                return;
            }
            if (ComM_EcuMRunMode != COMM_FULL_COMMUNICATION)
            {
                (void)EcuM_RequestRUN(ECUM_USER_COMM);
                ComM_EcuMRunMode = COMM_FULL_COMMUNICATION;
            }
            (void)Nm_NetworkRequest();  /* 通信が必要になったことを Nm へ伝える */
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
             * ComM_BusSMIndication(FULL_COMMUNICATION) を呼ぶため、その際に
             * 上の分岐（ComM_NmReleasePending が立っていなければ）で
             * Nm_NetworkRequest() が呼ばれ自動的に再開する。 */
            (void)Nm_NetworkRelease();
        }
    }
    /* SILENT_COM: EcuM の RUN 状態は維持（受信専用でも ECU は動作継続）。 */

    BswM_ComM_CurrentMode(Network, Mode);  /* BswM へ ComM モード変化を通知 */
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
 *          ComM_BusSMIndication 経由）を行う。
 *
 *          Bus-Off 回復中は CanSM_RequestComMode() が E_NOT_OK を返しうる
 *          （CanSM.c 参照）。この場合 ComM_NmReleasePending はクリアしない
 *          （まだ解放が完了していないという事実を保持し続ける必要がある。
 *          クリアするタイミングは対称性のため ComM_BusSMIndication() の
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
