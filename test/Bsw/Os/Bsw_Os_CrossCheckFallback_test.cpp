/**
 * \file    Bsw_Os_CrossCheckFallback_test.cpp
 * \brief   Gpt ティック停滞時の millis() へのフォールバックの単体テスト
 * \details クロスチェック周期（500ms）ごとに Gpt ティックと millis() の進みを比べ、Gpt が停滞していれば millis() へフォールバックする。
 *          フォールバック時は全タスクの最終実行時刻を現在へ揃え、一斉の追いつき実行を防ぐ。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Os.h"
#include "Gpt.h"
#include "Gpt_PBCfg.h"
#include "Gpt_Hw.h"
#include "Fake_Gpt_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
}

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

constexpr uint8 TEST_OS_GPT_CHANNEL = GPT_CHANNEL_1;

uint32 g_run[3];

void TestTask0(void) { g_run[0]++; }
void TestTask1(void) { g_run[1]++; }
void TestTask2(void) { g_run[2]++; }

/* タスク 0: 100ms 周期 / タスク 1: 1000ms 周期 / タスク 2: 周期 0（毎ステップ実行）。 */
const Os_TaskType g_tasks[3] = {
    { TestTask0, 100U },
    { TestTask1, 1000U },
    { TestTask2, 0U },
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Os の Gpt ティック停滞検知 の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Gpt_Hw・DET・時刻を初期化し、Os を未初期化へ戻して、2 チャネルの Gpt 設定と 3 タスクの Os 設定を用意する。 */
class Bsw_Os_CrossCheckFallback_Test : public ::testing::Test
{
protected:
    Gpt_ChannelConfigType channels[2];
    Gpt_ConfigType        gptConfig;
    Os_ConfigType         osConfig;

    void SetUp() override
    {
        FakeGptHw_Reset();
        FakeDetHw_Reset();
        FakeMillis_Reset();
        Os_Test_ResetInitState();
        for (uint32 i = 0U; i < 3U; i++)
        {
            g_run[i] = 0U;
        }

        channels[0].ChannelId       = GPT_CHANNEL_0;
        channels[0].Mode            = GPT_CH_MODE_CONTINUOUS;
        channels[0].TickFrequencyHz = 1000U;
        channels[0].TickValueMax    = 0xFFFFFFFFU;
        channels[0].Notification    = nullptr;

        channels[1].ChannelId       = TEST_OS_GPT_CHANNEL;
        channels[1].Mode            = GPT_CH_MODE_CONTINUOUS;
        channels[1].TickFrequencyHz = 1000U;
        channels[1].TickValueMax    = 0xFFFFFFFFU;
        channels[1].Notification    = nullptr;

        gptConfig.Channels     = channels;
        gptConfig.ChannelCount = 2U;

        osConfig.Tasks     = g_tasks;
        osConfig.TaskCount = 3U;
    }

    void TearDown() override
    {
        Gpt_StopTimer(TEST_OS_GPT_CHANNEL);
        Gpt_DeInit();
        Os_Test_ResetInitState();
    }

    void StartOs()
    {
        Gpt_Init(&gptConfig);
        Os_Init(&osConfig);
    }

    /* Gpt の時間源と millis() を同じだけ進める（正常に動いている状態）。 */
    void Advance(uint32 ms)
    {
        for (uint32 i = 0U; i < ms; i++)
        {
            Gpt_OnTick(TEST_OS_GPT_CHANNEL);
        }
        FakeMillis_Value += ms;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_Os_CrossCheckFallback_Test, Os_SchedulerStep_OK_GptTickStallFallsBackToMillisWithoutCatchUpRun)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StartOs();
    FakeMillis_Value += 1000UL;   // Gpt ティックは止まったまま millis() だけ進む

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Os_SchedulerStep();   // 時間源は Gpt（経過 0）のためタスクは実行されず、クロスチェックが停滞を検知する
    const uint32 runAtFallback1 = g_run[1];

    Os_SchedulerStep();   // フォールバック直後: 最終実行時刻を揃えたので追いつき実行しない
    const uint32 runJustAfter1 = g_run[1];
    const uint32 runJustAfter0 = g_run[0];

    FakeMillis_Value += 100UL;
    Os_SchedulerStep();   // 以降の時間源は millis()

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(runAtFallback1, 0U);
    EXPECT_EQ(runJustAfter1, 0U);
    EXPECT_EQ(runJustAfter0, 0U);
    EXPECT_EQ(g_run[0], 1U);   // millis() 基準で 100ms 経過した分だけ実行
    EXPECT_EQ(g_run[1], 0U);
}

}  // namespace
