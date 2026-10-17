/**
 * \file    Port_GetVersionInfo_test.cpp
 * \brief   Port_GetVersionInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Port_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Port_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Port.h"
#include "Fake_Port_Hw.h"
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

/* Port の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Port_Hw と DET の記録を初期化する。 */
class Port_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakePortHw_Reset();
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Port_GetVersionInfo_Test, GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Port_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, PORT_E_PARAM_POINTER);
}

TEST_F(Port_GetVersionInfo_Test, Port_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Port_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, PORT_VENDOR_ID);
    EXPECT_EQ(info.moduleID, PORT_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, PORT_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, PORT_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, PORT_SW_PATCH_VERSION);
}

}  // namespace
