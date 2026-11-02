/**
 * \file    EcuM_CheckWakeup_test.cpp
 * \brief   EcuM_CheckWakeup() の単体テスト
 * \details EcuM_CheckWakeup() は、CAN の起床要因について CanIf_CheckWakeup() を呼ぶだけの窓口
 *          （検証待ちにして検証を始めるのは、Can_CheckWakeup() が呼ぶ EcuM_SetWakeupEvent()。
 *          その検証は EcuM_SetWakeupEvent_test.cpp）。2026-11、EcuM_CheckWakeup() が検証待ちを兼ねていたのを分けた。
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
#include "Wrap_CanIf.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* EcuM_CheckWakeup() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして実体の EcuM を呼び、EcuM を未初期化へ戻す。CanIf の wrap と DET の記録も初期化する。
 * TearDown(): 模擬 EcuM の状態も含めて元に戻す。 */
class EcuM_CheckWakeup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeEcuM_Reset();
        FakeEcuM_PassThrough = 1U;
        WrapCanIf_Reset();
        EcuM_Test_ResetInitState();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapCanIf_Reset();
        EcuM_Test_ResetInitState();
        FakeEcuM_Reset();  // PassThrough も FALSE へ戻る
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(EcuM_CheckWakeup_Test, EcuM_CheckWakeup_NG_InvalidSource)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
    EXPECT_EQ(FakeDetHw_LastModuleId, ECUM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, ECUM_API_ID_CHECK_WAKEUP);
    EXPECT_EQ(FakeDetHw_LastErrorId, ECUM_E_INVALID_PAR);
    EXPECT_EQ(CallCount_CanIf_CheckWakeup, 0U);   // 未知の要因は CanIf へ渡さない
}

TEST_F(EcuM_CheckWakeup_Test, EcuM_CheckWakeup_OK_CanSourceCallsCanIfCheckWakeup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_CheckWakeup = 1U;
    ForcedReturn_CanIf_CheckWakeup      = E_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_CheckWakeup, 1U);
    EXPECT_EQ(LastWakeupSource_CanIf_CheckWakeup, ECUM_WKSOURCE_CAN);
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), 0U);   // 検証待ちにするのは、Can が呼ぶ EcuM_SetWakeupEvent()（ここでは呼ばれない）
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(EcuM_CheckWakeup_Test, EcuM_CheckWakeup_OK_OnlyCanBitIsPassedToCanIf)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_CheckWakeup = 1U;
    ForcedReturn_CanIf_CheckWakeup      = E_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN | 0x00000002UL);   // CAN 以外の要因のビットも立っている

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_CheckWakeup, 1U);
    EXPECT_EQ(LastWakeupSource_CanIf_CheckWakeup, ECUM_WKSOURCE_CAN);   // CAN のビットだけを渡す
}

TEST_F(EcuM_CheckWakeup_Test, EcuM_CheckWakeup_OK_CanIfNotAcceptedReturnsWithoutError)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_CheckWakeup = 1U;
    ForcedReturn_CanIf_CheckWakeup      = E_NOT_OK;   // 起床を検出していない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_CheckWakeup, 1U);
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 検出していないのは、エラーではない
}

}  // namespace
