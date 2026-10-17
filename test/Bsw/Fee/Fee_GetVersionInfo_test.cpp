/**
 * \file    Fee_GetVersionInfo_test.cpp
 * \brief   Fee_GetVersionInfo() の単体テスト
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
class Fee_GetVersionInfo_Test : public ::testing::Test
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

TEST_F(Fee_GetVersionInfo_Test, Fee_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Fee_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_PARAM_POINTER);
}

TEST_F(Fee_GetVersionInfo_Test, Fee_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Fee_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, FEE_VENDOR_ID);
    EXPECT_EQ(info.moduleID, FEE_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, FEE_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, FEE_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, FEE_SW_PATCH_VERSION);
}

}  // namespace
