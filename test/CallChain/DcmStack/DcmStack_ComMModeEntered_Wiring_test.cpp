/**
 * \file    DcmStack_ComMModeEntered_Wiring_test.cpp
 * \brief   ComM_BusSM_ModeIndication() が Dcm へチャネルのモードを通知し、応答の送信可否が追従することの、フルコールチェーンテスト。
 *
 * \details SILENT_COM の通知で応答は破棄され（診断要求でチャネルを起こさない設定 Dcm_SetActiveDiagnostic(FALSE) の下で）、
 *          FULL_COM の通知で再開する。Dcm_ComM_*ModeEntered() を直接呼ぶテスト
 *          （DcmStack_ComMModeEntered_Silent_test.cpp）と違い、ComM_BusSM_ModeIndication() 経由の配線を確認する。
 *          要求は WrapDcm_InjectRequest() で Dcm へ直接渡す（応答要求のたびに ComM/CanSM が下位層の状態を変えるため、
 *          CAN の受信は経由しない）。応答の送信可否は、CanTp_Transmit() が呼ばれたかで見る。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "ComM.h"
#include "PduR.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_PduR.h"
#include "Wrap_CanTp.h"
#include "Wrap_ComM.h"
#include "Wrap_Dcm.h"
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

// -----------------------------------------------------------------------
// テスト専用の最小 CanIf/PduR 設定（DcmStack_SID19_SF01_ReadDtcCount_test.cpp
// と同一。ファイル冒頭コメント参照）。
// -----------------------------------------------------------------------

const CanIf_RxPduConfigType kTestCanIfRxPdu = {
    /* CanId */                0x7E0U,
    /* Hrh */                  0U,  /* Can_MainFunction_Read() が構築する Mailbox は常に Hoh=0 */
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  8U,
    /* RxIndicationFct */      PduR_CanIfRxIndication,
    /* ReadRxPduDataEnabled */ 0U
};

const CanIf_TxPduConfigType kTestCanIfTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x7E8U,
    /* Dlc */               8U,
    /* Hth */               0U,
    /* TxConfirmFct */      PduR_CanIfTxConfirmation
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ &kTestCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ &kTestCanIfRxPdu,
    /* RxPduCount */  1U
};

const PduR_RxDestType kTestPduRRxDest = {
    /* Module */    PDUR_MODULE_CANTP,
    /* DestPduId */ CANTP_RX_SDU_ID,
    /* RxIndFct */  CanTp_RxIndication
};

const PduR_RxRoutingPathType kTestPduRRxPath = {
    /* SrcPduId */  0U,  /* = kTestCanIfRxPdu.UpperLayerRxPduId */
    /* Dests */     &kTestPduRRxDest,
    /* DestCount */ 1U
};

const PduR_TxRoutingPathType kTestPduRTxPath = {
    /* SrcPduId */             CANTP_PDUR_TX_SDU_ID,
    /* CanIfTxPduId */         0U,  /* = kTestCanIfTxPdu の登録順インデックス */
    /* ConfDestPduId */        0U,
    /* ConfFct */              CanTp_TxConfirmation,
    /* TransmitOverrideFct */  NULL,
    /* TransmitOverrideId */   0U
};

const PduR_PBConfigType kTestPduRConfig = {
    /* RxPaths */     &kTestPduRRxPath,
    /* RxPathCount */ 1U,
    /* TxPaths */     &kTestPduRTxPath,
    /* TxPathCount */ 1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* SID 0x3E TesterPresent のコールチェーンのテスト用フィクスチャ。CAN の受信（0x7E0）から応答の送信（0x7E8）までを実際に通して確認する。
 * SetUp(): 模擬 Can_Hw と wrap をリセットし、Can_Init() → CanIf_Init(&kTestCanIfConfig)（コントローラを STARTED、PDU を ONLINE にする）→ PduR_Init() → CanSM_Init(NULL)（CanIf_RxIndication() が呼ぶため）→ CanTp_Init(NULL) → Dem_Init(NULL) → Dcm_Init(NULL) の順に初期化する。
 * TearDown(): CanSM_DeInit() と CanIf_DeInit() で未初期化へ戻す。 */
class DcmStack_ComMModeEntered_Wiring_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeCanHw_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        WrapPduR_Reset();
        WrapCanTp_Reset();
        WrapComM_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0000U;
        canConfig.filter.mask     = 0x0000U;  // 全ID受理（Can_PBCfg.c の本番設定と同じ）
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
        /* CanIf_Init() 直後は CanIf_ControllerMode[] が CAN_CS_STOPPED に
         * 巻き戻るため、CanIf_SetControllerMode() 経由で明示的に起動する
         * （ComStack_Signal_Rx_test.cpp と同じ理由）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM 経由で FULL_COM を確立しないため、CanIf_Init()
         * 直後の既定値 CANIF_OFFLINE のままだと応答側の CanIf_Transmit() が
         * 常に E_NOT_OK になってしまう（ComStack_Signal_Tx_test.cpp
         * と同じ理由）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        /* 以前は CanIf_RxIndication() が CanSM_RxIndication() を無条件に呼んでおり（2026-10 に廃止、現在は呼ばない。共有 static の状態を揃えるため CanSM_Init は残している）、
         * CanSM 未初期化のままだと DET_E_UNINIT が毎回報告される
         * （ComStack_Signal_Rx_test.cpp と同じ理由。CanSM 自体の状態機械は
         * 本テストの対象外）。 */
        CanSM_Init(NULL);
        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);
        ComM_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(DcmStack_ComMModeEntered_Wiring_Test,
       OK_ComMBusSmModeIndicationDrivesDcmTransmission)
{
    /* ------------------------- */
    /* ---- 準備 (Arrange 1) --- */
    /* ------------------------- */
    // 診断要求が ComM_DCM_ActiveDiagnostic() でチャネルを起こさないようにしておく
    // （起こすと FULL_COM に戻って応答が出る。DcmStack_ActiveDiagnosticWake_test.cpp 参照）。
    ASSERT_EQ(Dcm_SetActiveDiagnostic(FALSE), E_OK);
    uint8 sdu[2] = { DCM_SID_TESTER_PRESENT, 0x00U };
    ComM_BusSM_ModeIndication(COMM_CHANNEL_0, COMM_SILENT_COMMUNICATION);

    /* ------------------------- */
    /* ---- 実行 (Act 1) ------- */
    /* ------------------------- */
    (void)WrapDcm_InjectRequest(sdu, (uint8)sizeof(sdu));

    /* ------------------------- */
    /* ---- 評価 (Assert 1) ---- */
    /* ------------------------- */
    EXPECT_EQ(CallCount_CanTp_Transmit, 0U);  // SILENT_COM の間は応答が破棄される

    /* ------------------------- */
    /* ---- 準備 (Arrange 2) --- */
    /* ------------------------- */
    ComM_BusSM_ModeIndication(COMM_CHANNEL_0, COMM_FULL_COMMUNICATION);

    /* ------------------------- */
    /* ---- 実行 (Act 2) ------- */
    /* ------------------------- */
    (void)WrapDcm_InjectRequest(sdu, (uint8)sizeof(sdu));

    /* ------------------------- */
    /* ---- 評価 (Assert 2) ---- */
    /* ------------------------- */
    EXPECT_EQ(CallCount_CanTp_Transmit, 1U);  // FULL_COM で送信が再開する
}

}  // namespace
