/**
 * \file    Os_GetElapsedValue_test.cpp
 * \brief   Os_GetElapsedValue() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Os_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Os_test.cpp の冒頭コメント（git の履歴）を参照。
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Os（協調スケジューラ）の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Gpt_Hw・DET・時刻を初期化し、Os を未初期化の状態へ戻して、2 チャネルの Gpt 設定と Os の設定を用意する。 */
class Os_GetElapsedValue_Test : public ::testing::Test
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

        osConfig.Tasks     = nullptr;
        osConfig.TaskCount = 0U;
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

    void TickGpt(uint32 ticks)
    {
        for (uint32 i = 0U; i < ticks; i++)
        {
            Gpt_OnTick(TEST_OS_GPT_CHANNEL);
        }
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Os_GetElapsedValue_Test, GetElapsedValue_NG_InvalidCounterIdReturnsEOsId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    TickType prev = 10U;
    TickType elapsed = 99U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StatusType ret = GetElapsedValue(SYSTEM_COUNTER + 1U, &prev, &elapsed);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(E_OS_ID, ret);
    EXPECT_EQ(10U, prev);
    EXPECT_EQ(99U, elapsed);
}

TEST_F(Os_GetElapsedValue_Test, GetElapsedValue_NG_NullValueReturnsEOsValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    TickType elapsed = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StatusType ret = GetElapsedValue(SYSTEM_COUNTER, nullptr, &elapsed);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(E_OS_VALUE, ret);
}

TEST_F(Os_GetElapsedValue_Test, GetElapsedValue_NG_NullElapsedValueReturnsEOsValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    TickType prev = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StatusType ret = GetElapsedValue(SYSTEM_COUNTER, &prev, nullptr);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(E_OS_VALUE, ret);
}

TEST_F(Os_GetElapsedValue_Test, GetElapsedValue_OK_ReturnsDifferenceAndUpdatesValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 1000UL;
    TickType prev = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &prev));

    FakeMillis_Value = 1450UL;
    TickType elapsed = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retFirst = GetElapsedValue(SYSTEM_COUNTER, &prev, &elapsed);
    TickType elapsedFirst = elapsed;
    TickType prevFirst    = prev;

    FakeMillis_Value = 1500UL;
    Std_ReturnType retSecond = GetElapsedValue(SYSTEM_COUNTER, &prev, &elapsed);
    TickType elapsedSecond = elapsed;
    TickType prevSecond    = prev;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(E_OK, retFirst);
    EXPECT_EQ(450U, elapsedFirst);
    EXPECT_EQ(1450U, prevFirst);

    EXPECT_EQ(E_OK, retSecond);
    EXPECT_EQ(50U, elapsedSecond);
    EXPECT_EQ(1500U, prevSecond);
}

TEST_F(Os_GetElapsedValue_Test, GetElapsedValue_OK_CorrectAcrossWrapAround)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 0xFFFFFFF0UL;
    TickType prev = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &prev));

    FakeMillis_Value = 0x00000010UL;  /* 32bit をラップした後 */
    TickType elapsed = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StatusType ret = GetElapsedValue(SYSTEM_COUNTER, &prev, &elapsed);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(E_OK, ret);
    EXPECT_EQ(0x20U, elapsed);
}

}  // namespace
