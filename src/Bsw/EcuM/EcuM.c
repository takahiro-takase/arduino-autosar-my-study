/**
 * \file    EcuM.c
 * \brief   ECU ステートマネージャ (AUTOSAR SWS_EcuStateManager 準拠)
 * \details BSW スタックの起動シーケンスと周期処理をカプセル化する。
 *          main.cpp が個々の BSW モジュールを直接制御することなく、
 *          EcuM_Init() と EcuM_MainFunction() の 2 つの API だけで
 *          ECU を起動・運転できるようにする。
 *
 *          起動シーケンス (EcuM_Init):
 *            0. Det_Init/Det_Start — 他の全 BSW モジュール初期化より前
 *                                (それらの Init 内での初期化前エラーも
 *                                report できる必要があるため)。Det.h 参照
 *            1. MemIf_Init     — Fee (Renesas RA) の EEPROM 抽象化層初期化
 *                                (NvM_Init より前、最初期)。EcuM 自身は
 *                                Fee という具体名を意識しない（詳細は
 *                                MemIf.c 冒頭のコメント参照）
 *            2. NvM_Init       — EEPROM → RAM ミラー一括ロード
 *            3. Port_Init      — ピン方向設定（Dio 操作より前に完了）
 *            4. Adc_Init       — ADC 初期化（Port と同じ MCAL 下位層。
 *                                IoHwAb_Init より前。本実装はハードウェア状態を
 *                                持たないためシグネチャ適合のみが目的）
 *            5. Gpt_Init       — HW タイマ準備（他モジュールに依存しない下位ドライバ。
 *                                App_GptDemo_Init だけでなく Os_Init もこの完了が
 *                                前提。Os は自身の専用チャネルを Os_Init 内で
 *                                Gpt_StartTimer するため、Gpt_Init 自体はここで
 *                                済ませておけば十分）
 *            6. Can_Init       — CAN コントローラ初期化（バスはまだ非アクティブ）
 *            7. CanIf_Init     — CAN インタフェース初期化
 *            8. PduR_Init      — PDU ルータ初期化
 *            9. Crypto_Init    — Crypto Driver 初期化（AES-128 自己診断を含む。
 *                                Csm/CryIf/Crypto レイヤの最下層）
 *           10. CryIf_Init     — Crypto Interface 初期化
 *           11. Csm_Init       — Crypto Service Manager 初期化
 *           12. KeyM_Init      — Key Manager 初期化（Csm の後。KeyM は
 *                                Csm_KeyElementSet/Csm_KeySetValid のみを呼び、
 *                                CryIf/Crypto の存在を知らない）
 *           13. SecOC_Init     — セキュアオンボード通信初期化（PduR の後・Com の前。
 *                                PduR→SecOC→Com の依存順。SecOC は Csm_MacGenerate/
 *                                Csm_MacVerify のみを呼び、Crypto/CryIf の存在を知らない）
 *           14. Com_Init       — COM モジュール初期化
 *           15. E2EXf_PBCfg_Init — E2E Check/Protect ステート初期化
 *                                （E2E Transformer 方式。Com は E2E を関知しないため
 *                                  Com_Init の直後にここで別途初期化する）
 *           16. CanTp_Init     — CAN トランスポートプロトコル初期化
 *           17. Dcm_Init       — 診断通信モジュール初期化
 *           18. Dem_Init       — NvM 経由で DTC ステータスを復元
 *           19. FiM_Init       — 機能抑止状態を初期化 (Dem_Init の後)
 *           20. BswM_Init      — ルールエンジン初期化。ComM_Init/ComM_RequestComMode
 *                                より前が必須（下記参照）
 *           21. ComM_Init      — 通信マネージャ初期化 (NO_COM 状態)
 *           22. ComM_CommunicationAllowed(TRUE) — [SWS_ComM_00871] 通信許可を
 *                                通知する。ComM_Init() 直後の既定値は FALSE
 *                                （[SWS_ComM_00884]）のため、これを呼ばないと
 *                                以降の ComM_RequestComMode(FULL_COM) は
 *                                CanSM へ届かず保留され続ける。本プロジェクトは
 *                                EcuM/Fixed 相当（動的な許可制御は未導入）の
 *                                ため、起動時に一度だけ許可し以後は常時許可のまま
 *                                運用する（ComM.c 冒頭のコメント参照）
 *           23. CanNm_Init        — CanNm 状態機械初期化 (Bus-Sleep Mode で開始)。
 *                                ComM_RequestComMode(FULL_COM) より必ず前に置くこと。
 *                                同要求は同期的に CanSM_RequestComMode →
 *                                ComM_BusSM_ModeIndication(FULL_COM) → Nm_NetworkRequest()
 *                                → CanNm_NetworkRequest() まで連鎖するため、これより後だと
 *                                CanNm 未初期化のまま呼ばれて失敗し、CanNm が Bus-Sleep
 *                                Mode に固着する（実機で確認された不具合）
 *           24. Nm_Init           — 汎用 Nm 層初期化。CanNm_Init の直後、
 *                                ComM_RequestComMode(FULL_COM) より必ず前に置くこと
 *                                （上記と同じ理由。Nm_NetworkRequest() が NM_E_UNINIT
 *                                で失敗すると Nm 層より下の CanNm まで要求が届かない）
 *               StbM_Init         — 同期時間ベースの初期化。CanTSyn_Init() より前に置くこと。
 *               CanTSyn_Init      — 時刻同期（時間マスター・時間スレーブ）初期化。CanIf_Init() の後で
 *                                あればよい（CanIf の TX PDU を使うだけで、他モジュールの
 *                                状態に依存しない）。ComM_RequestComMode() より前に置くのは
 *                                最初の SYNC を CAN の開通後に送るためではなく、全 Init を
 *                                ComM_RequestComMode(FULL_COM) 前に済ませる既存の方針に合わせたもの
 *           25. ComM_RequestComMode(FULL_COM) — CAN バス通信開始
 *                               （全上位層初期化後に開始することで
 *                                 フレーム到着時の未初期化アクセスを防ぐ。
 *                                 ComM_BusSM_ModeIndication 経由で BswM_ExecuteRules が
 *                                 同期的に呼ばれるため BswM_Init 済みである必要がある）
 *           26. Rte_Start/Rte_Init_EngineManager — RTE 自身の初期化と
 *                                EngineManager SW-C の Init Runnable 起動
 *                                （[SWS_Rte_02569]/[SWS_Rte_06749]、2026-09
 *                                追加。以前は EcuM がApp_EngineManager_Init()
 *                                を直接呼んでいた。Rte.c の Rte_Start() 付近の
 *                                「RTE ライフサイクル API」コメント参照）
 *           27. IoHwAb_Init    — I/O ハードウェア抽象化層初期化 (LED チャネル設定)
 *           28. Rte_Init_WarningIndicator — 警告灯 SW-C の Init Runnable 起動
 *                                (IoHwAb_Init() の後であること、同上)
 *           29. Wdg_Init       — Watchdog Driver 初期化（WdgM_Init より前。
 *                                コンフィグの記録のみ行い、HW はまだ有効化しない）
 *           30. WdgM_Init      — Alive/Logical Supervision 初期化。
 *                                実 HW ウォッチドッグもここで有効化する
 *                                （他の全モジュール初期化完了後、最後に有効化することで
 *                                  初期化処理自体がタイムアウトの影響を受けないようにする）
 *           31. Os_Init        — タスクスケジューラ初期化 (全モジュール初期化後)。
 *                                自身の Gpt チャネル (GPT_CHANNEL_1) を起動し、
 *                                以後のタスク周期判定は millis() ではなくこの
 *                                チャネルのティック値で行う（Os.c 冒頭のコメント参照）
 *
 *          周期処理 (EcuM_MainFunction):
 *            Os_SchedulerStep() — タスクテーブルに従い周期到来タスクを実行
 *              Task 0: Can_MainFunction_Read (1 ms) — RX 割り込みペンディングのドレイン
 *              Task 1: CanTp_MainFunction (1 ms)    — タイムアウト監視・CF 送信
 *              Task 2: Rte_ScheduleRunnables (3000 ms) — エンジン Runnable 起動
 *              Task 3: Rte_ScheduleWarningIndicator (500 ms) — 警告灯 Runnable 起動
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

#include "EcuM.h"
#include "EcuM_Cfg.h"
#include "BswM.h"
#include "BswM_PBCfg.h"
#include "WdgM.h"
#include "WdgM_PBCfg.h"
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "MemIf.h"
#include "NvM.h"
#include "Os.h"
#include "Os_PBCfg.h"
#include "Can.h"
#include "Can_PBCfg.h"
#include "CanIf.h"
#include "CanIf_PBCfg.h"
#include "PduR.h"
#include "PduR_PBCfg.h"
#include "Crypto.h"
#include "CryIf.h"
#include "Csm.h"
#include "KeyM.h"
#include "SecOC.h"
#include "SecOC_PBCfg.h"
#include "Com.h"
#include "Com_PBCfg.h"
#include "E2EXf_PBCfg.h"
#include "E2EMon.h"
#include "CanTp.h"
#include "Dcm.h"
#include "Dem.h"
#include "FiM.h"
#include "FiM_PBCfg.h"
#include "Port.h"
#include "Adc.h"
#include "Gpt.h"
#include "Gpt_PBCfg.h"
#include "CanSM.h"
#include "ComM.h"
#include "CanNm.h"
#include "CanTSyn.h"
#include "StbM.h"
#include "Nm.h"
#include "Rte.h"
#include "IoHwAb.h"
#include "App_EngineManager.h"
#include "App_WarningIndicator.h"
#include "App_GptDemo.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "EcuM"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */


/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */

/** 現在の EcuM フェーズ */
static EcuM_StateType  EcuM_State          = ECUM_STATE_STARTUP;

/** RUN 要求中ユーザのビットマスク (bit N = ECUM_USER_N が要求中) */
static uint8           EcuM_RunUsers       = 0U;

/** POST_RUN 要求中ユーザのビットマスク。[SWS_EcuM_04128]により RUN 要求とは
 *  独立して追跡する（EcuM_RunUsers とは別変数）。1件でも立っていれば
 *  POST_RUN → SHUTDOWN の自動タイムアウトを保留する。 */
static uint8           EcuM_PostRunUsers   = 0U;

/** POST_RUN フェーズ開始時刻 (ms) */
static unsigned long   EcuM_PostRunTimerMs = 0UL;

/** 検証待ちの起床要因のビットマスク（`EcuM_CheckWakeup()` が立て、
 *  `EcuM_ValidateWakeupEvent()` が下ろす）。立っている間、`EcuM_MainFunction()` が
 *  毎回 `CanIf_CheckValidation()` で検証を問い合わせる。 */
static EcuM_WakeupSourceType EcuM_PendingWakeupSources = 0U;

/** 検証済み（VALIDATED）の起床要因のビットマスク（`EcuM_ValidateWakeupEvent()` が立てる、[SWS_EcuM_02496]）。 */
static EcuM_WakeupSourceType EcuM_ValidatedWakeupSources = 0U;

/** 期限切れ（EXPIRED）の起床要因のビットマスク。検証のタイムアウトで、`EcuM_MainFunction()` が立てる
 *  （[SWS_EcuM_04081]）。 */
static EcuM_WakeupSourceType EcuM_ExpiredWakeupSources = 0U;

/** 検証のタイムアウトの起算時刻 (ms)。検証待ちが無い状態から、検証待ちになった時刻
 *  （[SWS_EcuM_02709]。タイマは 1 つで、検証待ちの間の再通知では起算し直さない、[SWS_EcuM_02712]）。 */
static unsigned long EcuM_ValidationStartMs = 0UL;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

static unsigned long EcuM_GetNowMs(void);
static void EcuM_SetWdgMMode(WdgM_ModeType mode);

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * EcuM_Init
 * ---------------------------------------------------------------------- */

/**
 * \brief   BSW スタック全体を起動フェーズ順に初期化する。
 *
 * \details AUTOSAR の依存関係順（下位層から上位層）に各モジュールの
 *          _Init 関数を呼び出す。
 *          - Det_Init/Det_Start: 他の全モジュールの Init より前に呼ぶ
 *            （それらの Init 内での初期化前エラーも report できるように
 *            するため）。
 *          - MemIf_Init: NvM_Init より前に、EEPROM 抽象化層 (Fee) を
 *            初期化する。
 *          - NvM_Init: 全 NvM ブロックを EEPROM から RAM ミラーへ一括ロードする。
 *          - Can_Init: CAN コントローラを初期化する（LISTEN_ONLY 状態で待機）。
 *            CanIf/PduR/Com 等の上位層初期化前に CAN バスをアクティブにしないため、
 *            Can_SetControllerMode() は直接呼ばず ComM 経由で行う。
 *          - CanIf_Init 〜 Dem_Init: 各 BSW モジュールを下位層から順に初期化。
 *          - ComM_Init: 通信マネージャを NO_COM 状態で初期化する。
 *          - ComM_RequestComMode(FULL_COM): 全上位層が初期化された後で CAN バスを
 *            アクティブにする。これにより起動直後のフレーム受信でも上位層が
 *            正しく処理できる。
 *
 * \pre        Arduino ランタイムが初期化済みであること（setup() の先頭で呼ぶ想定）。
 * \note       AUTOSAR EcuM では StartupOne (OS 起動前) と
 *             StartupTwo (OS 起動後) に分かれるが、本実装では一本化している。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_Init(void)
{
    Det_Init(NULL);  /* 他の全 BSW モジュール初期化より前（それらの Init 内での
                       * 初期化前エラーも report できる必要があるため）。Det.h 参照 */
    Det_Start();
    MemIf_Init();   /* NvM_Init より前: EEPROM 抽象化層 (Fee) を初期化 */
    NvM_Init(NULL); /* [SWS_NvM_00881]: ConfigPtr は常に NULL（2026-09 是正。
                      * 以前は&NvM_Configを渡し実際に依存していた。NvM.c 参照） */
    Port_Init(NULL);                    /* ピン方向設定（Dio 操作より前に完了）    */
    Adc_Init(NULL);                     /* ADC 初期化（Port と同じ MCAL 下位層。IoHwAb より前） */
    Gpt_Init(&Gpt_Config);           /* HW タイマ準備（他モジュールに依存しない下位ドライバ） */
    Can_Init(&Can_Config);          /* ハードウェア初期化（LISTEN_ONLY で待機） */
    CanIf_Init(&CanIf_Config);
    PduR_Init(&PduR_Config);
    Crypto_Init();  /* Csm/CryIf/Crypto レイヤの最下層。AES-128 自己診断を含む */
    CryIf_Init();
    Csm_Init();
    KeyM_Init(NULL); /* [SWS_KeyM_00158]: ConfigPtr は常に NULL_PTR */
    SecOC_Init(&SecOC_Config); /* PduR の後、Com の前（PduR→SecOC→Com の依存順） */
    Com_Init(&Com_Config);
    E2EXf_PBCfg_Init();       /* E2E Check/Protect ステート初期化（Com は E2E を関知しないため） */
    E2EMon_Init();            /* E2E 検証ネットワーク健全性モニタ（CDD 相当、Com_Init 済みが前提） */
    CanTp_Init(NULL);
    Dcm_Init(NULL);
    Dem_Init(NULL);
    FiM_Init(&FiM_Config);                                    /* Dem_Init の後（Dem 状態を参照するため） */
    CanSM_Init(NULL);                                             /* NO_COM 状態で開始 */
    BswM_Init(&BswM_Config);  /* ComM_Init/ComM_RequestComMode より前に必須:
                               * ComM_RequestComMode() → CanSM_RequestComMode() →
                               * ComM_BusSM_ModeIndication() → BswM_ComM_CurrentMode() →
                               * BswM_ExecuteRules() が同期的に連鎖し BswM_Cfg を
                               * 参照するため、これより後に置くと起動毎に
                               * NULL 参照となる。 */
    ComM_Init(NULL);
    ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);  /* [SWS_ComM_00871]:
                               * ComM_Init() 直後の既定値は FALSE
                               * （[SWS_ComM_00884]）のため、これを呼ばないと
                               * 下記 ComM_RequestComMode(FULL_COM) が CanSM へ
                               * 届かず保留され続ける（ComM.c 冒頭のコメント
                               * 参照）。本プロジェクトは EcuM/Fixed 相当で
                               * 動的な許可制御を持たないため、起動時に一度
                               * だけ許可する。 */
    CanNm_Init(NULL);             /* ComM_RequestComMode() より必ず前に置くこと。
                               * ComM_RequestComMode(FULL_COM) は同期的に
                               * CanSM_RequestComMode → ComM_BusSM_ModeIndication(FULL_COM)
                               * → Nm_NetworkRequest() → CanNm_NetworkRequest() まで
                               * 連鎖するため、ここより後に置くと CanNm 未初期化のため
                               * CanNm_NetworkRequest() が CANNM_E_UNINIT で失敗し、
                               * CanNm が Bus-Sleep Mode に固着したまま起動する
                               * （実機で確認された不具合）。 */
    Nm_Init(NULL);                /* CanNm_Init() の直後、ComM_RequestComMode() より
                               * 必ず前に置くこと（上記と同じ理由。Nm_NetworkRequest()
                               * が NM_E_UNINIT で失敗すると Nm 層より下の CanNm
                               * まで要求が届かない）。 */
    StbM_Init(NULL);              /* 同期時間ベース。CanTSyn_Init() より前（CanTSyn が時刻を読む） */
    CanTSyn_Init(NULL);           /* 時刻同期（時間マスター・時間スレーブ）。CanIf_Init() 後ならよい */
    (void)ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION);/* 全層初期化後に開通 */
    (void)Rte_Start();                /* RTE 自身の初期化（[SWS_Rte_02569]）。SW-C の
                                  * Init Runnable 起動より必ず前に置くこと
                                  * （[SWS_Rte_CONSTR_09035]、詳細は Rte.c の
                                  * Rte_Start() 付近の「RTE ライフサイクル API」コメント参照） */
    Rte_Init_EngineManager();   /* App_EngineManager_Init() を Rte 経由で起動
                                  * （[SWS_Rte_06749]相当、2026-09 是正。以前は
                                  * EcuM_Init() が直接呼んでいた） */
    IoHwAb_Init(NULL);
    Rte_Init_WarningIndicator();/* App_WarningIndicator_Init() を Rte 経由で起動。
                                  * IoHwAb_Init() 完了後であること（同上） */
    App_GptDemo_Init();  /* Gpt_Init 済みが前提。実 HW タイマの動作確認用デモ。
                           * Rte のポートを経由しない単独モジュールのため、他の
                           * BSW モジュール Init と同じく EcuM が直接呼ぶ
                           * （Rte.c の Rte_Start() 付近の「RTE ライフサイクル API」コメント参照）。 */
    Wdg_Init(&Wdg_Config);    /* WdgM_Init より前: Watchdog Driver 初期化 (HW はまだ有効化しない) */
    WdgM_Init(&WdgM_Config);  /* Alive Supervision 初期化 (Os_Init より前) */
    Os_Init(&Os_Config);      /* タスクテーブル初期化 (全タスク有効で起動)。
                               * 内部で GPT_CHANNEL_1 を起動するため Gpt_Init 済みが前提。 */

    /* 全モジュール初期化完了 → RUN フェーズへ遷移
     * ComM_RequestComMode(FULL_COM) がすでに EcuM_RequestRUN を呼んでいるため
     * EcuM_RunUsers の ECUM_USER_COMM ビットはここで既に立っている。         */
    EcuM_State = ECUM_STATE_RUN;
    DET_LOGI(TAG, "->RUN");
    BswM_EcuM_CurrentState(ECUM_STATE_RUN);  /* Rule 0: 全タスク有効化 */
}

/* ----------------------------------------------------------------------
 * EcuM_MainFunction
 * ---------------------------------------------------------------------- */

/**
 * \brief   BSW スタックの周期処理を実行する。
 *
 * \details Arduino の loop() から毎ループ呼び出される。
 *
 * \pre        EcuM_Init() が正常完了していること。
 * \note       AUTOSAR 標準の EcuM_MainFunction は主に状態遷移管理を行うが、
 *             本実装では BSW ポーリングと RTE スケジューリングを担う。
 *
 * \ServiceID      {0x18}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_MainFunction(void)
{
    if (EcuM_PendingWakeupSources != 0U)
    {
        /* 起床要因の検証（[SWS_CANIF_00178]）。起床後に有効な受信があれば、CanIf が
         * EcuM_ValidateWakeupEvent() を呼び返す。実仕様では EcuM_CheckValidation() という
         * コールアウト経由だが、ウェイクアップソース管理を持たない本プロジェクトでは、
         * CAN の起床だけなので CanIf を直接呼ぶ。 */
        (void)CanIf_CheckValidation(EcuM_PendingWakeupSources);

        /* 検証のタイムアウト（[SWS_EcuM_04081]）。上の問い合わせで検証されなかった要因は、期限切れにする。
         * 簡略化: 実仕様は、続けて BswM_EcuM_CurrentWakeup(要因, EXPIRED)（[SWS_EcuM_04082]）で BswM へ通知し、
         * EcuM_StopWakeupSources() を呼ぶが、本プロジェクトの EcuM は BswM への起床要因の通知を持たない
         * （CanSM が、自分の検証の時間でコントローラをスリープへ戻す）。 */
        if ((EcuM_PendingWakeupSources != 0U)
            && ((EcuM_GetNowMs() - EcuM_ValidationStartMs) >= ECUM_VALIDATION_TIMEOUT_MS))
        {
            EcuM_ExpiredWakeupSources |= EcuM_PendingWakeupSources;
            DET_LOGW(TAG, "wakeup validation expired (sources=0x%08lX, %lums)",
                     (unsigned long)EcuM_PendingWakeupSources, (unsigned long)ECUM_VALIDATION_TIMEOUT_MS);
            EcuM_PendingWakeupSources = 0U;
        }
    }

    switch (EcuM_State)
    {
        case ECUM_STATE_RUN:
            Os_SchedulerStep();
            break;

        case ECUM_STATE_POST_RUN:
            Os_SchedulerStep();
            /* EcuM_PostRunUsers に1件でも要求が残っていれば SHUTDOWN への
             * 自動タイムアウトを保留する（[SWS_EcuM_04128]）。 */
            if ((EcuM_PostRunUsers == 0U) && (EcuM_GetNowMs() - EcuM_PostRunTimerMs) >= ECUM_POST_RUN_TIMEOUT_MS)
            {
                EcuM_State = ECUM_STATE_SHUTDOWN;
                DET_LOGI(TAG, "->SHUTDOWN");
                BswM_EcuM_CurrentState(ECUM_STATE_SHUTDOWN);  /* Rule 2: WdgM_MainFunction 以外を無効化 */
            }
            break;

        case ECUM_STATE_SHUTDOWN:
            /* Arduino では電源断不可のためアイドルで待機するが、Os_SchedulerStep()
             * 自体は呼び続ける。BswM Rule 2 が WdgM_MainFunction 以外の全タスクを
             * 無効化済みのため、実質的に動くのはこの 1 タスクだけ。
             * ここで Os_SchedulerStep() を止めてしまうと、タスクの有効/無効に関係なく
             * 誰も呼ばれなくなり、無効化する手段のない Renesas RA の IWDT が
             * リフレッシュされずタイムアウトしてしまう（実機検証で確認済みの不具合）。 */
            Os_SchedulerStep();
            break;

        default:
            break;
    }
}

/* ----------------------------------------------------------------------
 * EcuM_GetState
 * ---------------------------------------------------------------------- */

EcuM_StateType EcuM_GetState(void)
{
    return EcuM_State;
}

/* ----------------------------------------------------------------------
 * EcuM_RequestRUN
 * ---------------------------------------------------------------------- */

Std_ReturnType EcuM_RequestRUN(EcuM_UserType user)
{
    if (user >= ECUM_USER_COUNT)
    {
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_REQUEST_RUN, ECUM_E_INVALID_PAR);
        return E_NOT_OK;
    }

    const uint8 mask = (uint8)(1U << user);

    if ((EcuM_RunUsers & mask) != 0U)
    {
        /* SWS_EcuM_04125: 同一ユーザからの要求はネストできない。既に
         * このユーザの RUN 要求が立っている状態での追加要求は DET
         * (ECUM_E_MULTIPLE_RUN_REQUESTS) へ報告し、E_NOT_OK を返す。
         * 重複時点で EcuM_State は必ず RUN である（POST_RUN/SHUTDOWN は
         * 全ユーザ解放後にのみ到達するため、この分岐に来た時点で
         * user のビットが立っているなら状態遷移の余地はない）。 */
        DET_LOGW(TAG, "RequestRUN E: multiple request user=%u", (unsigned)user);
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_REQUEST_RUN, ECUM_E_MULTIPLE_RUN_REQUESTS);
        return E_NOT_OK;
    }

    EcuM_RunUsers |= mask;

    /* POST_RUN 中に要求が来たら RUN へ戻る */
    if (EcuM_State == ECUM_STATE_POST_RUN)
    {
        EcuM_State = ECUM_STATE_RUN;
        DET_LOGI(TAG, "->RUN user=%u", (unsigned)user);
        /* Rte_Engine / Rte_Warning タスク（WdgM の監視対象、Entity 0/1）が
         * 再開するため、POST_RUN 移行時に無効化した HW ウォッチドッグを
         * 再度有効化する（WDGM_MODE_DEFAULT）。その前に WdgM_ResumeSupervision() で
         * チェックポイント基準をリセットしておく必要がある。POST_RUN 中の停止時間がそのまま
         * 残っていると、再開後最初のチェックポイントで Deadline Supervision が
         * 「停止時間」を実際の処理時間と誤認し、誤って FAILED と判定してしまう
         * （全エンティティ分まとめてリセットされる、WdgM.c 参照）。 */
        WdgM_ResumeSupervision();
        EcuM_SetWdgMMode(WDGM_MODE_DEFAULT);
        BswM_EcuM_CurrentState(ECUM_STATE_RUN);  /* Rule 0: 全タスク再有効化 */
        /* この POST_RUN サイクルは RUN 復帰により終了したため、残っている
         * POST_RUN 要求は今後の SHUTDOWN 判定にとって無意味になる。クリア
         * しないと、次に POST_RUN へ再突入した際にこの古いビットのせいで
         * SHUTDOWN への自動タイムアウトが永久に保留されてしまう
         * （/code-review 指摘）。 */
        EcuM_PostRunUsers = 0U;
    }
    /* SHUTDOWN 中に要求が来たら RUN へ戻る（CAN バスのウェイクアップ経由）。
     * CanSM_ControllerModeIndication() → ComM_BusSM_ModeIndication(FULL_COM) →
     * EcuM_RequestRUN(ECUM_USER_COMM) という経路からの復帰を許可する。
     * POST_RUN からの復帰と同じ後処理（チェックポイント基準リセット・
     * HW ウォッチドッグ再有効化・全タスク再有効化）を行う。 */
    else if (EcuM_State == ECUM_STATE_SHUTDOWN)
    {
        EcuM_State = ECUM_STATE_RUN;
        DET_LOGI(TAG, "SHUTDOWN ->RUN (wakeup) user=%u", (unsigned)user);
        WdgM_ResumeSupervision();
        EcuM_SetWdgMMode(WDGM_MODE_DEFAULT);
        BswM_EcuM_CurrentState(ECUM_STATE_RUN);  /* Rule 0: 全タスク再有効化 */
        EcuM_PostRunUsers = 0U;  /* 前回 POST_RUN サイクルの残留要求をクリア（上記と同じ理由） */
    }
    else
    {
        /* EcuM_State == ECUM_STATE_RUN: 既に RUN 中の別ユーザからの追加要求
         * のため、状態遷移は不要（MISRA 15.7 準拠のための防御的な明示）。 */
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * EcuM_ReleaseRUN
 * ---------------------------------------------------------------------- */

Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user)
{
    if (user >= ECUM_USER_COUNT)
    {
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_RELEASE_RUN, ECUM_E_INVALID_PAR);
        return E_NOT_OK;
    }

    const uint8 mask = (uint8)(1U << user);

    if ((EcuM_RunUsers & mask) == 0U)
    {
        /* SWS_EcuM_04127: 対応する要求なしの解放は DET
         * (ECUM_E_MISMATCHED_RUN_RELEASE) へ報告し、E_NOT_OK を返す。 */
        DET_LOGW(TAG, "ReleaseRUN E: mismatched release user=%u", (unsigned)user);
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_RELEASE_RUN, ECUM_E_MISMATCHED_RUN_RELEASE);
        return E_NOT_OK;
    }

    EcuM_RunUsers &= (uint8)(~mask);

    /* 全ユーザが解放した場合 POST_RUN へ遷移 */
    if ((EcuM_RunUsers == 0U) && (EcuM_State == ECUM_STATE_RUN))
    {
        EcuM_State          = ECUM_STATE_POST_RUN;
        EcuM_PostRunTimerMs = EcuM_GetNowMs();
        DET_LOGI(TAG, "->POST_RUN timeout=%lums", ECUM_POST_RUN_TIMEOUT_MS);
        /* Rule 1 で Rte_Engine / Rte_Warning タスク（WdgM の監視対象、
         * Entity 0/1 双方）が停止するため、ここで HW ウォッチドッグも
         * 無効化する。POST_RUN 中はタスクが意図的に停止しており、
         * 両エンティティとも Alive Supervision は必ず FAILED になるため、
         * SHUTDOWN 遷移（最大 ECUM_POST_RUN_TIMEOUT_MS 後）を待つと
         * HW ウォッチドッグのタイムアウト（WDGM_HW_WATCHDOG_TIMEOUT_MS=4000ms）
         * より先に意図しないリセットが発生し得る。POST_RUN への移行そのものを
         * 安全な無効化ポイントとする。 */
        EcuM_SetWdgMMode(WDGM_MODE_POST_RUN);
        BswM_EcuM_CurrentState(ECUM_STATE_POST_RUN);  /* Rule 1: アプリタスク無効化 */
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * EcuM_RequestPOST_RUN
 * ---------------------------------------------------------------------- */

/**
 * \brief   POST_RUN フェーズの継続を要求する。
 *
 * \details [SWS_EcuM_04128]: RUN 要求とは独立した `EcuM_PostRunUsers`
 *          ビットマスクで追跡する。1件でも残っていれば
 *          EcuM_MainFunction() の SHUTDOWN 自動タイムアウトを保留する
 *          （詳細は EcuM.h の宣言側コメント参照）。
 *
 * \AUTOSARReq     {SWS_EcuM_04128}
 * \ServiceID      {0x0a}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType EcuM_RequestPOST_RUN(EcuM_UserType user)
{
    if (user >= ECUM_USER_COUNT)
    {
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_REQUEST_POST_RUN, ECUM_E_INVALID_PAR);
        return E_NOT_OK;
    }

    const uint8 mask = (uint8)(1U << user);

    if ((EcuM_PostRunUsers & mask) != 0U)
    {
        DET_LOGW(TAG, "RequestPOST_RUN E: multiple request user=%u", (unsigned)user);
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_REQUEST_POST_RUN, ECUM_E_MULTIPLE_RUN_REQUESTS);
        return E_NOT_OK;
    }

    EcuM_PostRunUsers |= mask;
    DET_LOGI(TAG, "POST_RUN requested user=%u", (unsigned)user);
    return E_OK;
}

/* ----------------------------------------------------------------------
 * EcuM_ReleasePOST_RUN
 * ---------------------------------------------------------------------- */

/**
 * \brief   POST_RUN フェーズの継続要求を解放する。
 *
 * \details [SWS_EcuM_04129]。最後の1件を解放した瞬間から改めてタイムアウトを
 *          起算する（詳細は EcuM.h の宣言側コメント参照）。
 *
 * \AUTOSARReq     {SWS_EcuM_04129}
 * \ServiceID      {0x0b}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType EcuM_ReleasePOST_RUN(EcuM_UserType user)
{
    if (user >= ECUM_USER_COUNT)
    {
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_RELEASE_POST_RUN, ECUM_E_INVALID_PAR);
        return E_NOT_OK;
    }

    const uint8 mask = (uint8)(1U << user);

    if ((EcuM_PostRunUsers & mask) == 0U)
    {
        DET_LOGW(TAG, "ReleasePOST_RUN E: mismatched release user=%u", (unsigned)user);
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_RELEASE_POST_RUN, ECUM_E_MISMATCHED_RUN_RELEASE);
        return E_NOT_OK;
    }

    EcuM_PostRunUsers &= (uint8)(~mask);

    /* 最後の1件を解放した瞬間から改めてタイムアウトを起算する（保留していた
     * 分だけ SHUTDOWN までの猶予を必ず確保するため）。 */
    if ((EcuM_PostRunUsers == 0U) && (EcuM_State == ECUM_STATE_POST_RUN))
    {
        EcuM_PostRunTimerMs = EcuM_GetNowMs();
        DET_LOGI(TAG, "POST_RUN released user=%u, timeout restarted", (unsigned)user);
    }
    return E_OK;
}

/* ----------------------------------------------------------------------
 * EcuM_CheckWakeup
 * ---------------------------------------------------------------------- */

/**
 * \brief   ウェイクアップ要因を確認する（[SWS_Can_00271]、CAN ドライバの
 *          ウェイクアップ割り込み/ポーリング文脈からの直接通知先）。
 *          旧 `CanIf_ControllerWakeup`（実仕様に存在しない独自 API だった、
 *          2026-09-05 是正）の役割を引き継ぐ。
 *
 * \details 実仕様の `EcuM_CheckWakeup()` は EcuM 本体ではなく ECU 統合側が
 *          実装する「コールアウト」（EcuM 自身がポーリング時に、CAN
 *          ドライバの ISR/Can_MainFunction_Wakeup が割り込み時にそれぞれ
 *          呼ぶ）と位置づけられるが、本プロジェクトは単一 ECU 構成で、EcuM の
 *          ウェイクアップソース管理のうち、検証待ち（PENDING）・検証済み（VALIDATED）・
 *          期限切れ（EXPIRED）の保持と、検証のタイムアウト（`ECUM_VALIDATION_TIMEOUT_MS`）だけを
 *          持つ（`EcuM_SetWakeupEvent` の代わりに本関数が、検証待ちにして、タイムアウトを起算する。
 *          BswM への通知 `BswM_EcuM_CurrentWakeup()` は持たない）ため、EcuM.c 内の通常関数として
 *          簡略実装する。
 *
 *          本プロジェクトで実際にサポートするウェイクアップ要因は CAN バス
 *          起床（`ECUM_WKSOURCE_CAN`）のみである。実際の検証は、Listen-Only
 *          によるノイズ除去（`CanSM_ControllerModeIndication()` 側の既存の
 *          ステートマシン）と、有効フレーム受信の確認（`CanIf_CheckValidation()`
 *          → `EcuM_ValidateWakeupEvent()` → `ComM_EcuM_WakeUpIndication()`
 *          → `CanSM_RequestComMode(FULL_COM)` による FULL_COM 確定）が担うため、
 *          本関数は起床要因を検証待ちとして保留し、CanSM へ委譲する。
 *
 * \param[in]  wakeupSource  検出されたウェイクアップ要因のビットマスク。
 *                           `ECUM_WKSOURCE_CAN` ビットが立っていない場合は
 *                           未知の要因として `ECUM_E_INVALID_PAR` を DET へ
 *                           報告し、何もしない（本プロジェクトが持つ
 *                           ウェイクアップ要因は CAN バス起床のみのため）。
 *
 * \pre        `Can_MainFunction_Wakeup()`（または `Can_Isr()`）の文脈から
 *             呼ぶこと（[SWS_Can_00271]）。
 *
 * \AUTOSARReq     {SWS_Can_00271}
 * \ServiceID      {0x42}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_CheckWakeup(EcuM_WakeupSourceType wakeupSource)
{

    if ((wakeupSource & ECUM_WKSOURCE_CAN) == 0U)
    {
        DET_LOGW(TAG, "CheckWakeup: unknown source=0x%08lX", (unsigned long)wakeupSource);
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_CHECK_WAKEUP, ECUM_E_INVALID_PAR);
        return;
    }

    DET_LOGI(TAG, "CheckWakeup: CAN wakeup source confirmed");
    if ((EcuM_PendingWakeupSources & ECUM_WKSOURCE_CAN) == 0U)
    {
        /* 新しい起床: 検証のタイムアウトを起算する（[SWS_EcuM_02709]）。同じ要因が検証待ちの間の再通知では、起算し直さない
         * （[SWS_EcuM_02712]）。前回の検証済み・期限切れの記録は、新しい起床で消す。 */
        EcuM_ValidationStartMs = EcuM_GetNowMs();
        EcuM_ClearWakeupEvent(ECUM_WKSOURCE_CAN);   /* 検証待ちは、この分岐では立っていないので、消えるのは、検証済みと期限切れ */
    }
    EcuM_PendingWakeupSources |= ECUM_WKSOURCE_CAN;   /* 検証待ち。EcuM_MainFunction() が検証を問い合わせる */
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);
}

/* ----------------------------------------------------------------------
 * EcuM_GetVersionInfo
 * ---------------------------------------------------------------------- */

void EcuM_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_GET_VERSION_INFO, ECUM_E_NULL_POINTER);
        return;
    }

    versioninfo->vendorID         = ECUM_VENDOR_ID;
    versioninfo->moduleID         = ECUM_MODULE_ID;
    versioninfo->sw_major_version = ECUM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = ECUM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = ECUM_SW_PATCH_VERSION;
}

/* ----------------------------------------------------------------------
 * EcuM_GoDown
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GoHalt
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GoPoll
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_StartupTwo
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_Shutdown
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_SetState
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_SelectShutdownTarget
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GetShutdownTarget
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GetLastShutdownTarget
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_SelectShutdownCause
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GetShutdownCause
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GetPendingWakeupEvents
 * ---------------------------------------------------------------------- */

EcuM_WakeupSourceType EcuM_GetPendingWakeupEvents(void)
{
    return EcuM_PendingWakeupSources;
}

/* ----------------------------------------------------------------------
 * EcuM_ClearWakeupEvent
 * ---------------------------------------------------------------------- */

void EcuM_ClearWakeupEvent(EcuM_WakeupSourceType sources)
{
    const EcuM_WakeupSourceType keep = (EcuM_WakeupSourceType)(~sources);

    EcuM_PendingWakeupSources   &= keep;
    EcuM_ValidatedWakeupSources &= keep;
    EcuM_ExpiredWakeupSources   &= keep;
}

/* ----------------------------------------------------------------------
 * EcuM_GetValidatedWakeupEvents
 * ---------------------------------------------------------------------- */

EcuM_WakeupSourceType EcuM_GetValidatedWakeupEvents(void)
{
    return EcuM_ValidatedWakeupSources;
}

/* ----------------------------------------------------------------------
 * EcuM_GetExpiredWakeupEvents
 * ---------------------------------------------------------------------- */

EcuM_WakeupSourceType EcuM_GetExpiredWakeupEvents(void)
{
    return EcuM_ExpiredWakeupSources;
}

/* ----------------------------------------------------------------------
 * EcuM_SetRelWakeupAlarm
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_SetAbsWakeupAlarm
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_AbortWakeupAlarm
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GetCurrentTime
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GetWakeupTime
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_SetClock
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_SelectBootTarget
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GetBootTarget
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_SetWakeupEvent
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_ValidateWakeupEvent
 * ---------------------------------------------------------------------- */

/**
 * \brief   起床要因が検証されたことを EcuM へ通知する（[SWS_EcuM_02829]）。
 *
 * \details CanIf_CheckValidation() が、起床後の最初の受信を確認したときに呼ぶ
 *          （仕様の <User_ValidateWakeupEvent>、[SWS_CANIF_00179]）。
 *          検証待ち（保留中）の起床要因から、検証された要因を外し、
 *          起床したチャネルを ComM へ通知する（`ComM_EcuM_WakeUpIndication()`、
 *          [SWS_EcuM_02645]）。ComM は、チャネルが NO_COM のときに FULL_COM を
 *          CanSM へ要求し、CanSM がコントローラを起動して FULL_COM へ確定する
 *          （以前は CanIf_RxIndication() が独自 API の CanSM_RxIndication() を
 *          呼び、CanSM が自分で確定していた。2026-10 に置き換え）。
 *
 *          本プロジェクトの EcuM は、起床要因の管理（検証済み・期限切れの
 *          イベントの保持、`BswM_EcuM_CurrentWakeup()` の通知
 *          [SWS_EcuM_04078]/[SWS_EcuM_04079]）を持たない（`EcuM_CheckWakeup()`
 *          の説明参照）ため、保留中の起床要因の管理と ComM への通知のみを行う。
 *
 * \param[in]  sources  検証された起床要因のビットマスク。`ECUM_WKSOURCE_CAN` 以外の
 *                       ビットを含む、または 0 の場合は、未知の要因として
 *                       `ECUM_E_UNKNOWN_WAKEUP_SOURCE`（[SWS_EcuM_02868]）を DET へ
 *                       報告し、何もしない。
 *
 * \AUTOSARReq     {SWS_EcuM_02829, SWS_EcuM_02645, SWS_EcuM_02868}
 * \ServiceID      {0x14}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_ValidateWakeupEvent(EcuM_WakeupSourceType sources)
{
    if ((sources == 0U) || ((sources & (EcuM_WakeupSourceType)(~ECUM_WKSOURCE_CAN)) != 0U))
    {
        DET_LOGW(TAG, "ValidateWakeupEvent: unknown source=0x%08lX", (unsigned long)sources);
        (void)Det_ReportError(ECUM_MODULE_ID, 0U, ECUM_API_ID_VALIDATE_WAKEUP_EVENT, ECUM_E_UNKNOWN_WAKEUP_SOURCE);
        return;
    }

    EcuM_PendingWakeupSources   &= (EcuM_WakeupSourceType)(~sources);   /* 検証待ちが無くなれば、タイマは意味を失う（[SWS_EcuM_02710]） */
    EcuM_ValidatedWakeupSources |= sources;                             /* [SWS_EcuM_02496] */
    EcuM_ExpiredWakeupSources   &= (EcuM_WakeupSourceType)(~sources);   /* 期限切れの後に、遅れて検証された場合 */

    DET_LOGI(TAG, "ValidateWakeupEvent: CAN wakeup validated -> ComM_EcuM_WakeUpIndication");
    ComM_EcuM_WakeUpIndication(COMM_CHANNEL_0);
}

/* ----------------------------------------------------------------------
 * EcuM_ErrorHook
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_AL_SetProgrammableInterrupts
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_AL_DriverInitZero
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_DeterminePbConfiguration
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_AL_DriverInitOne
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_LoopDetection
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_OnGoOffOne
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_OnGoOffTwo
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_AL_SwitchOff
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_AL_Reset
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_EnableWakeupSources
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_GenerateRamHash
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_SleepActivity
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_StartCheckWakeup
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_EndCheckWakeup
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_CheckRamHash
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_DisableWakeupSources
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_AL_DriverRestart
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_StartWakeupSources
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_CheckValidation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * EcuM_StopWakeupSources
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * EcuM_GetNowMs
 * ---------------------------------------------------------------------- */

/**
 * \brief   WdgM のモードを切り替える（RUN: WDGM_MODE_DEFAULT、POST_RUN: WDGM_MODE_POST_RUN）。
 *
 * \details WdgM_SetMode() が E_NOT_OK を返した場合（WdgM 未初期化、モードが範囲外）は、エラーをログに出す。
 *          戻り値を黙って捨てると、POST_RUN から RUN への復帰に失敗した時に、監視の抑制が残ったまま
 *          （FAILED でもリフレッシュを続ける）になり、気づけないため。EcuM はモードの切り替えが失敗しても、
 *          遷移は続ける（通信・シャットダウンの動作を止めない）。
 *
 * \param[in]  mode  設定するモード（WDGM_MODE_DEFAULT / WDGM_MODE_POST_RUN）。
 */
static void EcuM_SetWdgMMode(WdgM_ModeType mode)
{
    if (WdgM_SetMode(mode) != E_OK)
    {
        DET_LOGE(TAG, "WdgM_SetMode(%u) failed", (unsigned)mode);
    }
}

/**
 * \brief   現在時刻 [ms] を返す（Os カウンタ SYSTEM_COUNTER の tick 値）。
 * \details millis() を直接呼ばず Os の GetCounterValue() を介する。値は Os_Init() の
 *          前後や millis() フォールバックの前後でも飛ばない（Os.h 参照）ため、
 *          本モジュールが保持する「前回時刻」との差分計算にそのまま使える。
 */
static unsigned long EcuM_GetNowMs(void)
{
    TickType now = 0U;
    (void)GetCounterValue(SYSTEM_COUNTER, &now);
    return (unsigned long)now;
}

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef ECUM_UNIT_TEST
void EcuM_Test_ResetInitState(void)
{
    EcuM_State          = ECUM_STATE_STARTUP;
    EcuM_RunUsers       = 0U;
    EcuM_PostRunUsers   = 0U;
    EcuM_PostRunTimerMs = 0UL;
    EcuM_PendingWakeupSources = 0U;
    EcuM_ValidatedWakeupSources = 0U;
    EcuM_ExpiredWakeupSources   = 0U;
    EcuM_ValidationStartMs      = 0UL;
}

/**
 * \brief   EcuM の状態を指定の値へ直接設定する（単体テスト専用）。
 *
 * \details 実 EcuM_Init() は BSW 全モジュールを初期化して ECUM_STATE_RUN へ
 *          入るため、EcuM 単体のテストでは RUN/POST_RUN/SHUTDOWN の状態から
 *          始める手段が無い。状態以外（RUN/POST_RUN 要求ビット・タイマ）は
 *          変えない（標準外の関数。EcuM_Test_ResetInitState() と同じ設計方針）。
 *
 * \param[in]  state  設定する EcuM の状態。
 */
void EcuM_Test_SetState(EcuM_StateType state)
{
    EcuM_State = state;
}
#endif
