/**
 * \file    Os_SetTaskActiveReEnable_test.cpp
 * \brief   Os_SetTaskActive() の再有効化と範囲外指定の単体テスト
 * \details 無効→有効へ戻した瞬間に、そのタスクの最終実行時刻を現在へリセットする（再開直後に周期を待たず即実行しない）。
 *          Os_Init() 前や TaskId が範囲外のときは何もしない。
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

/* Os_SetTaskActive()（再有効化） の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Gpt_Hw・DET・時刻を初期化し、Os を未初期化へ戻して、2 チャネルの Gpt 設定と 3 タスクの Os 設定を用意する。 */
class Os_SetTaskActiveReEnable_Test : public ::testing::Test
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

TEST_F(Os_SetTaskActiveReEnable_Test, Os_SetTaskActive_OK_ReEnableResetsLastRunTime)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StartOs();
    Os_SetTaskActive(0U, 0U);
    Advance(500U);   // 無効の間に周期 100ms を何度も超える

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Os_SetTaskActive(0U, 1U);
    Os_SchedulerStep();
    const uint32 runAtReEnable = g_run[0];

    Advance(100U);
    Os_SchedulerStep();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(runAtReEnable, 0U);   // 再開直後は周期を待つ（古い時刻のままなら即実行してしまう）
    EXPECT_EQ(g_run[0], 1U);        // 周期が来れば実行
}

TEST_F(Os_SetTaskActiveReEnable_Test, Os_SetTaskActive_NG_BeforeInitAndInvalidTaskIdAreIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（未初期化: Os_Init() を呼ばない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Os_SetTaskActive(0U, 0U);   // 未初期化: 何も起きない

    StartOs();
    Os_SetTaskActive(3U, 0U);   // TaskCount 以上: 無視
    Advance(100U);
    Os_SchedulerStep();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(g_run[0], 1U);
    EXPECT_EQ(g_run[1], 0U);
    EXPECT_EQ(g_run[2], 1U);
}

}  // namespace
