/**
 * \file    E2EXf_E2EHealthStatus_test.cpp
 * \brief   E2EXf_E2EHealthStatus() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 E2EXf_test.cpp）。
 *
 *          分割前の共通の説明は、旧 E2EXf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "E2EXf.h"
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

/* E2EXf（E2E Transformer）の単体テスト用フィクスチャ。
 * SetUp(): E2EXf_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): E2EXf_DeInit() で未初期化へ戻す。 */
class E2EXf_E2EHealthStatus_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        E2EXf_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        E2EXf_DeInit();  // 既に未初期化でも Det 報告されるだけで無害
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(E2EXf_E2EHealthStatus_Test, E2EXf_E2EHealthStatus_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2EXf_DeInit();
    uint8  buf[5];
    uint32 bufLen = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_E2EHealthStatus(buf, &bufLen, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_UNINIT);
}

TEST_F(E2EXf_E2EHealthStatus_Test, E2EXf_E2EHealthStatus_NG_NullBufferLengthPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[5];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_E2EHealthStatus(buf, NULL, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(E2EXf_E2EHealthStatus_Test, E2EXf_E2EHealthStatus_NG_NullBufferPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 bufLen = 5U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_E2EHealthStatus(NULL, &bufLen, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

}  // namespace
