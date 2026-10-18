/**
 * \file    IoHwAb_GetVersionInfo_test.cpp
 * \brief   IoHwAb_GetVersionInfo() の単体テスト
 * \details NULL を渡すと IOHWAB_E_PARAM_POINTER を DET へ報告して何も書かない。
 *          それ以外は、IoHwAb_Cfg.h に定義したベンダー ID・モジュール ID・バージョンを返す。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "IoHwAb.h"
#include "IoHwAb_Cfg.h"
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

/* IoHwAb_GetVersionInfo() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。 */
class IoHwAb_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(IoHwAb_GetVersionInfo_Test, IoHwAb_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    IoHwAb_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastModuleId, IOHWAB_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, IOHWAB_API_ID_GET_VERSION_INFO);
    EXPECT_EQ(FakeDetHw_LastErrorId, IOHWAB_E_PARAM_POINTER);
}

TEST_F(IoHwAb_GetVersionInfo_Test, IoHwAb_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    IoHwAb_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, IOHWAB_VENDOR_ID);
    EXPECT_EQ(info.moduleID, IOHWAB_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, IOHWAB_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, IOHWAB_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, IOHWAB_SW_PATCH_VERSION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // DET は報告しない
}

}  // namespace
