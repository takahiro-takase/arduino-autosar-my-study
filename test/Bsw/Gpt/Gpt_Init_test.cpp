/**
 * \file    Gpt_Init_test.cpp
 * \brief   Gpt_Init() の単体テスト
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
class Gpt_Init_Test : public ::testing::Test
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

TEST_F(Gpt_Init_Test, Gpt_Init_NG_RejectsNullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_PARAM_POINTER);
}

TEST_F(Gpt_Init_Test, Gpt_Init_NG_TwiceReportsAlreadyInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_Init(&config);
    FakeDetHw_Reset();

    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_ALREADY_INITIALIZED);
}

TEST_F(Gpt_Init_Test, Gpt_Init_NG_RejectsChannelCountAboveMax)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    config.ChannelCount = GPT_CHANNEL_COUNT + 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_Init(&config);
    const uint8 errAfterInit = FakeDetHw_LastErrorId;
    const uint8 apiAfterInit = FakeDetHw_LastApiId;

    /* 初期化に失敗しているので、続く API は未初期化として扱われる。 */
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(errAfterInit, GPT_E_INIT_FAILED);
    EXPECT_EQ(apiAfterInit, GPT_API_ID_INIT);
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_UNINIT);
}

TEST_F(Gpt_Init_Test, Gpt_Init_OK_SucceedsWithValidConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_Init(&config);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
