/**
 * \file    Gpt_EnableNotification_test.cpp
 * \brief   Gpt_EnableNotification() の単体テスト
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
class Gpt_EnableNotification_Test : public ::testing::Test
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

TEST_F(Gpt_EnableNotification_Test, Gpt_EnableNotification_NG_BeforeInitReportsUninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（Gpt_Init() を呼ばない未初期化の状態）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_EnableNotification(GPT_CHANNEL_0);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, GPT_API_ID_ENABLE_NOTIFICATION);
}

TEST_F(Gpt_EnableNotification_Test, Gpt_EnableNotification_NG_RejectsChannelWithoutNotificationConfigured)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    channels[0].Notification = NULL;
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_EnableNotification(GPT_CHANNEL_0);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_PARAM_CHANNEL);
}

TEST_F(Gpt_EnableNotification_Test, Gpt_EnableNotification_OK_FiresOnlyWhileEnabled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 10U);

    for (int i = 0; i < 10; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }
    EXPECT_EQ(g_notifyCount, 0U);  /* 目標到達済みだが通知は未 enable */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_EnableNotification(GPT_CHANNEL_0);
    for (int i = 0; i < 10; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(g_notifyCount, 1U);

    Gpt_DisableNotification(GPT_CHANNEL_0);
    for (int i = 0; i < 10; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }
    EXPECT_EQ(g_notifyCount, 1U);  /* 無効化後は増えない */
}

TEST_F(Gpt_EnableNotification_Test, Gpt_EnableNotification_NG_RejectsInvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_EnableNotification((Gpt_ChannelType)1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_PARAM_CHANNEL);
    EXPECT_EQ(FakeDetHw_LastApiId, GPT_API_ID_ENABLE_NOTIFICATION);
}

}  // namespace
