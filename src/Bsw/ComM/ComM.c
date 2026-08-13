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
 *          複数ユーザの調停 (AUTOSAR SWS_ComM_00069):
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
 *          Nm（CanNm）との連携:
 *            チャネルモードが実際に FULL_COM/NO_COM へ変化するたびに（本関数、
 *            ComM_BusSMIndication 参照）Nm_NetworkRequest()/Nm_NetworkRelease()
 *            を呼ぶ。以降の実際の CAN コントローラの物理スリープは Nm の
 *            状態機械が自律的に判断する（他ノードがまだ NM フレームを送信中
 *            なら実際には眠らない、という協調スリープ。詳細は Nm.c/CanSM.c 参照）。
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
        ComM_ChannelMode[i] = COMM_NO_COMMUNICATION;
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
 * \AUTOSARReq     {SWS_ComM_00069}
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

    /* 集約結果が現在のチャネル状態と同じなら何もしない
     * （このユーザの要求変化が他ユーザの要求に埋もれて無効化されたケースを含む） */
    if (ComM_ChannelMode[0U] == aggregated)
        return E_OK;

    /* CanSM に転送。成功すれば CanSM が ComM_BusSMIndication を呼んで
     * チャネル状態と EcuM を更新する。Bus-Off 回復中は E_NOT_OK が返る。 */
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
            if (ComM_EcuMRunMode != COMM_FULL_COMMUNICATION)
            {
                (void)EcuM_RequestRUN(ECUM_USER_COMM);
                ComM_EcuMRunMode = COMM_FULL_COMMUNICATION;
            }
            (void)Nm_NetworkRequest();  /* 通信が必要になったことを Nm へ伝える */
        }
        else if (Mode == COMM_NO_COMMUNICATION)
        {
            if (ComM_EcuMRunMode != COMM_NO_COMMUNICATION)
            {
                (void)EcuM_ReleaseRUN(ECUM_USER_COMM);
                ComM_EcuMRunMode = COMM_NO_COMMUNICATION;
            }
            (void)Nm_NetworkRelease();  /* 通信が不要になったことを Nm へ伝える。
                                         * 実際の CAN コントローラの物理スリープは
                                         * Nm が Bus-Sleep Mode へ到達してから
                                         * CanSM_NmBusSleepMode() 経由で行われる
                                         * （協調スリープ、CanSM.c 参照）。 */
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
             * 上の分岐で Nm_NetworkRequest() が呼ばれ自動的に再開する。 */
            (void)Nm_NetworkRelease();
        }
    }
    /* SILENT_COM: EcuM の RUN 状態は維持（受信専用でも ECU は動作継続）。 */

    BswM_ComM_CurrentMode(Network, Mode);  /* BswM へ ComM モード変化を通知 */
}

/**
 * \brief   ComM 周期処理。
 *
 * \details 本実装はスタブ。NM 連携・バス スリープは未対応。
 *
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
    /* NM / バス スリープ未対応のため NOP */
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
