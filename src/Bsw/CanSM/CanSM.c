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
 *                リトライを継続する。無期限に諦めない。回復成功時は常に
 *                CANSM_STATE_FULL_COM へ戻る）
 *
 *          RequestComMode(NO_COM) が CanSM へ届く時点で、上位層（ComM）は
 *          既に Nm の協調スリープ完了を確認済みである（[SWS_ComM_00133]/
 *          [SWS_ComM_00392] 準拠、ComM.c 参照）。そのため CanSM 自身は
 *          「解放要求はされたがまだ寝てはいけない」という中間状態を持たず、
 *          FULL_COM から物理スリープまで一気に遷移する（以前あった
 *          CANSM_STATE_NO_COM_PENDING_SLEEP という中間状態は、ComM が
 *          Nm の完了を待たず早期に NO_COM を通知していた旧設計の名残で、
 *          この設計では不要になったため削除した。詳細は ComM.c ファイル
 *          冒頭コメント参照）。
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
 *            NO_COM になるが、ComM はこの時点では CanSM を一切呼ばず
 *            Nm_NetworkRelease() のみを Nm（CanNm 状態機械、Nm.c 参照）へ送る
 *            （[SWS_ComM_00133]、詳細は ComM.c ファイル冒頭コメント参照）。
 *            Nm 自身が Ready Sleep → Prepare Bus-Sleep → Bus-Sleep Mode と
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
 *            フレーム受信が一定時間 (NM_TIMEOUT_MS+NM_WAIT_BUS_SLEEP_MS)
 *            なかったことを確認してから ComM_Nm_BusSleepMode() を呼ぶ
 *            （[SWS_ComM_00392]）。ComM はこれを受けて初めて
 *            CanSM_RequestComMode(NO_COM) を呼び、CanSM が
 *            CanIf_SetControllerMode(CAN_CS_SLEEP) で MCP2515 を実際に
 *            スリープさせる。これにより「他ノードがまだ通信中の間は実際には
 *            スリープしない」という NM 本来の協調スリープを実機で確認できる。
 *            CANSM_STATE_BUS_OFF は実 HW をスリープさせないため、これが
 *            本モジュールで唯一 HW を実際にスリープさせる経路である。
 *            なお CANSM_STATE_BUS_OFF は CAN_CS_STOPPED（受信は継続）扱いの
 *            ため、Bus-Off 回復待ち中でも CanSM_RxIndication()/
 *            Nm_RxIndication() は普通に発火しうる（Can_MainFunction_Read()
 *            が RX ドレインをスキップするのは CanState==CAN_CS_SLEEP の
 *            ときのみ）。CANSM_STATE_SILENT_COM は 2026-08 の変更後は
 *            CAN_CS_STARTED のまま（CanIf_SetPduMode(CANIF_TX_OFFLINE) で
 *            TX のみ抑制）のため、受信が継続する理由は BUS_OFF とは異なる
 *            （そもそもコントローラが止まっていない）が、結果として
 *            SILENT_COM 中も受信は同様に継続する。またこの変更により、
 *            SILENT_COM 中に Bus-Off が発生することも起こりうるようになった
 *            （CanSM_ControllerBusOff()/CanSM_PreBusOffState 参照。回復成功時は
 *            元が SILENT_COM だったか FULL_COM だったかに応じて正しい状態へ
 *            戻す）。Nm がこれを受けて Prepare
 *            Bus-Sleep Mode から Network Mode へ自律復帰した場合の扱いは
 *            ComM.c の ComM_Nm_NetworkMode() の doc コメントを参照
 *            （CanSM_RequestComMode(FULL_COM) は Bus-Off 中は拒否されるが、
 *            それでも安全に収束するよう ComM 側で設計されている）。
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
 *                  ComM_BusSM_ModeIndication(FULL_COM) で EcuM を RUN へ復帰させる。
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
#include "CanIf.h"
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
    CANSM_STATE_NO_COM,              /* 通信停止・HW も実際にスリープ済み（または未起動） */
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

/** Bus-Off 検出直前の CanSM_State（CANSM_STATE_FULL_COM または
 *  CANSM_STATE_SILENT_COM のいずれか）。2026-08 追加: SILENT_COMMUNICATION
 *  が CanIf_SetPduMode(CANIF_TX_OFFLINE) ベースになりコントローラを
 *  CAN_CS_STARTED のまま維持するようになったため、SILENT_COM 中でも本当に
 *  Bus-Off しうるようになった。回復成功時にどちらへ戻すか
 *  （CanSM_MainFunction() 参照）を判断するために使う。 */
static CanSM_InternalStateType CanSM_PreBusOffState;

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
void CanSM_Init(const CanSM_ConfigType* ConfigPtr)
{
    (void)ConfigPtr; /* 本プロジェクトは post-build 設定を持たない（CanSM.h 参照） */
    CanSM_State              = CANSM_STATE_NO_COM;
    CanSM_BusOffTimerMs      = 0UL;
    CanSM_BusOffRetries      = 0U;
    CanSM_ValidationTimerMs  = 0UL;
    CanSM_PreBusOffState     = CANSM_STATE_NO_COM;
    CanSM_Initialized        = 1U;
    DET_LOGI(TAG, "Init");
}

void CanSM_DeInit(void)
{
    DET_LOGT(TAG, "called");

    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_DEINIT, CANSM_E_UNINIT);
        return;
    }

    CanSM_Initialized = 0U;
    DET_LOGI(TAG, "DeInit ok");
}

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
 *          CanSM_RxIndication()/CanSM_MainFunction() の 3 箇所から呼ぶ）。
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

/**
 * \brief   ネットワークの通信モード遷移を要求する。
 *
 * \details ComM から呼び出される。Bus-Off 回復中は E_NOT_OK を返す。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType mode)
{
    DET_LOGT(TAG, "called");

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
            if (CanIf_SetControllerMode(0U, CAN_CS_STARTED) != E_OK)
            {
                /* CanIf_SetControllerMode() は CanState==CAN_CS_SLEEP からの
                 * CAN_T_START を拒否しうる（Can.c 参照）。現状の呼び出し
                 * 経路ではこの分岐に到達しないはずだが、もし到達すれば
                 * コントローラは実際にはまだ稼働していない。ここで
                 * CanSM_State を FULL_COM に進めてしまうと、CanSM/ComM/EcuM
                 * が「成功した」と誤認したまま MCP2515 は眠り続け、以降の
                 * 送受信がハードリセットするまで静かに壊れる
                 * （2026-08 のレビューで指摘）。実際に遷移が成功したときのみ
                 * 状態を進める。 */
                DET_LOGE(TAG, "RequestComMode E: CanIf_SetControllerMode(STARTED) failed, state unchanged");
                return E_NOT_OK;
            }
            /* [SWS_CANIF_00137]: SILENT_COM から戻る場合に備え、PDU チャネルを
             * CANIF_ONLINE へ戻す（TX 抑制解除）。NO_COM/WAKEUP_VALIDATING から
             * 来た場合も無条件で ONLINE にしてよい（Init 直後の既定
             * CANIF_OFFLINE、または以前 SILENT_COM だった名残のいずれでも、
             * FULL_COM では常に TX 有効であるべきため）。 */
            CanSM_SetPduModeOnlineBestEffort("RequestComMode");
            CanSM_State         = CANSM_STATE_FULL_COM;
            CanSM_BusOffRetries = 0U;
            DET_LOGI(TAG, "->FULL_COM");
            /* 通信確立を報告。デバウンス確定すれば CAN_BUSOFF の TF をクリアする */
            Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
            ComM_BusSM_ModeIndication(network, COMM_FULL_COMMUNICATION);
            break;

        case COMM_SILENT_COMMUNICATION:
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
            /* ComM は Nm が実際に Bus-Sleep Mode へ到達してから初めて本関数を
             * COMM_NO_COMMUNICATION で呼ぶ（ComM_Nm_BusSleepMode() 経由。
             * [SWS_ComM_00133]/[SWS_ComM_00392]、詳細は ComM.c および本ファイル
             * 冒頭コメント参照）。そのため呼ばれた時点で「もう眠ってよい」ことが
             * 確定しており、CANSM_STATE_FULL_COM から物理スリープまで一気に
             * 遷移してよい（以前あった「解放要求はされたがまだ寝てはいけない」
             * という中間状態 CANSM_STATE_NO_COM_PENDING_SLEEP は、この設計では
             * 不要になったため削除した）。
             * CAN_T_SLEEP は Can_SetControllerMode() 側で状態検証をしておらず
             * （CAN_CS_STARTED/STOPPED いずれから呼ばれても正当なため）、
             * 戻り値は常に E_OK。明示的に無視する。 */
            (void)CanIf_SetControllerMode(0U, CAN_CS_SLEEP);
            CanSM_State = CANSM_STATE_NO_COM;
            DET_LOGI(TAG, "->NO_COM (physical sleep)");
            ComM_BusSM_ModeIndication(network, COMM_NO_COMMUNICATION);
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
Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType* mode)
{
    DET_LOGT(TAG, "called");

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
 *          Dem への通知（SWS_CanSM_00522: `Dem_SetEventStatus(..., PRE_FAILED)`）は
 *          あえて行わない。本プロジェクトの `Dem_ReportErrorStatus()` は
 *          FAILED/PASSED のみを外部入力として受け付け、PRE_FAILED/PRE_PASSED は
 *          Dem 内部のデバウンスカウンタが導出する値として意図的に拒否する設計
 *          （Dem.c 参照）。ここで代わりに FAILED を渡すと
 *          `DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1` により単発の一時的な Bus-Off でも
 *          即座に DTC が確定してしまい、「L1 リトライの間は一時的障害として
 *          確定を待つ」という設計（CanSM_MainFunction 参照）と矛盾するため。
 *
 *          Nm 協調スリープ待ち中（ComM が Nm_NetworkRelease() を送信済みだが
 *          Nm がまだ Bus-Sleep Mode へ到達していない期間、ComM.c の
 *          ComM_NmReleasePending 参照）に Bus-Off が発生しても、CanSM の
 *          視点では CanSM_State は引き続き CANSM_STATE_FULL_COM のままのため
 *          （このプロジェクトの設計では ComM が Nm の完了確認を待ってから
 *          しか CanSM へ NO_COM を伝えない）、上記の通常の FULL_COM 経路が
 *          そのまま処理する。回復成功時は常に FULL_COM へ戻すが、Nm 自身は
 *          Bus-Off 中も独立したタイマで動作し続けているため、既に
 *          Bus-Sleep Mode へ到達済みの可能性がある。この場合に Nm を誤って
 *          再起床させないためのガードは ComM 側
 *          （`ComM_BusSM_ModeIndication()` の FULL_COM 分岐）に実装している。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerBusOff(uint8 ControllerId)
{
    DET_LOGT(TAG, "called");

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

    if (CanSM_State != CANSM_STATE_FULL_COM && CanSM_State != CANSM_STATE_SILENT_COM)
        return;

    /* 回復成功時にどちらへ戻すかの記録（CanSM_MainFunction() 参照）。
     * SILENT_COM 由来の場合、CanIf 側の PDU モード（CANIF_TX_OFFLINE）は
     * ここでは一切触らない。Can_Write() 自体がこの後コントローラ停止で
     * 拒否するため、TX 抑制の二重化は不要かつ、回復時にわざわざ
     * 再設定しなくて済む（下記コメント・CanSM_MainFunction() 参照）。 */
    CanSM_PreBusOffState = CanSM_State;

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
    CanSM_BusOffTimerMs = millis();

    ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);

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

    DET_LOGT(TAG, "called");

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
    if (CanIf_SetControllerMode(0U, CAN_CS_STOPPED) != E_OK)  /* CAN_CS_SLEEP -> CAN_CS_STOPPED (Listen-Only) */
    {
        /* 到達しないはずの経路（この分岐に来る時点で CanState==CAN_CS_SLEEP
         * であることは呼び出し元 Can_MainFunction_Wakeup() 側で保証されて
         * いる）。実際に失敗した場合、コントローラは Listen-Only へ遷移
         * していないため、検証タイマだけ回して確定させるのは危険。
         * CANSM_STATE_NO_COM のまま据え置き、次のウェイクアップ通知を
         * 待つ（2026-08 のレビューで指摘）。 */
        DET_LOGE(TAG, "ControllerWakeup E: CanIf_SetControllerMode(STOPPED) failed, staying in NO_COM");
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

    DET_LOGT(TAG, "called");

    if (!CanSM_Initialized)
    {
        Det_ReportError(CANSM_MODULE_ID, 0U, CANSM_API_ID_RX_INDICATION, CANSM_E_UNINIT);
        return;
    }

    if (CanSM_State != CANSM_STATE_WAKEUP_VALIDATING)
        return;

    DET_LOGI(TAG, "Wakeup validated (RX confirmed) -> FULL_COM");
    if (CanIf_SetControllerMode(0U, CAN_CS_STARTED) != E_OK)   /* CAN_CS_STOPPED -> CAN_CS_STARTED */
    {
        /* 到達しないはずの経路（CANSM_STATE_WAKEUP_VALIDATING に入っている
         * 時点で CanState==CAN_CS_STOPPED のはず）。失敗した場合に
         * CanSM_State を FULL_COM へ進めてしまうと、CanSM/ComM/EcuM は
         * 「成功した」と誤認したまま MCP2515 は Listen-Only のままで送信
         * できず、実害が静かに進行する（レビュー指摘の症状そのもの）。
         * WAKEUP_VALIDATING に留まり、タイムアウトで再スリープする
         * 既存のフェイルセーフ（CanSM_MainFunction）に委ねる。 */
        DET_LOGE(TAG, "RxIndication E: CanIf_SetControllerMode(STARTED) failed, staying in WAKEUP_VALIDATING");
        return;
    }
    /* WAKEUP_VALIDATING に入る直前の状態が SILENT_COM だった場合（眠る前に
     * TX を抑制していた）、CanIf の PDU モードは CANIF_TX_OFFLINE のまま
     * 変化していない。ここで明示的に CANIF_ONLINE へ戻さないと、起床後
     * FULL_COM に確定したのに TX だけ永久に塞がったままという静かなバグに
     * なる（2026-08 のレビュー方針同様、失敗しても DET のみで続行）。 */
    CanSM_SetPduModeOnlineBestEffort("RxIndication");
    CanSM_State         = CANSM_STATE_FULL_COM;
    CanSM_BusOffRetries = 0U;
    Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
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
    DET_LOGT(TAG, "called");

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
            /* CAN_T_SLEEP は失敗しない（CanSM_RequestComMode() の
             * COMM_NO_COMMUNICATION 分岐のコメント参照）。戻り値は明示的に無視する。 */
            (void)CanIf_SetControllerMode(0U, CAN_CS_SLEEP);  /* CAN_CS_STOPPED -> CAN_CS_SLEEP、ウェイクアップ割り込み再武装 */
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
    /* 回復成功を報告。デバウンス確定すれば CAN_BUSOFF の TF をクリアする
     * （CDTC/PDTC は上の FAILED 確定で既に立っていれば保持される。Dem.c の
     * PASSED デバウンス確定コメント参照）。 */
    Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);

    /* 2026-08 変更: Bus-Off は CANSM_STATE_FULL_COM だけでなく
     * CANSM_STATE_SILENT_COM からも起こりうるようになったため
     * （CanSM_ControllerBusOff() 参照）、回復成功時は Bus-Off 発生直前の
     * 状態（CanSM_PreBusOffState）へ戻す。Bus-Off 発生時点で ComM が既に
     * Nm へ解放を送信済み（ComM_NmReleasePending、ComM.c 参照）だった場合の
     * 「誤って Nm を再起床させない」ためのガードは ComM 側
     * （ComM_BusSM_ModeIndication() の FULL_COM 分岐）に実装している。 */
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
        /* 回復成功 → ComM に FULL_COM を通知 → EcuM_RequestRUN → RUN へ戻る
         * （ただし上記の Nm 解放ペンディング中だった場合は、ComM 側が代わりに
         * CanSM_RequestComMode(NO_COM) を呼び返して仕切り直す）。 */
        ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
    }
    /* 再度 Bus-Off が発生すれば CanIf → CanSM_ControllerBusOff() が呼ばれる */
}

void CanSM_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    DET_LOGT(TAG, "called");

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
