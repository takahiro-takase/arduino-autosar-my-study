/**
 * \file    App_EngineManager.c
 * \brief   エンジンマネージャ アプリケーション SW-Component
 * \details AUTOSAR スタイルのアプリケーション SW-C (ASW) として
 *          エンジンマネージャを実装する。
 *          周期 Runnable Entity (App_EngineManager_Run) が RTE ポートアクセサ
 *          経由でセンサシグナルを読み取り、エンジンステートマシンを評価して、
 *          3 秒ごとに結果を CAN バスへ送信する。
 *
 *          状態遷移:
 *            OFF      --[flag=1]-->              STARTING
 *            OFF      --[speed>0 かつ flag=0]--> FAULT
 *            STARTING --[flag=0]-->              OFF
 *            STARTING --[speed>=500]-->          RUNNING
 *            STARTING --[タイムアウト 5 秒]-->  FAULT
 *            RUNNING  --[flag=0]-->              OFF
 *            RUNNING  --[temp>=100]-->           FAULT（過熱）
 *            RUNNING  --[speed<100]-->           FAULT（失速）
 *            FAULT    --[flag=0]-->              OFF
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "App_EngineManager.h"
#include "Rte.h"
#include "Dem.h"
#include "Det.h"
#include "WdgM.h"
#include "FiM_Cfg.h"

#define TAG "AppEng"

/* millis() is declared in Arduino wiring.c with C linkage. */
extern unsigned long millis(void);

#define ENGINE_SPEED_RUNNING_THRESHOLD  ((EngineSpeed_t)500U)
#define ENGINE_SPEED_STALL_THRESHOLD    ((EngineSpeed_t)100U)
#define COOLANT_OVERHEAT_THRESHOLD      ((CoolantTemp_t)100U)
#define STARTING_TIMEOUT_MS             (5000UL)

/** ENGINE_STATE_OFF がこの周期数（Run 呼び出し回数）継続したら「通信不要」と
 *  判断し、ComM_USER_0 の FULL_COM 要求を解放する（ボランタリ CAN スリープ）。
 *  Run は 3000ms 周期のため、既定値 5 は実質 15 秒。 */
#define APP_ENGINE_SLEEP_OFF_CYCLES     5U

static EngineState_t  s_state           = ENGINE_STATE_OFF;
static unsigned long  s_startingEnterMs = 0UL;

/** ENGINE_STATE_OFF が連続した周期数（ボランタリスリープ判断用カウンタ） */
static uint8 s_offCycles = 0U;

/** 前回 Run 時点の ComM 通信モード（ボランタリスリープからの復帰エッジ検出用） */
static ComM_ModeType s_lastComMode = COMM_FULL_COMMUNICATION;

static void State_Off(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Starting(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Running(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Fault(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);

/**
 * \brief   エンジンマネージャ SW-Component を初期化する。
 *
 * \details エンジンステートマシンを ENGINE_STATE_OFF へリセットし、
 *          STARTING 状態のタイムアウト基準時刻をクリアする。
 *          RTE スケジューラが App_EngineManager_Run() を呼び始める前に
 *          システム初期化時に 1 回だけ呼び出すこと。
 *
 * \pre        RTE およびすべての BSW モジュールがこの呼び出しより前に
 *             初期化済みであること。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void App_EngineManager_Init(void)
{
    s_state           = ENGINE_STATE_OFF;
    s_startingEnterMs = 0UL;
    s_offCycles       = 0U;
    s_lastComMode     = COMM_FULL_COMMUNICATION;  /* EcuM_Init が起動時に要求済み */
    DET_LOGI(TAG, "Init->OFF");
}

/**
 * \brief   エンジンマネージャの周期 Runnable Entity を実行する。
 *
 * \details RTE から 3 つの RX シグナル（EngineSpeed / CoolantTemp /
 *          EngineOnFlag）を読み取り、現在の状態ハンドラへ委譲したのち、
 *          更新された EngineState シグナルを書き込んで CAN フレーム
 *          (CAN ID 0x200、DLC 1) の送信をトリガする。
 *          Rte_ScheduleRunnables() から 3000 ms ごとに呼び出される。
 *
 *          受信タイムアウト処理:
 *          いずれかの Rte_Read が RTE_E_COM_STOPPED を返した場合は EngineInfo の
 *          受信デッドラインを超過したと判断する（3 シグナルはすべて同一
 *          I-PDU に属するため、1 つが RTE_E_COM_STOPPED なら残りも同様）。
 *          STARTING / RUNNING 中のタイムアウトは FAULT 遷移し DEM に記録する。
 *          OFF / FAULT 中は通信不在が正常のためタイムアウトを無視する。
 *          RTE_E_HARD_TRANSFORMER_ERROR（E2E 検証不合格）は FAULT 遷移させず
 *          WARN ログのみ出す（DTC 報告は E2EXf が別途行う。詳細は
 *          README の「E2E 検証ステータスの Rte 経由での公開」参照）。
 *
 *          警告確認ボタンによる FAULT 解除は FiM_FID_BUTTON_ACK が抑止中
 *          （ボタン固着確定中）の間は受理しない。
 *
 * \pre        App_EngineManager_Init() が正常に完了していること。
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void App_EngineManager_Run(void)
{
    EngineSpeed_t  speed      = 0U;
    CoolantTemp_t  temp       = 0U;
    EngineOnFlag_t flag       = 0U;
    uint8          btnPressed = 0U;

    /* Runnable 開始を WdgM へ報告 (Logical Supervision チェックポイント) */
    (void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START);

    /* ボランタリスリープからの復帰エッジ検出。
     * NO_COM → FULL_COM への変化を検知したら、今回の周期に限り「OFF 継続」の
     * 判断をスキップして猶予を与える。復帰直後は CAN 受信がまだ最新の
     * EngineInfo を拾えていない可能性があり（ウェイクアップ契機となった
     * フレーム自体は取りこぼされうる、Can_Isr() のコメント参照）、
     * 古いバッファ値のまま再び OFF と判断して即座にスリープへ戻ってしまう
     * 「ウェイクしてすぐ再スリープ」を防ぐため。 */
    uint8 justResumed = 0U;
    {
        ComM_ModeType comModeNow = COMM_NO_COMMUNICATION;
        (void)Rte_Call_ComM_GetCurrentComMode(&comModeNow);
        if (comModeNow == COMM_FULL_COMMUNICATION && s_lastComMode != COMM_FULL_COMMUNICATION)
        {
            justResumed = 1U;
            DET_LOGI(TAG, "ComM FULL_COM resumed -> sleep countdown reset (grace cycle)");
        }
        /* s_lastComMode はここでは更新しない。この周期の終わりに、
         * 本周期自身のスリープ判断が反映された後の値で更新する
         * (下記参照)。ここで更新すると、まさにこの周期でスリープへ
         * 入る判断をした場合に NO_COM への変化を記録し損ね、次に
         * 目覚めたときのエッジ検出が機能しなくなる（実機で確認された不具合）。 */
    }

    /* ボタン状態読み取り（GPIO 入力: CAN 通信と独立して常に取得可能）*/
    (void)Rte_Call_Button_GetLevel(&btnPressed);

    const Rte_IStatusType speedRet = Rte_Read_SpeedSensor_EngineSpeed(&speed);
    const Rte_IStatusType tempRet  = Rte_Read_TempSensor_CoolantTemp(&temp);
    const Rte_IStatusType flagRet  = Rte_Read_EngineStatus_EngineOnFlag(&flag);

    /* FreezeFrame 用の現在値を更新（Dem_ReportErrorStatus の FAILED 遷移時にスナップショットされる） */
    Dem_SetFreezeFrameContext(speed, temp, (uint8)s_state);

    /* E2E ハードエラー時の観測ログ（AUTOSAR Rte_IStatusType の
     * RTE_E_HARD_TRANSFORMER_ERROR に相当）。DTC 報告自体は E2EXf/
     * Rte_COMCbk_EngineInfo が DEM_EVENT_E2E_ENGINEINFO へ直接行うため
     * ここでは行わない。SWC（本 Runnable）が Rte 経由で E2E の異常を
     * 直接観測できることを示す目的のログ。 */
    if (speedRet == RTE_E_HARD_TRANSFORMER_ERROR || tempRet == RTE_E_HARD_TRANSFORMER_ERROR
        || flagRet == RTE_E_HARD_TRANSFORMER_ERROR)
    {
        DET_LOGW(TAG, "EngineInfo E2E hard error this cycle, using last valid value");
    }

    /* EngineInfo タイムアウト検出（3 シグナルは同一 I-PDU のため代表値で判定）。
     * E2E エラー（HARD/SOFT_TRANSFORMER_ERROR）は Com のタイムアウトとは
     * 別軸のフェイルセーフ（前回値保持）が既に効いているため、ここでの
     * COMM_TIMEOUT 判定には含めない（RTE_E_COM_STOPPED のみを見る）。
     * OFF/FAULT 中はボランタリ CAN スリープ等で通信不在が正常なため、
     * Dem への FAILED 報告・FAULT 遷移のいずれも行わない（ここで報告して
     * しまうと、エンジン OFF を放置するたびにボランタリスリープで
     * DEM_EVENT_COMM_TIMEOUT が確定してしまう）。 */
    if (speedRet == RTE_E_COM_STOPPED || tempRet == RTE_E_COM_STOPPED || flagRet == RTE_E_COM_STOPPED)
    {
        if (s_state == ENGINE_STATE_STARTING || s_state == ENGINE_STATE_RUNNING)
        {
            Dem_ReportErrorStatus(DEM_EVENT_COMM_TIMEOUT, DEM_EVENT_STATUS_FAILED);
            s_state = ENGINE_STATE_FAULT;
            DET_LOGW(TAG, "->FAULT comm timeout");
        }
    }
    else
    {
        /* 正常受信 → COMM_TIMEOUT イベントを PASSED 報告 */
        Dem_ReportErrorStatus(DEM_EVENT_COMM_TIMEOUT, DEM_EVENT_STATUS_PASSED);

        switch (s_state)
        {
            case ENGINE_STATE_OFF:      State_Off(speed, temp, flag);      break;
            case ENGINE_STATE_STARTING: State_Starting(speed, temp, flag); break;
            case ENGINE_STATE_RUNNING:  State_Running(speed, temp, flag);  break;
            case ENGINE_STATE_FAULT:    State_Fault(speed, temp, flag);    break;
            default:                    s_state = ENGINE_STATE_FAULT;      break;
        }
    }

    /* ボランタリスリープ判断: ENGINE_STATE_OFF が APP_ENGINE_SLEEP_OFF_CYCLES
     * 周期継続したら、通信不要と判断して COMM_USER_0 の要求を解放する。
     * Dcm (COMM_USER_1) が extendedSession でなければ ComM の集約結果が NO_COM
     * になり、CanSM が実際に CAN コントローラをスリープさせる。
     * OFF 以外の状態になったら即座に FULL_COM を要求し直す（毎回呼んでも
     * ComM 側で現状と同じなら何もしないため無害）。 */
    if (s_state != ENGINE_STATE_OFF)
    {
        s_offCycles = 0U;
        (void)Rte_Call_ComM_RequestComMode(COMM_FULL_COMMUNICATION);
    }
    else if (justResumed)
    {
        s_offCycles = 0U;  /* 復帰直後の猶予サイクル: 今回は判断をスキップする */
    }
    else
    {
        if (s_offCycles < 0xFFU)
            s_offCycles++;
        if (s_offCycles >= APP_ENGINE_SLEEP_OFF_CYCLES)
        {
            DET_LOGI(TAG, "OFF continued %u cycles -> release COMM_USER_0 (voluntary sleep)",
                     (unsigned)s_offCycles);
            (void)Rte_Call_ComM_RequestComMode(COMM_NO_COMMUNICATION);
        }
    }

    /* s_lastComMode を今回の周期の最終的なモードで更新する（上記の
     * ボランタリスリープ判断による変化を含む）。次回 Run() 実行時の
     * 復帰エッジ検出はこの値を基準にする。 */
    (void)Rte_Call_ComM_GetCurrentComMode(&s_lastComMode);

    /* 警告確認ボタンは CAN 通信状態によらず常に有効。
     * comm timeout 中の FAULT（CAN E_NOT_OK 継続）でも上記 switch に到達しないため、
     * ここで独立してチェックする。
     * ただし FiM が抑止中（ボタン固着確定中）は、押下が物理的固着による
     * 偽信号である可能性を排除できないため受理しない。 */
    if (s_state == ENGINE_STATE_FAULT && btnPressed == 1U)
    {
        /* フェールセーフ既定値: 許可状態を確認できない間は抑止扱いとする
         * (FiM_GetFunctionPermission は失敗時も Status=0 を書き込むが、
         * 呼び出し側自身もその実装詳細に依存せず安全側を既定値にする) */
        uint8 ackPermitted = 0U;
        if (Rte_Call_FiM_GetFunctionPermission(FIM_FID_BUTTON_ACK, &ackPermitted) != E_OK)
        {
            ackPermitted = 0U;
        }

        if (ackPermitted == 1U)
        {
            s_state = ENGINE_STATE_OFF;
            DET_LOGI(TAG, "FAULT->OFF btn=1");
        }
        else
        {
            DET_LOGW(TAG, "FAULT->OFF btn=1 inhibited (FiM)");
        }
    }

    (void)Rte_Write_EngineStatus_EngineState(s_state);
    (void)Rte_TriggerTransmit(0U);

    /* ADC センサ電圧読み取り（ローカル電圧モニタ：参考値ログ出力） */
    {
        uint16 adcMv = 0U;
        (void)Rte_Call_Adc_GetValue_mV(&adcMv);
        DET_LOGD(TAG, "ADC=%umV", (unsigned)adcMv);
    }

    /* Runnable 実行完了を WdgM へ報告 (Alive + Logical Supervision チェックポイント) */
    (void)WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
}

/**
 * \brief   現在のエンジン状態を返す。
 *
 * \details 内部エンジン状態変数への読み取り専用アクセスを提供する。
 *          状態遷移は発生しない（診断・テスト用途）。
 *
 * \return  現在の EngineState_t 値
 *          (ENGINE_STATE_OFF / STARTING / RUNNING / FAULT)。
 *
 * \pre        App_EngineManager_Init() が正常に完了していること。
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
EngineState_t App_EngineManager_GetState(void)
{
    return s_state;
}

/* -----------------------------------------------------------------------
 * 内部状態ハンドラ — App_EngineManager_Run からのみ呼び出す
 * ----------------------------------------------------------------------- */

/**
 * \brief   ENGINE_STATE_OFF 状態を処理する。
 *
 * \details EngineOnFlag が立ったときに OFF から遷移する:
 *          - flag=1            → STARTING（タイムアウト基準時刻を記録）
 *          - speed>0 かつ flag=0 → FAULT（フラグなしで速度あり = 異常）
 *
 * \param[in]  speed  現在のエンジン回転数 (RPM)。
 * \param[in]  temp   現在の冷却水温 (°C)。この状態では未使用。
 * \param[in]  flag   エンジン起動フラグ（1 = 起動要求、0 = 要求なし）。
 *
 * \ServiceID      {0xF2}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void State_Off(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    (void)temp;

    if (flag == 1U)
    {
        s_state           = ENGINE_STATE_STARTING;
        s_startingEnterMs = millis();
        Dem_ReportErrorStatus(DEM_EVENT_ENGINE_SPEED_NO_FLAG, DEM_EVENT_STATUS_PASSED);
        DET_LOGI(TAG, "OFF->STARTING");
    }
    else if (speed > 0U)
    {
        s_state = ENGINE_STATE_FAULT;
        Dem_ReportErrorStatus(DEM_EVENT_ENGINE_SPEED_NO_FLAG, DEM_EVENT_STATUS_FAILED);
        DET_LOGW(TAG, "OFF->FAULT spd_no_flag");
    }
    else
    {
        Dem_ReportErrorStatus(DEM_EVENT_ENGINE_SPEED_NO_FLAG, DEM_EVENT_STATUS_PASSED);
    }
}

/**
 * \brief   ENGINE_STATE_STARTING 状態を処理する。
 *
 * \details 5 秒のタイムアウト付きでクランキング段階を監視する:
 *          - flag=0                             → OFF（起動キャンセル）
 *          - speed >= ENGINE_SPEED_RUNNING_THRESHOLD (500) → RUNNING
 *          - 経過時間 >= STARTING_TIMEOUT_MS (5000 ms) → FAULT
 *
 * \param[in]  speed  現在のエンジン回転数 (RPM)。
 * \param[in]  temp   現在の冷却水温 (°C)。この状態では未使用。
 * \param[in]  flag   エンジン起動フラグ（STARTING 継続には 1 が必要）。
 *
 * \ServiceID      {0xF3}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void State_Starting(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    (void)temp;

    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        DET_LOGI(TAG, "STARTING->OFF");
        return;
    }
    if (speed >= ENGINE_SPEED_RUNNING_THRESHOLD)
    {
        s_state = ENGINE_STATE_RUNNING;
        Dem_ReportErrorStatus(DEM_EVENT_STARTING_TIMEOUT, DEM_EVENT_STATUS_PASSED);
        DET_LOGI(TAG, "STARTING->RUNNING");
        return;
    }
    if (millis() - s_startingEnterMs >= STARTING_TIMEOUT_MS)
    {
        s_state = ENGINE_STATE_FAULT;
        Dem_ReportErrorStatus(DEM_EVENT_STARTING_TIMEOUT, DEM_EVENT_STATUS_FAILED);
        DET_LOGW(TAG, "STARTING->FAULT timeout");
    }
}

/**
 * \brief   ENGINE_STATE_RUNNING 状態を処理する。
 *
 * \details エンジン正常稼働中の異常条件を監視する:
 *          - flag=0                                      → OFF
 *          - temp >= COOLANT_OVERHEAT_THRESHOLD (100°C)  → FAULT（過熱）
 *          - speed < ENGINE_SPEED_STALL_THRESHOLD (100)  → FAULT（失速）
 *          上記以外の場合は現在の回転数と水温を毎周期ログ出力する。
 *
 * \param[in]  speed  現在のエンジン回転数 (RPM)。
 * \param[in]  temp   現在の冷却水温 (°C)。
 * \param[in]  flag   エンジン起動フラグ（RUNNING 継続には 1 が必要）。
 *
 * \ServiceID      {0xF4}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void State_Running(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        DET_LOGI(TAG, "RUNNING->OFF");
        return;
    }
    if (temp >= COOLANT_OVERHEAT_THRESHOLD)
    {
        s_state = ENGINE_STATE_FAULT;
        Dem_ReportErrorStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);
        Dem_ReportErrorStatus(DEM_EVENT_ENGINE_STALL,    DEM_EVENT_STATUS_PASSED);
        DET_LOGW(TAG, "RUNNING->FAULT overheat=%u", (unsigned)temp);
        return;
    }
    if (speed < ENGINE_SPEED_STALL_THRESHOLD)
    {
        s_state = ENGINE_STATE_FAULT;
        Dem_ReportErrorStatus(DEM_EVENT_ENGINE_STALL,    DEM_EVENT_STATUS_FAILED);
        Dem_ReportErrorStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_PASSED);
        DET_LOGW(TAG, "RUNNING->FAULT stall=%u", (unsigned)speed);
        return;
    }

    /* 正常 RUNNING: 両フォルト条件を PASSED 報告 */
    Dem_ReportErrorStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_PASSED);
    Dem_ReportErrorStatus(DEM_EVENT_ENGINE_STALL,    DEM_EVENT_STATUS_PASSED);

    DET_LOGD(TAG, "RUNNING spd=%u tmp=%u", (unsigned)speed, (unsigned)temp);
}

/**
 * \brief   ENGINE_STATE_FAULT 状態を処理する。
 *
 * \details CAN 経由の flag をもとに FAULT 状態を維持する:
 *          - flag=0 → OFF（EngineEcu が起動要求をクリア）
 *          - それ以外 → FAULT 継続（毎周期待機メッセージをログ出力）
 *          警告確認ボタン（btnPressed）による FAULT クリアは、CAN 通信状態に依存させない
 *          ため App_EngineManager_Run 側で独立してチェックする。
 *          この状態では速度と水温は評価しない。
 *
 * \param[in]  speed  現在のエンジン回転数 (RPM)。この状態では未使用。
 * \param[in]  temp   現在の冷却水温 (°C)。この状態では未使用。
 * \param[in]  flag   エンジン起動フラグ（0 で異常をクリアして OFF へ戻る）。
 *
 * \ServiceID      {0xF5}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void State_Fault(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    (void)speed;
    (void)temp;

    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        DET_LOGI(TAG, "FAULT->OFF flag=0");
    }
    else
    {
        DET_LOGD(TAG, "FAULT wait flag=0 or btn");
    }
}
