/**
 * \file    EcuM_StartWakeupSources_test.cpp
 * \brief   EcuM_StartWakeupSources() の単体テスト（コールアウト、[SWS_EcuM_02924]）
 * \details CAN の起床要因が含まれていれば CanSM の 検証を始める（CanSM_StartWakeupSource）、含まれていなければ何もしない。
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
#include "Wrap_CanSM.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

namespace
{

/** CAN 以外の起床要因（予約済みのビットのひとつ。ECUM_WKSOURCE_CAN と重ならない値） */
constexpr EcuM_WakeupSourceType kOtherSource = 0x01UL;

/* EcuM_StartWakeupSources() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして EcuM を未初期化へ戻し、CanSM の wrap と DET の記録を初期化する（CanSM は未初期化のまま、呼び出し回数だけを見る）。
 * TearDown(): 模擬 EcuM と wrap の状態も含めて元に戻す。 */
class EcuM_StartWakeupSources_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeEcuM_Reset();
        FakeEcuM_PassThrough = 1U;
        EcuM_Test_ResetInitState();
        WrapCanSM_Reset();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
    }

    void TearDown() override
    {
        EcuM_Test_ResetInitState();
        FakeEcuM_Reset();  // PassThrough も FALSE へ戻る
        WrapCanSM_Reset();
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// OK: CAN の起床要因が含まれていれば、CanSM の 検証を始める（CanSM_StartWakeupSource）
TEST_F(EcuM_StartWakeupSources_Test, EcuM_StartWakeupSources_OK_CanSourceCallsCanSm)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_StartWakeupSources(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanSM_StartWakeupSource, 1U);
    EXPECT_EQ(CallCount_CanSM_StopWakeupSource, 0U);
}

// OK: CAN 以外の要因だけなら、何もしない
TEST_F(EcuM_StartWakeupSources_Test, EcuM_StartWakeupSources_OK_NonCanSourceIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_StartWakeupSources(kOtherSource);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanSM_StartWakeupSource, 0U);
    EXPECT_EQ(CallCount_CanSM_StopWakeupSource, 0U);
}

// OK: CAN と CAN 以外の要因が混ざっていても、CAN の分だけ、CanSM を 1 回呼ぶ
TEST_F(EcuM_StartWakeupSources_Test, EcuM_StartWakeupSources_OK_MixedMaskCallsCanSmOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    static_assert((ECUM_WKSOURCE_CAN & 0x01UL) == 0U, "kOtherSource は CAN と重ならないこと");

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_StartWakeupSources(ECUM_WKSOURCE_CAN | kOtherSource);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanSM_StartWakeupSource, 1U);
    EXPECT_EQ(CallCount_CanSM_StopWakeupSource, 0U);
}

}  // namespace
