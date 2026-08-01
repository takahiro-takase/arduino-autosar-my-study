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
 *              ↓ RequestComMode(NO_COM)                      │ │
 *              │ (Nm が Bus-Sleep Mode へ到達するまで          │ │
 *              │  実際のスリープは保留。CanSM_NmBusSleepMode() │ │
 *              │  経由で CAN_T_SLEEP)                          │ │
 *              └───────────────────────────────────────────────┘
 *              ↓ CanSM_ControllerBusOff()                    │ 回復成功
 *            CANSM_STATE_BUS_OFF                             │
 *              ↓ L1/L2 周期経過 (MainFunction)                │
 *              → Can_SetControllerMode(CAN_T_START) ─────────┘
 *              （回復に失敗すれば CanSM_ControllerBusOff() が再度呼ばれ、
 *                リトライを継続する。無期限に諦めない）
 *
 *          CanSM_ControllerBusOff() は CANSM_STATE_NO_COM_PENDING_SLEEP
 *          （↑図中、FULL_COM→NO_COM の途中、Nm の Bus-Sleep Mode 待ちで
 *          HW はまだ稼働中の区間）からも受け付ける。この場合の回復成功時は
 *          FULL_COM ではなく NO_COM_PENDING_SLEEP へ戻す（ComM が既に NO_COM
 *          を要求済みのため、FULL_COM へ戻すと誰も再要求せず取り残される）。
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
 *          正常系（ボランタリ）スリープとウェイクアップ（Nm による協調スリープ）:
 *            App_EngineManager が「エンジン OFF が一定サイクル継続 = 通信不要」と
 *            判断すると ComM_RequestComMode(COMM_USER_0, NO_COM) を要求する。
 *            Dcm（COMM_USER_1）も extendedSession でなければ ComM の集約結果が
 *            NO_COM になり、CanSM_RequestComMode(NO_COM) が呼ばれて
 *            CANSM_STATE_NO_COM へ遷移する。ただしこの時点では実際には
 *            スリープしない（以前の実装は即座に Can_SetControllerMode
 *            (CAN_T_SLEEP) していたが、Nm（CanNm 状態機械、Nm.c 参照）を
 *            導入したことで、ComM_BusSMIndication() が呼ぶ
 *            Nm_NetworkRelease() を契機に Nm 自身が Ready Sleep → Prepare
 *            Bus-Sleep → Bus-Sleep Mode と自律的に遷移し、他ノード（仮想他ECU）
 *            からの NM フレーム受信が一定時間 (NM_TIMEOUT_MS+NM_WAIT_BUS_SLEEP_MS)
 *            なかったことを確認してから CanSM_NmBusSleepMode() を呼ぶように
 *            変更した。CanSM はこの通知を受けて初めて
 *            Can_SetControllerMode(CAN_T_SLEEP) で MCP2515 を実際にスリープ
 *            させる。これにより「他ノードがまだ通信中の間は実際にはスリープ
 *            しない」という NM 本来の協調スリープを実機で確認できる。
 *            CANSM_STATE_BUS_OFF は実 HW をスリープさせないため、これが
 *            本モジュールで唯一 HW を実際にスリープさせる経路である。
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
    CANSM_STATE_NO_COM,               /* 通信停止・HW も実際にスリープ済み（または未起動） */
    CANSM_STATE_NO_COM_PENDING_SLEEP, /* NO_COM 要求済みだが Nm が Bus-Sleep Mode へ
                                        * 到達するまで HW はまだ稼働中（物理スリープ保留） */
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

/** 1 = Bus-Off 検出時点の状態が CANSM_STATE_NO_COM_PENDING_SLEEP だった
 *  （回復後は CANSM_STATE_FULL_COM ではなく CANSM_STATE_NO_COM_PENDING_SLEEP へ
 *  戻す。CanSM_ControllerBusOff()/CanSM_MainFunction() 参照）。 */
static uint8 CanSM_BusOffFromPendingSleep;

/** [SWS_CanSM_00184/00188/00190] 用の初期化済みフラグ。CanSM_State の既定値
 *  (CANSM_STATE_NO_COM=0) は「未初期化」と区別が付かないため別途持つ。 */
static uint8 CanSM_Initialized = 0U;

/**
 * \brief   CanSM モジュールを初期化する。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_Init(void)
{
    CanSM_State                  = CANSM_STATE_NO_COM;
    CanSM_BusOffTimerMs          = 0UL;
    CanSM_BusOffRetries          = 0U;
    CanSM_ValidationTimerMs      = 0UL;
    CanSM_BusOffFromPendingSleep = 0U;
    CanSM_Initialized            = 1U;
    DET_LOGI(TAG, "Init");
}

void CanSM_DeInit(void)
{
    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_DEINIT, CANSM_E_UNINIT);
        return;
    }

    CanSM_Initialized = 0U;
    DET_LOGI(TAG, "DeInit ok");
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
    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_REQUEST_COM_MODE, CANSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (network >= CANSM_CHANNEL_COUNT)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_REQUEST_COM_MODE, CANSM_E_INVALID_NETWORK_HANDLE);
        return E_NOT_OK;
    }

    /* Bus-Off 回復中は上位からのモード変更を受け付けない */
    if (CanSM_State == CANSM_STATE_BUS_OFF)
    {
        DET_LOGW(TAG, "RequestComMode ignored: BusOff recovery in progress");
        return E_NOT_OK;
    }

    switch (mode)
    {
        case COMM_FULL_COMMUNICATION:
            if (Can_SetControllerMode(0U, CAN_T_START) != CAN_OK)
            {
                /* Can_SetControllerMode() は CanState==CAN_CS_SLEEP からの
                 * CAN_T_START を拒否しうる（Can.c 参照）。現状の呼び出し
                 * 経路ではこの分岐に到達しないはずだが、もし到達すれば
                 * コントローラは実際にはまだ稼働していない。ここで
                 * CanSM_State を FULL_COM に進めてしまうと、CanSM/ComM/EcuM
                 * が「成功した」と誤認したまま MCP2515 は眠り続け、以降の
                 * 送受信がハードリセットするまで静かに壊れる
                 * （2026-08 のレビューで指摘）。実際に遷移が成功したときのみ
                 * 状態を進める。 */
                DET_LOGE(TAG, "RequestComMode E: Can_SetControllerMode(T_START) failed, state unchanged");
                return E_NOT_OK;
            }
            CanSM_State         = CANSM_STATE_FULL_COM;
            CanSM_BusOffRetries = 0U;
            DET_LOGI(TAG, "->FULL_COM");
            /* 通信確立を報告。デバウンス確定すれば CAN_BUSOFF の TF をクリアする */
            Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
            ComM_BusSMIndication(network, COMM_FULL_COMMUNICATION);
            break;

        case COMM_SILENT_COMMUNICATION:
            /* CANSM_STATE_SILENT_COM は「コントローラは停止済み」という不変条件を
             * 持つ（CanSM_ControllerBusOff() のガードがこれを前提に SILENT_COM を
             * 除外している）。この不変条件を保つため、コントローラが物理的に
             * 稼働中でありうる状態（FULL_COM、および Nm 協調スリープ待ちで
             * CAN_T_START のまま稼働継続する NO_COM_PENDING_SLEEP）から遷移する
             * 場合は必ず停止させる。それ以外（NO_COM/WAKEUP_VALIDATING 等）は
             * 既にコントローラが停止済みのため何もしなくてよい
             * （2026-08 のスペック監査で、NO_COM_PENDING_SLEEP からの遷移が
             * 抜けていたことを発見・修正）。 */
            if (CanSM_State == CANSM_STATE_FULL_COM || CanSM_State == CANSM_STATE_NO_COM_PENDING_SLEEP)
            {
                if (Can_SetControllerMode(0U, CAN_T_STOP) != CAN_OK)
                {
                    DET_LOGE(TAG, "RequestComMode E: Can_SetControllerMode(T_STOP) failed, state unchanged");
                    return E_NOT_OK;
                }
            }
            CanSM_State = CANSM_STATE_SILENT_COM;
            DET_LOGI(TAG, "->SILENT_COM");
            ComM_BusSMIndication(network, COMM_SILENT_COMMUNICATION);
            break;

        case COMM_NO_COMMUNICATION:
            /* NO_COM は「もう通信が不要」であることを意味するが、実際の
             * 物理スリープ (CAN_T_SLEEP) はここでは行わない。Nm（CanNm 状態
             * 機械）が Bus-Sleep Mode へ到達したときに CanSM_NmBusSleepMode()
             * を呼ぶので、そこで初めてスリープさせる（協調スリープ。
             * 本ファイル冒頭コメント参照）。以前は CANSM_STATE_FULL_COM から
             * の遷移時のみ実際にスリープしていたため、その場合だけ物理
             * スリープ保留状態（HW はまだ稼働中）へ遷移する。 */
            if (CanSM_State == CANSM_STATE_FULL_COM)
            {
                CanSM_State = CANSM_STATE_NO_COM_PENDING_SLEEP;
                DET_LOGI(TAG, "->NO_COM (awaiting Nm Bus-Sleep Mode for physical sleep)");
            }
            else
            {
                CanSM_State = CANSM_STATE_NO_COM;
                DET_LOGI(TAG, "->NO_COM");
            }
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
    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_GET_CURRENT_COM_MODE, CANSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (network >= CANSM_CHANNEL_COUNT)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_GET_CURRENT_COM_MODE, CANSM_E_INVALID_NETWORK_HANDLE);
        return E_NOT_OK;
    }

    if (mode == NULL)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_GET_CURRENT_COM_MODE, CANSM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

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
 *          受け付けるのは CANSM_STATE_FULL_COM と CANSM_STATE_NO_COM_PENDING_SLEEP
 *          の 2 状態のみ。どちらもコントローラが物理的に稼働中（CAN_T_START 済み）
 *          であり、実際に Bus-Off しうる。NO_COM_PENDING_SLEEP は「もう通信は
 *          不要だが Nm が Bus-Sleep Mode に到達するまでは HW を稼働させ続ける」
 *          状態であり（本ファイル冒頭コメント参照）、この間の Bus-Off を無視すると
 *          回復シーケンスが一切起動せず、コントローラがハードウェア的に Bus-Off
 *          のまま放置されてしまう（2026-08 のスペック監査で発見・修正）。
 *          それ以外の状態（NO_COM/SILENT_COM=HW 停止中または未起動、BUS_OFF=
 *          回復試行中で二重に入る必要がない、WAKEUP_VALIDATING=Listen-Only で
 *          送信しないため原理的に Bus-Off しない）は無視する。
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
 *          実機検証メモ（2026-08）: NO_COM_PENDING_SLEEP 経路は FULL_COM 経路
 *          （相手ノード不在による自然発生 Bus-Off、繰り返し実機で確認済み）と
 *          同一のコードを通るため間接的には検証済みだが、この分岐そのものを
 *          実機で直接踏ませるのは意図的に難しい。Nm_MainFunction() の
 *          NM_STATE_READY_SLEEP ケース（Nm.c 参照）は送信を一切行わないため
 *          （ログの "Ready Sleep State (tx stopped)" のとおり）、また Com の
 *          TX I-PDU グループも Com_IpduGroupStop() で停止済みのため、
 *          NO_COM_PENDING_SLEEP から実際のスリープ（CanSM_NmBusSleepMode()）
 *          までの数秒間はほぼ何も送信されない。Can_Write() の連続失敗を
 *          トリガとする Bus-Off（一次検出の EFLG.TXBO も同様、送信しなければ
 *          TEC が上がらない）は、相手ノードを完全に外してもこの区間内では
 *          事実上発生しない。この分岐の正しさは、FULL_COM 経路での反復実証
 *          （L1/L2 双方、EcuM 側の DET 誤検知なし）とコードレビューをもって
 *          十分とみなした。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerBusOff(uint8 ControllerId)
{
    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_CONTROLLER_BUSOFF, CANSM_E_UNINIT);
        return;
    }

    if (ControllerId != 0U)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_CONTROLLER_BUSOFF, CANSM_E_PARAM_CONTROLLER);
        return;
    }

    if (CanSM_State != CANSM_STATE_FULL_COM && CanSM_State != CANSM_STATE_NO_COM_PENDING_SLEEP)
        return;

    CanSM_BusOffFromPendingSleep = (CanSM_State == CANSM_STATE_NO_COM_PENDING_SLEEP) ? 1U : 0U;

    if (Can_SetControllerMode(0U, CAN_T_STOP) != CAN_OK)
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
        DET_LOGE(TAG, "ControllerBusOff E: Can_SetControllerMode(T_STOP) failed (state desync)");
    }

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

    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_CONTROLLER_WAKEUP, CANSM_E_UNINIT);
        return;
    }

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
    if (Can_SetControllerMode(0U, CAN_T_WAKEUP) != CAN_OK)  /* CAN_CS_SLEEP -> CAN_CS_STOPPED (Listen-Only) */
    {
        /* 到達しないはずの経路（この分岐に来る時点で CanState==CAN_CS_SLEEP
         * であることは呼び出し元 Can_MainFunction_Wakeup() 側で保証されて
         * いる）。実際に失敗した場合、コントローラは Listen-Only へ遷移
         * していないため、検証タイマだけ回して確定させるのは危険。
         * CANSM_STATE_NO_COM のまま据え置き、次のウェイクアップ通知を
         * 待つ（2026-08 のレビューで指摘）。 */
        DET_LOGE(TAG, "ControllerWakeup E: Can_SetControllerMode(T_WAKEUP) failed, staying in NO_COM");
        return;
    }
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

    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_RX_INDICATION, CANSM_E_UNINIT);
        return;
    }

    if (CanSM_State != CANSM_STATE_WAKEUP_VALIDATING)
        return;

    DET_LOGI(TAG, "Wakeup validated (RX confirmed) -> FULL_COM");
    if (Can_SetControllerMode(0U, CAN_T_START) != CAN_OK)   /* CAN_CS_STOPPED -> CAN_CS_STARTED */
    {
        /* 到達しないはずの経路（CANSM_STATE_WAKEUP_VALIDATING に入っている
         * 時点で CanState==CAN_CS_STOPPED のはず）。失敗した場合に
         * CanSM_State を FULL_COM へ進めてしまうと、CanSM/ComM/EcuM は
         * 「成功した」と誤認したまま MCP2515 は Listen-Only のままで送信
         * できず、実害が静かに進行する（レビュー指摘の症状そのもの）。
         * WAKEUP_VALIDATING に留まり、タイムアウトで再スリープする
         * 既存のフェイルセーフ（CanSM_MainFunction）に委ねる。 */
        DET_LOGE(TAG, "RxIndication E: Can_SetControllerMode(T_START) failed, staying in WAKEUP_VALIDATING");
        return;
    }
    CanSM_State         = CANSM_STATE_FULL_COM;
    CanSM_BusOffRetries = 0U;
    Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
    ComM_BusSMIndication(0U, COMM_FULL_COMMUNICATION);
}

void CanSM_NmBusSleepMode(void)
{
    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_NM_BUS_SLEEP_MODE, CANSM_E_UNINIT);
        return;
    }

    /* 呼び出し順序の入れ替わりへの防御: NO_COM 要求後に別要因（ウェイクアップ、
     * 新たな FULL_COM 要求等）で既に状態が変わっていれば何もしない。物理
     * スリープが必要なのは CANSM_STATE_NO_COM_PENDING_SLEEP のときだけ
     * （元々 FULL_COM からの遷移ではなかった場合は、以前の実装と同じく
     * 物理スリープ不要。CanSM_c 冒頭コメント参照）。 */
    if (CanSM_State != CANSM_STATE_NO_COM_PENDING_SLEEP)
        return;

    /* CAN_T_SLEEP は Can_SetControllerMode() 側で状態検証をしておらず
     * （Can.c 参照。協調スリープ設計上 CAN_CS_STARTED/STOPPED どちらから
     * 呼ばれても正当なため、この遷移に無効な遷移元状態は存在しない）、
     * 戻り値は常に CAN_OK。明示的に無視する。 */
    (void)Can_SetControllerMode(0U, CAN_T_SLEEP);
    CanSM_State = CANSM_STATE_NO_COM;
    DET_LOGI(TAG, "Nm reached Bus-Sleep Mode -> CAN controller SLEEP");
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
    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_MAIN_FUNCTION, CANSM_E_UNINIT);
        return;
    }

    if (CanSM_State == CANSM_STATE_WAKEUP_VALIDATING)
    {
        if ((millis() - CanSM_ValidationTimerMs) >= CANSM_WAKEUP_VALIDATION_MS)
        {
            DET_LOGW(TAG, "Wakeup validation timeout (%lums, no confirmed RX) -> back to SLEEP",
                     (unsigned long)CANSM_WAKEUP_VALIDATION_MS);
            /* CAN_T_SLEEP は失敗しない（上記 CanSM_NmBusSleepMode() のコメント
             * 参照）。戻り値は明示的に無視する。 */
            (void)Can_SetControllerMode(0U, CAN_T_SLEEP);  /* CAN_CS_STOPPED -> CAN_CS_SLEEP、ウェイクアップ割り込み再武装 */
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

    if (Can_SetControllerMode(0U, CAN_T_START) != CAN_OK)
    {
        /* 到達しないはずの経路（BUS_OFF 中は CanState==CAN_CS_STOPPED の
         * はず、CanSM_ControllerBusOff() が Bus-Off 検出時に CAN_T_STOP
         * 済み）。失敗した場合に回復成功とみなして CanSM_State を進めると、
         * CanSM/ComM/EcuM が「復帰した」と誤認したまま MCP2515 は止まった
         * ままになる。CanSM_BusOffRetries は既にインクリメント済みのため、
         * 状態は BUS_OFF のまま据え置き、次の L1/L2 周期で再試行させる
         * （通常の回復失敗と同じ扱い。2026-08 のレビューで指摘）。 */
        DET_LOGE(TAG, "MainFunction E: Can_SetControllerMode(T_START) failed during BusOff recovery, retry next cycle");
        return;
    }
    /* 回復成功を報告。デバウンス確定すれば CAN_BUSOFF の TF をクリアする
     * （CDTC/PDTC は上の FAILED 確定で既に立っていれば保持される。Dem.c の
     * PASSED デバウンス確定コメント参照）。 */
    Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);

    if (CanSM_BusOffFromPendingSleep)
    {
        /* Bus-Off 発生時点で「通信不要、Nm の Bus-Sleep Mode 到達待ち」
         * だった場合、回復後も CANSM_STATE_FULL_COM へ引き戻してはならない。
         * ComM は既に NO_COM を要求済みで、誰も再要求しないため FULL_COM に
         * 戻すと二度と NO_COM_PENDING_SLEEP へ戻れず取り残されてしまう。
         * コントローラ自体は CAN_T_START のまま（Nm が引き続きバス活動を
         * 監視できるようにするため）、CanSM の状態と ComM への通知だけを
         * 元の意図どおり NO_COM_PENDING_SLEEP/NO_COMMUNICATION へ戻す。 */
        CanSM_State = CANSM_STATE_NO_COM_PENDING_SLEEP;
        ComM_BusSMIndication(0U, COMM_NO_COMMUNICATION);
    }
    else
    {
        CanSM_State = CANSM_STATE_FULL_COM;
        /* 回復成功 → ComM に FULL_COM を通知 → EcuM_RequestRUN → RUN へ戻る */
        ComM_BusSMIndication(0U, COMM_FULL_COMMUNICATION);
    }
    /* 再度 Bus-Off が発生すれば CanIf → CanSM_ControllerBusOff() が呼ばれる */
}

void CanSM_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    if (VersionInfo == NULL)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_GET_VERSION_INFO, CANSM_E_PARAM_POINTER);
        return;
    }

    VersionInfo->vendorID         = CANSM_VENDOR_ID;
    VersionInfo->moduleID         = CANSM_MODULE_ID;
    VersionInfo->sw_major_version = CANSM_SW_MAJOR_VERSION;
    VersionInfo->sw_minor_version = CANSM_SW_MINOR_VERSION;
    VersionInfo->sw_patch_version = CANSM_SW_PATCH_VERSION;
}
