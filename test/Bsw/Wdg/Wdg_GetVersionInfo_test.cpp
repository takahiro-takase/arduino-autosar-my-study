/**
 * \file    Wdg_GetVersionInfo_test.cpp
 * \brief   Wdg_GetVersionInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Wdg_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Wdg_test.cpp の冒頭コメント（git の履歴）を参照。
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

/* Wdg の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Wdg_Hw をリセットし、Wdg_Init(&Wdg_Config) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Wdg_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeWdgHw_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Wdg_Init(&Wdg_Config);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
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

TEST_F(Wdg_GetVersionInfo_Test, Wdg_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Wdg_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_PARAM_POINTER);
}

TEST_F(Wdg_GetVersionInfo_Test, Wdg_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Wdg_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, WDG_VENDOR_ID);
    EXPECT_EQ(info.moduleID, WDG_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, WDG_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, WDG_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, WDG_SW_PATCH_VERSION);
}

}  // namespace
