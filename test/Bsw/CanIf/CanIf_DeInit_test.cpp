/**
 * \file    CanIf_DeInit_test.cpp
 * \brief   CanIf_DeInit() の単体テスト
 * \details 初期化済みからの DeInit（未初期化へ戻ること）と、未初期化での DeInit が CANIF_E_UNINIT を報告すること
 *          （[SWS_CANIF_00901]）を確認する。
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

/* CanIf（上記 2 グループ以外の API）の単体テスト用フィクスチャ。
 * SetUp(): 以前は CanIf_RxIndication() が CanSM_RxIndication() を無条件に呼んでおり（2026-10 に廃止。共有 static の状態を揃えるため CanSM_Init は残している） CanSM_Init(NULL) を先に呼び、続けて CanIf_Init(&kTestCanIfConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanIf_DeInit() と CanSM_DeInit() で未初期化へ戻す。 */
class CanIf_DeInit_Test : public ::testing::Test
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

TEST_F(CanIf_DeInit_Test, CanIf_DeInit_OK_ReturnsToUninitializedWithoutDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanIf_DeInit();
    const Std_ReturnType ret = CanIf_Transmit(0U, NULL);   // 未初期化かどうかの確認（NULL でも、未初期化の判定が先）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // DeInit() 自体は報告せず、その後の API が CANIF_E_UNINIT を報告する
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_TRANSMIT);
}

TEST_F(CanIf_DeInit_Test, CanIf_DeInit_NG_UninitializedReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanIf_DeInit();   // 初期化済みから未初期化へ戻す（正常）
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanIf_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // 未初期化は DET へ報告する（[SWS_CANIF_00901]）
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_DEINIT);
}

}  // namespace
