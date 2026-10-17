/**
 * \file    Dcm_StartOfReception_test.cpp
 * \brief   Dcm_StartOfReception() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Dcm_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Dcm_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cbk.h"
#include "Dcm_Cfg.h"
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

/* Dcm（UDS の個別サービス以外の API）の単体テスト用フィクスチャ。
 * SetUp(): Dcm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dcm_StartOfReception_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Dcm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dcm_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dcm_StartOfReception_Test, Dcm_StartOfReception_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_StartOfReception(0U, NULL, 1U, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Dcm_StartOfReception_Test, Dcm_StartOfReception_NG_NullBufferSizePtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_StartOfReception(0U, NULL, 1U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

TEST_F(Dcm_StartOfReception_Test, Dcm_StartOfReception_NG_Overflow)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_StartOfReception(0U, NULL, (PduLengthType)(DCM_RX_BUF_SIZE + 1U), &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_OVFL);
}

TEST_F(Dcm_StartOfReception_Test, Dcm_StartOfReception_OK_ZeroLengthQueriesBufferSize)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_StartOfReception(0U, NULL, 0U, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_OK);
    EXPECT_EQ(bufSize, (PduLengthType)DCM_RX_BUF_SIZE);
}

}  // namespace
