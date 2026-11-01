/**
 * \file    CanSM.c
 * \brief   CAN ステートマネージャ 実装 (AUTOSAR SWS_CanSM 準拠)
 * \details CAN ネットワークの通信モード遷移と Bus-Off 回復シーケンスを管理する。
 *
 *          内部状態機械:
 *
 *            CANSM_STATE_NO_COM ──────────────────────────────┐
 *              ↓ RequestComMode(FULL_COM) → CAN_T_START        │ ウェイクアップ
 *            CANSM_STATE_FULL_COM  ←────────────────────────┐  │ (バス活動検知)
 *              ↓ RequestComMode(NO_COM) → CAN_T_SLEEP         │ │
 *              └───────────────────────────────────────────────┘
 *              ↓ CanSM_ControllerBusOff()                    │ 回復成功
 *            CANSM_STATE_BUS_OFF                             │
 *              ↓ L1/L2 周期経過 (MainFunction)                │
 *              → CanIf_SetControllerMode(CAN_CS_STARTED) ────┘
 *              （回復に失敗すれば CanSM_ControllerBusOff() が再度呼ばれ、
 *                リトライを継続する。無期限に諦めない。回復成功時は Bus-Off 発生直前の
 *                状態（CANSM_STATE_FULL_COM、または CANSM_STATE_SILENT_COM）へ戻る。
 *                図は FULL_COM 経由のみを示す）
 *
 *          コントローラのモード変更の手順（2026-11、S_PRE_FULLCOM / S_PRE_NOCOM）:
 *            NO_COM / WAKEUP_VALIDATING から FULL_COM への要求は、CANSM_STATE_PRE_FULL_COM へ入り、コントローラを
 *            STOPPED → STARTED の順に変える。FULL_COM から NO_COM への要求は、CANSM_STATE_PRE_NO_COM へ入り、
 *            STOPPED → SLEEP の順に変える。現在のモードが既に STOPPED なら、STOPPED の要求は省く。
 *            各要求のあと、CanSM_ControllerModeIndication()（Can_MainFunction_Mode() → CanIf から届く完了通知）で、
 *            手順を次へ進める。CANSM_MODEREQ_REPEAT_TIME_MS が過ぎても通知が来なければ、要求を繰り返し
 *            （CanSM_MainFunction()）、CANSM_MODEREQ_REPEAT_MAX を超えたら T_REPEAT_MAX（PRE_FULL_COM からは
 *            PRE_NO_COM へ戻り、PRE_NO_COM からは、あきらめて NO_COM へ入る）。
 *            ComM への通知（ComM_BusSM_ModeIndication）は、手順の完了のとき（FULL_COM / NO_COM へ入り終えたとき）に出す。
 *            PRE_NO_COM の最中の FULL_COM の要求は、保留して、NO_COM へ入り終えたあとで、始める。
 *            PRE_FULL_COM の最中の NO_COM の要求は、PRE_NO_COM へ向きを変える。
 *            SILENT_COM から FULL_COM へ戻るときは、コントローラが既に STARTED のため、手順を経ずに、PDU を ONLINE にする。
 *
 *          RequestComMode(NO_COM) が CanSM へ届く時点で、上位層（ComM）は
 *          既に CanNm の協調スリープ完了を確認済みである（[SWS_ComM_00133]/
 *          [SWS_ComM_00392] 準拠、ComM.c 参照）。そのため CanSM 自身は
 *          「解放要求はされたがまだ寝てはいけない」という中間状態を持たず、
 *          FULL_COM から物理スリープまで一気に遷移する（以前あった
 *          CANSM_STATE_NO_COM_PENDING_SLEEP という中間状態は、ComM が
 *          CanNm の完了を待たず早期に NO_COM を通知していた旧設計の名残で、
 *          この設計では不要になったため削除した。詳細は ComM.c ファイル
 *          冒頭コメント参照）。
 *
 *          Bus-Off 回復シーケンス（SWS_CanSM_00514/00515/00636 準拠、
 *          CanSMEnableBusOffDelay=FALSE 相当の L1/L2 バックオフ）:
 *            1. Bus-Off 検出 → コントローラ停止・タイマ起動
 *            2. CANSM_BUSOFF_RECOVERY_L1_MS（短い周期）待機（CanSM_MainFunction が監視）
 *            3. コントローラ再起動 → Bus-Off 前の状態（FULL_COM / SILENT_COM）に復帰。回復の確定
 *               （G_BUS_OFF_PASSIVE）のとき、Dem へ PASSED を報告する（[SWS_CanSM_00498]）
 *            4. 再度 Bus-Off が発生した場合は試行回数をカウント。Bus-Off を検出するたびに、Dem へ PREFAILED を報告する
 *               （DemConf_DemEventParameter_CAN_BUSOFF、[SWS_CanSM_00522]）
 *            5. 試行回数が CANSM_BUSOFF_L1_TO_L2_COUNT を超えたら、一時的な
 *               バス障害ではなく持続的な Bus-Off と判断する。Dem 側は、PREFAILED の積み重ねが
 *               DEM_DEBOUNCE_LIMIT_CAN_BUSOFF（= CANSM_BUSOFF_L1_TO_L2_COUNT + 1）に達した時点で DTC を確定する。
 *               以降はリトライ周期を長い
 *               CANSM_BUSOFF_RECOVERY_L2_MS へ切り替えるだけで、回復試行その
 *               ものは無期限に継続する（AUTOSAR 仕様には「回復を諦めて二度と
 *               復帰しない」状態は存在しない。一時的なバス障害が長引いただけで
 *               実機リセットが必要になることを避けるため、本実装も仕様通り
 *               無期限リトライとした）。
 *
 *          正常系（ボランタリ）スリープとウェイクアップ（CanNm による協調スリープ）:
 *            App_EngineManager が「エンジン OFF が一定サイクル継続 = 通信不要」と
 *            判断すると ComM_RequestComMode(COMM_USER_0, NO_COM) を要求する。
 *            Dcm も ComM_DCM_ActiveDiagnostic() で extendedSession 中を通知して
 *            いなければ ComM の集約結果が NO_COM になるが、ComM はこの時点では
 *            CanSM を一切呼ばず
 *            CanNm_NetworkRelease() のみを CanNm（CanNm 状態機械、CanNm.c 参照）へ送る
 *            （[SWS_ComM_00133]、詳細は ComM.c ファイル冒頭コメント参照）。
 *            CanNm 自身が Ready Sleep → Prepare Bus-Sleep → Bus-Sleep Mode と
 *            自律的に遷移する。Prepare Bus-Sleep Mode へ入った時点で
 *            ComM_Nm_PrepareBusSleepMode()（[SWS_ComM_00826]）経由で
 *            CanSM_RequestComMode(SILENT_COM) が呼ばれ、CANSM_STATE_SILENT_COM
 *            （受信専用）へ遷移する（2026-08 追加。これにより本状態が
 *            初めて実際の呼び出し元を持つ。以前は Bus-Off 検出時に
 *            ComM_BusSM_ModeIndication() を直接 SILENT_COMMUNICATION で呼ぶだけで、
 *            CanSM_State 自体は CANSM_STATE_BUS_OFF のままだった）。
 *            SILENT_COM の実現方式は 2026-08 に再度変更した:
 *            当初は Can_SetControllerMode(CAN_T_STOP) でコントローラ全体を
 *            Listen-Only へ落としていたが、実 AUTOSAR の SILENT_COMMUNICATION
 *            は PDU チャネル単位の TX 抑制（CanIf_SetPduMode(CANIF_TX_OFFLINE)、
 *            [SWS_CANIF_00137]）で実現するものであり、コントローラ自体は
 *            CAN_CS_STARTED のまま維持する方式へ置き換えた。この間に他ノードの NM
 *            フレーム受信が一定時間 (CANNM_TIMEOUT_MS+CANNM_WAIT_BUS_SLEEP_MS)
 *            なかったことを確認してから ComM_Nm_BusSleepMode() を呼ぶ
 *            （[SWS_ComM_00392]）。ComM はこれを受けて初めて
 *            CanSM_RequestComMode(NO_COM) を呼び、CanSM が
 *            S_PRE_NOCOM（STOPPED → SLEEP）で MCP2515 を実際に
 *            スリープさせる。これにより「他ノードがまだ通信中の間は実際には
 *            スリープしない」という NM 本来の協調スリープを実機で確認できる。
 *            CANSM_STATE_BUS_OFF は実 HW をスリープさせないため、これが
 *            本モジュールで唯一 HW を実際にスリープさせる経路である。
 *            なお CANSM_STATE_BUS_OFF は CAN_CS_STOPPED（受信は継続）扱いの
 *            ため、Bus-Off 回復待ち中でも CanIf_RxIndication()/
 *            CanNm_RxIndication() は普通に発火しうる（Can_MainFunction_Read()
 *            が RX ドレインをスキップするのは CanState==CAN_CS_SLEEP の
 *            ときのみ）。CANSM_STATE_SILENT_COM は 2026-08 の変更後は
 *            CAN_CS_STARTED のまま（CanIf_SetPduMode(CANIF_TX_OFFLINE) で
 *            TX のみ抑制）のため、受信が継続する理由は BUS_OFF とは異なる
 *            （そもそもコントローラが止まっていない）が、結果として
 *            SILENT_COM 中も受信は同様に継続する。またこの変更により、
 *            SILENT_COM 中に Bus-Off が発生することも起こりうるようになった
 *            （CanSM_ControllerBusOff()/CanSM_PreBusOffState 参照。回復成功時は
 *            元が SILENT_COM だったか FULL_COM だったかに応じて正しい状態へ
 *            戻す）。CanNm がこれを受けて Prepare
 *            Bus-Sleep Mode から Network Mode へ自律復帰した場合の扱いは
 *            ComM.c の ComM_Nm_NetworkMode() の doc コメントを参照
 *            （CanSM_RequestComMode() は Bus-Off の回復中の要求を受理する。[SWS_CanSM_00369]/[SWS_CanSM_00182]）。
 *
 *          ウェイクアップ検証（Wakeup Validation、AUTOSAR EcuM の
 *          Wakeup Validation Protocol に相当）:
 *            MCP2515 は電気的ノイズ等でも WAKIF を誤って立てうるため、
 *            INT ピンのアサートを検知しただけで即座に FULL_COM へ復帰せず、
 *            「本当に有効な CAN フレームを受信できたか」を確認してから
 *            復帰する 2 段階の手順を踏む。
 *              1. Can_Isr()（真の割り込み）が SLEEP 中の INT アサートを検出
 *                 → Can_MainFunction_Wakeup() が EcuM_CheckWakeup() を呼ぶ
 *                   （[SWS_Can_00271]。2026-09-05 是正前は実仕様に存在しない
 *                   独自 API CanIf_ControllerWakeup() 経由だった）
 *                 → CanSM_StartWakeupSource()（[SWS_CanSM_00609]。以前は CanSM_ControllerModeIndication() を流用していた）
 *                 → CAN_T_WAKEUP のみ実行（SLEEP→STOPPED、Listen-Only）。
 *                   CANSM_STATE_WAKEUP_VALIDATING へ遷移し、検証タイマ開始。
 *                   ComM/EcuM へはまだ何も通知しない（この時点ではノイズの
 *                   可能性を否定できないため）。
 *              2a. 検証タイマ (CANSM_WAKEUP_VALIDATION_MS) 内に何らかの
 *                  CAN フレームを正常受信 → CanIf_RxIndication() が記憶し、
 *                  EcuM_MainFunction() が CanIf_CheckValidation() で確認して
 *                  EcuM_ValidateWakeupEvent() → ComM_EcuM_WakeUpIndication()
 *                  → CanSM_RequestComMode(FULL_COM) が呼ばれる（検証成功。
 *                  2026-10 に、独自 API の CanSM_RxIndication() から置き換え）。
 *                  S_PRE_FULLCOM（コントローラは既に STOPPED のため STARTED だけ）を経て CANSM_STATE_FULL_COM へ
 *                  確定し、ComM_BusSM_ModeIndication(FULL_COM) で EcuM を RUN へ復帰させる。
 *              2b. 検証タイマが切れても何も受信できなければ、CanSM_MainFunction()
 *                  がノイズによる誤ウェイクアップと判断し、CAN_CS_SLEEP で
 *                  再びスリープへ戻す（ComM/EcuM は一切関与しないため、
 *                  他レイヤに影響を与えず静かに元の状態へ戻れる）。
 *            CANSM_STATE_NO_COM からの起床のみを受け付ける。CANSM_STATE_BUS_OFF
 *            は実 HW をスリープさせないため、この状態から呼ばれることは
 *            原理的にない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "CanSM.h"
#include "CanIf.h"
#include "Dem.h"
#include "Det.h"
#include "Os.h"

#if DEM_DEBOUNCE_LIMIT_CAN_BUSOFF != (CANSM_BUSOFF_L1_TO_L2_COUNT + 1)
#error "DEM_DEBOUNCE_LIMIT_CAN_BUSOFF (config/data/Dem.json) must be CANSM_BUSOFF_L1_TO_L2_COUNT + 1"
#endif

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "CanSM"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * 内部型定義
 * ----------------------------------------------------------------------- */
typedef enum
{
    CANSM_STATE_NO_COM,              /* 通信停止・HW も実際にスリープ済み（または未起動） */
    CANSM_STATE_SILENT_COM,          /* 受信専用 */
    CANSM_STATE_FULL_COM,            /* 全二重通信（正常動作） */
    CANSM_STATE_BUS_OFF,             /* Bus-Off 回復中 */
    CANSM_STATE_WAKEUP_VALIDATING,   /* ウェイクアップ検証中（Listen-Only、RX確認待ち） */
    CANSM_STATE_PRE_FULL_COM,        /* S_PRE_FULLCOM: FULL_COM へ入る途中（モード変更の完了通知待ち。[SWS_CanSM_00483〜00495]） */
    CANSM_STATE_PRE_NO_COM           /* S_PRE_NOCOM: NO_COM へ入る途中（モード変更の完了通知待ち。[SWS_CanSM_00464〜00480]） */
} CanSM_InternalStateType;

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */
static CanSM_InternalStateType CanSM_State;
static unsigned long           CanSM_BusOffTimerMs;   /* Bus-Off 検出時刻 (直近のリトライ基準点) */
static uint8                   CanSM_BusOffRetries;   /* 回復試行回数 (L1→L2 切替判定にも使う。回復が確定すると 0 へ戻す) */

/** 回復の再起動をしたが、確定条件 G_BUS_OFF_PASSIVE をまだ満たしていない（[SWS_CanSM_00496]〜[00498]）。
 *  満たすと Dem へ PASSED を報告し、CanSM_BusOffRetries を 0 へ戻す。満たす前に再度 Bus-Off が起きたら、
 *  確定しないまま次の回復試行へ進む（回数は戻らない）。 */
static uint8                   CanSM_BorPassivePending;
static unsigned long           CanSM_BorRestartMs;    /* 直近の回復の再起動の時刻（確定条件を時間で判定するときの起点） */
static unsigned long           CanSM_ValidationTimerMs; /* ウェイクアップ検証開始時刻 */

/** S_PRE_FULLCOM / S_PRE_NOCOM で、コントローラのモードを変える手順の状態（[SWS_CanSM_00464]〜[00495]）。
 *  CanSM_ModeReqTarget: いま要求して、完了通知（CanSM_ControllerModeIndication）を待っているモード。
 *  CanSM_ModeReqTimerMs: その要求を出した時刻。CANSM_MODEREQ_REPEAT_TIME_MS を過ぎても通知が来なければ、要求を繰り返す。
 *  CanSM_ModeReqRepeats: 同じ要求を繰り返した回数。CANSM_MODEREQ_REPEAT_MAX を超えたら T_REPEAT_MAX。
 *  CanSM_PendingFullRequest: PRE_NO_COM の最中に FULL_COM が要求された。NO_COM へ入り終えたあとで、始める。 */
static Can_ControllerStateType CanSM_ModeReqTarget;
static unsigned long           CanSM_ModeReqTimerMs;
static uint8                   CanSM_ModeReqRepeats;
static uint8                   CanSM_PendingFullRequest;

/** PRE_NO_COM へ入る前の、ネットワークのモード（COMM_FULL_COMMUNICATION / COMM_SILENT_COMMUNICATION /
 *  COMM_NO_COMMUNICATION）。CanSM_GetCurrentComMode() は、PRE_NO_COM の最中、この値を返す
 *  （[SWS_CanSM_00266]: 現在のネットワークモードは、各遷移の効果 E_NOCOM などで更新される。
 *  NO_COM へ入り終えるまでは、ComM へ通知した直前のモードのまま）。 */
static ComM_ModeType           CanSM_ModeBeforePreNoCom;

/** Bus-Off の回復の後に戻る状態（CANSM_STATE_FULL_COM または CANSM_STATE_SILENT_COM のいずれか）。
 *  Bus-Off 検出直前の CanSM_State で始まり、回復中に CanSM_RequestComMode() が FULL_COM / SILENT_COM を
 *  要求されたら、そのモードへ変わる（[SWS_CanSM_00182]）。2026-08 追加: SILENT_COMMUNICATION
 *  が CanIf_SetPduMode(CANIF_TX_OFFLINE) ベースになりコントローラを
 *  CAN_CS_STARTED のまま維持するようになったため、SILENT_COM 中でも本当に
 *  Bus-Off しうるようになった。回復成功時にどちらへ戻すか
 *  （CanSM_MainFunction() 参照）を判断するために使う。 */
static CanSM_InternalStateType CanSM_PreBusOffState;

/** [SWS_CanSM_00184/00188/00190] 用の初期化済みフラグ。CanSM_State の既定値
 *  (CANSM_STATE_NO_COM=0) は「未初期化」と区別が付かないため別途持つ。 */
static uint8 CanSM_Initialized = 0U;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static unsigned long CanSM_GetNowMs(void);

static void CanSM_SetPduModeOnlineBestEffort(const char* callerTag);

static boolean CanSM_BorPassiveGuardMet(void);
static void CanSM_AbortWakeupValidation(void);
static Can_ControllerStateType CanSM_GetControllerModeOrUnknown(void);
static void CanSM_IssueModeRequest(Can_ControllerStateType target);
static void CanSM_EnterPreFullCom(void);
static void CanSM_EnterPreNoCom(void);
static void CanSM_CompleteFullCom(void);
static void CanSM_CompleteNoCom(void);
static void CanSM_AdvanceModeSequence(void);
static void CanSM_ServiceModeRequest(void);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanSM_Init
 * ---------------------------------------------------------------------- */

/**
 * \brief   CanSM モジュールを初期化する。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_Init(const CanSM_ConfigType* ConfigPtr)
{
    (void)ConfigPtr; /* 本プロジェクトは post-build 設定を持たない（CanSM.h 参照） */
    CanSM_State              = CANSM_STATE_NO_COM;
    CanSM_BusOffTimerMs      = 0UL;
    CanSM_BusOffRetries      = 0U;
    CanSM_BorPassivePending  = 0U;
    CanSM_BorRestartMs       = 0UL;
    CanSM_ValidationTimerMs  = 0UL;
    CanSM_PreBusOffState     = CANSM_STATE_NO_COM;
    CanSM_ModeReqTarget      = CAN_CS_STOPPED;
    CanSM_ModeReqTimerMs     = 0UL;
    CanSM_ModeReqRepeats     = 0U;
    CanSM_PendingFullRequest = 0U;
    CanSM_ModeBeforePreNoCom = COMM_NO_COMMUNICATION;
    CanSM_Initialized        = 1U;
    DET_LOGI(TAG, "Init");
}

/* ----------------------------------------------------------------------
 * CanSM_DeInit
 * ---------------------------------------------------------------------- */

void CanSM_DeInit(void)
{
    if (!CanSM_Initialized)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_DEINIT, CANSM_E_UNINIT);
        return;
    }

    CanSM_Initialized = 0U;
    DET_LOGI(TAG, "DeInit ok");
}

/* ----------------------------------------------------------------------
 * CanSM_RequestComMode
 * ---------------------------------------------------------------------- */

/**
 * \brief   ネットワークの通信モード遷移を要求する。
 *
 * \details ComM から呼び出される。ネットワークハンドルが正しければ、要求を受理し（E_OK）、状態機械が
 *          考慮する（[SWS_CanSM_00369]/[SWS_CanSM_00182]）。FULL_COM / NO_COM への遷移は、コントローラのモードを変える
 *          要求を出して、完了通知を待つ手順（S_PRE_FULLCOM / S_PRE_NOCOM）で行う。ComM への結果の通知は、
 *          手順の完了のときに出る（本関数の戻りのあと）。Bus-Off 回復中の要求は、次のとおり扱う。
 *          - NO_COM: すぐに受け付けて、回復を取り消し、コントローラをスリープさせる（Bus-Off の回復の途中でも、
 *            S_FULLCOM と S_SILENTCOM_BOR のどちらからも、T_NO_COM_MODE_REQUEST で E_PRE_NOCOM へ遷移する）。
 *          - FULL_COM / SILENT_COM: 回復の後に戻る状態（CanSM_PreBusOffState）を、要求されたモードへ変える
 *            （回復の完了で、G_FULL_COM_MODE_REQUESTED / G_SILENT_COM_MODE_REQUESTED が、要求されたモードを選ぶ）。
 *            SILENT_COM への変更は、回復を待たず、TX を止める（CanIf_SetPduMode(CANIF_TX_OFFLINE)）。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType mode)
{
    if (!CanSM_Initialized)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_REQUEST_COM_MODE, CANSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (network >= CANSM_CHANNEL_COUNT)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_REQUEST_COM_MODE, CANSM_E_INVALID_NETWORK_HANDLE);
        return E_NOT_OK;
    }

    /* Bus-Off 回復中の要求（[SWS_CanSM_00369]/[SWS_CanSM_00182]: 受理して、状態機械が考慮する）。
     * NO_COM は、下の通常の分岐が、すぐに処理する（CanSM_State が BUS_OFF でも動く）。
     * FULL_COM / SILENT_COM は、回復の後に戻る状態を変えるだけで、回復の処理は止めない。 */
    if ((CanSM_State == CANSM_STATE_BUS_OFF) && (mode != COMM_NO_COMMUNICATION))
    {
        if (mode == COMM_FULL_COMMUNICATION)
        {
            CanSM_PreBusOffState = CANSM_STATE_FULL_COM;   /* 回復の完了で、PDU を ONLINE にして、FULL_COM へ戻る */
        }
        else if (mode == COMM_SILENT_COMMUNICATION)
        {
            if (CanSM_PreBusOffState == CANSM_STATE_FULL_COM)
            {
                /* Bus-Off の前は FULL_COM（PDU は ONLINE）。回復の完了で SILENT_COM へ戻すには、TX を止めておく
                 * （SILENT_COM から戻る分岐は、PDU モードを触らない）。 */
                if (CanIf_SetPduMode(0U, CANIF_TX_OFFLINE) != E_OK)
                {
                    DET_LOGE(TAG, "RequestComMode E: CanIf_SetPduMode(TX_OFFLINE) failed during BusOff, request denied");
                    return E_NOT_OK;
                }
            }
            CanSM_PreBusOffState = CANSM_STATE_SILENT_COM;
        }
        else
        {
            return E_NOT_OK;
        }
        DET_LOGI(TAG, "RequestComMode accepted during BusOff recovery: will return to %s",
                 (mode == COMM_FULL_COMMUNICATION) ? "FULL_COM" : "SILENT_COM");
        return E_OK;
    }

    switch (mode)
    {
        case COMM_FULL_COMMUNICATION:
            if (CanSM_State == CANSM_STATE_SILENT_COM)
            {
                /* [SWS_CanSM_00433] E_SILENT_TO_FULL_COM: コントローラは、SILENT_COM でも CAN_CS_STARTED のままなので、
                 * 完了通知を待たずに、PDU チャネルを CANIF_ONLINE へ戻す（TX 抑制解除、[SWS_CANIF_00137]）だけで、
                 * FULL_COM へ戻れる。 */
                CanSM_CompleteFullCom();
            }
            else if (CanSM_State == CANSM_STATE_PRE_NO_COM)
            {
                /* NO_COM へ入る途中。入り終えたあとで、FULL_COM へ向かう（[SWS_CanSM_00182]: 受理して、状態機械が考慮する）。 */
                CanSM_PendingFullRequest = 1U;
            }
            else if ((CanSM_State == CANSM_STATE_FULL_COM) || (CanSM_State == CANSM_STATE_PRE_FULL_COM))
            {
                /* 既に FULL_COM、または向かっている途中。何もしない（要求は受理する）。 */
            }
            else
            {
                /* NO_COM / WAKEUP_VALIDATING から: S_PRE_FULLCOM へ入り、コントローラを STOPPED → STARTED の順に変えて、
                 * 各完了通知を待つ。FULL_COM へ入り終えたら、ComM へ通知する（CanSM_CompleteFullCom()）。 */
                CanSM_EnterPreFullCom();
            }
            break;

        case COMM_SILENT_COMMUNICATION:
            if ((CanSM_State == CANSM_STATE_PRE_FULL_COM) || (CanSM_State == CANSM_STATE_PRE_NO_COM))
            {
                /* コントローラのモードを変えている最中（STARTED ではないかもしれない）。SILENT_COM は、コントローラが
                 * STARTED であることが前提なので、受け付けない。手順は、そのまま続ける（ComM は、FULL_COM に入り終えて
                 * から、Prepare Bus-Sleep のときに、要求する。仕様でも、SILENT_COM へは FULL_COM からしか遷移しない）。 */
                DET_LOGW(TAG, "RequestComMode W: SILENT_COM denied while changing the controller mode (state=%u)",
                         (unsigned)CanSM_State);
                return E_NOT_OK;
            }
            /* 2026-08 変更: 以前はコントローラ全体を Can_SetControllerMode
             * (CAN_T_STOP) で Listen-Only へ落としていたが、実 AUTOSAR の
             * SILENT_COMMUNICATION は PDU チャネル単位の TX 抑制
             * （CanIf_SetPduMode(CANIF_TX_OFFLINE)、[SWS_CANIF_00137]）で
             * 実現するものであり、コントローラ自体は CAN_CS_STARTED の
             * ままでよい（むしろそうあるべき）。そのためコントローラは
             * 一切操作しない。FULL_COM から遷移する場合のみ
             * CanIf_SetPduMode() を呼ぶ（それ以外＝NO_COM/WAKEUP_VALIDATING
             * は既にコントローラ未起動のため、PDU モードは Init 直後の
             * CANIF_OFFLINE のままで構わない）。
             *
             * この変更により CANSM_STATE_SILENT_COM は「コントローラは
             * CAN_CS_STARTED のまま」という新しい不変条件を持つようになり、
             * SILENT_COM 中でも本当に Bus-Off しうる（CanSM_ControllerBusOff()
             * のガード拡張・CanSM_PreBusOffState 参照）。 */
            if (CanSM_State == CANSM_STATE_FULL_COM)
            {
                if (CanIf_SetPduMode(0U, CANIF_TX_OFFLINE) != E_OK)
                {
                    DET_LOGE(TAG, "RequestComMode E: CanIf_SetPduMode(TX_OFFLINE) failed, state unchanged");
                    return E_NOT_OK;
                }
            }
            CanSM_State = CANSM_STATE_SILENT_COM;
            DET_LOGI(TAG, "->SILENT_COM");
            ComM_BusSM_ModeIndication(network, COMM_SILENT_COMMUNICATION);
            break;

        case COMM_NO_COMMUNICATION:
            /* ComM は CanNm が実際に Bus-Sleep Mode へ到達してから初めて本関数を COMM_NO_COMMUNICATION で呼ぶ
             * （ComM_Nm_BusSleepMode() 経由。[SWS_ComM_00133]/[SWS_ComM_00392]、詳細は ComM.c および本ファイル冒頭コメント参照）。
             * そのため呼ばれた時点で「もう眠ってよい」ことが確定している。
             * FULL_COM / SILENT_COM / BUS_OFF / WAKEUP_VALIDATING / PRE_FULL_COM からは、S_PRE_NOCOM へ入り、
             * コントローラを STOPPED → SLEEP の順に変えて、各完了通知を待つ（[SWS_CanSM_00464]〜）。入り終えたら、
             * ComM へ NO_COM を通知する（CanSM_CompleteNoCom()）。
             * 既に NO_COM、または向かっている途中なら、何もしない（FULL_COM の要求が保留されていれば、取り消す）。 */
            if (CanSM_State == CANSM_STATE_NO_COM)
            {
                /* 既に NO_COM（コントローラも、スリープ済み）。何もしない。 */
            }
            else if (CanSM_State == CANSM_STATE_PRE_NO_COM)
            {
                CanSM_PendingFullRequest = 0U;
            }
            else
            {
                CanSM_EnterPreNoCom();
            }
            break;

        default:
            return E_NOT_OK;
    }

    return E_OK;
}

/* ----------------------------------------------------------------------
 * CanSM_GetCurrentComMode
 * ---------------------------------------------------------------------- */

/**
 * \brief   ネットワークの現在の通信モードを取得する。
 *
 * \details Bus-Off 状態は COMM_NO_COMMUNICATION として報告する。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType* mode)
{
    if (!CanSM_Initialized)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_GET_CURRENT_COM_MODE, CANSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (network >= CANSM_CHANNEL_COUNT)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_GET_CURRENT_COM_MODE, CANSM_E_INVALID_NETWORK_HANDLE);
        return E_NOT_OK;
    }

    if (mode == NULL)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_GET_CURRENT_COM_MODE, CANSM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    switch (CanSM_State)
    {
        case CANSM_STATE_FULL_COM:   *mode = COMM_FULL_COMMUNICATION;   break;
        case CANSM_STATE_SILENT_COM: *mode = COMM_SILENT_COMMUNICATION; break;
        case CANSM_STATE_PRE_NO_COM: *mode = CanSM_ModeBeforePreNoCom;  break;   /* NO_COM へ入り終えるまでは、直前のモード */
        default:                     *mode = COMM_NO_COMMUNICATION;     break;
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * CanSM_StartWakeupSource
 * ---------------------------------------------------------------------- */

/**
 * \brief   起床の検証を始める（[SWS_CanSM_00609]）。EcuM から呼ばれる。
 *
 * \details 初期化済みで、ネットワークハンドルが正しければ、E_OK を返し、状態機械の T_START_WAKEUP_SOURCE の
 *          トリガとして扱う（[SWS_CanSM_00616]/[SWS_CanSM_00607]）。トリガが効くのは NO_COM（ボランタリスリープ中）だけで、
 *          それ以外の状態では、何もしない（要求自体は受理する）。
 *          NO_COM のとき、CAN_T_WAKEUP（SLEEP→STOPPED、Listen-Only）だけを実行して CANSM_STATE_WAKEUP_VALIDATING へ入り、
 *          検証タイマを開始する。MCP2515 の WAKIF はノイズでも誤って立ちうるため、この時点では FULL_COM へ確定しない。
 *          FULL_COM へ確定するのは、有効な受信の確認後に ComM が CanSM_RequestComMode(FULL_COM) を呼んだとき（検証成功）、
 *          または CanSM_MainFunction() が検証のタイムアウトを検出して、再スリープするとき（検証失敗）。
 *
 *          以前は、同じ処理が CanSM_ControllerModeIndication() の中にあり、EcuM_CheckWakeup() が
 *          CanSM_ControllerModeIndication(0, CAN_CS_STOPPED) を「起床の合図」として呼んでいた（2026-11 に、本関数へ移した。
 *          仕様では、CanSM_ControllerModeIndication() は、コントローラがそのモードに達した完了通知）。
 *
 * \param[in]  network  対象のネットワークハンドル（0 〜 CANSM_CHANNEL_COUNT-1）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、またはネットワークハンドルが範囲外。
 *
 * \AUTOSARReq     {SWS_CanSM_00609, SWS_CanSM_00607, SWS_CanSM_00616, SWS_CanSM_00617, SWS_CanSM_00612}
 * \ServiceID      {0x11}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_StartWakeupSource(NetworkHandleType network)
{
    if (!CanSM_Initialized)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_START_WAKEUP_SOURCE, CANSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (network >= CANSM_CHANNEL_COUNT)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_START_WAKEUP_SOURCE, CANSM_E_INVALID_NETWORK_HANDLE);
        return E_NOT_OK;
    }

    if (CanSM_State != CANSM_STATE_NO_COM)
    {
        /* トリガが効くのは NO_COM（ボランタリスリープ中）だけ。CANSM_STATE_BUS_OFF は、実 HW をスリープさせないので、
         * そこから起床の検証を始めることは、原理的にない（フェイルセーフとして残す）。 */
        DET_LOGW(TAG, "StartWakeupSource ignored: not in voluntary NO_COM sleep (state=%u)",
                 (unsigned)CanSM_State);
        return E_OK;
    }

    DET_LOGI(TAG, "Wakeup detected -> validating (Listen-Only, waiting for confirmed RX)");
    if (CanIf_SetControllerMode(0U, CAN_CS_STOPPED) != E_OK)  /* CAN_CS_SLEEP -> CAN_CS_STOPPED (Listen-Only) */
    {
        /* 到達しないはずの経路（この分岐に来る時点で CanState==CAN_CS_SLEEP であることは、呼び出し元
         * （Can_MainFunction_Wakeup() → EcuM_CheckWakeup()）側で保証されている）。実際に失敗した場合、コントローラは
         * Listen-Only へ遷移していないため、検証タイマだけ回して確定させるのは危険。CANSM_STATE_NO_COM のまま据え置き、
         * 次のウェイクアップ通知を待つ（2026-08 のレビューで指摘）。 */
        DET_LOGE(TAG, "StartWakeupSource E: CanIf_SetControllerMode(STOPPED) failed, staying in NO_COM");
        return E_OK;
    }
    CanSM_State             = CANSM_STATE_WAKEUP_VALIDATING;
    CanSM_ValidationTimerMs = CanSM_GetNowMs();
    return E_OK;
}

/* ----------------------------------------------------------------------
 * CanSM_StopWakeupSource
 * ---------------------------------------------------------------------- */

/**
 * \brief   起床の検証を止める（[SWS_CanSM_00610]）。EcuM から呼ばれる。
 *
 * \details 初期化済みで、ネットワークハンドルが正しければ、E_OK を返し、状態機械の T_STOP_WAKEUP_SOURCE の
 *          トリガとして扱う（[SWS_CanSM_00622]/[SWS_CanSM_00608]）。トリガが効くのは WAKEUP_VALIDATING（検証中）だけで、
 *          コントローラを再びスリープさせて、NO_COM へ戻る（検証失敗と同じ。CanSM_MainFunction() の検証タイムアウトと
 *          共通の処理）。それ以外の状態では、何もしない（要求自体は受理する）。
 *          本プロジェクトの EcuM は、現在この関数を呼ばない（検証の失敗は、CanSM_MainFunction() の検証タイムアウトが
 *          自分で検出して、再スリープする）。
 *
 * \param[in]  network  対象のネットワークハンドル（0 〜 CANSM_CHANNEL_COUNT-1）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、またはネットワークハンドルが範囲外。
 *
 * \AUTOSARReq     {SWS_CanSM_00610, SWS_CanSM_00608, SWS_CanSM_00618, SWS_CanSM_00619, SWS_CanSM_00620, SWS_CanSM_00622}
 * \ServiceID      {0x12}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_StopWakeupSource(NetworkHandleType network)
{
    if (!CanSM_Initialized)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_STOP_WAKEUP_SOURCE, CANSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (network >= CANSM_CHANNEL_COUNT)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_STOP_WAKEUP_SOURCE, CANSM_E_INVALID_NETWORK_HANDLE);
        return E_NOT_OK;
    }

    if (CanSM_State == CANSM_STATE_WAKEUP_VALIDATING)
    {
        DET_LOGI(TAG, "StopWakeupSource: wakeup validation stopped -> back to SLEEP");
        CanSM_AbortWakeupValidation();
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * CanSM_GetVersionInfo
 * ---------------------------------------------------------------------- */

void CanSM_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    if (VersionInfo == NULL)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_GET_VERSION_INFO, CANSM_E_PARAM_POINTER);
        return;
    }

    VersionInfo->vendorID         = CANSM_VENDOR_ID;
    VersionInfo->moduleID         = CANSM_MODULE_ID;
    VersionInfo->sw_major_version = CANSM_SW_MAJOR_VERSION;
    VersionInfo->sw_minor_version = CANSM_SW_MINOR_VERSION;
    VersionInfo->sw_patch_version = CANSM_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * CanSM_SetBaudrate
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_SetIcomConfiguration
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_SetEcuPassive
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Call-back notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanSM_ControllerBusOff
 * ---------------------------------------------------------------------- */

/**
 * \brief   Bus-Off 通知コールバック（CanIf → CanSM の通知経路）。
 *
 * \details CAN コントローラが Bus-Off 状態を検出したとき CanIf 経由で呼ばれる。
 *          コントローラを即座に停止し、T_REC タイマを起動する。
 *          CanSM_MainFunction が T_REC ms 後にコントローラの再起動を試みる。
 *
 *          受け付けるのは CANSM_STATE_FULL_COM と CANSM_STATE_SILENT_COM の
 *          2 状態（2026-08 変更）。いずれもコントローラが物理的に稼働中
 *          （CAN_CS_STARTED）であり、実際に Bus-Off しうる。SILENT_COM は
 *          CanIf_SetPduMode(CANIF_TX_OFFLINE) で上位層からの新規送信こそ
 *          止めているが、コントローラ自体は通常どおりバスに参加している
 *          （ACK・エラーフレームは出しうる）ため、Bus-Off が起こりえないと
 *          みなすのは誤り（旧: Can_T_STOP による Listen-Only は物理的に
 *          送信しないため Bus-Off しなかったが、この前提が崩れた）。
 *          どちらから来たかは `CanSM_PreBusOffState` に記録し、回復成功時
 *          （CanSM_MainFunction() 参照）に同じ状態へ戻す。
 *          それ以外の状態（NO_COM=未起動、BUS_OFF=回復試行中で二重に入る
 *          必要がない、WAKEUP_VALIDATING=Listen-Only で送信しないため
 *          原理的に Bus-Off しない）は無視する。
 *
 *          SWS_CanSM_00521: 検出直後（回復試行の前）に
 *          `ComM_BusSM_ModeIndication(Network, COMM_SILENT_COMMUNICATION)` を呼び、
 *          ComM のチャネル状態を回復完了まで FULL_COM のまま放置しないようにする。
 *          SILENT_COM は EcuM の RUN 状態を変化させない（`ComM_BusSM_ModeIndication()`
 *          参照）ため、回復試行中も RUN は維持される。
 *
 *          [SWS_CanSM_00522]/[00605]: Bus-Off を検出するたびに、Dem へ
 *          `Dem_SetEventStatus(CAN_BUSOFF, DEM_EVENT_STATUS_PREFAILED)` を報告する。
 *          Dem のデバウンスカウンタが、確定回数（DEM_DEBOUNCE_LIMIT_CAN_BUSOFF
 *          = CANSM_BUSOFF_L1_TO_L2_COUNT + 1）に達した瞬間に、DTC が確定する
 *          （単発の一時的な Bus-Off では確定せず、L1 の回復で戻れば確定しない）。
 *          回復が確定したとき（G_BUS_OFF_PASSIVE）は、PASSED を報告して、
 *          カウンタを戻す（CanSM_MainFunction 参照）。
 *
 *          CanNm 協調スリープ待ち中（ComM が CanNm_NetworkRelease() を送信済みだが
 *          CanNm がまだ Bus-Sleep Mode へ到達していない期間、ComM.c の
 *          ComM_NmReleasePending 参照）に Bus-Off が発生しても、CanSM の
 *          視点では CanSM_State は引き続き CANSM_STATE_FULL_COM のままのため
 *          （このプロジェクトの設計では ComM が CanNm の完了確認を待ってから
 *          しか CanSM へ NO_COM を伝えない）、上記の通常の FULL_COM 経路が
 *          そのまま処理する。回復成功時は常に FULL_COM へ戻すが、CanNm 自身は
 *          Bus-Off 中も独立したタイマで動作し続けているため、既に
 *          Bus-Sleep Mode へ到達済みの可能性がある。その場合の NO_COM の要求
 *          （ComM_Nm_BusSleepMode() 経由）は、Bus-Off の回復中でも CanSM_RequestComMode() が
 *          受け付けて、回復を取り消す（2026-11 変更）ので、取りこぼさない。回復が先に成功して
 *          FULL_COM へ戻った場合に CanNm を誤って再起床させないためのガードは ComM 側
 *          （`ComM_BusSM_ModeIndication()` の FULL_COM 分岐）に実装している。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerBusOff(uint8 ControllerId)
{
    if (!CanSM_Initialized)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_CONTROLLER_BUSOFF, CANSM_E_UNINIT);
        return;
    }

    if (ControllerId != 0U)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_CONTROLLER_BUSOFF, CANSM_E_PARAM_CONTROLLER);
        return;
    }

    /* FULL_COM へ向かう途中で、コントローラが既に STARTED の段階（STARTED の完了通知を待っている間）に、Bus-Off に
     * なることもある。その Bus-Off を落とすと、FULL_COM へ入り終えたあとも、回復が走らない。FULL_COM のときの
     * Bus-Off として扱う（STOPPED の完了通知を待っている間は、コントローラが止まっているので、Bus-Off は起こらない）。 */
    const uint8 startingFullCom = ((CanSM_State == CANSM_STATE_PRE_FULL_COM) && (CanSM_ModeReqTarget == CAN_CS_STARTED)) ? 1U : 0U;
    if ((CanSM_State != CANSM_STATE_FULL_COM) && (CanSM_State != CANSM_STATE_SILENT_COM) && (startingFullCom == 0U))
    {
        return;
    }

    /* 回復成功時にどちらへ戻すかの記録（CanSM_MainFunction() 参照）。
     * SILENT_COM 由来の場合、CanIf 側の PDU モード（CANIF_TX_OFFLINE）は
     * ここでは一切触らない。Can_Write() 自体がこの後コントローラ停止で
     * 拒否するため、TX 抑制の二重化は不要かつ、回復時にわざわざ
     * 再設定しなくて済む（下記コメント・CanSM_MainFunction() 参照）。 */
    CanSM_PreBusOffState = (startingFullCom != 0U) ? CANSM_STATE_FULL_COM : CanSM_State;

    /* 確定条件を満たす前に再度 Bus-Off が起きた。回復は確定しないので、確定待ちを破棄する
     * （回復試行回数は戻さない。[SWS_CanSM_00514]/[00515]） */
    CanSM_BorPassivePending = 0U;

    if (CanIf_SetControllerMode(0U, CAN_CS_STOPPED) != E_OK)
    {
        /* 到達しないはずの経路（CanIf_ControllerBusOff は Can 側が
         * CanState==CAN_CS_STARTED のときしか呼ばないため、CAN_T_STOP が
         * 拒否される SLEEP 中の到達は原理的にない。Can_MainFunction_BusOff()/
         * Can_Write() 参照）。ただし CanIf からは実際に Bus-Off が発生した
         * という事実は既に届いているため、ここで CanSM_State への反映を
         * 諦めると「Bus-Off が起きたのに回復シーケンスが一切起動しない」
         * という finding #1 と同じ症状に逆戻りしてしまう。矛盾を DET へ
         * 記録した上で、届いた Bus-Off 通知の処理そのものは続行する
         * （2026-08 のレビューで指摘）。 */
        DET_LOGE(TAG, "ControllerBusOff E: CanIf_SetControllerMode(STOPPED) failed (state desync)");
    }

    CanSM_State         = CANSM_STATE_BUS_OFF;
    CanSM_BusOffTimerMs = CanSM_GetNowMs();

    /* [SWS_CanSM_00522]/[00605]: Bus-Off のたびに PREFAILED。確定は Dem のデバウンスカウンタが決める。 */
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_PREFAILED);

    ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);

    const uint8 inL2 = (CanSM_BusOffRetries >= CANSM_BUSOFF_L1_TO_L2_COUNT) ? 1U : 0U;
    DET_LOGW(TAG, "BusOff detected! retry=%u (%s) recovery in %lums",
             (unsigned)CanSM_BusOffRetries,
             inL2 ? "L2" : "L1",
             inL2 ? (unsigned long)CANSM_BUSOFF_RECOVERY_L2_MS : (unsigned long)CANSM_BUSOFF_RECOVERY_L1_MS);
}

/* ----------------------------------------------------------------------
 * CanSM_ControllerModeIndication
 * ---------------------------------------------------------------------- */

/**
 * \brief   コントローラモード変化通知コールバック（[SWS_CanSM_00396]）。
 *
 * \details CAN ドライバ（Can_MainFunction_Mode() → CanIf_ControllerModeIndication()）が、コントローラが
 *          要求されたモードへ達したことを知らせるために呼ぶ完了通知。
 *
 *          S_PRE_FULLCOM / S_PRE_NOCOM（[SWS_CanSM_00483]〜[00495]、[SWS_CanSM_00464]〜[00480]）では、いま要求して待っている
 *          モードへ達した通知で、手順を次へ進める（2026-11）。FULL_COM へは STOPPED → STARTED、NO_COM へは STOPPED → SLEEP の順。
 *          それ以外の状態や、待っていないモードの通知では、何もしない。
 *
 *          以前は、この関数が「起床の合図」を兼ねていた（EcuM_CheckWakeup() が CanSM_ControllerModeIndication(0,
 *          CAN_CS_STOPPED) を呼んで、NO_COM から WAKEUP_VALIDATING へ進めていた）。仕様では、起床の検証は、EcuM が呼ぶ
 *          CanSM_StartWakeupSource() で始める（[SWS_CanSM_00607]）ので、その処理を移した。
 *
 * \param[in]  ControllerId   モードが変化したコントローラ ID（0 のみ）。
 * \param[in]  ControllerMode 通知されたコントローラモード。
 *
 * \note    実仕様の \Reentrancy は「Reentrant (only for different CAN controllers)」だが、本実装は共有 static 変数を
 *          排他制御なしに読み書きする兄弟関数（`CanSM_ControllerBusOff`/`CanSM_RequestComMode` 等、いずれも
 *          `Non Reentrant`）と同様に `Non Reentrant` のまま扱う（単一コントローラ構成で、ControllerId!=0 を DET 拒否する
 *          ため、「異なるコントローラ間」のケース自体が到達しない）。
 *
 * \AUTOSARReq     {SWS_CanSM_00396, SWS_CanSM_00397, SWS_CanSM_00398}
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    if (!CanSM_Initialized)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_CONTROLLER_MODE_INDICATION, CANSM_E_UNINIT);
        return;
    }

    if (ControllerId != 0U)
    {
        /* [SWS_CanSM_00397] */
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_CONTROLLER_MODE_INDICATION, CANSM_E_PARAM_CONTROLLER);
        return;
    }

    DET_LOGD(TAG, "ControllerModeIndication ch=%u mode=%u", (unsigned)ControllerId, (unsigned)ControllerMode);

    /* S_PRE_FULLCOM / S_PRE_NOCOM で、いま待っているモードへ達した通知だけが、手順を進める
     * （それ以外の状態や、待っていないモードの通知は、無視する）。 */
    if (((CanSM_State == CANSM_STATE_PRE_FULL_COM) || (CanSM_State == CANSM_STATE_PRE_NO_COM))
        && (ControllerMode == CanSM_ModeReqTarget))
    {
        CanSM_AdvanceModeSequence();
    }
}

/* ----------------------------------------------------------------------
 * CanSM_TransceiverModeIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_TxTimeoutException
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_ClearTrcvWufFlagIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_CheckTransceiverWakeFlagIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_ConfirmPnAvailability
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_CurrentIcomConfiguration
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanSM_MainFunction
 * ---------------------------------------------------------------------- */

/**
 * \brief   CanSM 周期処理（Bus-Off 回復タイマ管理）。
 *
 * \details Bus-Off 状態のとき、L1/L2 のいずれかの周期（下記）が経過すると
 *          コントローラの再起動を試みる。再起動できた時点では、まだ回復は確定しない。
 *          確定条件（G_BUS_OFF_PASSIVE、CanSM_BorPassiveGuardMet() 参照）を満たしたとき
 *          （[SWS_CanSM_00496]〜[00498]）、はじめて
 *          Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, PASSED) を報告し、
 *          回復試行回数を 0 へ戻す。確定する前に再度 Bus-Off が発生すると
 *          CanSM_ControllerBusOff() が呼ばれ、回数を持ち越したまま次の回復試行へ進む
 *          （単発の Bus-Off が起動後に累積して、突然 L2 になることはない）。
 *          各リトライ試行そのものの時刻を基準点として毎回更新するため、
 *          `CanIf_SetControllerMode(CAN_CS_STARTED)` が失敗して BUS_OFF に
 *          据え置かれた場合でも、次の試行は L1/L2 周期を空けてから行われる
 *          （2026-09 是正、下記本体コメント参照）。
 *
 *          L1/L2 バックオフ（SWS_CanSM_00514/00515 準拠）:
 *            試行回数 <= CANSM_BUSOFF_L1_TO_L2_COUNT の間は
 *            CANSM_BUSOFF_RECOVERY_L1_MS（短い周期）でリトライする。
 *            この回数を超えたら、一時的なバス障害ではなく持続的な Bus-Off と
 *            判断し、以降は CANSM_BUSOFF_RECOVERY_L2_MS
 *            （長い周期）でリトライを継続する（DTC の確定は、この時点の Bus-Off の
 *            検出で、Dem のデバウンスが行う。CanSM_ControllerBusOff() 参照）。
 *            AUTOSAR 仕様には「回復を諦めて
 *            二度と復帰しない」状態は存在しないため、L2 に切り替わった後も
 *            回復試行そのものは無期限に続ける。
 *
 *          ウェイクアップ検証中 (CANSM_STATE_WAKEUP_VALIDATING) は、
 *          CANSM_WAKEUP_VALIDATION_MS 以内に有効な受信による
 *          検証成功（CanSM_RequestComMode(FULL_COM)）がなければタイムアウトと判断し、ノイズによる誤ウェイクアップ
 *          とみなして Can_T_SLEEP で再びスリープへ戻す。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_MainFunction(void)
{
    if (!CanSM_Initialized)
    {
        (void)Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_MAIN_FUNCTION, CANSM_E_UNINIT);
        return;
    }

    /* [SWS_CanSM_00496]/[00497]: 回復の再起動後、確定条件（G_BUS_OFF_PASSIVE）を満たしたら、回復が確定したとして
     * Dem へ PASSED を報告し（[SWS_CanSM_00498]）、回復試行回数を 0 へ戻す（[SWS_CanSM_00514]/[00515] の回数は
     * 「G_BUS_OFF_PASSIVE を通らずに繰り返した回数」）。 */
    if ((CanSM_BorPassivePending != 0U)
        && ((CanSM_State == CANSM_STATE_FULL_COM) || (CanSM_State == CANSM_STATE_SILENT_COM))
        && CanSM_BorPassiveGuardMet())
    {
        CanSM_BorPassivePending = 0U;
        CanSM_BusOffRetries     = 0U;
        DET_LOGI(TAG, "BusOff recovery confirmed (G_BUS_OFF_PASSIVE), retries reset");
        (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
    }

    if ((CanSM_State == CANSM_STATE_PRE_FULL_COM) || (CanSM_State == CANSM_STATE_PRE_NO_COM))
    {
        /* 完了通知を待っている間、CANSM_MODEREQ_REPEAT_TIME_MS ごとに要求を繰り返す（[SWS_CanSM_00467]/[00471]/[00490]/[00494]） */
        CanSM_ServiceModeRequest();
        return;
    }

    if (CanSM_State == CANSM_STATE_WAKEUP_VALIDATING)
    {
        if ((CanSM_GetNowMs() - CanSM_ValidationTimerMs) >= CANSM_WAKEUP_VALIDATION_MS)
        {
            DET_LOGW(TAG, "Wakeup validation timeout (%lums, no confirmed RX) -> back to SLEEP",
                     (unsigned long)CANSM_WAKEUP_VALIDATION_MS);
            CanSM_AbortWakeupValidation();
        }
        return;
    }

    if (CanSM_State != CANSM_STATE_BUS_OFF)
    {
        return;
    }

    const uint8 inL2 = (CanSM_BusOffRetries >= CANSM_BUSOFF_L1_TO_L2_COUNT) ? 1U : 0U;
    const unsigned long interval = inL2 ? (unsigned long)CANSM_BUSOFF_RECOVERY_L2_MS
                                         : (unsigned long)CANSM_BUSOFF_RECOVERY_L1_MS;

    if ((CanSM_GetNowMs() - CanSM_BusOffTimerMs) < interval)
    {
        return;
    }

    /* L1/L2 周期経過: 回復試行。この試行自体の時刻を基準点として更新する
     * （2026-09 是正: 以前は本行が無く、CanSM_BusOffTimerMs が
     * CanSM_ControllerBusOff() 検出時の1回しか更新されなかった。
     * 下記 CanIf_SetControllerMode() が失敗した場合に return するだけで
     * 基準点を更新していなかったため、失敗後は次回以降の
     * CanSM_MainFunction() 呼び出しのたびに毎回リトライ条件を満たし続け、
     * L1/L2 バックオフのレート制限が実質効かなくなっていた
     * （native_chain_wrap 試作テストで偶然発見）。失敗・成功のいずれでも
     * 「この時点で1回試行した」という事実は変わらないため、成否判定より
     * 前でまとめて更新する。
     * 仕様（[SWS_CanSM_00514]/[00515]）の基準点は厳密には「直近の確定
     * Bus-Off発生時刻(T_BUS_OFF)」であり「リトライ試行を行った瞬間」とは
     * 概念上異なるが、本プロジェクトは単一コントローラ・同期的な
     * CanIf_SetControllerMode() 呼び出しのみのため、試行失敗は実質即座の
     * 再Bus-Off相当とみなせ、両者は事実上一致する。自己spec-citation検証で
     * 確認済みの意図的な近似）。 */
    /* 確定しないまま繰り返すと回数は増え続けるので、uint8 の上限で止める（折り返して 0 に戻ると、
     * 持続的な Bus-Off なのに L1 の間隔へ戻り、DTC の FAILED を重ねて報告してしまう）。 */
    if (CanSM_BusOffRetries < 0xFFU)
    {
        CanSM_BusOffRetries++;
    }
    CanSM_BusOffTimerMs = CanSM_GetNowMs();

    if (CanSM_BusOffRetries == (CANSM_BUSOFF_L1_TO_L2_COUNT + 1U))
    {
        /* L1→L2 に降格するちょうどこの瞬間: 一時的なバス障害ではなく持続的な
         * Bus-Off と判断する。DTC は、Bus-Off 検出ごとの PREFAILED が、
         * Dem のデバウンスで確定済み（DEM_DEBOUNCE_LIMIT_CAN_BUSOFF）。
         * 回復試行そのものは止めない（下へ続く）。 */
        DET_LOGE(TAG, "BusOff: L1(%u) exceeded, degrade to L2 (%lums)",
                 (unsigned)CANSM_BUSOFF_L1_TO_L2_COUNT, (unsigned long)CANSM_BUSOFF_RECOVERY_L2_MS);
    }

    DET_LOGI(TAG, "BusOff: restart attempt %u (%s, next in %lums)",
             (unsigned)CanSM_BusOffRetries, inL2 ? "L2" : "L1", interval);

    if (CanIf_SetControllerMode(0U, CAN_CS_STARTED) != E_OK)
    {
        /* 到達しないはずの経路（BUS_OFF 中は CanState==CAN_CS_STOPPED の
         * はず、CanSM_ControllerBusOff() が Bus-Off 検出時に CAN_T_STOP
         * 済み）。失敗した場合に回復成功とみなして CanSM_State を進めると、
         * CanSM/ComM/EcuM が「復帰した」と誤認したまま MCP2515 は止まった
         * ままになる。CanSM_BusOffRetries は既にインクリメント済みのため、
         * 状態は BUS_OFF のまま据え置き、次の L1/L2 周期で再試行させる
         * （通常の回復失敗と同じ扱い。2026-08 のレビューで指摘）。 */
        DET_LOGE(TAG, "MainFunction E: CanIf_SetControllerMode(STARTED) failed during BusOff recovery, retry next cycle");
        return;
    }
    /* コントローラは再起動できたが、回復の確定はまだ（[SWS_CanSM_00496]〜[00498]）。確定条件（G_BUS_OFF_PASSIVE）を
     * 満たすまで、Dem へ PASSED は報告せず、回復試行回数も戻さない（上の判定ブロック参照）。確定する前に再度
     * Bus-Off が起きれば、回数を持ち越して次の回復試行へ進む。 */
    CanSM_BorPassivePending = 1U;
    CanSM_BorRestartMs      = CanSM_GetNowMs();

    /* 2026-08 変更: Bus-Off は CANSM_STATE_FULL_COM だけでなく
     * CANSM_STATE_SILENT_COM からも起こりうるようになったため
     * （CanSM_ControllerBusOff() 参照）、回復成功時は Bus-Off 発生直前の
     * 状態（CanSM_PreBusOffState）へ戻す。Bus-Off 発生時点で ComM が既に
     * CanNm へ解放を送信済み（ComM_NmReleasePending、ComM.c 参照）だった場合の
     * 「誤って CanNm を再起床させない」ためのガードは ComM 側
     * （ComM_BusSM_ModeIndication() の FULL_COM 分岐。何もしないで、CanNm の協調スリープを
     * そのまま続けさせる）に実装している。 */
    if (CanSM_PreBusOffState == CANSM_STATE_SILENT_COM)
    {
        /* CanIf の PDU モードは Bus-Off 中も CANIF_TX_OFFLINE のまま
         * 触っていない（CanSM_ControllerBusOff() 参照）ため、再設定不要で
         * そのまま SILENT_COM へ戻ってよい。 */
        CanSM_State = CANSM_STATE_SILENT_COM;
        DET_LOGI(TAG, "BusOff recovered -> SILENT_COM (was silent before BusOff)");
        ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);
    }
    else
    {
        CanSM_SetPduModeOnlineBestEffort("MainFunction");
        CanSM_State = CANSM_STATE_FULL_COM;
        /* CAN ツールの「ECU 状態」パネルが、このログから CanSM の状態を FULL_COM へ戻す
         * （SILENT_COM へ戻る分岐のログと対）。 */
        DET_LOGI(TAG, "BusOff recovered -> FULL_COM");
        /* 回復成功 → ComM に FULL_COM を通知 → EcuM_RequestRUN → RUN へ戻る
         * （ただし上記の CanNm 解放ペンディング中だった場合は、ComM 側は EcuM の RUN も
         * CanNm のネットワーク要求も触らず、CanNm の協調スリープを続けさせる。CanNm が
         * Bus-Sleep Mode へ到達すれば、ComM_Nm_BusSleepMode() が CanSM_RequestComMode(NO_COM) を呼ぶ）。 */
        ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
    }
    /* 再度 Bus-Off が発生すれば CanIf → CanSM_ControllerBusOff() が呼ばれる */
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanSM_GetControllerModeOrUnknown
 * ---------------------------------------------------------------------- */

/**
 * \brief   コントローラの現在のモードを取る。取れなければ CAN_CS_UNINIT（不明）を返す。
 *
 * \details S_PRE_FULLCOM / S_PRE_NOCOM が、最初の要求を選ぶために使う（現在のモードと違うときだけ要求する、
 *          [SWS_CanSM_00487]/[SWS_CanSM_00464]）。不明のときは、最初から（STOPPED から）始める。
 *
 * \return  CanIf_GetControllerMode() が返した現在のモード。取れなければ CAN_CS_UNINIT。
 */
static Can_ControllerStateType CanSM_GetControllerModeOrUnknown(void)
{
    Can_ControllerStateType current = CAN_CS_UNINIT;
    if (CanIf_GetControllerMode(0U, &current) != E_OK)
    {
        current = CAN_CS_UNINIT;
    }
    return current;
}

/* ----------------------------------------------------------------------
 * CanSM_IssueModeRequest
 * ---------------------------------------------------------------------- */

/**
 * \brief   コントローラのモード変更を要求し、完了通知を待つ状態にする（DO_SET_CC_MODE_xxx、[SWS_CanSM_00464]〜）。
 *
 * \details 待つモード（CanSM_ModeReqTarget）と、要求の時刻（CanSM_ModeReqTimerMs）を記録してから、
 *          CanIf_SetControllerMode() を呼ぶ。戻り値が E_OK でなくても、状態は変えない。CANSM_MODEREQ_REPEAT_TIME_MS が
 *          過ぎても、完了通知が来なければ、CanSM_MainFunction() が、要求を繰り返す
 *          （[SWS_CanSM_00495]: E_OK を得られず、通知も得られないまま、繰り返した回数が上限を超えたら T_REPEAT_MAX）。
 *
 * \param[in]  target  要求するコントローラのモード（CAN_CS_STOPPED / CAN_CS_STARTED / CAN_CS_SLEEP）。
 */
static void CanSM_IssueModeRequest(Can_ControllerStateType target)
{
    CanSM_ModeReqTarget  = target;
    CanSM_ModeReqTimerMs = CanSM_GetNowMs();
    if (CanIf_SetControllerMode(0U, target) != E_OK)
    {
        DET_LOGW(TAG, "ModeRequest W: CanIf_SetControllerMode(%u) failed, will repeat", (unsigned)target);
    }
}

/* ----------------------------------------------------------------------
 * CanSM_EnterPreFullCom
 * ---------------------------------------------------------------------- */

/**
 * \brief   S_PRE_FULLCOM へ入り、FULL_COM へ向かう手順を始める（[SWS_CanSM_00487]/[00491]）。
 *
 * \details コントローラを STOPPED → STARTED の順に変える。現在のモードが STOPPED なら、STOPPED の要求は省く
 *          （[SWS_CanSM_00487]: 現在のモードと違うときだけ要求する）。各要求のあと、完了通知を待つ
 *          （CanSM_ControllerModeIndication() が CanSM_AdvanceModeSequence() を呼ぶ）。
 */
static void CanSM_EnterPreFullCom(void)
{
    Can_ControllerStateType current;
    CanSM_State              = CANSM_STATE_PRE_FULL_COM;
    CanSM_ModeReqRepeats     = 0U;
    CanSM_BorPassivePending  = 0U;   /* 新しく FULL_COM へ入り直すので、前の回復の確定待ちは破棄する */
    DET_LOGI(TAG, "->PRE_FULL_COM");

    current = CanSM_GetControllerModeOrUnknown();

    if (current == CAN_CS_STARTED)
    {
        CanSM_CompleteFullCom();   /* 既に STARTED。完了通知は来ないので、待たずに、入り終える */
    }
    else if (current == CAN_CS_STOPPED)
    {
        CanSM_IssueModeRequest(CAN_CS_STARTED);
    }
    else
    {
        CanSM_IssueModeRequest(CAN_CS_STOPPED);
    }
}

/* ----------------------------------------------------------------------
 * CanSM_EnterPreNoCom
 * ---------------------------------------------------------------------- */

/**
 * \brief   S_PRE_NOCOM へ入り、NO_COM へ向かう手順を始める（[SWS_CanSM_00464]/[00468]）。
 *
 * \details コントローラを STOPPED → SLEEP の順に変える。現在のモードが STOPPED なら、STOPPED の要求は省く
 *          （Bus-Off の回復中や、起床の検証中は、コントローラが既に STOPPED）。各要求のあと、完了通知を待つ。
 *          FULL_COM の要求の保留は、取り消さない（CanSM_RequestComMode() が、状況に応じて、立てる）。
 */
static void CanSM_EnterPreNoCom(void)
{
    Can_ControllerStateType current;

    /* NO_COM へ入り終えるまで、CanSM_GetCurrentComMode() が返す、直前のネットワークのモードを記録する
     * （PRE_FULL_COM / PRE_NO_COM / NO_COM 等からは、NO_COM とみなす） */
    if (CanSM_State == CANSM_STATE_FULL_COM)
    {
        CanSM_ModeBeforePreNoCom = COMM_FULL_COMMUNICATION;
    }
    else if (CanSM_State == CANSM_STATE_SILENT_COM)
    {
        CanSM_ModeBeforePreNoCom = COMM_SILENT_COMMUNICATION;
    }
    else if (CanSM_State != CANSM_STATE_PRE_NO_COM)
    {
        CanSM_ModeBeforePreNoCom = COMM_NO_COMMUNICATION;
    }
    else
    {
        /* PRE_NO_COM のまま（再入）。記録は、そのまま */
    }

    CanSM_State              = CANSM_STATE_PRE_NO_COM;
    CanSM_ModeReqRepeats     = 0U;
    CanSM_BorPassivePending  = 0U;   /* 眠るので、前の回復の確定待ちは破棄する */
    DET_LOGI(TAG, "->PRE_NO_COM");

    current = CanSM_GetControllerModeOrUnknown();

    if (current == CAN_CS_SLEEP)
    {
        CanSM_CompleteNoCom();   /* 既に SLEEP。完了通知は来ないので、待たずに、入り終える（起こして、また寝かせない） */
    }
    else if (current == CAN_CS_STOPPED)
    {
        CanSM_IssueModeRequest(CAN_CS_SLEEP);
    }
    else
    {
        CanSM_IssueModeRequest(CAN_CS_STOPPED);
    }
}

/* ----------------------------------------------------------------------
 * CanSM_CompleteFullCom
 * ---------------------------------------------------------------------- */

/**
 * \brief   FULL_COM へ入り終える（E_FULL_COM、[SWS_CanSM_00432]/[SWS_CanSM_00518]）。
 *
 * \details PDU チャネルを CANIF_ONLINE にし（[SWS_CANIF_00137]。TX 抑制の解除。NO_COM / WAKEUP_VALIDATING から来た場合も、
 *          無条件で ONLINE にしてよい）、CanSM_State を FULL_COM にして、ComM へ FULL_COM を通知する。
 *          Bus-Off の回復試行回数と、回復の確定待ちは、戻す（新しく FULL_COM へ入り直すため）。
 */
static void CanSM_CompleteFullCom(void)
{
    CanSM_SetPduModeOnlineBestEffort("CompleteFullCom");
    CanSM_State             = CANSM_STATE_FULL_COM;
    CanSM_BusOffRetries     = 0U;
    CanSM_BorPassivePending = 0U;
    DET_LOGI(TAG, "->FULL_COM");
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
}

/* ----------------------------------------------------------------------
 * CanSM_CompleteNoCom
 * ---------------------------------------------------------------------- */

/**
 * \brief   NO_COM へ入り終える（E_NOCOM、[SWS_CanSM_00430]）。
 *
 * \details CanSM_State を NO_COM にして、ComM へ NO_COM を通知する。NO_COM へ入る途中に FULL_COM が要求されて
 *          いたら（CanSM_PendingFullRequest）、通知のあとで、FULL_COM へ向かう手順を始める。
 */
static void CanSM_CompleteNoCom(void)
{
    CanSM_State             = CANSM_STATE_NO_COM;
    CanSM_BorPassivePending = 0U;
    DET_LOGI(TAG, "->NO_COM (physical sleep)");
    ComM_BusSM_ModeIndication(0U, COMM_NO_COMMUNICATION);

    if (CanSM_PendingFullRequest != 0U)
    {
        CanSM_PendingFullRequest = 0U;
        CanSM_EnterPreFullCom();
    }
}

/* ----------------------------------------------------------------------
 * CanSM_AdvanceModeSequence
 * ---------------------------------------------------------------------- */

/**
 * \brief   待っていたモード変更の完了通知を受け取ったので、手順を次へ進める。
 *
 * \details S_PRE_FULLCOM: STOPPED の完了 → STARTED を要求。STARTED の完了 → FULL_COM へ入り終える。
 *          S_PRE_NOCOM: STOPPED の完了 → SLEEP を要求。SLEEP の完了 → NO_COM へ入り終える。
 */
static void CanSM_AdvanceModeSequence(void)
{
    CanSM_ModeReqRepeats = 0U;

    if (CanSM_State == CANSM_STATE_PRE_FULL_COM)
    {
        if (CanSM_ModeReqTarget == CAN_CS_STOPPED)
        {
            CanSM_IssueModeRequest(CAN_CS_STARTED);
        }
        else
        {
            CanSM_CompleteFullCom();
        }
    }
    else
    {
        if (CanSM_ModeReqTarget == CAN_CS_STOPPED)
        {
            CanSM_IssueModeRequest(CAN_CS_SLEEP);
        }
        else
        {
            CanSM_CompleteNoCom();
        }
    }
}

/* ----------------------------------------------------------------------
 * CanSM_ServiceModeRequest
 * ---------------------------------------------------------------------- */

/**
 * \brief   完了通知の待ちの時間切れを監視して、要求を繰り返す（CanSM_MainFunction() から）。
 *
 * \details 要求の時刻から CANSM_MODEREQ_REPEAT_TIME_MS が過ぎたら、繰り返し回数を数えて、同じ要求をもう一度出す
 *          （[SWS_CanSM_00490]/[00494]/[00467]/[00471] の T_xxx_TIMEOUT）。回数が CANSM_MODEREQ_REPEAT_MAX を超えたら、
 *          T_REPEAT_MAX（[SWS_CanSM_00495]/[00480]）: S_PRE_FULLCOM からは、S_PRE_NOCOM へ戻り（E_PRE_NOCOM）、
 *          S_PRE_NOCOM からは、あきらめて NO_COM へ入り終える（E_NOCOM）。
 */
static void CanSM_ServiceModeRequest(void)
{
    if ((CanSM_GetNowMs() - CanSM_ModeReqTimerMs) < CANSM_MODEREQ_REPEAT_TIME_MS)
    {
        return;
    }

    CanSM_ModeReqRepeats++;
    if (CanSM_ModeReqRepeats > CANSM_MODEREQ_REPEAT_MAX)
    {
        DET_LOGE(TAG, "ModeRequest E: no controller mode indication after %u repeats (T_REPEAT_MAX, state=%u)",
                 (unsigned)CANSM_MODEREQ_REPEAT_MAX, (unsigned)CanSM_State);
        if (CanSM_State == CANSM_STATE_PRE_FULL_COM)
        {
            CanSM_EnterPreNoCom();
        }
        else
        {
            CanSM_CompleteNoCom();
        }
        return;
    }

    DET_LOGW(TAG, "ModeRequest W: no controller mode indication yet, repeat %u/%u (state=%u)",
             (unsigned)CanSM_ModeReqRepeats, (unsigned)CANSM_MODEREQ_REPEAT_MAX, (unsigned)CanSM_State);
    CanSM_IssueModeRequest(CanSM_ModeReqTarget);
}

/* ----------------------------------------------------------------------
 * CanSM_AbortWakeupValidation
 * ---------------------------------------------------------------------- */

/**
 * \brief   起床の検証を打ち切り、コントローラを再スリープさせて、NO_COM へ戻す。
 *
 * \details 検証のタイムアウト（CanSM_MainFunction()）と、CanSM_StopWakeupSource() に共通の処理。
 *          CAN_T_SLEEP は失敗しない（CanSM_RequestComMode() の COMM_NO_COMMUNICATION 分岐のコメント参照）。
 *          戻り値は、明示的に無視する。ウェイクアップ割り込みは、再武装される。
 */
static void CanSM_AbortWakeupValidation(void)
{
    (void)CanIf_SetControllerMode(0U, CAN_CS_SLEEP);  /* CAN_CS_STOPPED -> CAN_CS_SLEEP、ウェイクアップ割り込み再武装 */
    CanSM_State             = CANSM_STATE_NO_COM;
    CanSM_BorPassivePending = 0U;
}

/* ----------------------------------------------------------------------
 * CanSM_GetNowMs
 * ---------------------------------------------------------------------- */

/**
 * \brief   現在時刻 [ms] を返す（Os カウンタ SYSTEM_COUNTER の tick 値）。
 * \details millis() を直接呼ばず Os の GetCounterValue() を介する。値は Os_Init() の
 *          前後や millis() フォールバックの前後でも飛ばない（Os.h 参照）ため、
 *          本モジュールが保持する「前回時刻」との差分計算にそのまま使える。
 */
static unsigned long CanSM_GetNowMs(void)
{
    TickType now = 0U;
    (void)GetCounterValue(SYSTEM_COUNTER, &now);
    return (unsigned long)now;
}

/* ----------------------------------------------------------------------
 * CanSM_BorPassiveGuardMet
 * ---------------------------------------------------------------------- */

/**
 * \brief   Bus-Off 回復の確定条件 G_BUS_OFF_PASSIVE を満たしたかを返す。
 *
 * \details CANSM_BOR_TX_CONFIRMATION_POLLING が有効で、FULL_COM に戻っているなら、再起動後に
 *          CanIf_GetTxConfirmationState() が CANIF_TX_RX_NOTIFICATION（再起動後に TX 確認が 1 回でもあった）を
 *          返したときに満たす（[SWS_CanSM_00497]）。それ以外は、再起動から CANSM_BOR_TIME_TX_ENSURED_MS 以上
 *          経過したときに満たす（[SWS_CanSM_00496]）。無効のときはもちろん、SILENT_COM に戻った場合も時間で判定する
 *          （SILENT_COM は CanIf 層で TX を抑制しており、TX 確認が得られないため）。TX 確認の状態は、コントローラを
 *          起動し直すたびに CanIf がリセットする（[SWS_CANIF_00734]）ので、回復の再起動より前の確認は数えない。
 *
 * \return  TRUE=確定条件を満たした。FALSE=まだ満たしていない。
 */
static boolean CanSM_BorPassiveGuardMet(void)
{
#if (CANSM_BOR_TX_CONFIRMATION_POLLING != 0U)
    if (CanSM_State == CANSM_STATE_FULL_COM)
    {
        return (CanIf_GetTxConfirmationState(0U) == CANIF_TX_RX_NOTIFICATION) ? TRUE : FALSE;
    }
#endif
    return ((CanSM_GetNowMs() - CanSM_BorRestartMs) >= CANSM_BOR_TIME_TX_ENSURED_MS) ? TRUE : FALSE;
}

/* ----------------------------------------------------------------------
 * CanSM_SetPduModeOnlineBestEffort
 * ---------------------------------------------------------------------- */

/**
 * \brief   FULL_COM 確定直前に CanIf の PDU モードを CANIF_ONLINE へ戻す
 *          （ベストエフォート）。
 *
 * \details CanIf_SetPduMode() は本プロジェクトでは ControllerId=0/
 *          CANIF_ONLINE のいずれも常に妥当なため実質失敗しない（到達しない
 *          はず）。万一失敗しても、呼び出し元は既にコントローラが物理的に
 *          稼働していることを確認済みの状態でこれを呼ぶため、ここで状態遷移
 *          そのものを諦めると CanSM/ComM/EcuM が「まだ FULL_COM でない」と
 *          誤認したまま実際には動いているコントローラを放置することになり、
 *          かえって実害が大きい。DET のみ記録して呼び出し元は続行する
 *          （2026-08 のレビュー方針を踏襲。CanSM_RequestComMode()/
 *          CanSM_MainFunction() の 2 箇所から呼ぶ）。
 *
 * \param[in]  callerTag  DET ログに残す呼び出し元の名前（例: "RequestComMode"）。
 */
static void CanSM_SetPduModeOnlineBestEffort(const char* callerTag)
{
    if (CanIf_SetPduMode(0U, CANIF_ONLINE) != E_OK)
    {
        DET_LOGE(TAG, "%s E: CanIf_SetPduMode(ONLINE) failed, proceeding anyway", callerTag);
    }
}
