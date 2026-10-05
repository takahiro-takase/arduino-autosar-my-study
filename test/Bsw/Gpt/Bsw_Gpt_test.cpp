/**
 * \file    Bsw_Gpt_test.cpp
 * \brief   Gpt.c（src/Bsw/Gpt/Gpt.c）の単体テスト（GoogleTest / PlatformIO native環境）
 * \details 実 HW 依存部分（Gpt_Hw / FspTimer、Det_Hw / Serial出力、SchM_Hw /
 *          割り込み制御）のみを Fake_Gpt_Hw.c / Fake_Det_Hw.c /
 *          Fake_SchM_Hw.c に差し替え、Gpt.c・Det.c 自体は実物をリンクして
 *          ロジックを検証する。実 HW 割り込みは Gpt_OnTick() を直接呼ぶことで
 *          模擬する（Gpt.c がこの関数を素の呼び出し可能関数として公開する
 *          設計になっているため、モックの割り込みコントローラ等は不要）。
 *
 *          Gpt.c の内部状態（Gpt_ChannelState 等）はファイルスコープの
 *          static 変数であり、テストケースをまたいでプロセス内に残り続ける。
 *          そのため各テストの TearDown で必ず Gpt_StopTimer()+Gpt_DeInit() を
 *          呼び、次のテストが「未初期化」から始められるようにしている
 *          （C言語のグローバル状態を持つモジュールをテストする際の定石）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
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

extern "C" void TestNotification(void)
{
    g_notifyCount++;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Gpt の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Gpt_Hw と DET の記録を初期化し、通知コールバック付きの 1 チャネルの設定（連続モード、1000 Hz）を用意する。Gpt_Init() は各テストが呼ぶ。
 * TearDown(): タイマを停止する。 */
class GptTest : public ::testing::Test
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

TEST_F(GptTest, Gpt_Init_OK_SucceedsWithValidConfig)
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

TEST_F(GptTest, Gpt_Init_NG_RejectsNullConfig)
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

TEST_F(GptTest, Gpt_Init_NG_TwiceReportsAlreadyInitialized)
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

TEST_F(GptTest, Gpt_ApiCalls_NG_BeforeInitReportUninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（Gpt_Init() を呼ばない未初期化の状態）

    /* ----------------------------------- */
    /* ---- 実行 + 評価 (Act + Assert) --- */
    /* ----------------------------------- */
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_UNINIT);

    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_UNINIT);
}

TEST_F(GptTest, Gpt_StartTimer_OK_SucceedsAndDelegatesToHw)
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

TEST_F(GptTest, Gpt_StartTimer_NG_RejectsInvalidChannel)
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

TEST_F(GptTest, Gpt_StartTimer_NG_RejectsZeroValue)
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

TEST_F(GptTest, Gpt_StartTimer_NG_RejectsValueAboveTickValueMax)
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

TEST_F(GptTest, Gpt_StartTimer_NG_WhileRunningReportsBusy)
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

TEST_F(GptTest, Gpt_StartTimer_NG_RollsBackStateWhenHwFails)
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

TEST_F(GptTest, Gpt_OnTick_OK_IncrementsElapsedAndDecrementsRemaining)
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

TEST_F(GptTest, Gpt_OnTick_OK_ContinuousModeWrapsAtTargetWithoutStoppingHw)
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

TEST_F(GptTest, Gpt_OnTick_OK_OneshotModeStopsHwAndFreezesAtTarget)
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

TEST_F(GptTest, Gpt_StopTimer_OK_FreezesElapsedAndIsIdempotent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    for (int i = 0; i < 300; i++)
    {
        Gpt_OnTick(GPT_CHANNEL_0);
    }

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_StopTimer(GPT_CHANNEL_0);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeGptHw_StopCount, 1U);
    EXPECT_EQ(Gpt_GetTimeElapsed(GPT_CHANNEL_0), 300U);

    /* stopped 状態への再度の StopTimer は無害 ([SWS_Gpt_00344])。
     * Hw 側も再度は呼ばれない。 */
    Gpt_StopTimer(GPT_CHANNEL_0);
    EXPECT_EQ(FakeGptHw_StopCount, 1U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(GptTest, Gpt_DeInit_NG_WhileRunningReportsBusyAndStaysInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Gpt_Init(&config);
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Gpt_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_BUSY);

    /* DeInit が実行されず初期化状態のままなら、running チャネルへの
     * StartTimer は (UNINIT ではなく) BUSY になるはず。 */
    FakeDetHw_Reset();
    Gpt_StartTimer(GPT_CHANNEL_0, 1000U);
    EXPECT_EQ(FakeDetHw_LastErrorId, GPT_E_BUSY);
}

TEST_F(GptTest, Gpt_EnableNotification_OK_FiresOnlyWhileEnabled)
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

TEST_F(GptTest, Gpt_EnableNotification_NG_RejectsChannelWithoutNotificationConfigured)
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

TEST_F(GptTest, Gpt_GetVersionInfo_OK_FillsExpectedModuleId)
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

TEST_F(GptTest, Gpt_GetVersionInfo_NG_RejectsNullPointer)
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

}  // namespace
