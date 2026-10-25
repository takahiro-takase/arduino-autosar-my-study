/**
 * \file    EcuM_CheckWakeup_test.cpp
 * \brief   EcuM_CheckWakeup() の CAN 起床要因の単体テスト
 * \details CAN の起床要因を受けると検証待ちにして CanSM_ControllerModeIndication() へ委譲し、以降の EcuM_MainFunction() が検証を問い合わせる。
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

/* EcuM_CheckWakeup() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして実体の EcuM を呼び、EcuM を未初期化へ戻す。時刻の源（Gpt と Os、タスク 0 件）・Dem・Wdg・WdgM を
 *          EcuM_Init() と同じ順序で初期化し、各 wrap の呼び出し回数と DET の記録も初期化する（WdgM のモード切替が実際に反映されるように、
 *          WdgM も実体を初期化する）。
 * Advance(): Gpt の時間源と millis() を同じだけ進める（EcuM の POST_RUN タイムアウトの時刻は Os のカウンタから取る）。
 * ExpireCanWakeup(): CAN の起床を、検証されないまま、期限切れ（EXPIRED）にする。
 * TearDown(): Gpt・Os・EcuM と模擬 EcuM を元に戻す。 */
class EcuM_CheckWakeup_Test : public ::testing::Test
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

    /** CAN の起床を検出し、検証されないまま、検証のタイムアウトを超えさせて、期限切れ（EXPIRED）にする。 */
    void ExpireCanWakeup()
    {
        EcuM_Test_SetState(ECUM_STATE_RUN);
        EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);
        Advance(ECUM_VALIDATION_TIMEOUT_MS + 1UL);
        EcuM_MainFunction();
    }
};

/* EcuM の単体テスト用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして（実体の EcuM を呼ぶ）EcuM を未初期化へ戻し、DET の記録を初期化する。ExpectDet() は、DET 報告が 1 件で、API ID とエラー ID が一致することを確認する補助関数。
 * TearDown(): 模擬 EcuM の状態も含めて元に戻す。 */
class EcuM_CheckWakeup_Base_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeEcuM_Reset();
        FakeEcuM_PassThrough = 1U;
        EcuM_Test_ResetInitState();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        EcuM_Test_ResetInitState();
        FakeEcuM_Reset();  // PassThrough も FALSE へ戻る
    }

    static void ExpectDet(uint8 apiId, uint8 errorId)
    {
        EXPECT_EQ(FakeDetHw_ReportCount, 1U);
        EXPECT_EQ(FakeDetHw_LastModuleId, ECUM_MODULE_ID);
        EXPECT_EQ(FakeDetHw_LastApiId, apiId);
        EXPECT_EQ(FakeDetHw_LastErrorId, errorId);
    }

    static const EcuM_UserType kInvalidUser = ECUM_USER_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(EcuM_CheckWakeup_Base_Test, EcuM_CheckWakeup_NG_InvalidSource)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(0U);

    ExpectDet(ECUM_API_ID_CHECK_WAKEUP, ECUM_E_INVALID_PAR);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // 異常終了せずに戻ること（EXPECT/ASSERT は無い）
}

TEST_F(EcuM_CheckWakeup_Test, EcuM_CheckWakeup_OK_CanSourceDelegatesToCanSm)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);
    const uint8 moduleAfterCheck = static_cast<uint8>(FakeDetHw_LastModuleId);
    const uint8 apiAfterCheck = FakeDetHw_LastApiId;

    EcuM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // CanSM は未初期化のため、委譲先の CanSM が UNINIT を報告する（委譲された証拠）
    EXPECT_EQ(moduleAfterCheck, static_cast<uint8>(CANSM_MODULE_ID));
    EXPECT_EQ(apiAfterCheck, CANSM_API_ID_CONTROLLER_MODE_INDICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
    EXPECT_EQ(CallCount_Os_SchedulerStep, 1U);
}

// OK: 起床を検出（EcuM_CheckWakeup）した要因は、検証待ち（PENDING）として返る（[SWS_EcuM_01156]）。
TEST_F(EcuM_CheckWakeup_Test, EcuM_GetPendingWakeupEvents_OK_ReturnsPendingEventAfterCheckWakeup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), 0U);   // 何も起きていない間は、空

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), ECUM_WKSOURCE_CAN);
    EXPECT_EQ(EcuM_GetValidatedWakeupEvents(), 0U);
    EXPECT_EQ(EcuM_GetExpiredWakeupEvents(), 0U);
}

// OK: 検証のタイムアウト（ECUM_VALIDATION_TIMEOUT_MS）の前は、検証待ちのまま（期限切れにしない）。
TEST_F(EcuM_CheckWakeup_Test, EcuM_MainFunction_OK_PendingWakeupIsKeptBeforeValidationTimeout)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);
    Advance(ECUM_VALIDATION_TIMEOUT_MS - 100UL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), ECUM_WKSOURCE_CAN);
    EXPECT_EQ(EcuM_GetExpiredWakeupEvents(), 0U);
}

// OK: 検証のタイムアウトまでに検証されなかった起床要因は、期限切れ（EXPIRED）になり、検証待ちから外れる（[SWS_EcuM_04081]）。
TEST_F(EcuM_CheckWakeup_Test, EcuM_MainFunction_OK_PendingWakeupExpiresAfterValidationTimeout)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);
    Advance(ECUM_VALIDATION_TIMEOUT_MS + 1UL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), 0U);
    EXPECT_EQ(EcuM_GetExpiredWakeupEvents(), ECUM_WKSOURCE_CAN);
    EXPECT_EQ(EcuM_GetValidatedWakeupEvents(), 0U);
}

// OK: 検証待ちの間に、同じ要因を再び検出しても、検証のタイムアウトは起算し直さない（[SWS_EcuM_02712]）。
TEST_F(EcuM_CheckWakeup_Test, EcuM_CheckWakeup_OK_RepeatedCallDoesNotRestartValidationTimeout)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);
    Advance(ECUM_VALIDATION_TIMEOUT_MS - 100UL);
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);   // 再通知（起算し直さない）
    Advance(200UL);   // 最初の検出から、タイムアウトを超える

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetExpiredWakeupEvents(), ECUM_WKSOURCE_CAN);
}

// OK: 期限切れの後に、新しく起床を検出すると、再び検証待ちになり、期限切れの記録は消える。
TEST_F(EcuM_CheckWakeup_Test, EcuM_CheckWakeup_OK_NewWakeupAfterExpiryIsPendingAgain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ExpireCanWakeup();
    ASSERT_EQ(EcuM_GetExpiredWakeupEvents(), ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), ECUM_WKSOURCE_CAN);
    EXPECT_EQ(EcuM_GetExpiredWakeupEvents(), 0U);
}

// OK: 検証されると、検証済み（VALIDATED）になり、検証のタイムアウトは止まる（[SWS_EcuM_02710]）。時間が経っても、期限切れにならない。
TEST_F(EcuM_CheckWakeup_Test, EcuM_ValidateWakeupEvent_OK_RecordsValidatedAndStopsValidationTimeout)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);
    EcuM_ValidateWakeupEvent(ECUM_WKSOURCE_CAN);
    Advance(ECUM_VALIDATION_TIMEOUT_MS + 1UL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), 0U);
    EXPECT_EQ(EcuM_GetValidatedWakeupEvents(), ECUM_WKSOURCE_CAN);
    EXPECT_EQ(EcuM_GetExpiredWakeupEvents(), 0U);
}

// OK: 期限切れになった後に、遅れて検証されると、検証済みになり、期限切れの記録は消える。
TEST_F(EcuM_CheckWakeup_Test, EcuM_ValidateWakeupEvent_OK_LateValidationRemovesExpiredEvent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ExpireCanWakeup();
    ASSERT_EQ(EcuM_GetExpiredWakeupEvents(), ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_ValidateWakeupEvent(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetExpiredWakeupEvents(), 0U);
    EXPECT_EQ(EcuM_GetValidatedWakeupEvents(), ECUM_WKSOURCE_CAN);
}

// OK: EcuM_ClearWakeupEvent は、検証待ち（PENDING）から、指定した要因を消す（[SWS_EcuM_02683]）。
TEST_F(EcuM_CheckWakeup_Test, EcuM_ClearWakeupEvent_OK_ClearsPendingEvent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);
    ASSERT_EQ(EcuM_GetPendingWakeupEvents(), ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_ClearWakeupEvent(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), 0U);
}

// OK: EcuM_ClearWakeupEvent は、検証済み（VALIDATED）から、指定した要因を消す（[SWS_EcuM_02683]）。
TEST_F(EcuM_CheckWakeup_Test, EcuM_ClearWakeupEvent_OK_ClearsValidatedEvent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);
    EcuM_ValidateWakeupEvent(ECUM_WKSOURCE_CAN);
    ASSERT_EQ(EcuM_GetValidatedWakeupEvents(), ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_ClearWakeupEvent(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetValidatedWakeupEvents(), 0U);
}

// OK: EcuM_ClearWakeupEvent は、期限切れ（EXPIRED）から、指定した要因を消す（[SWS_EcuM_02683]）。
TEST_F(EcuM_CheckWakeup_Test, EcuM_ClearWakeupEvent_OK_ClearsExpiredEvent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ExpireCanWakeup();
    ASSERT_EQ(EcuM_GetExpiredWakeupEvents(), ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_ClearWakeupEvent(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(EcuM_GetExpiredWakeupEvents(), 0U);
}

}  // namespace
