/**
 * \file    CryIf_GetVersionInfo_test.cpp
 * \brief   CryIf_GetVersionInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CryIf_test.cpp）。
 *
 *          分割前の共通の説明は、旧 CryIf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CryIf.h"
#include "Crypto_Cfg.h"
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

/* CryIf の単体テスト用フィクスチャ。
 * SetUp(): CryIf_Init() を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): 未初期化の状態へ戻す。 */
class CryIf_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        CryIf_Init();
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CryIf_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CryIf_GetVersionInfo_Test, CryIf_GetVersionInfo_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CryIf_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CryIf_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_UNINIT);
}

TEST_F(CryIf_GetVersionInfo_Test, CryIf_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CryIf_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_POINTER);
}

}  // namespace
