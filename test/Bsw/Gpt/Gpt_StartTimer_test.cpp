/**
 * \file    Gpt_StartTimer_test.cpp
 * \brief   Gpt_StartTimer() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Gpt_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Gpt_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Gpt.h"
#include "Gpt_PBCfg.h"
#include "Gpt_Hw.h"  /* Gpt_OnTick() — テストから ISR tick を模擬するために呼ぶ */
#include "Fake_Gpt_Hw.h"
#include "Fake_Det_Hw.h"
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

uint32 g_notifyCount = 0U;

void TestNotification(void)
{
    g_notifyCount++;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Gpt の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Gpt_Hw と DET の記録を初期化し、通知コールバック付きの 1 チャネルの設定（連続モード、1000 Hz）を用意する。Gpt_Init() は各テストが呼ぶ。
 * TearDown(): タイマを停止する。 */
class Gpt_StartTimer_Test : public ::testing::Test
{
protected:
    Gpt_ChannelConfigType channels[1];
    Gpt_ConfigType         config;

    void SetUp() override
    {
        FakeGptHw_Reset();
        FakeDetHw_Reset();
        g_notifyCount = 0U;

        channels[0].ChannelId       = GPT_CHANNEL_0;
        channels[0].Mode            = GPT_CH_MODE_CONTINUOUS;
        channels[0].TickFrequencyHz = 1000U;
        channels[0].TickValueMax    = 0xFFFFFFFFU;
        channels[0].Notification    = TestNotification;

        config.Channels     = channels;
        config.ChannelCount = 1U;
    }

    void TearDown() override
    {
        Gpt_StopTimer(GPT_CHANNEL_0);
        Gpt_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Gpt_StartTimer_Test, Gpt_StartTimer_NG_RejectsInvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_StartTimer((Gpt_ChannelType)1U, 1000U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_PARAM_CHANNEL);
}

TEST_F(Gpt_StartTimer_Test, Gpt_StartTimer_NG_RejectsZeroValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_StartTimer(GPT_CHANNEL_0, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_PARAM_VALUE);
    EXPECT_EQ(FakeGptHw_StartCount, 0U);
}

TEST_F(Gpt_StartTimer_Test, Gpt_StartTimer_NG_RejectsValueAboveTickValueMax)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    channels[0].TickValueMax = 100U;
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_StartTimer(GPT_CHANNEL_0, 101U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_PARAM_VALUE);
    EXPECT_EQ(FakeGptHw_StartCount, 0U);
}

TEST_F(Gpt_StartTimer_Test, Gpt_StartTimer_NG_WhileRunningReportsBusy)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    FakeDetHw_Reset();

    Gpt_StartTimer(GPT_CHANNEL_0, 500U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_BUSY);
    EXPECT_EQ(FakeGptHw_StartCount, 1U);  /* 2 回目は Hw まで到達しない */
}

TEST_F(Gpt_StartTimer_Test, Gpt_StartTimer_NG_RollsBackStateWhenHwFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeGptHw_StartShouldFail = 1U;
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    /* HW が起動していないため running ではない = 再度 Start できる
     * （BUSY にならないことで "stopped" 相当へロールバックしたことを確認）。 */
    FakeGptHw_StartShouldFail = 0U;
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_NE(FakeDetHw_LastErrorId, GPT_E_BUSY);
    EXPECT_EQ(FakeGptHw_StartCount, 2U);
}

TEST_F(Gpt_StartTimer_Test, Gpt_StartTimer_OK_SucceedsAndDelegatesToHw)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
    EXPECT_EQ(FakeGptHw_StartCount, 1U);
    EXPECT_EQ(FakeGptHw_LastStartChannel, GPT_CHANNEL_0);
    EXPECT_EQ(FakeGptHw_LastTickFrequencyHz, 1000U);
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 0U);
    EXPECT_EQ(Gpt_GetTimeRemaining(GPT_CHANNEL_0), 1000U);
}

}  // namespace
