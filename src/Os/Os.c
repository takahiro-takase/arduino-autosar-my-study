/**
 * \file    Os.c
 * \brief   OS タイムトリガスケジューラ実装 (AUTOSAR SWS_Os 準拠)
 * \details Gpt (Renesas RA FspTimer による HW 割り込み駆動) の専用チャネル
 *          (GPT_CHANNEL_1) を時間源とする協調スケジューラ。
 *          タスクごとに最終実行時刻を記録し、
 *          Os_SchedulerStep() 呼び出しごとに周期到来チェックを行う。
 *
 *          なぜ Gpt 駆動にしたか / なぜ millis() を残すか:
 *            従来は Arduino コアの millis() を時間源にしていたが、
 *            AUTOSAR の実機 OS は OsCounter を HW タイマ割り込みで駆動する
 *            のが本来の姿であるため、Os 専用の Gpt チャネルを新設し
 *            Gpt_GetTimeElapsed(OS_GPT_CHANNEL) を時間源とする。ただし
 *            docs/DEVLOG.md「Can: RX 割り込み化の実機検証で得られた教訓」の
 *            とおり、本プロジェクトでは実機で割り込みが期待どおり発火しない
 *            事象を一度経験している。Os の時間源が完全に止まると
 *            WdgM_TriggerHwWatchdog を含む全タスクが二度と発火しなくなり、
 *            実 HW ウォッチドッグ（WDGM_HW_WATCHDOG_TIMEOUT_MS=4000ms、
 *            WdgM_Cfg.h 参照）でリセットされてしまう。CAN フレーム 1 個の
 *            欠落より遥かに影響が大きいため、単なる「検知してログを残す」
 *            では不十分（後述）で、Os_CrossCheckTickSource() は異常を検知
 *            すると millis()（Arduino コアが Gpt とは別系統のタイマ割り込みで
 *            駆動しており、こちらが同時に止まる可能性は低い）へ実際に
 *            フォールバックし、スケジューラを止めない（詳細は
 *            Os_CrossCheckTickSource() 参照）。
 *
 *            クロスチェック周期 (OS_TICK_CROSSCHECK_PERIOD_MS) は
 *            WDGM_HW_WATCHDOG_TIMEOUT_MS (4000ms) より必ず短く保つこと。
 *            2026-08 のレビューで、初版の周期 (5000ms) がこの制約に違反して
 *            おり、最悪ケースでは診断ログどころかフォールバック自体が
 *            HW リセットより先に間に合わない（ちょうど診断しようとしている
 *            事象を見逃す）ことが指摘され、500ms に修正した経緯がある。
 *
 *          オーバフロー安全性:
 *            (now - Os_LastRunMs[i]) は unsigned long 演算のため、
 *            時間源が 2^32 ms (約 49 日) でオーバフロー（Gpt 側は
 *            Gpt_PBCfg.c の TickValueMax=0xFFFFFFFF 到達によるソフトウェア
 *            リセット、millis() 側は自然なオーバフロー）しても
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
#include "Gpt.h"
#include "Gpt_PBCfg.h"

#define TAG "Os"

/** Os が専有する Gpt チャネル (Gpt_PBCfg.c 参照)。Gpt_Init() 済みであることが
 *  Os_Init() の前提（Wdg_Init が WdgM_Init の前提であるのと同じ考え方）。 */
#define OS_GPT_CHANNEL  GPT_CHANNEL_1

/** Gpt ティック停滞検知の実施周期 (ms)。
 *  WdgM の実 HW ウォッチドッグタイムアウト (WDGM_HW_WATCHDOG_TIMEOUT_MS,
 *  WdgM_Cfg.h、本コメント執筆時点で 4000ms) より必ず短くすること。
 *  ここが長すぎると、Gpt ティックが本当に停止した場合に millis() への
 *  フォールバックが HW リセットより先に間に合わず、検知自体が意味を
 *  なさなくなる（2026-08 レビュー指摘）。500ms なら 4000ms に対して
 *  8 倍のマージンがあり、ジッタを考慮しても複数回の検知機会がある。
 *  タスクの最短周期 (1ms) よりは十分長いため、通常タスクの実行時間の
 *  揺らぎで誤検知することもない（ブロッキングタスクで CPU が止まっている
 *  間も millis()/Gpt ティックはどちらも HW 駆動で進み続けるため、両者の
 *  差分自体はブロッキングの影響を受けない）。 */
#define OS_TICK_CROSSCHECK_PERIOD_MS  500UL

extern unsigned long millis(void);

static const Os_ConfigType* Os_Cfg = NULL;

/** 各タスクの最終実行時刻 (ms、時間源は Gpt。Os_GetTimeMs() 参照) */
static unsigned long Os_LastRunMs[OS_TASK_COUNT];

/** タスク有効フラグ: 1=有効(実行対象), 0=無効(スキップ); BswM が切り替える */
static uint8 Os_TaskActive[OS_TASK_COUNT];

/** Os_CrossCheckTickSource() が前回サンプルした millis()/Gpt ティック値。 */
static unsigned long Os_LastCrossCheckMillis = 0UL;
static unsigned long Os_LastCrossCheckTick   = 0UL;

/** Gpt ティック停滞を検知し、時間源を millis() へフォールバック済みか
 *  （ラッチ式。一度フォールバックしたら、この起動中は millis() を使い
 *  続ける。DET ログのスパム防止も兼ねる）。 */
static uint8 Os_UseMillisFallback = 0U;

/**
 * \brief   Os スケジューラの時間源 (ms)。
 * \details 通常時は Gpt_GetTimeElapsed(OS_GPT_CHANNEL) を返す
 *          （Gpt.c 側で SchM 排他エリアによる ISR 安全性が担保されている
 *          ため、ここでは追加のロックは不要）。Os_CrossCheckTickSource()
 *          が Gpt ティックの停滞を検知した後は millis() を返す
 *          （Os_UseMillisFallback、下記参照）。
 */
static unsigned long Os_GetTimeMs(void)
{
    if (Os_UseMillisFallback != 0U)
        return millis();
    return (unsigned long)Gpt_GetTimeElapsed(OS_GPT_CHANNEL);
}

/**
 * \brief   Gpt ティックが millis() に対して停滞していないかを確認し、
 *          停滞していれば時間源を millis() へフォールバックする。
 *
 * \details OS_TICK_CROSSCHECK_PERIOD_MS ごとに、その期間中の Gpt ティック
 *          進み幅と millis() 進み幅を比較する。millis() は Arduino コアが
 *          Gpt とは別系統の HW タイマ割り込みで駆動しており、Gpt 側の
 *          割り込みだけが停止する事象（docs/DEVLOG.md の CAN RX 割り込みの
 *          教訓と同種。Gpt_Init() 済みのはずの Os 専用チャネルが
 *          Gpt_StartTimer() 時点で HW タイマを確保できず起動に失敗した
 *          場合も、Gpt ティックが最初から一切進まないため同じ経路で
 *          検知される）を検出するための、独立した参照時計として使う。
 *
 *          検知したら実際に millis() へ切り替える（検知のみでは、
 *          クロスチェック周期をどれだけ WdgM の HW ウォッチドッグ
 *          タイムアウトより短くしても、ログが Serial に出るだけで
 *          スケジューラ自体は停止したままのため、結局 HW リセットに
 *          至ってしまう。2026-08 レビュー指摘）。切り替える瞬間、全タスクの
 *          Os_LastRunMs[] を現在の millis() 値へリセットする
 *          （Os_SetTaskActive() が無効→有効へ切り替える際に行うのと同じ
 *          考え方。リセットしないと "Gpt ティック(0 付近で停止)" から
 *          "millis()(現在の実時刻)" へ基準が飛び、ほぼ全タスクが
 *          その場で「周期を大幅に超過している」と判定されて一斉に
 *          追いつき実行されてしまう。WdgM の Alive Supervision は
 *          「監視対象タスクが実行される機会がほとんどないまま判定される」
 *          形の誤検知を過去に繰り返しており、同種の事故を避けるため）。
 */
static void Os_CrossCheckTickSource(void)
{
    if (Os_UseMillisFallback != 0U)
        return;  /* 既にフォールバック済み: これ以上チェックする意味がない */

    const unsigned long nowMillis = millis();

    if ((nowMillis - Os_LastCrossCheckMillis) < OS_TICK_CROSSCHECK_PERIOD_MS)
        return;

    const unsigned long nowTick     = Os_GetTimeMs();
    const unsigned long millisDelta = nowMillis - Os_LastCrossCheckMillis;
    const unsigned long tickDelta   = nowTick   - Os_LastCrossCheckTick;

    if (tickDelta < (millisDelta / 2UL))
    {
        DET_LOGE(TAG, "Gpt tick stalled: tickDelta=%lu millisDelta=%lu -> fallback to millis()",
                 tickDelta, millisDelta);
        Os_UseMillisFallback = 1U;

        for (uint8 i = 0U; i < Os_Cfg->TaskCount; i++)
        {
            Os_LastRunMs[i] = nowMillis;
        }
        return;
    }

    Os_LastCrossCheckMillis = nowMillis;
    Os_LastCrossCheckTick   = nowTick;
}

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
 *          呼び出し前提: Gpt_Init() が完了していること（OS_GPT_CHANNEL の
 *          Gpt_StartTimer() をここで行うため。EcuM_Init() での呼び出し順序は
 *          Gpt_Init() が Os_Init() よりずっと前）。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Os_Init(const Os_ConfigType* ConfigPtr)
{
    Os_Cfg = ConfigPtr;

    /* Value は 32-bit フルレンジ (millis() と同じラップアラウンド周期)。
     * このチャネルは Os が唯一の所有者であり、Os_DeInit 相当の API が
     * 存在しないため Gpt_StopTimer() で止める経路は無い（動作中は起動しっぱなし）。 */
    Gpt_StartTimer(OS_GPT_CHANNEL, 0xFFFFFFFFU);

    const unsigned long now = Os_GetTimeMs();
    for (uint8 i = 0U; i < ConfigPtr->TaskCount; i++)
    {
        Os_LastRunMs[i]  = now;
        Os_TaskActive[i] = 1U;  /* BswM が制御するまでは全タスク有効 */
    }

    /* クロスチェックの基準点も起動時刻に合わせる */
    Os_LastCrossCheckMillis = millis();
    Os_LastCrossCheckTick   = now;
    Os_UseMillisFallback    = 0U;

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

        unsigned long now = Os_GetTimeMs();
        if ((now - Os_LastRunMs[i]) >= (unsigned long)Os_Cfg->Tasks[i].PeriodMs)
        {
            Os_LastRunMs[i] = now;
            Os_Cfg->Tasks[i].Func();
        }
    }

    Os_CrossCheckTickSource();
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
        Os_LastRunMs[TaskId] = Os_GetTimeMs();

    Os_TaskActive[TaskId] = Active;
}
