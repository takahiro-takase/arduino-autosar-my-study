/**
 * \file    Gpt_OnTick_test.cpp
 * \brief   Gpt_OnTick() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Gpt_test.cpp）。
 *
 *          実 HW 依存部分（Gpt_Hw / FspTimer、Det_Hw / Serial出力、SchM_Hw /
 *          割り込み制御）のみを Fake_Gpt_Hw.c / Fake_Det_Hw.c /
 *          Fake_SchM_Hw.c に差し替え、Gpt.c・Det.c 自体は実物をリンクして
 *          ロジックを検証する。実 HW 割り込みは Gpt_OnTick() を直接呼ぶことで
 *          模擬する（Gpt.c がこの関数を素の呼び出し可能関数として公開する
 *          設計になっているため、モックの割り込みコントローラ等は不要）。
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
class Gpt_OnTick_Test : public ::testing::Test
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

TEST_F(Gpt_OnTick_Test, Gpt_OnTick_NG_IgnoresInvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_OnTick((Gpt_ChannelType)1U);
    Gpt_OnTick((Gpt_ChannelType)0xFFU);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 0U);   // 有効チャネルの状態は変わらない
    EXPECT_EQ(g_notifyCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // ISR 経路では DET を報告しない
}

TEST_F(Gpt_OnTick_Test, Gpt_OnTick_OK_IncrementsElapsedAndDecrementsRemaining)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (int i = 0; i < 500; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 500U);
    EXPECT_EQ(Gpt_GetTimeRemaining(GPT_CHANNEL_0), 500U);
}

TEST_F(Gpt_OnTick_Test, Gpt_OnTick_OK_ContinuousModeWrapsAtTargetWithoutStoppingHw)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (int i = 0; i < 1000; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 0U);  /* [SWS_Gpt_00361] */
    EXPECT_EQ(FakeGptHw_StopCount, 0U);
}

TEST_F(Gpt_OnTick_Test, Gpt_OnTick_OK_OneshotModeStopsHwAndFreezesAtTarget)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    channels[0].Mode = GPT_CH_MODE_ONESHOT;
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 100U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (int i = 0; i < 100; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeGptHw_StopCount, 1U);
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 100U);
    EXPECT_EQ(Gpt_GetTimeRemaining(GPT_CHANNEL_0), 0U);  /* [SWS_Gpt_00305] */

    /* expired 後にさらに tick が来ても状態機械は変化しない
     * （Gpt_OnTick は state != RUNNING で即 return する）。 */
    Gpt_OnTick(GPT_CHANNEL_0);
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 100U);
    EXPECT_EQ(FakeGptHw_StopCount, 1U);
}

TEST_F(Gpt_OnTick_Test, Gpt_OnTick_NG_BeforeInitIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（Gpt_Init() を呼ばない未初期化の状態）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_OnTick(GPT_CHANNEL_0);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(g_notifyCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // ISR 経路では DET を報告しない
}

TEST_F(Gpt_OnTick_Test, Gpt_OnTick_OK_EnabledChannelWithoutNotificationFunctionIsSafe)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    Gpt_EnableNotification(GPT_CHANNEL_0);   // 通知関数があるうちに有効化する
    channels[0].Notification = NULL;         // その後で、設定から通知関数が無くなった状態にする

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_OnTick(GPT_CHANNEL_0);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(g_notifyCount, 0U);   // NULL の通知関数は呼ばない
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 1U);   // タイマー自体は進む
}

}  // namespace
