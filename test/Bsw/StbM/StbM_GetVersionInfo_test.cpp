/**
 * \file    StbM_GetVersionInfo_test.cpp
 * \brief   StbM_GetVersionInfo() の単体テスト
 * \details NULL を渡すと STBM_E_PARAM_POINTER を DET へ報告して何も書かない。それ以外は、StbM_Cfg.h に定義したベンダー ID・モジュール ID・バージョンを返す。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "StbM.h"
#include "StbM_Cfg.h"
#include "Fake_Millis.h"
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

/* StbM_GetVersionInfo() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeMillis_Value = 12345UL;
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        StbM_Init(NULL);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(StbM_GetVersionInfo_Test, StbM_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StbM_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_VERSION_INFO);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_GetVersionInfo_Test, StbM_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StbM_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, STBM_VENDOR_ID);
    EXPECT_EQ(info.moduleID, STBM_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, STBM_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, STBM_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, STBM_SW_PATCH_VERSION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // DET は報告しない
}

}  // namespace
