/**
 * \file    Bsw_EcuM_MainFunction_test.cpp
 * \brief   EcuM_MainFunction() の状態ごとのスケジューラ呼び出しの単体テスト
 * \details RUN では呼び出しごとに Os_SchedulerStep() を 1 回呼ぶ。STARTUP（EcuM_Init() 前）は何もしない。EcuM_GetState() は現在の状態を返す。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "EcuM.h"
#include "EcuM_Cfg.h"
#include "Fake_Bsw_EcuM.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Gpt_Hw.h"
#include "Fake_Wdg_Hw.h"
#include "Gpt.h"
#include "Gpt_PBCfg.h"
#include "Gpt_Hw.h"
#include "Os.h"
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "WdgM.h"
#include "WdgM_PBCfg.h"
#include "Dem.h"
#include "CanSM_Cfg.h"
#include "Wrap_Dem.h"
#include "Wrap_Os.h"
#include "Wrap_BswM.h"
#include "Wrap_WdgM.h"
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

/* EcuM_MainFunction() / EcuM_GetState() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして実体の EcuM を呼び、EcuM を未初期化へ戻す。時刻の源（Gpt と Os、タスク 0 件）・Dem・Wdg・WdgM を
 *          EcuM_Init() と同じ順序で初期化し、各 wrap の呼び出し回数と DET の記録も初期化する（WdgM のモード切替が実際に反映されるように、
 *          WdgM も実体を初期化する）。
 * Advance(): Gpt の時間源と millis() を同じだけ進める（EcuM の POST_RUN タイムアウトの時刻は Os のカウンタから取る）。
 * TearDown(): Gpt・Os・EcuM と模擬 EcuM を元に戻す。 */
class Bsw_EcuM_MainFunction_Test : public ::testing::Test
{
protected:
    Gpt_ChannelConfigType channels[2];
    Gpt_ConfigType        gptConfig;
    Os_ConfigType         osConfig;

    void SetUp() override
    {
        FakeEcuM_Reset();
        FakeEcuM_PassThrough = 1U;
        FakeGptHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        FakeDetHw_Reset();
        FakeMillis_Reset();
        FakeWdgHw_Reset();
        WrapOs_Reset();
        WrapBswM_Reset();
        WrapWdgM_Reset();
        WrapDem_Reset();
        EcuM_Test_ResetInitState();
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

        Gpt_Init(&gptConfig);
        Os_Init(&osConfig);
        Dem_Init(NULL);
        Wdg_Init(&Wdg_Config);
        WdgM_Init(&WdgM_Config);

        WrapOs_Reset();
        WrapBswM_Reset();
        WrapWdgM_Reset();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Gpt_StopTimer(TEST_OS_GPT_CHANNEL);
        Gpt_DeInit();
        Os_Test_ResetInitState();
        EcuM_Test_ResetInitState();
        FakeEcuM_Reset();  // PassThrough も FALSE へ戻る
    }

    void Advance(uint32 ms)
    {
        for (uint32 i = 0U; i < ms; i++)
        {
            Gpt_OnTick(TEST_OS_GPT_CHANNEL);
        }
        FakeMillis_Value += ms;
    }

    WdgM_ModeType WdgMMode()
    {
        WdgM_ModeType mode = 0xFFU;
        (void)WdgM_GetMode(&mode);
        return mode;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_EcuM_MainFunction_Test, EcuM_MainFunction_OK_RunStepsSchedulerAndStartupDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const EcuM_StateType stateAtStart = EcuM_GetState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_MainFunction();   // STARTUP
    const uint32 stepsInStartup = CallCount_Os_SchedulerStep;

    EcuM_Test_SetState(ECUM_STATE_RUN);
    EcuM_MainFunction();
    EcuM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(stateAtStart, ECUM_STATE_STARTUP);
    EXPECT_EQ(stepsInStartup, 0U);
    EXPECT_EQ(CallCount_Os_SchedulerStep, 2U);
    EXPECT_EQ(EcuM_GetState(), ECUM_STATE_RUN);
}

}  // namespace
