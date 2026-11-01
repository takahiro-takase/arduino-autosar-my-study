/**
 * \file    CanIf_CancelTransmit_test.cpp
 * \brief   CanIf_CancelTransmit() の単体テスト
 * \details 仕様上のダミー関数（常に E_NOT_OK）と、TxPduId の検査（[SWS_CANIF_00652]）を確認する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "CanSM.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
#include "Wrap_ComM.h"
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

const CanIf_TxPduConfigType kTestTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x100U,
    /* Dlc */               8U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_RxPduConfigType kTestRxPdu = {
    /* CanId */                0x200U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  1U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 0U  // CanIf_ReadRxPduData_NG_NotOptedIn がこの既定値に依存する
};

const CanIf_ConfigType kTestCanIfConfig = {
    &kTestTxPdu, 1U,
    &kTestRxPdu, 1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanIf_CancelTransmit() の単体テスト用フィクスチャ。
 * SetUp(): CanSM_Init(NULL) を先に呼び（CanSM は共有 static の状態を持つため、状態を揃える）、続けて CanIf_Init(&kTestCanIfConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanIf_DeInit() と CanSM_DeInit() で未初期化へ戻す。 */
class CanIf_CancelTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapCanIf_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        CanSM_Init(NULL);
        CanIf_Init(&kTestCanIfConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanIf_DeInit();
        CanSM_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanIf_CancelTransmit_Test, CanIf_CancelTransmit_OK_DummyReturnsNotOk)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み。TxPduCount=1）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_CancelTransmit(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);                  // 仕様どおりのダミー動作（取り消しを拒否する）
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);      // 正しい TxPduId では、DET へ報告しない
}

TEST_F(CanIf_CancelTransmit_Test, CanIf_CancelTransmit_NG_InvalidTxPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み。TxPduCount=1）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_CancelTransmit(1U);   // 範囲外

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_TXPDUID);   // [SWS_CANIF_00652]
}

TEST_F(CanIf_CancelTransmit_Test, CanIf_CancelTransmit_NG_UninitializedReturnsError)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanIf_DeInit();   // 未初期化にする

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_CancelTransmit(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // 未初期化は DET へ報告する（[SWS_CANIF_00901]）
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_CANCEL_TRANSMIT);
}

}  // namespace
