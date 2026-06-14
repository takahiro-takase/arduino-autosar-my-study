/**
 * \file    Os.c
 * \brief   OS タイムトリガスケジューラ実装 (AUTOSAR SWS_Os 準拠)
 * \details millis() を時間源とする協調スケジューラ。
 *          タスクごとに最終実行時刻を記録し、
 *          Os_SchedulerStep() 呼び出しごとに周期到来チェックを行う。
 *
 *          オーバフロー安全性:
 *            (now - Os_LastRunMs[i]) は unsigned long 演算のため、
 *            millis() が 2^32 ms (約 49 日) でオーバフローしても
 *            差分計算は正しく動作する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Os.h"
#include "Det.h"

#define TAG "Os"

extern unsigned long millis(void);

static const Os_ConfigType* Os_Cfg = NULL;

/** 各タスクの最終実行時刻 (ms) */
static unsigned long Os_LastRunMs[OS_TASK_COUNT];

/** タスク有効フラグ: 1=有効(実行対象), 0=無効(スキップ); BswM が切り替える */
static uint8 Os_TaskActive[OS_TASK_COUNT];

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   全タスクの最終実行時刻を現在時刻で初期化する。
 *
 * \details 起動直後に全タスクを「今実行した」とみなすことで、
 *          最初の Os_SchedulerStep() 呼び出しから PeriodMs 後に
 *          初回タスクが起動するようにする。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Os_Init(const Os_ConfigType* ConfigPtr)
{
    Os_Cfg = ConfigPtr;
    unsigned long now = millis();
    for (uint8 i = 0U; i < ConfigPtr->TaskCount; i++)
    {
        Os_LastRunMs[i]  = now;
        Os_TaskActive[i] = 1U;  /* BswM が制御するまでは全タスク有効 */
    }
    DET_LOGI(TAG, "Init ok tasks=%u", (unsigned)ConfigPtr->TaskCount);
}

/**
 * \brief   周期が到来したタスクを順に実行する。
 *
 * \details 全タスクを走査し (now - Os_LastRunMs[i]) >= PeriodMs であれば
 *          Os_LastRunMs[i] を更新してタスク関数を呼び出す。
 *          PeriodMs == 0 のタスクは毎ステップ無条件に実行される。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Os_SchedulerStep(void)
{
    unsigned long now = millis();
    for (uint8 i = 0U; i < Os_Cfg->TaskCount; i++)
    {
        if (Os_TaskActive[i] == 0U)
            continue;

        if ((now - Os_LastRunMs[i]) >= (unsigned long)Os_Cfg->Tasks[i].PeriodMs)
        {
            Os_LastRunMs[i] = now;
            Os_Cfg->Tasks[i].Func();
        }
    }
}

void Os_SetTaskActive(uint8 TaskId, uint8 Active)
{
    if (TaskId >= Os_Cfg->TaskCount)
        return;
    Os_TaskActive[TaskId] = Active;
}
