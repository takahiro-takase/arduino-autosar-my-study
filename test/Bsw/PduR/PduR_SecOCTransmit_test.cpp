/**
 * \file    PduR_SecOCTransmit_test.cpp
 * \brief   PduR_SecOCTransmit() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 PduR_test.cpp）。
 *
 *          分割前の共通の説明は、旧 PduR_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "PduR.h"
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

const PduR_TxRoutingPathType kTestTxPath = {
    /* SrcPduId */             10U,
    /* CanIfTxPduId */         0U,
    /* ConfDestPduId */        20U,
    /* ConfFct */              NULL,
    /* TransmitOverrideFct */  NULL
};

const PduR_RxDestType kTestRxDest = {
    /* Module */    PDUR_MODULE_COM,
    /* DestPduId */ 30U,
    /* RxIndFct */  NULL
};

const PduR_RxRoutingPathType kTestRxPath = {
    /* SrcPduId */  40U,
    /* Dests */     &kTestRxDest,
    /* DestCount */ 1U
};

const PduR_PBConfigType kTestConfig = {
    &kTestRxPath, 1U,
    &kTestTxPath, 1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* PduR（上記以外の API）の単体テスト用フィクスチャ。
 * SetUp(): PduR_Init(&kTestConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
class PduR_SecOCTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        PduR_Init(&kTestConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    // PduR に DeInit() は無いため TearDown は書かない。未初期化状態が必要な
    // テストは各自 PduR_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭
    // コメント参照）。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(PduR_SecOCTransmit_Test, PduR_SecOCTransmit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_Test_ResetInitState();
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = PduR_SecOCTransmit(10U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
}

TEST_F(PduR_SecOCTransmit_Test, PduR_SecOCTransmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = PduR_SecOCTransmit(10U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
}

TEST_F(PduR_SecOCTransmit_Test, PduR_SecOCTransmit_NG_NoMatchingRoute)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = PduR_SecOCTransmit(99U, &info);  // 99 は kTestTxPath.SrcPduId と不一致

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
}

}  // namespace
