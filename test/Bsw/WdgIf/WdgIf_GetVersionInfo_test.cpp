/**
 * \file    WdgIf_GetVersionInfo_test.cpp
 * \brief   WdgIf_GetVersionInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 WdgIf_test.cpp）。
 *
 *          WdgIf.c は `WdgIf_CheckDevice()`（1箇所の `Det_ReportError()`
 *          呼び出し）を SetMode/SetTriggerCondition の2エントリポイントから
 *          呼ぶ薄い層のため、ソース上の `Det_ReportError()` 呼び出し箇所は
 *          2箇所（本関数＋GetVersionInfo）のみだが、各エントリポイントで
 *          報告される ApiId が正しい値になっていることを個別に検証する
 *          （`Fake_Det_Hw.h`の`FakeDetHw_LastErrorId`/`FakeDetHw_LastApiId`）。
 *
 *          分割前の共通の説明は、旧 WdgIf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "WdgIf.h"
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

/* WdgIf の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。 */
class WdgIf_GetVersionInfo_Test : public ::testing::Test
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

TEST_F(WdgIf_GetVersionInfo_Test, WdgIf_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgIf_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGIF_E_PARAM_POINTER);
    EXPECT_EQ(FakeDetHw_LastApiId, WDGIF_API_ID_GET_VERSION_INFO);
}

TEST_F(WdgIf_GetVersionInfo_Test, WdgIf_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgIf_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, WDGIF_VENDOR_ID);
    EXPECT_EQ(info.moduleID, WDGIF_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, WDGIF_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, WDGIF_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, WDGIF_SW_PATCH_VERSION);
}

}  // namespace
