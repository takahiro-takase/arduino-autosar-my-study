/**
 * \file    ComM_GetRequestedComMode_test.cpp
 * \brief   ComM_GetRequestedComMode() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 ComM_test.cpp）。
 *
 *          分割前の共通の説明は、旧 ComM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
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

/**
 * \details AUTOSAR SWS_ComM_00079 準拠のシグネチャで新設。集約前のユーザ単位
 *          の要求値（`ComM_UserRequest[User]`）をそのまま返す薄いgetterで
 *          あることを検証する。異常系と初期値のみ検証し、
 *          「ComM_RequestComMode() で設定した値を反映する」正常系は
 *          ComM_RequestComMode() 自体のカスケード（CanSM/CanIf/Can）が絡む
 *          static 共有状態のハザードを避けるためあえて省略する（詳細は末尾の
 *          コメント参照）。
 */

/* ComM_GetRequestedComMode() の単体テスト用フィクスチャ。
 * SetUp(): ComM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のユーザーを表す kInvalidUser も用意する。
 * TearDown(): ComM_DeInit() で未初期化へ戻す。 */
class ComM_GetRequestedComModeX_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        ComM_DeInit();
    }

    static const ComM_UserHandleType kInvalidUser = COMM_USER_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComM_GetRequestedComModeX_Test, ComM_GetRequestedComMode_NG_UninitializedReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();
    FakeDetHw_Reset();

    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(ComM_GetRequestedComModeX_Test, ComM_GetRequestedComMode_NG_InvalidUserReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetRequestedComMode(kInvalidUser, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(ComM_GetRequestedComModeX_Test, ComM_GetRequestedComMode_NG_NullPointerReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_PARAM_POINTER);
}

TEST_F(ComM_GetRequestedComModeX_Test, ComM_GetRequestedComMode_OK_ReturnsNoComRightAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
