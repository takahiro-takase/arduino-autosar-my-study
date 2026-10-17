/**
 * \file    WdgM_GetVersionInfo_test.cpp
 * \brief   WdgM_GetVersionInfo() の単体テスト
 * \details モジュールのベンダー ID・モジュール ID・バージョンを返す。
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

/* WdgM_GetVersionInfo() の単体テスト用フィクスチャ。
 * SetUp()/TearDown(): なし（WdgM_GetVersionInfo() は初期化に依存しない）。 */
class WdgM_GetVersionInfo_Test : public ::testing::Test
{
protected:
    // 初期化に依存しないため、準備と後始末は不要。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(WdgM_GetVersionInfo_Test, WdgM_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_INV_POINTER);
}

TEST_F(WdgM_GetVersionInfo_Test, WdgM_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, WDGM_VENDOR_ID);
    EXPECT_EQ(info.moduleID, WDGM_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, WDGM_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, WDGM_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, WDGM_SW_PATCH_VERSION);
}

}  // namespace
