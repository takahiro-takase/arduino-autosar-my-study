/**
 * \file    E2E_Check_test.cpp
 * \brief   E2E_Check() の単体テスト
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
class E2E_Check_Test : public ::testing::Test
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

TEST_F(E2E_Check_Test, Check_NG_NullPointerReturnsInputErrNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret1 = E2E_SMCheck(E2E_P_OK, nullptr, &state);
    Std_ReturnType ret2 = E2E_SMCheck(E2E_P_OK, &config, nullptr);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret1, E2E_E_INPUTERR_NULL);
    EXPECT_EQ(ret2, E2E_E_INPUTERR_NULL);
}

TEST_F(E2E_Check_Test, Check_NG_DeinitStateReturnsWrongStateWithoutChangingState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* E2E_SMCheckInit() を一度も呼んでいない場合を模擬するため、State を
     * 明示的に E2E_SM_DEINIT にする。注意: [SWS_E2E_00343] の値定義は
     * E2E_SM_VALID=0x00・E2E_SM_DEINIT=0x01 のため、ゼロ初期化しただけでは
     * DEINIT にはならず（誤って VALID 扱いになってしまう）、呼び出し元は
     * 必ず E2E_SMCheckInit() を明示的に呼ぶ必要がある（本テストはその
     * 「明示的な初期化が必須」という前提を裏付けるためのもの）。 */
    state.SMState = E2E_SM_DEINIT;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_SMCheck(E2E_P_OK, &config, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_WRONGSTATE);
    EXPECT_EQ(state.SMState, E2E_SM_DEINIT); /* 状態遷移しないこと */
}

TEST_F(E2E_Check_Test, Check_OK_NodataStaysUntilFirstGoodStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retError = E2E_SMCheck(E2E_P_ERROR, &config, &state);
    E2E_SMCheckStateType stateAfterError = state;

    Std_ReturnType retNoNewData = E2E_SMCheck(E2E_P_NONEWDATA, &config, &state);
    E2E_SMCheckStateType stateAfterNoNewData = state;

    Std_ReturnType retOk = E2E_SMCheck(E2E_P_OK, &config, &state);
    E2E_SMCheckStateType stateAfterOk = state;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retError, E2E_E_OK);
    EXPECT_EQ(stateAfterError.SMState, E2E_SM_NODATA);

    ASSERT_EQ(retNoNewData, E2E_E_OK);
    EXPECT_EQ(stateAfterNoNewData.SMState, E2E_SM_NODATA);

    /* NODATA からの遷移では E2E_SMAddStatus() を呼ばない（PDFベクタ座標解析で
     * 確認済み、E2E_SMCheck() の Doxygen 参照）ため、ここまで OkCount/
     * ErrorCount は 0 のままのはず。 */
    EXPECT_EQ(stateAfterNoNewData.OkCount, 0U);
    EXPECT_EQ(stateAfterNoNewData.ErrorCount, 0U);

    ASSERT_EQ(retOk, E2E_E_OK);
    EXPECT_EQ(stateAfterOk.SMState, E2E_SM_INIT);
    EXPECT_EQ(stateAfterOk.OkCount, 0U);    /* この遷移自体もウィンドウには追加しない */
    EXPECT_EQ(stateAfterOk.ErrorCount, 0U);
}

TEST_F(E2E_Check_Test, Check_OK_InitPromotesToValidOnceWindowFullOfOk)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retNodataToInit = E2E_SMCheck(E2E_P_OK, &config, &state); /* NODATA -> INIT */
    E2E_SMCheckStateType stateNodataToInit = state;

    /* WindowSize(3) 回分 OK を積むまでは INIT に留まる */
    Std_ReturnType retOk1 = E2E_SMCheck(E2E_P_OK, &config, &state);
    E2E_SMCheckStateType stateOk1 = state;
    Std_ReturnType retOk2 = E2E_SMCheck(E2E_P_OK, &config, &state);
    E2E_SMCheckStateType stateOk2 = state;

    /* 3回目でウィンドウが全て OK になり VALID へ昇格 */
    Std_ReturnType retOk3 = E2E_SMCheck(E2E_P_OK, &config, &state);
    E2E_SMCheckStateType stateOk3 = state;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retNodataToInit, E2E_E_OK);
    ASSERT_EQ(stateNodataToInit.SMState, E2E_SM_INIT);

    ASSERT_EQ(retOk1, E2E_E_OK);
    EXPECT_EQ(stateOk1.SMState, E2E_SM_INIT);
    ASSERT_EQ(retOk2, E2E_E_OK);
    EXPECT_EQ(stateOk2.SMState, E2E_SM_INIT);

    ASSERT_EQ(retOk3, E2E_E_OK);
    EXPECT_EQ(stateOk3.SMState, E2E_SM_VALID);
    EXPECT_EQ(stateOk3.OkCount, 3U);
    EXPECT_EQ(stateOk3.ErrorCount, 0U);
}

TEST_F(E2E_Check_Test, Check_OK_InitDemotesDirectlyToInvalidWhenErrorExceedsMax)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret1 = E2E_SMCheck(E2E_P_OK, &config, &state);

    /* MaxErrorStateInit=0 のため、INIT 中に ERROR が1回でも発生すると
     * (OK が十分溜まる前でも) 即座に INVALID へ落ちる。 */
    Std_ReturnType ret2 = E2E_SMCheck(E2E_P_ERROR, &config, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret1, E2E_E_OK); /* NODATA -> INIT */
    ASSERT_EQ(ret2, E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INVALID);
}

TEST_F(E2E_Check_Test, Check_OK_ValidStaysValidWithinErrorLimitThenDemotesWhenExceeded)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* NODATA -> INIT（AddStatus は呼ばれない） */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* window=[OK,NA,NA] */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* window=[OK,OK,NA] */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* window=[OK,OK,OK] -> VALID */
    ASSERT_EQ(state.SMState, E2E_SM_VALID);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* MaxErrorStateValid=1 のため、1件だけの ERROR は許容され VALID を維持する
     * (window=[ERROR,OK,OK] は OkCount=2>=MinOkStateValid(1)、ErrorCount=1<=1)。 */
    Std_ReturnType retError1 = E2E_SMCheck(E2E_P_ERROR, &config, &state);
    E2E_SMCheckStateType stateError1 = state;

    /* 2件目の ERROR で許容量(1)を超え INVALID へ落ちる
     * (window=[ERROR,ERROR,OK] は ErrorCount=2>1)。 */
    Std_ReturnType retError2 = E2E_SMCheck(E2E_P_ERROR, &config, &state);
    E2E_SMCheckStateType stateError2 = state;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retError1, E2E_E_OK);
    EXPECT_EQ(stateError1.SMState, E2E_SM_VALID);

    ASSERT_EQ(retError2, E2E_E_OK);
    EXPECT_EQ(stateError2.SMState, E2E_SM_INVALID);
}

TEST_F(E2E_Check_Test, Check_OK_InvalidRecoversToValidOnceWindowFullOfOk)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* INIT を経由せず、INVALID から直接検証する（E2E_SMAddStatus() は
     * 呼び出し毎にウィンドウ全体を再走査して OkCount/ErrorCount を再計算する
     * ため、事前の Ok/ErrorCount の値そのものは結果に影響しない）。 */
    window[0] = static_cast<uint8_t>(E2E_P_ERROR);
    window[1] = static_cast<uint8_t>(E2E_P_ERROR);
    window[2] = static_cast<uint8_t>(E2E_P_ERROR);
    state.WindowTopIndex = 0U;
    state.SMState        = E2E_SM_INVALID;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* MinOkStateInvalid=3/MaxErrorStateInvalid=0 のため、ウィンドウ全体が
     * OK で埋まるまで INVALID に留まる。 */
    Std_ReturnType retOk1 = E2E_SMCheck(E2E_P_OK, &config, &state);
    E2E_SMCheckStateType stateOk1 = state;
    Std_ReturnType retOk2 = E2E_SMCheck(E2E_P_OK, &config, &state);
    E2E_SMCheckStateType stateOk2 = state;

    /* 3回目でウィンドウが全て OK になり VALID へ復帰 */
    Std_ReturnType retOk3 = E2E_SMCheck(E2E_P_OK, &config, &state);
    E2E_SMCheckStateType stateOk3 = state;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retOk1, E2E_E_OK);
    EXPECT_EQ(stateOk1.SMState, E2E_SM_INVALID);
    ASSERT_EQ(retOk2, E2E_E_OK);
    EXPECT_EQ(stateOk2.SMState, E2E_SM_INVALID);

    ASSERT_EQ(retOk3, E2E_E_OK);
    EXPECT_EQ(stateOk3.SMState, E2E_SM_VALID);
    EXPECT_EQ(stateOk3.OkCount, 3U);
    EXPECT_EQ(stateOk3.ErrorCount, 0U);
}

TEST_F(E2E_Check_Test, Check_OK_ValidDemotesWhenOkCountFallsBelowMinimumEvenWithoutErrors)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* エラーは 1 件も無いが、OK も 1 件も無い（新規データなし）状態の VALID。
     * ErrorCount(0)<=MaxErrorStateValid(1) は満たすが、OkCount(0)<MinOkStateValid(1) のため INVALID へ落ちる。 */
    window[0] = static_cast<uint8_t>(E2E_P_NONEWDATA);
    window[1] = static_cast<uint8_t>(E2E_P_NONEWDATA);
    window[2] = static_cast<uint8_t>(E2E_P_NONEWDATA);
    state.WindowTopIndex = 0U;
    state.SMState        = E2E_SM_VALID;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_SMCheck(E2E_P_NONEWDATA, &config, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INVALID);
    EXPECT_EQ(state.ErrorCount, 0U);   // エラーが理由ではない
    EXPECT_EQ(state.OkCount, 0U);
}

TEST_F(E2E_Check_Test, Check_OK_InvalidStaysInvalidWhenOkCountBelowMinimumEvenWithoutErrors)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* エラーは 1 件も無いが、OK も足りない（1 件だけ）状態の INVALID。
     * ErrorCount(0)<=MaxErrorStateInvalid(0) は満たすが、OkCount(1)<MinOkStateInvalid(3) のため INVALID のまま。 */
    window[0] = static_cast<uint8_t>(E2E_P_NONEWDATA);
    window[1] = static_cast<uint8_t>(E2E_P_NONEWDATA);
    window[2] = static_cast<uint8_t>(E2E_P_NONEWDATA);
    state.WindowTopIndex = 0U;
    state.SMState        = E2E_SM_INVALID;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_SMCheck(E2E_P_OK, &config, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INVALID);
    EXPECT_EQ(state.ErrorCount, 0U);   // エラーが理由ではない
    EXPECT_EQ(state.OkCount, 1U);
}

}  // namespace
