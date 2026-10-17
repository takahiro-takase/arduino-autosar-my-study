/**
 * \file    Fee_Read_test.cpp
 * \brief   Fee_Read() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Fee_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Fee_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Fee.h"
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

/* Fee の単体テスト用フィクスチャ。
 * SetUp(): Fee_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。ArrangeJobActive() は、書き込みジョブを 1 件積んで Fee を BUSY にする補助関数。
 * TearDown(): 未初期化の状態へ戻す。 */
class Fee_Read_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Fee_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Fee_Test_ResetInitState();
    }

    /* Fee_Job.Active=1（MEMIF_BUSY）を作るヘルパー。 */
    void ArrangeJobActive()
    {
        static const uint8 data[1] = { 0x00U };
        ASSERT_EQ(Fee_Write(0U, data, 1U), E_OK);
        ASSERT_EQ(Fee_GetStatus(), MEMIF_BUSY);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Fee_Read_Test, Fee_Read_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Fee_Test_ResetInitState();
    uint8 buf[1];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Fee_Read(0U, buf, 1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_UNINIT);
}

TEST_F(Fee_Read_Test, Fee_Read_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Fee_Read(0U, NULL, 1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_PARAM_POINTER);
}

TEST_F(Fee_Read_Test, Fee_Read_NG_InvalidBlockLen)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[1];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Fee_Read(0U, buf, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_INVALID_BLOCK_LEN);
}

TEST_F(Fee_Read_Test, Fee_Read_NG_Busy)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ArrangeJobActive();
    uint8 buf[1];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Fee_Read(0U, buf, 1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_BUSY);
}

}  // namespace
