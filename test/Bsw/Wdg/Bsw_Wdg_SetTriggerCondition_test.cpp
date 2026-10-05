/**
 * \file    Bsw_Wdg_SetTriggerCondition_test.cpp
 * \brief   Wdg_SetTriggerCondition()（src/Bsw/Wdg/Wdg.c）のタイムアウト値ごとの動作の単体テスト
 * \details [SWS_Wdg_00140]: timeout=0 は「トリガの（ほぼ）即時停止と ECU の
 *          （ほぼ）即時リセット」の要求で、一度 0 を受けた後の呼び出しは何もしない。
 *          以前の実装は timeout の値によらずリフレッシュしていたため、0 を渡すと
 *          逆にウォッチドッグがリフレッシュされていた。
 *          timeout が 0 以外のときは、HW の制約により値によらずリフレッシュのみ行う。
 *
 *          実 HW への依存は `stub/Hal/Fake_Wdg_Hw.c` で満たし、リフレッシュ回数と
 *          強制リセット回数で観測する。
 *
 *          Det の NG ケースは Bsw_Wdg_test.cpp にある。GoogleTest の main() は
 *          test_main.cpp に集約しているため、本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "Fake_Wdg_Hw.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Wdg_SetTriggerCondition() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Wdg_Hw をリセットし、Wdg を未初期化の状態へ戻したうえで Wdg_Init(&Wdg_Config) を呼ぶ（Init 自体の DET 記録は消す）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Bsw_Wdg_SetTriggerCondition_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeWdgHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        Wdg_Test_ResetInitState();
        Wdg_Init(&Wdg_Config);
        FakeDetHw_Reset();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Wdg_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_Wdg_SetTriggerCondition_Test, Wdg_SetTriggerCondition_OK_NonZeroTimeoutRefreshesOnly)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Wdg_SetTriggerCondition(Wdg_Config.DefaultTimeoutMs);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeWdgHw_RefreshCount, 1U);
    EXPECT_EQ(FakeWdgHw_ForceResetCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_Wdg_SetTriggerCondition_Test, Wdg_SetTriggerCondition_OK_ZeroTimeoutStopsTriggerAndForcesReset)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeDetHw_LogSuppressed = 1U;  // 強制リセット要求の ERROR ログを抑制

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Wdg_SetTriggerCondition(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeWdgHw_ForceResetCount, 1U);
    EXPECT_EQ(FakeWdgHw_RefreshCount, 0U);  // 0 を渡してリフレッシュしてはならない
}

TEST_F(Bsw_Wdg_SetTriggerCondition_Test, Wdg_SetTriggerCondition_OK_CallsAfterZeroAreIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeDetHw_LogSuppressed = 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Wdg_SetTriggerCondition(0U);

    Wdg_SetTriggerCondition(Wdg_Config.DefaultTimeoutMs);
    Wdg_SetTriggerCondition(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeWdgHw_ForceResetCount, 1U);  // 2 回目の 0 でもリセット要求を重ねない
    EXPECT_EQ(FakeWdgHw_RefreshCount, 0U);     // 0 を受けた後はリフレッシュしない
}

TEST_F(Bsw_Wdg_SetTriggerCondition_Test, Wdg_SetTriggerCondition_OK_ReInitClearsStoppedState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeDetHw_LogSuppressed = 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Wdg_SetTriggerCondition(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(FakeWdgHw_ForceResetCount, 1U);

    Wdg_Init(&Wdg_Config);  // 初期化し直すと timeout=0 によるトリガ停止も解除される
    Wdg_SetTriggerCondition(Wdg_Config.DefaultTimeoutMs);

    EXPECT_EQ(FakeWdgHw_RefreshCount, 1U);
}

}  // namespace
