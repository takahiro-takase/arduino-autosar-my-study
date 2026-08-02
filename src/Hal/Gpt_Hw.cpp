/**
 * \file    Gpt_Hw.cpp
 * \brief   Gpt ハードウェア依存層 実装 (Renesas RA FspTimer)
 * \details 本プロジェクトが対応する MCU は Renesas RA (Arduino UNO R4) のみ。
 *          チャネルごとに 1 個の FspTimer インスタンスを AGT/GPT の空き
 *          チャネルへ割り当て、TickFrequencyHz どおりの周期割り込みを
 *          発生させるだけの単純な周期タイマとして使う（目標時間との比較は
 *          Gpt.c 側のソフトウェア処理。Gpt_Hw.h 冒頭のコメント参照）。
 *
 *          本ファイルが .cpp である理由:
 *          FspTimer クラス（Arduino_Core_Renesas 提供、AGT/GPT ペリフェラルの
 *          C++ ラッパー）を使うため、Can_Hw.cpp / Wdg_Hw.cpp 等と同じ理由で
 *          C++ として実装する。
 *
 *          HW チャネル自動選択:
 *          AGT0 等を固定で使うと Arduino コア自身が内部的に予約している
 *          チャネルと衝突する可能性があるため、FspTimer::get_available_timer()
 *          で実行時に空きチャネルを選ばせる（公式サンプルの推奨パターン）。
 *          選ばれる物理チャネル番号は AUTOSAR の Gpt_ChannelType（論理チャネル、
 *          Gpt_PBCfg.c の配列インデックス）とは独立である。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <stdint.h>
#include <FspTimer.h>
#include "Gpt_Hw.h"
#include "Det.h"

#define TAG "Gpt_Hw"

typedef struct
{
    FspTimer timer;
    uint8    opened;  /**< 1: FspTimer::begin/open/start 済み */
} Gpt_Hw_ChannelType;

static Gpt_Hw_ChannelType Gpt_Hw_Channels[GPT_CHANNEL_COUNT];

/**
 * \brief   FspTimer 周期割り込みのトランポリン。
 *
 * \details ctx には Gpt_Hw_StartTimer() で渡した論理チャネル番号
 *          (Gpt_ChannelType) をそのまま格納してある。ISR コンテキストで
 *          実行されるため、Gpt_OnTick() 以外の処理は行わない。
 */
static void Gpt_Hw_TimerCallback(timer_callback_args_t* args)
{
    const Gpt_ChannelType channel = (Gpt_ChannelType)(uintptr_t)args->p_context;
    Gpt_OnTick(channel);
}

void Gpt_Hw_Init(void)
{
    for (uint8 i = 0U; i < GPT_CHANNEL_COUNT; i++)
    {
        Gpt_Hw_Channels[i].opened = 0U;
    }
}

void Gpt_Hw_DeInit(void)
{
    for (uint8 i = 0U; i < GPT_CHANNEL_COUNT; i++)
    {
        if (Gpt_Hw_Channels[i].opened != 0U)
        {
            /* close() だけでは FspTimer::gpt_used_channel[]/agt_used_channel[]
             * の空き管理テーブルが TIMER_USED のまま残り、二度とこの物理
             * チャネルが get_available_timer() で選ばれなくなる
             * (FspTimer.cpp の end() 実装を参照。テーブルの解放は end() の
             * 中でしか行われない)。end() は stop()+close() に加えてこの
             * テーブル解放も行うため、DeInit は必ず end() を使う。 */
            Gpt_Hw_Channels[i].timer.end();
            Gpt_Hw_Channels[i].opened = 0U;
        }
    }
}

Std_ReturnType Gpt_Hw_StartTimer(Gpt_ChannelType Channel, uint32 TickFrequencyHz)
{
    if (Channel >= GPT_CHANNEL_COUNT)
    {
        return E_NOT_OK;  /* Gpt.c 側で検証済みだが防御的に */
    }

    Gpt_Hw_ChannelType* hw = &Gpt_Hw_Channels[Channel];

    if (hw->opened != 0U)
    {
        /* 前回 Start された物理タイマが（明示的な Gpt_StopTimer、または
         * ONESHOT 自動停止のいずれかで）stop() 済みのまま、まだ空きプールに
         * 返却されていない状態。Gpt_Hw_StopTimer() は ISR コンテキストからも
         * 呼ばれるため stop() だけに留めており（Gpt_Hw_StopTimer() 内の
         * コメント参照）、実際の解放（end()）はここ、次に本チャネルを
         * Start する同期コンテキストまで遅延させている。end() を呼ばずに
         * 毎回新しい物理チャネルを get_available_timer() で確保し続けると、
         * 古いチャネルが二度と空きプールに戻らず、有限個の AGT/GPT
         * チャネルを再起動のたびに 1 つずつ消費してしまう
         * （2026-08 のレビューで指摘されたリソースリーク）。 */
        hw->timer.end();
        hw->opened = 0U;
    }

    uint8_t hwType  = GPT_TIMER;
    int8_t  hwIndex = FspTimer::get_available_timer(hwType);
    if (hwIndex < 0)
    {
        DET_LOGE(TAG, "no free HW timer for channel %u", (unsigned)Channel);
        return E_NOT_OK;
    }

    if (!hw->timer.begin(TIMER_MODE_PERIODIC, hwType, (uint8_t)hwIndex,
                          (float)TickFrequencyHz, 0.0f,
                          Gpt_Hw_TimerCallback, (void*)(uintptr_t)Channel))
    {
        DET_LOGE(TAG, "FspTimer.begin failed ch=%u", (unsigned)Channel);
        return E_NOT_OK;
    }

    /* FspTimer::begin() は timer_cfg.cycle_end_ipl を無条件に BSP_IRQ_DISABLED
     * にする（FspTimer.cpp 実装を参照）。begin() に渡したコールバックを
     * 登録するだけでは周期割り込みの優先度が「無効」のままで、open()/
     * start() が成功してもカウンタが動くだけで実際の NVIC 割り込みは
     * 一度も発生しない。setup_overflow_irq() を明示的に呼んで初めて
     * cycle_end_ipl に実際の優先度が入り、begin() で渡したコールバックへ
     * ディスパッチする ISR ベクタが IRQManager に登録される
     * （isr_fnc 省略時は FSP の汎用 xxx_counter_overflow_isr が
     * begin() で設定済みの p_callback/p_context を呼ぶ経路になる）。
     * コメント（FspTimer.cpp 内）にあるとおり open() 後は割り込み設定が
     * 無視されるため、必ず open()/start() より前に呼ぶこと
     * （2026-08 の実機検証で「begin/open/start は全て成功するのに
     * コールバックが一度も呼ばれない」形で発覚）。 */
    if (!hw->timer.setup_overflow_irq())
    {
        DET_LOGE(TAG, "FspTimer.setup_overflow_irq failed ch=%u", (unsigned)Channel);
        hw->timer.end();
        return E_NOT_OK;
    }

    if (!hw->timer.open())
    {
        DET_LOGE(TAG, "FspTimer.open failed ch=%u", (unsigned)Channel);
        return E_NOT_OK;
    }
    if (!hw->timer.start())
    {
        DET_LOGE(TAG, "FspTimer.start failed ch=%u", (unsigned)Channel);
        /* open()済みで物理チャネルはプールから確保済みのため、close() だけ
         * では TIMER_USED のまま解放されない（DeInit の end() と同じ理由）。
         * hw->opened は立てていないため、end() を呼ばないと二度と
         * get_available_timer() で選ばれなくなる。 */
        hw->timer.end();
        return E_NOT_OK;
    }

    hw->opened = 1U;
    return E_OK;
}

void Gpt_Hw_StopTimer(Gpt_ChannelType Channel)
{
    if (Channel >= GPT_CHANNEL_COUNT) return;

    Gpt_Hw_ChannelType* hw = &Gpt_Hw_Channels[Channel];
    if (hw->opened != 0U)
    {
        /* ISR (ONESHOT 自動停止) からも呼ばれるため close() はしない
         * (ペリフェラル解放はレジスタ操作が重く、割り込みコンテキストでの
         * 実行を避ける。次回 Gpt_StartTimer() 時に改めて begin/open/start
         * し直す設計のため、opened フラグはここでは落とさない)。 */
        hw->timer.stop();
    }
}
