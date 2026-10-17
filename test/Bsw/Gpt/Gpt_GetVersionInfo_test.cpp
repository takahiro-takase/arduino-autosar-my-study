/**
 * \file    Gpt_GetVersionInfo_test.cpp
 * \brief   Gpt_GetVersionInfo() の単体テスト
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
class Gpt_GetVersionInfo_Test : public ::testing::Test
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

TEST_F(Gpt_GetVersionInfo_Test, Gpt_GetVersionInfo_NG_RejectsNullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_PARAM_POINTER);
}

TEST_F(Gpt_GetVersionInfo_Test, Gpt_GetVersionInfo_OK_FillsExpectedModuleId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.moduleID, GPT_MODULE_ID);
}

}  // namespace
