/**
 * \file    test_gpt.cpp
 * \brief   Gpt.c（src/Bsw/Gpt/Gpt.c）の単体テスト（GoogleTest / PlatformIO native環境）
 * \details 実 HW（Gpt_Hw / FspTimer）・実 Det（Serial出力）は fake_gpt_hw.c /
 *          fake_det.c / fake_schm_hw.c に差し替え、Gpt.c のロジックのみを
 *          ホスト上で検証する。実 HW 割り込みは Gpt_OnTick() を直接呼ぶことで
 *          模擬する（Gpt.c がこの関数を素の呼び出し可能関数として公開する
 *          設計になっているため、モックの割り込みコントローラ等は不要）。
 *
 *          Gpt.c の内部状態（Gpt_ChannelState 等）はファイルスコープの
 *          static 変数であり、テストケースをまたいでプロセス内に残り続ける。
 *          そのため各テストの TearDown で必ず Gpt_StopTimer()+Gpt_DeInit() を
 *          呼び、次のテストが「未初期化」から始められるようにしている
 *          （C言語のグローバル状態を持つモジュールをテストする際の定石）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Gpt.h"
#include "Gpt_PBCfg.h"
#include "Gpt_Hw.h"  /* Gpt_OnTick() — テストから ISR tick を模擬するために呼ぶ */
#include "fake_gpt_hw.h"
#include "fake_det.h"
}

namespace
{

uint32 g_notifyCount = 0U;

extern "C" void TestNotification(void)
{
    g_notifyCount++;
}

class GptTest : public ::testing::Test
{
protected:
    Gpt_ChannelConfigType channels[1];
    Gpt_ConfigType         config;

    void SetUp() override
    {
        FakeGptHw_Reset();
        FakeDet_Reset();
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

TEST_F(GptTest, InitSucceedsWithValidConfig)
{
    Gpt_Init(&config);

    EXPECT_EQ(FakeDet_ReportCount, 0U);
}

TEST_F(GptTest, InitRejectsNullConfig)
{
    Gpt_Init(NULL);

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_PARAM_POINTER);
}

TEST_F(GptTest, InitTwiceReportsAlreadyInitialized)
{
    Gpt_Init(&config);
    FakeDet_Reset();

    Gpt_Init(&config);

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_ALREADY_INITIALIZED);
}

TEST_F(GptTest, ApiCallsBeforeInitReportUninit)
{
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 0U);
    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_UNINIT);

    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_UNINIT);
}

TEST_F(GptTest, StartTimerRejectsZeroValue)
{
    Gpt_Init(&config);

    Gpt_StartTimer(GPT_CHANNEL_0, 0U);

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_PARAM_VALUE);
    EXPECT_EQ(FakeGptHw_StartCount, 0U);
}

TEST_F(GptTest, StartTimerRejectsValueAboveTickValueMax)
{
    channels[0].TickValueMax = 100U;
    Gpt_Init(&config);

    Gpt_StartTimer(GPT_CHANNEL_0, 101U);

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_PARAM_VALUE);
    EXPECT_EQ(FakeGptHw_StartCount, 0U);
}

TEST_F(GptTest, StartTimerRejectsInvalidChannel)
{
    Gpt_Init(&config);

    Gpt_StartTimer((Gpt_ChannelType)1U, 1000U);

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_PARAM_CHANNEL);
}

TEST_F(GptTest, StartTimerSucceedsAndDelegatesToHw)
{
    Gpt_Init(&config);

    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    EXPECT_EQ(FakeDet_ReportCount, 0U);
    EXPECT_EQ(FakeGptHw_StartCount, 1U);
    EXPECT_EQ(FakeGptHw_LastStartChannel, GPT_CHANNEL_0);
    EXPECT_EQ(FakeGptHw_LastTickFrequencyHz, 1000U);
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 0U);
    EXPECT_EQ(Gpt_GetTimeRemaining(GPT_CHANNEL_0), 1000U);
}

TEST_F(GptTest, StartTimerWhileRunningReportsBusy)
{
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    FakeDet_Reset();

    Gpt_StartTimer(GPT_CHANNEL_0, 500U);

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_BUSY);
    EXPECT_EQ(FakeGptHw_StartCount, 1U);  /* 2 回目は Hw まで到達しない */
}

TEST_F(GptTest, StartTimerRollsBackStateWhenHwFails)
{
    FakeGptHw_StartShouldFail = 1U;
    Gpt_Init(&config);

    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    /* HW が起動していないため running ではない = 再度 Start できる
     * （BUSY にならないことで "stopped" 相当へロールバックしたことを確認）。 */
    FakeGptHw_StartShouldFail = 0U;
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    EXPECT_NE(FakeDet_LastErrorId, GPT_E_BUSY);
    EXPECT_EQ(FakeGptHw_StartCount, 2U);
}

TEST_F(GptTest, OnTickIncrementsElapsedAndDecrementsRemaining)
{
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    for (int i = 0; i < 500; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 500U);
    EXPECT_EQ(Gpt_GetTimeRemaining(GPT_CHANNEL_0), 500U);
}

TEST_F(GptTest, ContinuousModeWrapsAtTargetWithoutStoppingHw)
{
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);

    for (int i = 0; i < 1000; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 0U);  /* [SWS_Gpt_00361] */
    EXPECT_EQ(FakeGptHw_StopCount, 0U);
}

TEST_F(GptTest, OneshotModeStopsHwAndFreezesAtTarget)
{
    channels[0].Mode = GPT_CH_MODE_ONESHOT;
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 100U);

    for (int i = 0; i < 100; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    EXPECT_EQ(FakeGptHw_StopCount, 1U);
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 100U);
    EXPECT_EQ(Gpt_GetTimeRemaining(GPT_CHANNEL_0), 0U);  /* [SWS_Gpt_00305] */

    /* expired 後にさらに tick が来ても状態機械は変化しない
     * （Gpt_OnTick は state != RUNNING で即 return する）。 */
    Gpt_OnTick(GPT_CHANNEL_0);
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 100U);
    EXPECT_EQ(FakeGptHw_StopCount, 1U);
}

TEST_F(GptTest, StopTimerFreezesElapsedAndIsIdempotent)
{
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    for (int i = 0; i < 300; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    Gpt_StopTimer(GPT_CHANNEL_0);
    EXPECT_EQ(FakeGptHw_StopCount, 1U);
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 300U);

    /* stopped 状態への再度の StopTimer は無害 ([SWS_Gpt_00344])。
     * Hw 側も再度は呼ばれない。 */
    Gpt_StopTimer(GPT_CHANNEL_0);
    EXPECT_EQ(FakeGptHw_StopCount, 1U);
    EXPECT_EQ(FakeDet_ReportCount, 0U);
}

TEST_F(GptTest, DeInitWhileRunningReportsBusyAndStaysInitialized)
{
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    FakeDet_Reset();

    Gpt_DeInit();

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_BUSY);

    /* DeInit が実行されず初期化状態のままなら、running チャネルへの
     * StartTimer は (UNINIT ではなく) BUSY になるはず。 */
    FakeDet_Reset();
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_BUSY);
}

TEST_F(GptTest, NotificationFiresOnlyWhileEnabled)
{
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 10U);

    for (int i = 0; i < 10; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }
    EXPECT_EQ(g_notifyCount, 0U);  /* 目標到達済みだが通知は未 enable */

    Gpt_EnableNotification(GPT_CHANNEL_0);
    for (int i = 0; i < 10; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }
    EXPECT_EQ(g_notifyCount, 1U);

    Gpt_DisableNotification(GPT_CHANNEL_0);
    for (int i = 0; i < 10; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }
    EXPECT_EQ(g_notifyCount, 1U);  /* 無効化後は増えない */
}

TEST_F(GptTest, EnableNotificationRejectsChannelWithoutNotificationConfigured)
{
    channels[0].Notification = NULL;
    Gpt_Init(&config);

    Gpt_EnableNotification(GPT_CHANNEL_0);

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_PARAM_CHANNEL);
}

TEST_F(GptTest, GetVersionInfoRejectsNullPointer)
{
    Gpt_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDet_LastErrorId, GPT_E_PARAM_POINTER);
}

TEST_F(GptTest, GetVersionInfoFillsExpectedModuleId)
{
    Std_VersionInfoType info;

    Gpt_GetVersionInfo(&info);

    EXPECT_EQ(info.moduleID, GPT_MODULE_ID);
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
