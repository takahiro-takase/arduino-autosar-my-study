/**
 * \file    Dcm_CopyRxData_test.cpp
 * \brief   Dcm_CopyRxData() の単体テスト
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
class Dcm_CopyRxData_Test : public ::testing::Test
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

TEST_F(Dcm_CopyRxData_Test, Dcm_CopyRxData_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();
    uint8 sdu[1] = { 0x22U };
    PduInfoType info = { sdu, 1U };
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Dcm_CopyRxData_Test, Dcm_CopyRxData_NG_NullInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, NULL, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

TEST_F(Dcm_CopyRxData_Test, Dcm_CopyRxData_NG_NotReceiving)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dcm_StartOfReception() を呼ばず、受信中ではない状態にしておく。
    uint8 sdu[1] = { 0x22U };
    PduInfoType info = { sdu, 1U };
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
}

TEST_F(Dcm_CopyRxData_Test, Dcm_CopyRxData_NG_ExceedsAnnouncedLength)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufSize = 0U;
    ASSERT_EQ(Dcm_StartOfReception(0U, NULL, 2U, &bufSize), BUFREQ_OK);
    uint8 sdu[3] = { 0x22U, 0x01U, 0x01U };
    PduInfoType info = { sdu, 3U };  // 通知した長さ（2）より多い

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
}

TEST_F(Dcm_CopyRxData_Test, Dcm_CopyRxData_NG_NullBufferSizePtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[1] = { 0U };
    PduInfoType info = { buf, 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, NULL);   // info は有効、bufferSizePtr だけ NULL

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

TEST_F(Dcm_CopyRxData_Test, Dcm_CopyRxData_NG_NullSduDataPtrWithLength)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 1U };   // コピーするバイト数があるのに、SduDataPtr が NULL
    PduLengthType bufferSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, &bufferSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

TEST_F(Dcm_CopyRxData_Test, Dcm_CopyRxData_OK_ZeroLengthWithNullDataQueriesFreeSpace)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufferSize = 0U;
    ASSERT_EQ(Dcm_StartOfReception(0U, NULL, 2U, &bufferSize), BUFREQ_OK);
    PduInfoType info = { NULL, 0U };   // コピーするバイト数が 0 なら、SduDataPtr は NULL でもよい（空きの問い合わせ）
    bufferSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, &bufferSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_OK);
    EXPECT_NE(bufferSize, 0U);   // 受信バッファの空きを返す
    EXPECT_NE(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

}  // namespace
