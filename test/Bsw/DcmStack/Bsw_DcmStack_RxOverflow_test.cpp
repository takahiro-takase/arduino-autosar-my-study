/**
 * \file    Bsw_DcmStack_RxOverflow_test.cpp
 * \brief   受信する N-SDU が Dcm の受信バッファより長いとき、Flow Control (Overflow) を返すことの、フルコールチェーンテスト。
 *
 * \details CanTp は組み立てバッファを持たず、FF を受信すると Dcm_StartOfReception() で受け入れを確認する。Dcm が
 *          受信バッファ（DCM_RX_BUF_SIZE）より長い N-SDU を BUFREQ_E_OVFL で拒否すると、CanTp は FC (Overflow) を
 *          返し、データの受け渡し（Dcm_CopyRxData）も受信完了の通知（Dcm_TpRxIndication）も行わない。
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
// テスト専用の最小 CanIf/PduR 設定（Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp
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
class Bsw_DcmStack_RxOverflow_Test : public ::testing::Test
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
        WrapDcm_Reset();
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
         * （Bsw_ComStack_Signal_Rx_test.cpp と同じ理由）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM 経由で FULL_COM を確立しないため、CanIf_Init()
         * 直後の既定値 CANIF_OFFLINE のままだと応答側の CanIf_Transmit() が
         * 常に E_NOT_OK になってしまう（Bsw_ComStack_Signal_Tx_test.cpp
         * と同じ理由）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        /* 以前は CanIf_RxIndication() が CanSM_RxIndication() を無条件に呼んでおり（2026-10 に廃止、現在は呼ばない。共有 static の状態を揃えるため CanSM_Init は残している）、
         * CanSM 未初期化のままだと DET_E_UNINIT が毎回報告される
         * （Bsw_ComStack_Signal_Rx_test.cpp と同じ理由。CanSM 自体の状態機械は
         * 本テストの対象外）。 */
        CanSM_Init(NULL);
        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanSM_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_DcmStack_RxOverflow_Test,
       OK_FirstFrameLongerThanDcmRxBufferReturnsFlowControlOverflow)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // FF: 0x7E0 へ、全体の長さが DCM_RX_BUF_SIZE + 1 の N-SDU を、FF（先頭 6 バイト）で通知する。
    const uint16 totalLen = (uint16)(DCM_RX_BUF_SIZE + 1U);
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = (uint8)(0x10U | (uint8)((totalLen >> 8U) & 0x0FU));
    FakeCanHw_RxData[1] = (uint8)(totalLen & 0xFFU);
    for (uint8 i = 2U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0x11U;
    FakeCanHw_RxPendingCount = 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // Dcm は受信を、バッファ不足（BUFREQ_E_OVFL）として拒否し、CanTp は Flow Control (Overflow) を返す。
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 1U);
    EXPECT_EQ(LastTpSduLength_Dcm_StartOfReception, totalLen);
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x32U);  // FC PCI: 0x3 | FS=2 (Overflow)
    // 受け入れていないので、データの受け渡しも、受信完了の通知も無い。
    EXPECT_EQ(CallCount_Dcm_CopyRxData, 0U);
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 0U);
}

}  // namespace
