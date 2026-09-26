/**
 * \file    App_GptDemo.c
 * \brief   Gpt 実 HW タイマ動作確認用 SW-C 実装
 * \details App_GptDemo_OnTick() は Gpt モジュールの ISR コンテキストから
 *          直接呼ばれる（Gpt_PBCfg.c の Channel 0 GptNotification として
 *          登録されている。Gpt.h 冒頭のコメント参照）。このため
 *          App_GptDemo_OnTick() は volatile カウンタのインクリメントのみを
 *          行い、Serial 出力（DET_LOGx）や他モジュール呼び出しは一切
 *          行わない。実際のログ出力は、Os の周期タスクから呼ばれる
 *          App_GptDemo_Run()（非 ISR コンテキスト）側でのみ行う。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "App_GptDemo.h"
#include "Gpt.h"
#include "Det.h"

#define TAG "GptDemo"

/** Gpt の ISR (App_GptDemo_OnTick) と App_GptDemo_Run() の両方から
 *  読み書きされるため volatile（Can.c の Can_RxIrqPending と同じ考え方）。 */
static volatile uint32 s_TickCount = 0U;

void App_GptDemo_Init(void)
{
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    Gpt_EnableNotification(GPT_CHANNEL_0);

    DET_LOGI(TAG, "Init ok ch=%u target=1000tick(1000ms)", (unsigned)GPT_CHANNEL_0);
}

void App_GptDemo_Run(void)
{
    DET_LOGI(TAG, "isrTicks=%lu elapsed=%lu/1000",
             (unsigned long)s_TickCount,
             (unsigned long)Gpt_GetTimeElapsed(GPT_CHANNEL_0));
}

void App_GptDemo_OnTick(void)
{
    s_TickCount++;
}
