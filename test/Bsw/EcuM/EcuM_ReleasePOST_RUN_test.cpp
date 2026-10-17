/**
 * \file    EcuM_ReleasePOST_RUN_test.cpp
 * \brief   EcuM_ReleasePOST_RUN() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 EcuM_test.cpp）。
 *
 *          分割前の共通の説明は、旧 EcuM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "EcuM.h"
#include "EcuM_Cfg.h"
#include "Fake_Bsw_EcuM.h"
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

/* EcuM の単体テスト用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして（実体の EcuM を呼ぶ）EcuM を未初期化へ戻し、DET の記録を初期化する。ExpectDet() は、DET 報告が 1 件で、API ID とエラー ID が一致することを確認する補助関数。
 * TearDown(): 模擬 EcuM の状態も含めて元に戻す。 */
class EcuM_ReleasePOST_RUN_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeEcuM_Reset();
        FakeEcuM_PassThrough = 1U;
        EcuM_Test_ResetInitState();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        EcuM_Test_ResetInitState();
        FakeEcuM_Reset();  // PassThrough も FALSE へ戻る
    }

    static void ExpectDet(uint8 apiId, uint8 errorId)
    {
        EXPECT_EQ(FakeDetHw_ReportCount, 1U);
        EXPECT_EQ(FakeDetHw_LastModuleId, ECUM_MODULE_ID);
        EXPECT_EQ(FakeDetHw_LastApiId, apiId);
        EXPECT_EQ(FakeDetHw_LastErrorId, errorId);
    }

    static const EcuM_UserType kInvalidUser = ECUM_USER_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(EcuM_ReleasePOST_RUN_Test, EcuM_ReleasePOST_RUN_NG_InvalidUser)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = EcuM_ReleasePOST_RUN(kInvalidUser);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ExpectDet(ECUM_API_ID_RELEASE_POST_RUN, ECUM_E_INVALID_PAR);
}

TEST_F(EcuM_ReleasePOST_RUN_Test, EcuM_ReleasePOST_RUN_NG_MismatchedRelease)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = EcuM_ReleasePOST_RUN(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ExpectDet(ECUM_API_ID_RELEASE_POST_RUN, ECUM_E_MISMATCHED_RUN_RELEASE);
}

TEST_F(EcuM_ReleasePOST_RUN_Test, EcuM_ReleasePOST_RUN_OK_OutsidePostRunKeepsState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);
    ASSERT_EQ(EcuM_RequestPOST_RUN(ECUM_USER_COMM), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = EcuM_ReleasePOST_RUN(ECUM_USER_COMM);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(EcuM_GetState(), ECUM_STATE_RUN);   // POST_RUN ではないので、状態は変わらない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
