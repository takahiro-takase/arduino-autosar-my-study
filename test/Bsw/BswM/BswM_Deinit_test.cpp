/**
 * \file    BswM_Deinit_test.cpp
 * \brief   BswM_Deinit() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 BswM_test.cpp）。
 *
 *          BswM には `BswM_Deinit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `BswM_Deinit()` を呼んでから検証する）。
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
class BswM_Deinit_Test : public ::testing::Test
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

TEST_F(BswM_Deinit_Test, BswM_Deinit_NG_NoInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Deinit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    BswM_Deinit();  // 2回目: 既に未初期化のため NG

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

}  // namespace
