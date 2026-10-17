/**
 * \file    WdgM_ResumeSupervision_test.cpp
 * \brief   WdgM_ResumeSupervision() の単体テスト
 * \details 初期化前に呼んでも、何もせずに戻る。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "WdgM.h"
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

/* WdgM_ResumeSupervision() の単体テスト用フィクスチャ。
 * SetUp(): WdgM_DeInit() で未初期化の状態から始める。
 * TearDown(): WdgM_DeInit() で未初期化へ戻す。 */
class WdgM_ResumeSupervision_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WdgM_DeInit();   // 他のテストファイルが初期化したまま残していても、未初期化から始める
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WdgM_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(WdgM_ResumeSupervision_Test, WdgM_ResumeSupervision_NG_UninitializedDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で未初期化にしてある）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_ResumeSupervision();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // DET も報告しない
}

}  // namespace
