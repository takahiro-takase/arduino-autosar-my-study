/**
 * \file    Bsw_EcuM_PostRunHold_test.cpp
 * \brief   EcuM の POST_RUN 継続要求の単体テスト
 * \details POST_RUN 継続要求が残っている間は、タイムアウトを過ぎても SHUTDOWN へ遷移しない（[SWS_EcuM_04128]）。最後の要求を解放した時点から
 *          改めてタイムアウトを数える（[SWS_EcuM_04129]）。
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

/* EcuM_RequestPOST_RUN() / EcuM_ReleasePOST_RUN() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして実体の EcuM を呼び、EcuM を未初期化へ戻す。時刻の源（Gpt と Os、タスク 0 件）・Dem・Wdg・WdgM を
 *          EcuM_Init() と同じ順序で初期化し、各 wrap の呼び出し回数と DET の記録も初期化する（WdgM のモード切替が実際に反映されるように、
 *          WdgM も実体を初期化する）。
 * Advance(): Gpt の時間源と millis() を同じだけ進める（EcuM の POST_RUN タイムアウトの時刻は Os のカウンタから取る）。
 * TearDown(): Gpt・Os・EcuM と模擬 EcuM を元に戻す。 */
class Bsw_EcuM_PostRunHold_Test : public ::testing::Test
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

TEST_F(Bsw_EcuM_PostRunHold_Test, EcuM_ReleasePOST_RUN_OK_LastReleaseRestartsTimeout)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);
    ASSERT_EQ(EcuM_RequestRUN(ECUM_USER_COMM), E_OK);
    ASSERT_EQ(EcuM_ReleaseRUN(ECUM_USER_COMM), E_OK);
    ASSERT_EQ(EcuM_GetState(), ECUM_STATE_POST_RUN);
    WrapBswM_Reset();
    WrapWdgM_Reset();
    ASSERT_EQ(EcuM_RequestPOST_RUN(ECUM_USER_COMM), E_OK);
    Advance(ECUM_POST_RUN_TIMEOUT_MS + 1000UL);
    EcuM_MainFunction();
    ASSERT_EQ(EcuM_GetState(), ECUM_STATE_POST_RUN);   // 継続要求中は保留

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = EcuM_ReleasePOST_RUN(ECUM_USER_COMM);
    Advance(ECUM_POST_RUN_TIMEOUT_MS - 1UL);
    EcuM_MainFunction();
    const EcuM_StateType stateBeforeTimeout = EcuM_GetState();

    Advance(1U);
    EcuM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(stateBeforeTimeout, ECUM_STATE_POST_RUN);   // 解放から 4999ms ではまだ
    EXPECT_EQ(EcuM_GetState(), ECUM_STATE_SHUTDOWN);
}

}  // namespace
