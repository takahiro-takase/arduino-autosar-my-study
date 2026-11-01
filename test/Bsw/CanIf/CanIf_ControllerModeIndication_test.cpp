/**
 * \file    CanIf_ControllerModeIndication_test.cpp
 * \brief   CanIf_ControllerModeIndication() の単体テスト
 * \details CanSM への転送（[SWS_CANIF_00711]）、範囲外の ControllerId（[SWS_CANIF_00700]）、未初期化（[SWS_CANIF_00702]）を確認する。
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

/* CanIf_ControllerModeIndication() の単体テスト用フィクスチャ。
 * SetUp(): CanSM_Init(NULL) を先に呼び（CanSM は共有 static の状態を持つため、状態を揃える）、続けて CanIf_Init(&kTestCanIfConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanIf_DeInit() と CanSM_DeInit() で未初期化へ戻す。 */
class CanIf_ControllerModeIndication_Test : public ::testing::Test
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

TEST_F(CanIf_ControllerModeIndication_Test, CanIf_ControllerModeIndication_OK_ForwardsToCanSM)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // CanSM が要求するコントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanIf_ControllerModeIndication(0U, CAN_CS_STOPPED);   // CanSM は NO_COM（起床の検出）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // CanSM が起床を受け付け、リスン専用への移行（停止）をコントローラへ要求した
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore + 1U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanIf_ControllerModeIndication_Test, CanIf_ControllerModeIndication_NG_InvalidControllerId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // CanSM が要求するコントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanIf_ControllerModeIndication(CANIF_CONTROLLER_MAX, CAN_CS_STOPPED);   // 範囲外

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);          // [SWS_CANIF_00700]
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);                                  // 報告は 1 回だけ
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);           // CanSM へは通知しない
}

TEST_F(CanIf_ControllerModeIndication_Test, CanIf_ControllerModeIndication_NG_UninitializedDoesNotNotifyAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // CanSM が要求するコントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;
    CanIf_DeInit();   // 未初期化にする

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanIf_ControllerModeIndication(0U, CAN_CS_STOPPED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);           // 通知しない（[SWS_CANIF_00702]）
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // 未初期化は DET へ報告する（[SWS_CANIF_00901]）
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_CONTROLLER_MODE_INDICATION);
}

}  // namespace
