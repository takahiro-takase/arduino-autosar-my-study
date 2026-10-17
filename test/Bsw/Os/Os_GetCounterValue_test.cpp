/**
 * \file    Os_GetCounterValue_test.cpp
 * \brief   Os_GetCounterValue() の単体テスト
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
class Os_GetCounterValue_Test : public ::testing::Test
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

TEST_F(Os_GetCounterValue_Test, GetCounterValue_NG_InvalidCounterIdReturnsEOsId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    TickType value = 777U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StatusType ret = GetCounterValue(SYSTEM_COUNTER + 1U, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(E_OS_ID, ret);
    EXPECT_EQ(777U, value);
}

TEST_F(Os_GetCounterValue_Test, GetCounterValue_NG_NullValueReturnsEOsValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StatusType ret = GetCounterValue(SYSTEM_COUNTER, nullptr);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(E_OS_VALUE, ret);
}

TEST_F(Os_GetCounterValue_Test, GetCounterValue_OK_BeforeOsInitReturnsMillis)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 1234UL;
    TickType value = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StatusType ret = GetCounterValue(SYSTEM_COUNTER, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(E_OK, ret);
    EXPECT_EQ(1234U, value);
}

TEST_F(Os_GetCounterValue_Test, GetCounterValue_OK_ContinuesFromMillisAtOsInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 5000UL;
    TickType before = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retBefore = GetCounterValue(SYSTEM_COUNTER, &before);

    StartOs();

    TickType afterInit = 0U;
    Std_ReturnType retAfterInit = GetCounterValue(SYSTEM_COUNTER, &afterInit);

    TickGpt(300U);
    TickType later = 0U;
    Std_ReturnType retLater = GetCounterValue(SYSTEM_COUNTER, &later);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(E_OK, retBefore);
    ASSERT_EQ(E_OK, retAfterInit);
    EXPECT_EQ(before, afterInit);

    ASSERT_EQ(E_OK, retLater);
    EXPECT_EQ(5300U, later);
}

TEST_F(Os_GetCounterValue_Test, GetCounterValue_OK_ContinuousAcrossMillisFallback)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 8000UL;
    StartOs();

    TickGpt(100U);
    FakeMillis_Value += 100UL;
    TickType beforeFallback = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retBefore = GetCounterValue(SYSTEM_COUNTER, &beforeFallback);

    /* Gpt tick が止まったまま millis() だけが進む（OS_TICK_CROSSCHECK_PERIOD_MS 超）。
     * Os_SchedulerStep() のクロスチェックが millis() へフォールバックする。 */
    FakeMillis_Value += 1000UL;
    Os_SchedulerStep();

    TickType afterFallback = 0U;
    Std_ReturnType retAfter = GetCounterValue(SYSTEM_COUNTER, &afterFallback);

    FakeMillis_Value += 250UL;
    TickType later = 0U;
    Std_ReturnType retLater = GetCounterValue(SYSTEM_COUNTER, &later);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(E_OK, retBefore);
    EXPECT_EQ(8100U, beforeFallback);

    ASSERT_EQ(E_OK, retAfter);
    EXPECT_EQ(beforeFallback, afterFallback);

    ASSERT_EQ(E_OK, retLater);
    EXPECT_EQ(afterFallback + 250U, later);
}

}  // namespace
