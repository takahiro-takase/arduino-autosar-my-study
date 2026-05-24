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

#define TAG "AppEng"

/* millis() is declared in Arduino wiring.c with C linkage. */
extern unsigned long millis(void);

#define ENGINE_SPEED_RUNNING_THRESHOLD  ((EngineSpeed_t)500U)
#define ENGINE_SPEED_STALL_THRESHOLD    ((EngineSpeed_t)100U)
#define COOLANT_OVERHEAT_THRESHOLD      ((CoolantTemp_t)100U)
#define STARTING_TIMEOUT_MS             (5000UL)

static EngineState_t  s_state           = ENGINE_STATE_OFF;
static unsigned long  s_startingEnterMs = 0UL;

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
 *          この関数はエンジンマネージャ SW-C の唯一の Runnable Entity である。
 *          完全な AUTOSAR OS 環境では周期 OsTask へマッピングされる。
 *
 * \pre        App_EngineManager_Init() が正常に完了していること。
 * \note       RTE Read の戻り値は破棄している。シグナルが取得できない場合は
 *             前回のバッファ値（起動直後は 0）を使用するため、
 *             ステートマシンは OFF のまま維持される。
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void App_EngineManager_Run(void)
{
    EngineSpeed_t  speed = 0U;
    CoolantTemp_t  temp  = 0U;
    EngineOnFlag_t flag  = 0U;

    (void)Rte_Read_SpeedSensor_EngineSpeed(&speed);
    (void)Rte_Read_TempSensor_CoolantTemp(&temp);
    (void)Rte_Read_EngineStatus_EngineOnFlag(&flag);

    switch (s_state)
    {
        case ENGINE_STATE_OFF:      State_Off(speed, temp, flag);      break;
        case ENGINE_STATE_STARTING: State_Starting(speed, temp, flag); break;
        case ENGINE_STATE_RUNNING:  State_Running(speed, temp, flag);  break;
        case ENGINE_STATE_FAULT:    State_Fault(speed, temp, flag);    break;
        default:                    s_state = ENGINE_STATE_FAULT;      break;
    }

    (void)Rte_Write_EngineStatus_EngineState(s_state);
    (void)Rte_TriggerTransmit(0U);
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
 * \details オペレータが起動要求をクリアするまで FAULT 状態を維持する:
 *          - flag=0 → OFF（異常確認済み、システムリセット）
 *          - flag=1 → FAULT 継続（毎周期待機メッセージをログ出力）
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
        DET_LOGI(TAG, "FAULT->OFF");
    }
    else
    {
        DET_LOGD(TAG, "FAULT wait flag=0");
    }
}
