/**
 * \file    Bsw_Os_test.cpp
 * \brief   Os.c（src/Os/Os.c）のカウンタAPI（GetCounterValue / GetElapsedValue）の単体テスト
 * \details Os.c・Gpt.c・Det.c は実物をリンクし、実 HW 依存部分（Gpt_Hw /
 *          Det_Hw / SchM_Hw / millis）のみ Fake へ差し替えて検証する。
 *          millis() は FakeMillis_Value、Gpt の時間源は Gpt_OnTick() を直接
 *          呼んで 1 tick（= 1 ms）ずつ進めて模擬する（Bsw_Gpt_test.cpp と同じ方式）。
 *
 *          Os.c の内部状態（Os_Cfg・カウンタのオフセット・フォールバック状態）は
 *          static 変数でテストケースをまたいで残るため、SetUp で
 *          Os_Test_ResetInitState() を呼んで未初期化へ戻す（OS_UNIT_TEST、
 *          Wdg_Test_ResetInitState() と同じ設計方針）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
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

class OsTest : public ::testing::Test
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

/* ---------------------------------------------------------------------
 * GetCounterValue
 * --------------------------------------------------------------------- */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(OsTest, GetCounterValue_OK_BeforeOsInitReturnsMillis)
{
    FakeMillis_Value = 1234UL;
    TickType value = 0U;

    EXPECT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &value));
    EXPECT_EQ(1234U, value);
}

TEST_F(OsTest, GetCounterValue_OK_ContinuesFromMillisAtOsInit)
{
    FakeMillis_Value = 5000UL;
    TickType before = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &before));

    StartOs();

    TickType afterInit = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &afterInit));
    EXPECT_EQ(before, afterInit);

    TickGpt(300U);
    TickType later = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &later));
    EXPECT_EQ(5300U, later);
}

TEST_F(OsTest, GetCounterValue_OK_ContinuousAcrossMillisFallback)
{
    FakeMillis_Value = 8000UL;
    StartOs();

    TickGpt(100U);
    FakeMillis_Value += 100UL;
    TickType beforeFallback = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &beforeFallback));
    EXPECT_EQ(8100U, beforeFallback);

    /* Gpt tick が止まったまま millis() だけが進む（OS_TICK_CROSSCHECK_PERIOD_MS 超）。
     * Os_SchedulerStep() のクロスチェックが millis() へフォールバックする。 */
    FakeMillis_Value += 1000UL;
    Os_SchedulerStep();

    TickType afterFallback = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &afterFallback));
    EXPECT_EQ(beforeFallback, afterFallback);

    FakeMillis_Value += 250UL;
    TickType later = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &later));
    EXPECT_EQ(afterFallback + 250U, later);
}

TEST_F(OsTest, GetCounterValue_NG_InvalidCounterIdReturnsEOsId)
{
    TickType value = 777U;

    EXPECT_EQ(E_OS_ID, GetCounterValue(SYSTEM_COUNTER + 1U, &value));
    EXPECT_EQ(777U, value);
}

TEST_F(OsTest, GetCounterValue_NG_NullValueReturnsEOsValue)
{
    EXPECT_EQ(E_OS_VALUE, GetCounterValue(SYSTEM_COUNTER, nullptr));
}

/* ---------------------------------------------------------------------
 * GetElapsedValue
 * --------------------------------------------------------------------- */

TEST_F(OsTest, GetElapsedValue_OK_ReturnsDifferenceAndUpdatesValue)
{
    FakeMillis_Value = 1000UL;
    TickType prev = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &prev));

    FakeMillis_Value = 1450UL;
    TickType elapsed = 0U;
    EXPECT_EQ(E_OK, GetElapsedValue(SYSTEM_COUNTER, &prev, &elapsed));
    EXPECT_EQ(450U, elapsed);
    EXPECT_EQ(1450U, prev);

    FakeMillis_Value = 1500UL;
    EXPECT_EQ(E_OK, GetElapsedValue(SYSTEM_COUNTER, &prev, &elapsed));
    EXPECT_EQ(50U, elapsed);
    EXPECT_EQ(1500U, prev);
}

TEST_F(OsTest, GetElapsedValue_OK_CorrectAcrossWrapAround)
{
    FakeMillis_Value = 0xFFFFFFF0UL;
    TickType prev = 0U;
    ASSERT_EQ(E_OK, GetCounterValue(SYSTEM_COUNTER, &prev));

    FakeMillis_Value = 0x00000010UL;  /* 32bit をラップした後 */
    TickType elapsed = 0U;
    EXPECT_EQ(E_OK, GetElapsedValue(SYSTEM_COUNTER, &prev, &elapsed));
    EXPECT_EQ(0x20U, elapsed);
}

TEST_F(OsTest, GetElapsedValue_NG_InvalidCounterIdReturnsEOsId)
{
    TickType prev = 10U;
    TickType elapsed = 99U;

    EXPECT_EQ(E_OS_ID, GetElapsedValue(SYSTEM_COUNTER + 1U, &prev, &elapsed));
    EXPECT_EQ(10U, prev);
    EXPECT_EQ(99U, elapsed);
}

TEST_F(OsTest, GetElapsedValue_NG_NullValueReturnsEOsValue)
{
    TickType elapsed = 0U;

    EXPECT_EQ(E_OS_VALUE, GetElapsedValue(SYSTEM_COUNTER, nullptr, &elapsed));
}

TEST_F(OsTest, GetElapsedValue_NG_NullElapsedValueReturnsEOsValue)
{
    TickType prev = 0U;

    EXPECT_EQ(E_OS_VALUE, GetElapsedValue(SYSTEM_COUNTER, &prev, nullptr));
}

}  // namespace
