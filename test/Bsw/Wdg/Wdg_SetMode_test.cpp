/**
 * \file    Wdg_SetMode_test.cpp
 * \brief   Wdg_SetMode() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Wdg_test.cpp）。
 *
 *          `Wdg_SetMode(WDGIF_OFF_MODE)`（本プロジェクトの HW 制約により常に
 *          E_NOT_OK を返すが、報告は Det ではなく Dem 経由）は本ファイルの
 *          スコープ外（Det_ReportError() を呼ばないため）。
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
class Wdg_SetMode_Test : public ::testing::Test
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

TEST_F(Wdg_SetMode_Test, Wdg_SetMode_NG_DriverState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Wdg_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Wdg_SetMode(WDGIF_FAST_MODE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_DRIVER_STATE);
}

TEST_F(Wdg_SetMode_Test, Wdg_SetMode_NG_ParamMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // WDGIF_SLOW_MODE は本プロジェクトが対応しない値（default 分岐、
    // Wdg.c 参照）。
    Std_ReturnType ret = Wdg_SetMode(static_cast<WdgIf_ModeType>(0xFFU));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_PARAM_MODE);
}

}  // namespace
