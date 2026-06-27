/**
 * \file    EcuM.c
 * \brief   ECU ステートマネージャ (AUTOSAR SWS_EcuStateManager 準拠)
 * \details BSW スタックの起動シーケンスと周期処理をカプセル化する。
 *          main.cpp が個々の BSW モジュールを直接制御することなく、
 *          EcuM_Init() と EcuM_MainFunction() の 2 つの API だけで
 *          ECU を起動・運転できるようにする。
 *
 *          起動シーケンス (EcuM_Init):
 *            1. NvM_Init       — EEPROM → RAM ミラー一括ロード (最初期)
 *            2. Port_Init      — ピン方向設定（Dio 操作より前に完了）
 *            3. Can_Init       — CAN コントローラ初期化（バスはまだ非アクティブ）
 *            3. CanIf_Init     — CAN インタフェース初期化
 *            4. PduR_Init      — PDU ルータ初期化
 *            5. Com_Init       — COM モジュール初期化
 *            6. CanTp_Init     — CAN トランスポートプロトコル初期化
 *            7. Dcm_Init       — 診断通信モジュール初期化
 *            8. Dem_Init       — NvM 経由で DTC ステータスを復元
 *            9. FiM_Init       — 機能抑止状態を初期化 (Dem_Init の後)
 *           10. ComM_Init      — 通信マネージャ初期化 (NO_COM 状態)
 *           11. ComM_RequestComMode(FULL_COM) — CAN バス通信開始
 *                               （全上位層初期化後に開始することで
 *                                 フレーム到着時の未初期化アクセスを防ぐ）
 *           12. App_EngineManager_Init — SW-C 初期化
 *           13. IoHwAb_Init    — I/O ハードウェア抽象化層初期化 (LED チャネル設定)
 *           14. App_WarningIndicator_Init — 警告灯 SW-C 初期化
 *           15. BswM_Init      — ルールエンジン初期化
 *           16. WdgM_Init      — Alive/Logical Supervision 初期化。
 *                                AVR 実ハードウェアウォッチドッグもここで有効化する
 *                                （他の全モジュール初期化完了後、最後に有効化することで
 *                                  初期化処理自体がタイムアウトの影響を受けないようにする）
 *           17. Os_Init        — タスクスケジューラ初期化 (全モジュール初期化後)
 *
 *          周期処理 (EcuM_MainFunction):
 *            Os_SchedulerStep() — タスクテーブルに従い周期到来タスクを実行
 *              Task 0: Can_Isr            (1 ms)    — CAN 受信ポーリング
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

#include "EcuM.h"
#include "EcuM_Cfg.h"
#include "BswM.h"
#include "BswM_PBCfg.h"
#include "WdgM.h"
#include "WdgM_PBCfg.h"
#include "NvM.h"
#include "NvM_PBCfg.h"
#include "Os.h"
#include "Os_PBCfg.h"
#include "Can.h"
#include "Can_PBCfg.h"
#include "CanIf.h"
#include "CanIf_PBCfg.h"
#include "PduR.h"
#include "PduR_PBCfg.h"
#include "Com.h"
#include "Com_PBCfg.h"
#include "CanTp.h"
#include "Dcm.h"
#include "Dem.h"
#include "FiM.h"
#include "FiM_PBCfg.h"
#include "Port.h"
#include "CanSM.h"
#include "ComM.h"
#include "Rte.h"
#include "IoHwAb.h"
#include "App_EngineManager.h"
#include "App_WarningIndicator.h"
#include "Det.h"

#define TAG "EcuM"

/* Arduino wiring.c（C リンケージ）で定義 */
extern unsigned long millis(void);

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */

/** 現在の EcuM フェーズ */
static EcuM_StateType  EcuM_State          = ECUM_STATE_STARTUP;

/** RUN 要求中ユーザのビットマスク (bit N = ECUM_USER_N が要求中) */
static uint8           EcuM_RunUsers       = 0U;

/** POST_RUN フェーズ開始時刻 (ms) */
static unsigned long   EcuM_PostRunTimerMs = 0UL;

/**
 * \brief   BSW スタック全体を起動フェーズ順に初期化する。
 *
 * \details AUTOSAR の依存関係順（下位層から上位層）に各モジュールの
 *          _Init 関数を呼び出す。
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
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_Init(void)
{
    NvM_Init(&NvM_Config);
    Port_Init();                    /* ピン方向設定（Dio 操作より前に完了）    */
    Can_Init(&Can_Config);          /* ハードウェア初期化（LISTEN_ONLY で待機） */
    CanIf_Init(&CanIf_Config);
    PduR_Init(&PduR_Config);
    Com_Init(&Com_Config);
    CanTp_Init();
    Dcm_Init();
    Dem_Init();
    FiM_Init(&FiM_Config);                                    /* Dem_Init の後（Dem 状態を参照するため） */
    CanSM_Init();                                             /* NO_COM 状態で開始 */
    ComM_Init();
    ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION);/* 全層初期化後に開通 */
    App_EngineManager_Init();
    IoHwAb_Init();
    App_WarningIndicator_Init();
    BswM_Init(&BswM_Config);  /* Os_Init より前: ルール評価の準備だけ行う */
    WdgM_Init(&WdgM_Config);  /* Alive Supervision 初期化 (Os_Init より前) */
    Os_Init(&Os_Config);      /* タスクテーブル初期化 (全タスク有効で起動) */

    /* 全モジュール初期化完了 → RUN フェーズへ遷移
     * ComM_RequestComMode(FULL_COM) がすでに EcuM_RequestRUN を呼んでいるため
     * EcuM_RunUsers の ECUM_USER_COMM ビットはここで既に立っている。         */
    EcuM_State = ECUM_STATE_RUN;
    DET_LOGI(TAG, "->RUN");
    BswM_EcuM_CurrentState(ECUM_STATE_RUN);  /* Rule 0: 全タスク有効化 */
}

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
    switch (EcuM_State)
    {
        case ECUM_STATE_RUN:
            Os_SchedulerStep();
            break;

        case ECUM_STATE_POST_RUN:
            Os_SchedulerStep();
            if ((millis() - EcuM_PostRunTimerMs) >= ECUM_POST_RUN_TIMEOUT_MS)
            {
                EcuM_State = ECUM_STATE_SHUTDOWN;
                DET_LOGI(TAG, "->SHUTDOWN");
                BswM_EcuM_CurrentState(ECUM_STATE_SHUTDOWN);  /* Rule 2: 全タスク無効化 */
            }
            break;

        case ECUM_STATE_SHUTDOWN:
            /* スケジューラを停止。Arduino では電源断不可のためアイドルで待機 */
            break;

        default:
            break;
    }
}

EcuM_StateType EcuM_GetState(void)
{
    return EcuM_State;
}

Std_ReturnType EcuM_RequestRUN(EcuM_UserType user)
{
    if (user >= ECUM_USER_COUNT)
        return E_NOT_OK;
    if (EcuM_State == ECUM_STATE_SHUTDOWN)
        return E_NOT_OK;

    EcuM_RunUsers |= (uint8)(1U << user);

    /* POST_RUN 中に要求が来たら RUN へ戻る */
    if (EcuM_State == ECUM_STATE_POST_RUN)
    {
        EcuM_State = ECUM_STATE_RUN;
        DET_LOGI(TAG, "->RUN user=%u", (unsigned)user);
        /* Rte_Engine タスク（WdgM の監視対象）が再開するため、
         * POST_RUN 移行時に無効化した HW ウォッチドッグを再度有効化する */
        WdgM_EnableHwWatchdog();
        BswM_EcuM_CurrentState(ECUM_STATE_RUN);  /* Rule 0: 全タスク再有効化 */
    }
    return E_OK;
}

Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user)
{
    if (user >= ECUM_USER_COUNT)
        return E_NOT_OK;

    EcuM_RunUsers &= (uint8)(~(1U << user));

    /* 全ユーザが解放した場合 POST_RUN へ遷移 */
    if ((EcuM_RunUsers == 0U) && (EcuM_State == ECUM_STATE_RUN))
    {
        EcuM_State          = ECUM_STATE_POST_RUN;
        EcuM_PostRunTimerMs = millis();
        DET_LOGI(TAG, "->POST_RUN timeout=%lums", ECUM_POST_RUN_TIMEOUT_MS);
        /* Rule 1 で Rte_Engine タスク（WdgM の唯一の監視対象）が停止するため、
         * ここで HW ウォッチドッグも無効化する。POST_RUN 中はタスクが
         * 意図的に停止しており Alive Supervision は必ず FAILED になるため、
         * SHUTDOWN 遷移（最大 ECUM_POST_RUN_TIMEOUT_MS 後）を待つと
         * HW ウォッチドッグのタイムアウト（8000ms）より先に意図しないリセットが
         * 発生し得る。POST_RUN への移行そのものを安全な無効化ポイントとする。 */
        WdgM_DisableHwWatchdog();
        BswM_EcuM_CurrentState(ECUM_STATE_POST_RUN);  /* Rule 1: アプリタスク無効化 */
    }
    return E_OK;
}
