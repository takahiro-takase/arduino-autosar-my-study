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
 *              ↓ RequestComMode(NO_COM) → CAN_T_SLEEP        │ │
 *              └───────────────────────────────────────────────┘
 *              ↓ CanSM_ControllerBusOff()                    │ 回復成功
 *            CANSM_STATE_BUS_OFF                             │
 *              ↓ L1/L2 周期経過 (MainFunction)                │
 *              → Can_SetControllerMode(CAN_T_START) ─────────┘
 *              （回復に失敗すれば CanSM_ControllerBusOff() が再度呼ばれ、
 *                リトライを継続する。無期限に諦めない）
 *
 *          Bus-Off 回復シーケンス（SWS_CanSM_00514/00515/00636 準拠、
 *          CanSMEnableBusOffDelay=FALSE 相当の L1/L2 バックオフ）:
 *            1. Bus-Off 検出 → コントローラ停止・タイマ起動
 *            2. CANSM_BUSOFF_RECOVERY_L1_MS（短い周期）待機（CanSM_MainFunction が監視）
 *            3. コントローラ再起動 → FULL_COM に復帰 → Dem へ PASSED 報告
 *            4. 再度 Bus-Off が発生した場合は試行回数をカウント
 *            5. 試行回数が CANSM_BUSOFF_L1_TO_L2_COUNT を超えたら、一時的な
 *               バス障害ではなく持続的な Bus-Off と判断し、Dem へ FAILED 報告
 *               (DEM_EVENT_CAN_BUSOFF)。Dem 側は DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1
 *               のため即座に確定する。以降はリトライ周期を長い
 *               CANSM_BUSOFF_RECOVERY_L2_MS へ切り替えるだけで、回復試行その
 *               ものは無期限に継続する（AUTOSAR 仕様には「回復を諦めて二度と
 *               復帰しない」状態は存在しない。一時的なバス障害が長引いただけで
 *               実機リセットが必要になることを避けるため、本実装も仕様通り
 *               無期限リトライとした）。
 *
 *          正常系（ボランタリ）スリープとウェイクアップ:
 *            App_EngineManager が「エンジン OFF が一定サイクル継続 = 通信不要」と
 *            判断すると ComM_RequestComMode(COMM_USER_0, NO_COM) を要求する。
 *            Dcm（COMM_USER_1）も extendedSession でなければ ComM の集約結果が
 *            NO_COM になり、CanSM_RequestComMode(NO_COM) が呼ばれて
 *            CANSM_STATE_NO_COM へ遷移し、実際に Can_SetControllerMode(CAN_T_SLEEP)
 *            で MCP2515 をスリープさせる。CANSM_STATE_BUS_OFF は実 HW を
 *            スリープさせないため、これが本モジュールで唯一 HW を実際に
 *            スリープさせる経路である。
 *
 *          ウェイクアップ検証（Wakeup Validation、AUTOSAR EcuM の
 *          Wakeup Validation Protocol に相当）:
 *            MCP2515 は電気的ノイズ等でも WAKIF を誤って立てうるため、
 *            INT ピンのアサートを検知しただけで即座に FULL_COM へ復帰せず、
 *            「本当に有効な CAN フレームを受信できたか」を確認してから
 *            復帰する 2 段階の手順を踏む。
 *              1. Can_Isr()（真の割り込み）が SLEEP 中の INT アサートを検出
 *                 → Can_MainFunction_Wakeup() が CanIf_ControllerWakeup() を呼ぶ
 *                 → CanSM_ControllerWakeup()
 *                 → CAN_T_WAKEUP のみ実行（SLEEP→STOPPED、Listen-Only）。
 *                   CANSM_STATE_WAKEUP_VALIDATING へ遷移し、検証タイマ開始。
 *                   ComM/EcuM へはまだ何も通知しない（この時点ではノイズの
 *                   可能性を否定できないため）。
 *              2a. 検証タイマ (CANSM_WAKEUP_VALIDATION_MS) 内に何らかの
 *                  CAN フレームを正常受信 → CanIf_RxIndication() 経由で
 *                  CanSM_RxIndication() が呼ばれ、検証成功と判断して
 *                  CAN_T_START → CANSM_STATE_FULL_COM へ確定し、
 *                  ComM_BusSMIndication(FULL_COM) で EcuM を RUN へ復帰させる。
 *              2b. 検証タイマが切れても何も受信できなければ、CanSM_MainFunction()
 *                  がノイズによる誤ウェイクアップと判断し、Can_T_SLEEP で
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

#include "CanSM.h"
#include "Can.h"
#include "Dem.h"
#include "Det.h"

#define TAG "CanSM"

/* Arduino wiring.c（C リンケージ）で定義 */
extern unsigned long millis(void);

/* -----------------------------------------------------------------------
 * 内部型定義
 * ----------------------------------------------------------------------- */
typedef enum
{
    CANSM_STATE_NO_COM,              /* 通信停止（スリープ含む） */
    CANSM_STATE_SILENT_COM,          /* 受信専用 */
    CANSM_STATE_FULL_COM,            /* 全二重通信（正常動作） */
    CANSM_STATE_BUS_OFF,             /* Bus-Off 回復中 */
    CANSM_STATE_WAKEUP_VALIDATING    /* ウェイクアップ検証中（Listen-Only、RX確認待ち） */
} CanSM_InternalStateType;

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */
static CanSM_InternalStateType CanSM_State;
static unsigned long           CanSM_BusOffTimerMs;   /* Bus-Off 検出時刻 (直近のリトライ基準点) */
static uint8                   CanSM_BusOffRetries;   /* 回復試行回数 (L1→L2 切替判定にも使う) */
static unsigned long           CanSM_ValidationTimerMs; /* ウェイクアップ検証開始時刻 */

/**
 * \brief   CanSM モジュールを初期化する。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_Init(void)
{
    CanSM_State             = CANSM_STATE_NO_COM;
    CanSM_BusOffTimerMs     = 0UL;
    CanSM_BusOffRetries     = 0U;
    CanSM_ValidationTimerMs = 0UL;
    DET_LOGI(TAG, "Init");
}

/**
 * \brief   ネットワークの通信モード遷移を要求する。
 *
 * \details ComM から呼び出される。Bus-Off 回復中は E_NOT_OK を返す。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_RequestComMode(CanSM_NetworkHandleType network, ComM_ModeType mode)
{
    if (network >= CANSM_CHANNEL_COUNT)
        return E_NOT_OK;

    /* Bus-Off 回復中は上位からのモード変更を受け付けない */
    if (CanSM_State == CANSM_STATE_BUS_OFF)
    {
        DET_LOGW(TAG, "RequestComMode ignored: BusOff recovery in progress");
        return E_NOT_OK;
    }

    switch (mode)
    {
        case COMM_FULL_COMMUNICATION:
            Can_SetControllerMode(0U, CAN_T_START);
            CanSM_State         = CANSM_STATE_FULL_COM;
            CanSM_BusOffRetries = 0U;
            DET_LOGI(TAG, "->FULL_COM");
            /* 通信確立を報告。デバウンス確定すれば CAN_BUSOFF の TF をクリアする */
            Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
            ComM_BusSMIndication(network, COMM_FULL_COMMUNICATION);
            break;

        case COMM_SILENT_COMMUNICATION:
            if (CanSM_State == CANSM_STATE_FULL_COM)
                Can_SetControllerMode(0U, CAN_T_STOP);
            CanSM_State = CANSM_STATE_SILENT_COM;
            DET_LOGI(TAG, "->SILENT_COM");
            ComM_BusSMIndication(network, COMM_SILENT_COMMUNICATION);
            break;

        case COMM_NO_COMMUNICATION:
            /* NO_COM は「もう通信が不要」であることを意味するため、Listen-Only
             * (CAN_T_STOP) ではなく実際に低消費電力スリープ (CAN_T_SLEEP) へ
             * 落とす。CanSM_ControllerWakeup() による復帰経路を持つ。 */
            if (CanSM_State == CANSM_STATE_FULL_COM)
                Can_SetControllerMode(0U, CAN_T_SLEEP);
            CanSM_State = CANSM_STATE_NO_COM;
            DET_LOGI(TAG, "->NO_COM (CAN controller SLEEP)");
            ComM_BusSMIndication(network, COMM_NO_COMMUNICATION);
            break;

        default:
            return E_NOT_OK;
    }

    return E_OK;
}

/**
 * \brief   ネットワークの現在の通信モードを取得する。
 *
 * \details Bus-Off 状態は COMM_NO_COMMUNICATION として報告する。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_GetCurrentComMode(CanSM_NetworkHandleType network, ComM_ModeType* mode)
{
    if (network >= CANSM_CHANNEL_COUNT || mode == NULL)
        return E_NOT_OK;

    switch (CanSM_State)
    {
        case CANSM_STATE_FULL_COM:   *mode = COMM_FULL_COMMUNICATION;   break;
        case CANSM_STATE_SILENT_COM: *mode = COMM_SILENT_COMMUNICATION; break;
        default:                     *mode = COMM_NO_COMMUNICATION;     break;
    }
    return E_OK;
}

/**
 * \brief   Bus-Off 通知コールバック（CanIf → CanSM の通知経路）。
 *
 * \details CAN コントローラが Bus-Off 状態を検出したとき CanIf 経由で呼ばれる。
 *          コントローラを即座に停止し、T_REC タイマを起動する。
 *          CanSM_MainFunction が T_REC ms 後にコントローラの再起動を試みる。
 *
 *          SWS_CanSM_00521: 検出直後（回復試行の前）に
 *          `ComM_BusSMIndication(Network, COMM_SILENT_COMMUNICATION)` を呼び、
 *          ComM のチャネル状態を回復完了まで FULL_COM のまま放置しないようにする。
 *          SILENT_COM は EcuM の RUN 状態を変化させない（`ComM_BusSMIndication()`
 *          参照）ため、回復試行中も RUN は維持される。
 *
 *          Dem への通知（SWS_CanSM_00522: `Dem_SetEventStatus(..., PRE_FAILED)`）は
 *          あえて行わない。本プロジェクトの `Dem_ReportErrorStatus()` は
 *          FAILED/PASSED のみを外部入力として受け付け、PRE_FAILED/PRE_PASSED は
 *          Dem 内部のデバウンスカウンタが導出する値として意図的に拒否する設計
 *          （Dem.c 参照）。ここで代わりに FAILED を渡すと
 *          `DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1` により単発の一時的な Bus-Off でも
 *          即座に DTC が確定してしまい、「L1 リトライの間は一時的障害として
 *          確定を待つ」という設計（CanSM_MainFunction 参照）と矛盾するため。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerBusOff(uint8 ControllerId)
{
    (void)ControllerId;

    if (CanSM_State != CANSM_STATE_FULL_COM)
        return;

    Can_SetControllerMode(0U, CAN_T_STOP);

    CanSM_State         = CANSM_STATE_BUS_OFF;
    CanSM_BusOffTimerMs = millis();

    ComM_BusSMIndication(0U, COMM_SILENT_COMMUNICATION);

    const uint8 inL2 = (CanSM_BusOffRetries >= CANSM_BUSOFF_L1_TO_L2_COUNT) ? 1U : 0U;
    DET_LOGW(TAG, "BusOff detected! retry=%u (%s) recovery in %lums",
             (unsigned)CanSM_BusOffRetries,
             inL2 ? "L2" : "L1",
             inL2 ? (unsigned long)CANSM_BUSOFF_RECOVERY_L2_MS : (unsigned long)CANSM_BUSOFF_RECOVERY_L1_MS);
}

/**
 * \brief   ウェイクアップ通知コールバック（CanIf から呼び出される）。
 *
 * \details CAN_CS_SLEEP 中に MCP2515 がバス活動を検知して自律的にウェイクアップ
 *          した際、Can_Isr()（割り込み） → Can_MainFunction_Wakeup() →
 *          CanIf_ControllerWakeup() 経由で呼び出される。
 *
 *          CANSM_STATE_NO_COM（ComM の NO_COM 要求によるボランタリスリープ）
 *          からの起床のみを受け付ける。CANSM_STATE_BUS_OFF は Can_T_STOP/
 *          Can_T_START のみで回復を試行し、実 HW をスリープさせることが
 *          そもそもないため、この状態から本関数が呼ばれることは原理的にない。
 *
 *          この時点ではまだ FULL_COM へ確定しない。MCP2515 の WAKIF はノイズでも
 *          誤って立ちうるため、CAN_T_WAKEUP（SLEEP→STOPPED、Listen-Only）のみを
 *          実行して CANSM_STATE_WAKEUP_VALIDATING へ遷移し、検証タイマを開始する。
 *          実際に FULL_COM へ確定するのは CanSM_RxIndication() が有効な受信を
 *          確認したとき（検証成功）、または CanSM_MainFunction() が検証タイムアウトを
 *          検出して再スリープするとき（検証失敗）のいずれかである。
 *
 * \param[in]  ControllerId  ウェイクアップを検出したコントローラ ID。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerWakeup(uint8 ControllerId)
{
    (void)ControllerId;

    if (CanSM_State != CANSM_STATE_NO_COM)
    {
        /* 想定される呼び出し元は Can_MainFunction_Wakeup()（Can_Isr() の SLEEP
         * 分岐が立てたフラグをドレインするタスク）のみであり、
         * CanState == CAN_CS_SLEEP のときにしか到達しない。CANSM_STATE_BUS_OFF
         * は実 HW をスリープさせないため、この分岐に到達すること自体が
         * 原理的にない（フェイルセーフとして残す）。 */
        DET_LOGW(TAG, "Wakeup ignored: not in voluntary NO_COM sleep (state=%u)",
                 (unsigned)CanSM_State);
        return;
    }

    DET_LOGI(TAG, "Wakeup detected -> validating (Listen-Only, waiting for confirmed RX)");
    Can_SetControllerMode(0U, CAN_T_WAKEUP);  /* CAN_CS_SLEEP -> CAN_CS_STOPPED (Listen-Only) */
    CanSM_State             = CANSM_STATE_WAKEUP_VALIDATING;
    CanSM_ValidationTimerMs = millis();
}

/**
 * \brief   受信通知コールバック（CanIf から全受信フレームについて呼び出される）。
 *
 * \details AUTOSAR SWS_CanSM の CanSMRxIndicationUsed 設定に相当し、CanIf が
 *          フレームを受信するたびに（上位 PDU への振り分け結果に関わらず）
 *          通知される。通常運用中（CANSM_STATE_FULL_COM 等）は何もしない。
 *
 *          CANSM_STATE_WAKEUP_VALIDATING 中にのみ意味を持つ: 有効な CAN
 *          フレームを実際に受信できたことは、直前のウェイクアップがノイズ
 *          ではなく本物のバス活動だったことの確証となる。これを検証成功と
 *          判断し、CAN_T_START で FULL_COM へ確定して EcuM を RUN へ復帰させる。
 *
 * \param[in]  ControllerId  受信したコントローラ ID。
 *
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_RxIndication(uint8 ControllerId)
{
    (void)ControllerId;

    if (CanSM_State != CANSM_STATE_WAKEUP_VALIDATING)
        return;

    DET_LOGI(TAG, "Wakeup validated (RX confirmed) -> FULL_COM");
    Can_SetControllerMode(0U, CAN_T_START);   /* CAN_CS_STOPPED -> CAN_CS_STARTED */
    CanSM_State         = CANSM_STATE_FULL_COM;
    CanSM_BusOffRetries = 0U;
    Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
    ComM_BusSMIndication(0U, COMM_FULL_COMMUNICATION);
}

/**
 * \brief   CanSM 周期処理（Bus-Off 回復タイマ管理）。
 *
 * \details Bus-Off 状態のとき、L1/L2 のいずれかの周期（下記）が経過すると
 *          コントローラの再起動を試みる。再起動時は
 *          Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, PASSED) を報告する
 *          （CanSM_RequestComMode を経由しない自動復帰のため、ここで明示的に報告する）。
 *          再起動後に再度 Bus-Off が発生すると CanSM_ControllerBusOff() が
 *          呼ばれ、試行回数がインクリメントされる（リトライ回数は次回の
 *          CanSM_RequestComMode(FULL_COM) までリセットされない）。
 *
 *          L1/L2 バックオフ（SWS_CanSM_00514/00515 準拠）:
 *            試行回数 <= CANSM_BUSOFF_L1_TO_L2_COUNT の間は
 *            CANSM_BUSOFF_RECOVERY_L1_MS（短い周期）でリトライする。
 *            この回数を超えたら、一時的なバス障害ではなく持続的な Bus-Off と
 *            判断し、Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, FAILED) を
 *            1 回だけ報告した上で（Dem 側は DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1
 *            のため即座に確定する）、以降は CANSM_BUSOFF_RECOVERY_L2_MS
 *            （長い周期）でリトライを継続する。AUTOSAR 仕様には「回復を諦めて
 *            二度と復帰しない」状態は存在しないため、L2 に切り替わった後も
 *            回復試行そのものは無期限に続ける。
 *
 *          ウェイクアップ検証中 (CANSM_STATE_WAKEUP_VALIDATING) は、
 *          CANSM_WAKEUP_VALIDATION_MS 以内に CanSM_RxIndication() による
 *          検証成功がなければタイムアウトと判断し、ノイズによる誤ウェイクアップ
 *          とみなして Can_T_SLEEP で再びスリープへ戻す。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_MainFunction(void)
{
    if (CanSM_State == CANSM_STATE_WAKEUP_VALIDATING)
    {
        if ((millis() - CanSM_ValidationTimerMs) >= CANSM_WAKEUP_VALIDATION_MS)
        {
            DET_LOGW(TAG, "Wakeup validation timeout (%lums, no confirmed RX) -> back to SLEEP",
                     (unsigned long)CANSM_WAKEUP_VALIDATION_MS);
            Can_SetControllerMode(0U, CAN_T_SLEEP);  /* CAN_CS_STOPPED -> CAN_CS_SLEEP、ウェイクアップ割り込み再武装 */
            CanSM_State = CANSM_STATE_NO_COM;
        }
        return;
    }

    if (CanSM_State != CANSM_STATE_BUS_OFF)
        return;

    const uint8 inL2 = (CanSM_BusOffRetries >= CANSM_BUSOFF_L1_TO_L2_COUNT) ? 1U : 0U;
    const unsigned long interval = inL2 ? (unsigned long)CANSM_BUSOFF_RECOVERY_L2_MS
                                         : (unsigned long)CANSM_BUSOFF_RECOVERY_L1_MS;

    if ((millis() - CanSM_BusOffTimerMs) < interval)
        return;

    /* L1/L2 周期経過: 回復試行 */
    CanSM_BusOffRetries++;

    if (CanSM_BusOffRetries == (CANSM_BUSOFF_L1_TO_L2_COUNT + 1U))
    {
        /* L1→L2 に降格するちょうどこの瞬間: 一時的なバス障害ではなく持続的な
         * Bus-Off と判断し DTC を確定する（FreezeFrame にはこの時点の車両状態が
         * 残る）。回復試行そのものは止めない（下へ続く）。 */
        DET_LOGE(TAG, "BusOff: L1(%u) exceeded, degrade to L2 (%lums)",
                 (unsigned)CANSM_BUSOFF_L1_TO_L2_COUNT, (unsigned long)CANSM_BUSOFF_RECOVERY_L2_MS);
        Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    }

    DET_LOGI(TAG, "BusOff: restart attempt %u (%s, next in %lums)",
             (unsigned)CanSM_BusOffRetries, inL2 ? "L2" : "L1", interval);

    Can_SetControllerMode(0U, CAN_T_START);
    CanSM_State = CANSM_STATE_FULL_COM;
    /* 回復成功を報告。デバウンス確定すれば CAN_BUSOFF の TF をクリアする
     * （CDTC/PDTC は上の FAILED 確定で既に立っていれば保持される。Dem.c の
     * PASSED デバウンス確定コメント参照）。 */
    Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
    /* 回復成功 → ComM に FULL_COM を通知 → EcuM_RequestRUN → RUN へ戻る */
    ComM_BusSMIndication(0U, COMM_FULL_COMMUNICATION);
    /* 再度 Bus-Off が発生すれば CanIf → CanSM_ControllerBusOff() が呼ばれる */
}
