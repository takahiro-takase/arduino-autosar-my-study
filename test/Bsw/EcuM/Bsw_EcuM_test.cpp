/**
 * \file    Bsw_EcuM_test.cpp
 * \brief   EcuM.c（src/Bsw/EcuM/EcuM.c）の単体テスト（NG系）
 * \details EcuM.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ModuleId /
 *          ApiId / ErrorId が仕様どおり正しい値になっていることを
 *          `Fake_Det_Hw.h` で検証する。
 *
 *          検証対象はいずれも他モジュールを呼ぶ処理へ到達する前に reject
 *          される分岐のみのため、EcuM_Init() は不要（呼ばない）。実 EcuM.c を
 *          リンクするための未解決シンボルは `Fake_EcuM_Deps.c` が空定義で
 *          埋めている。
 *
 *          EcuM_RequestRUN/ReleaseRUN/CheckWakeup は他のテスト向けに
 *          `--wrap` でスパイへ差し替えてあるため、本ファイルでは
 *          `FakeEcuM_PassThrough` を立てて実 EcuM.c へ素通しさせる。
 *
 *          EcuM には DeInit に相当する API が無く、RUN/POST_RUN 要求ビットは
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          各ケースで `EcuM_Test_ResetInitState()`（`ECUM_UNIT_TEST`
 *          ビルドのみに存在するテスト専用関数）により初期化する。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
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
class Bsw_EcuM_Test : public ::testing::Test
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

// ------------------------------------------------------------
// EcuM_RequestRUN()
// ------------------------------------------------------------

TEST_F(Bsw_EcuM_Test, EcuM_RequestRUN_NG_InvalidUser)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = EcuM_RequestRUN(kInvalidUser);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ExpectDet(ECUM_API_ID_REQUEST_RUN, ECUM_E_INVALID_PAR);
}

TEST_F(Bsw_EcuM_Test, EcuM_RequestRUN_NG_MultipleRequest)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(EcuM_RequestRUN(0U), E_OK);
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = EcuM_RequestRUN(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ExpectDet(ECUM_API_ID_REQUEST_RUN, ECUM_E_MULTIPLE_RUN_REQUESTS);
}

// ------------------------------------------------------------
// EcuM_ReleaseRUN()
// ------------------------------------------------------------

TEST_F(Bsw_EcuM_Test, EcuM_ReleaseRUN_NG_InvalidUser)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = EcuM_ReleaseRUN(kInvalidUser);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ExpectDet(ECUM_API_ID_RELEASE_RUN, ECUM_E_INVALID_PAR);
}

TEST_F(Bsw_EcuM_Test, EcuM_ReleaseRUN_NG_MismatchedRelease)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = EcuM_ReleaseRUN(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ExpectDet(ECUM_API_ID_RELEASE_RUN, ECUM_E_MISMATCHED_RUN_RELEASE);
}

// ------------------------------------------------------------
// EcuM_RequestPOST_RUN()
// ------------------------------------------------------------

TEST_F(Bsw_EcuM_Test, EcuM_RequestPOST_RUN_NG_InvalidUser)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = EcuM_RequestPOST_RUN(kInvalidUser);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ExpectDet(ECUM_API_ID_REQUEST_POST_RUN, ECUM_E_INVALID_PAR);
}

TEST_F(Bsw_EcuM_Test, EcuM_RequestPOST_RUN_NG_MultipleRequest)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(EcuM_RequestPOST_RUN(0U), E_OK);
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = EcuM_RequestPOST_RUN(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ExpectDet(ECUM_API_ID_REQUEST_POST_RUN, ECUM_E_MULTIPLE_RUN_REQUESTS);
}

// ------------------------------------------------------------
// EcuM_ReleasePOST_RUN()
// ------------------------------------------------------------

TEST_F(Bsw_EcuM_Test, EcuM_ReleasePOST_RUN_NG_InvalidUser)
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

TEST_F(Bsw_EcuM_Test, EcuM_ReleasePOST_RUN_NG_MismatchedRelease)
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

// ------------------------------------------------------------
// EcuM_CheckWakeup()
// ------------------------------------------------------------

TEST_F(Bsw_EcuM_Test, EcuM_CheckWakeup_NG_InvalidSource)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckWakeup(0U);

    ExpectDet(ECUM_API_ID_CHECK_WAKEUP, ECUM_E_INVALID_PAR);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // 異常終了せずに戻ること（EXPECT/ASSERT は無い）
}

// ------------------------------------------------------------
// EcuM_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_EcuM_Test, EcuM_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_GetVersionInfo(NULL);

    ExpectDet(ECUM_API_ID_GET_VERSION_INFO, ECUM_E_NULL_POINTER);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // 異常終了せずに戻ること（EXPECT/ASSERT は無い）
}

}  // namespace
