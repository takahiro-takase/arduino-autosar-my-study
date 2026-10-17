/**
 * \file    Dio_GetVersionInfo_test.cpp
 * \brief   Dio_GetVersionInfo() の単体テスト
 * \details モジュールのベンダー ID・モジュール ID・バージョンを返す。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dio.h"
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

/* Dio_GetVersionInfo() の単体テスト用フィクスチャ。
 * SetUp()/TearDown(): なし（Dio_GetVersionInfo() は初期化に依存しない）。 */
class Dio_GetVersionInfo_Test : public ::testing::Test
{
protected:
    // 初期化に依存しないため、準備と後始末は不要。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dio_GetVersionInfo_Test, Dio_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, DIO_VENDOR_ID);
    EXPECT_EQ(info.moduleID, DIO_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, DIO_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, DIO_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, DIO_SW_PATCH_VERSION);
}

}  // namespace
