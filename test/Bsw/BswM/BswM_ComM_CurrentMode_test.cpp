/**
 * \file    BswM_ComM_CurrentMode_test.cpp
 * \brief   BswM_ComM_CurrentMode() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 BswM_test.cpp）。
 *
 *          分割前の共通の説明は、旧 BswM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "BswM.h"
#include "BswM_PBCfg.h"
#include "Wrap_BswM.h"
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

/* BswM の単体テスト用フィクスチャ。
 * SetUp(): wrap の状態を戻し、BswM_Init(&BswM_Config) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): BswM_Deinit() で未初期化へ戻す。 */
class BswM_ComM_CurrentMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapBswM_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        BswM_Init(&BswM_Config);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        BswM_Deinit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(BswM_ComM_CurrentMode_Test, BswM_ComM_CurrentMode_NG_NoInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    BswM_Deinit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_ComM_CurrentMode(0U, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

TEST_F(BswM_ComM_CurrentMode_Test, BswM_ComM_CurrentMode_NG_ReqModeOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_ComM_CurrentMode(0U, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION + 1U));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_REQ_MODE_OUT_OF_RANGE);
}

}  // namespace
