/**
 * \file    E2E_CheckInit_test.cpp
 * \brief   E2E_CheckInit() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 E2E_test.cpp）。
 *
 *          分割前の共通の説明は、旧 E2E_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "E2E.h"
#include "E2E_P01.h"
#include "E2E_P05.h"
#include "Wrap_E2E.h"
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

/**
 * \brief  E2E_SMCheck()/E2E_SMCheckInit() の単体テスト（[SWS_E2E_00340]/
 *         [SWS_E2E_00353]）。
 * \details 状態遷移の閾値は WindowSize=3、Init系=3件全OK/0件Errorで昇格、
 *          Valid系=1件OK/1件Errorまで許容、Invalid系=3件全OK/0件Errorで
 *          復帰、という小さく手計算しやすい値にしている。
 */
class E2E_CheckInit_Test : public ::testing::Test
{
protected:
    uint8_t          window[3];
    E2E_SMConfigType config;
    E2E_SMCheckStateType state;

    void SetUp() override
    {
        /* E2E_SMCheck() は -Wl,--wrap=E2E_SMCheck 経由でリンクされるため、
         * 他ファイル（E2EXf_SMCheckFailure_test.cpp 等）が
         * FailFromCallCount_E2E_SMCheck を設定したまま（自身の次のテストの
         * SetUp() でのみリセットする設計）だと、native_chain_tests 全体の
         * 実行順序によっては本フィクスチャへ漏れ伝わる（2026-09、
         * --gtest_shuffle でのみ顕在化する形で発見。他モジュールのテストと
         * 同じ「他ファイルの故障注入が漏れ伝わらないよう防御的にリセット」
         * 方針をここにも適用する）。 */
        WrapE2E_Reset();

        config.WindowSize           = 3U;
        config.MinOkStateInit       = 3U;
        config.MaxErrorStateInit    = 0U;
        config.MinOkStateValid      = 1U;
        config.MaxErrorStateValid   = 1U;
        config.MinOkStateInvalid    = 3U;
        config.MaxErrorStateInvalid = 0U;

        state.ProfileStatusWindow = window;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(E2E_CheckInit_Test, CheckInit_NG_NullPointerReturnsInputErrNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret1 = E2E_SMCheckInit(nullptr, &config);
    Std_ReturnType ret2 = E2E_SMCheckInit(&state, nullptr);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret1, E2E_E_INPUTERR_NULL);
    EXPECT_EQ(ret2, E2E_E_INPUTERR_NULL);
}

TEST_F(E2E_CheckInit_Test, CheckInit_OK_SetsNodataStateAndClearsWindow)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_SMCheckInit(&state, &config);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E2E_E_OK);

    EXPECT_EQ(state.SMState, E2E_SM_NODATA);
    EXPECT_EQ(state.WindowTopIndex, 0U);
    EXPECT_EQ(state.OkCount, 0U);
    EXPECT_EQ(state.ErrorCount, 0U);
    for (uint8_t i = 0U; i < config.WindowSize; i++)
        EXPECT_EQ(window[i], static_cast<uint8_t>(E2E_P_NOTAVAILABLE));
}

}  // namespace
