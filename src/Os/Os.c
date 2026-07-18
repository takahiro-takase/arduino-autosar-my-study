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
 *          now はタスクごとにループ内で毎回取得し直す（1 回のスキャン開始時に
 *          取得した値を全タスクで使い回さない）。あるタスクの Func() が
 *          ブロッキング処理（例: NvM_WriteBlock() 経由の EEPROM 同期書き込み）で
 *          数百ms 専有した場合、使い回した古い now で後続タスクの
 *          Os_LastRunMs[] を更新してしまうと、「実際に実行した時刻」ではなく
 *          「スキャン開始時点の古い時刻」が記録される。次回のスキャンでは
 *          既に進んだ実時刻との差分が本来より大きく計算され、周期が
 *          到来していないタスクが早期に（本来の周期より短い間隔で）
 *          再実行されてしまう不具合が実機で見つかった
 *          （WdgM の Deadline Supervision が周期の短すぎる呼び出しとして検出）。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Os_SchedulerStep(void)
{
    for (uint8 i = 0U; i < Os_Cfg->TaskCount; i++)
    {
        if (Os_TaskActive[i] == 0U)
            continue;

        unsigned long now = millis();
        if ((now - Os_LastRunMs[i]) >= (unsigned long)Os_Cfg->Tasks[i].PeriodMs)
        {
            Os_LastRunMs[i] = now;
            Os_Cfg->Tasks[i].Func();
        }
    }
}

/**
 * \brief   タスクの有効/無効を切り替える (BswM が呼び出す)。
 *
 * \details 無効→有効へ遷移する瞬間、そのタスクの Os_LastRunMs[] を
 *          現在時刻にリセットする。長時間無効だったタスク（例: SHUTDOWN 中に
 *          停止していた WdgM_MainFunction）は Os_LastRunMs[] が無効化前の
 *          古い時刻のまま残るため、リセットしないと再開直後の
 *          Os_SchedulerStep() で「経過時間が周期を大幅に超えている」と
 *          判定され、本来の周期を 1 度も待たずに即座に実行されてしまう。
 *          WdgM_MainFunction のように「直近の一定時間内に十分な報告が
 *          あったか」を評価する Alive Supervision では、この即時実行が
 *          「再開直後でまだ監視対象タスクに実行機会がほとんどない」状態を
 *          誤って FAILED と判定する原因になっていた（実機で確認済み）。
 *          有効→無効や、既に有効な状態への再設定では何もしない。
 *
 * \param[in]  TaskId  対象タスクの ID。
 * \param[in]  Active  1=有効化、0=無効化。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Os_SetTaskActive(uint8 TaskId, uint8 Active)
{
    if (Os_Cfg == NULL || TaskId >= Os_Cfg->TaskCount)
        return;  /* Os_Init() 未実行 (呼び出し順序の誤りに対する保険) */

    if (Active != 0U && Os_TaskActive[TaskId] == 0U)
        Os_LastRunMs[TaskId] = millis();

    Os_TaskActive[TaskId] = Active;
}
