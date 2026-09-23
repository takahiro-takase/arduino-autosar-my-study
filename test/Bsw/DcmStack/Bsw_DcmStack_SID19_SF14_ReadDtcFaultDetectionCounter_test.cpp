/**
 * \file    Bsw_DcmStack_SID19_SF14_ReadDtcFaultDetectionCounter_test.cpp
 * \brief   UDS SID 0x19/0x14 reportDTCFaultDetectionCounter の、物理層
 *          （Can_Hw フェイク）を起点・終点とするフルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp で確立した
 *          チェーンの型（Can_ConfigType/CanIf_ConfigType/PduR_PBConfigType の
 *          テスト専用ローカル設定、Init() の呼び出し順序、Can_Hw への
 *          べた書きリクエスト）をそのまま適用する。リクエスト（2バイト）・
 *          応答（未着手時2バイト／prefailed1件時6バイト）とも Single Frame
 *          に収まるため、CanTp のマルチフレーム分割は関与しない
 *          （SID19_SF0A_ReadDtcSupported_test.cpp とは異なる）。
 *
 *          0x14 の応答ヘッダは [0x59, subFunc] の2バイトのみ（0x01/0x0A の
 *          ような DTCStatusAvailabilityMask バイトを含まない）点、DTC
 *          レコードが (DTC_H, DTC_M, DTC_L, FaultDetectionCounter) の
 *          4バイト構成である点に注意（Dcm.c の
 *          Dcm_HandleReadDtcFaultDetectionCounter() 参照）。
 *
 *          期待値は Bsw_Dcm_ReadDtcInfo_test.cpp の
 *          ReadDtcFaultDetectionCounter_OK_* と同じもの（境界での
 *          Dcm→CanTp キャプチャに加え、Can_Hw への物理送信まで検証する点が
 *          新規）。
 */
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
#include "Dem_Cfg.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_PduR.h"
#include "Wrap_CanTp.h"
}

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

class Bsw_DcmStack_SID19_SF14_ReadDtcFaultDetectionCounter_Test : public ::testing::Test
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
        /* CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため、
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

// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF14_ReadDtcFaultDetectionCounter_Test,
       ReadDtcFdc_OK_NoPrefailedEventProducesHeaderOnlyResponseOnCanHw)
{
    /* 準備 (Arrange): [0x19, 0x14] を 0x7E0 の受信バッファへセットする
     * （SF: 02 19 14）。Dem_Init() 直後は prefailed イベントが無い。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_FDC;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): DTC 列挙部分の無い [0x59, 0x14] のみが Can_Hw まで
     * 到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x59U);  // 肯定応答 SID (0x19+0x40)
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTC_SUBFUNC_REPORT_FDC);

    EXPECT_EQ(CallCount_CanTp_RxIndication, 1U);
    EXPECT_EQ(CallCount_CanTp_Transmit, 1U);
}

// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF14_ReadDtcFaultDetectionCounter_Test,
       ReadDtcFdc_OK_OnePrefailedEventIsReflectedInResponseOnCanHw)
{
    /* 準備 (Arrange): DEM_EVENT_ENGINE_OVERHEAT（limit=2）を1回だけ FAILED
     * 報告する（生カウンタ1は確定閾値2未満のため prefailed）。 */
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_FDC;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): [0x59, 0x14, DTC_H=0x00, DTC_M=0x01, DTC_L=0x01,
     * FDC=63]（生カウンタ1を limit=2 で線形写像: (1*127)/2=63）が Can_Hw
     * まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x06U);  // SF PCI（UDSペイロード長=6）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x59U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTC_SUBFUNC_REPORT_FDC);
    EXPECT_EQ(FakeCanHw_LastSendData[3], 0x00U);  // DTC_H
    EXPECT_EQ(FakeCanHw_LastSendData[4], 0x01U);  // DTC_M
    EXPECT_EQ(FakeCanHw_LastSendData[5], 0x01U);  // DTC_L
    EXPECT_EQ(FakeCanHw_LastSendData[6], 63U);    // FaultDetectionCounter
}

// ------------------------------------------------------------
// NG: 余分な1バイト（[0x19, 0x14, 0x00]、3バイト。udsLen!=2 のため）は
// incorrectMessageLength (NRC 0x13) になり、それも同じ経路で Can_Hw まで
// 届くことを確認する（OK と同じファイルに同居させる方針、
// [[feedback_test_file_one_scenario_per_file]]）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF14_ReadDtcFaultDetectionCounter_Test,
       ReadDtcFdc_NG_ExtraByteProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): 余分な1バイトを付けた [0x19, 0x14, 0x00] を 0x7E0 の
     * 受信バッファへセットする（SF: 03 19 14 00）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_FDC;
    FakeCanHw_RxData[3] = 0x00U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x19, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_READ_DTC_INFO);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

}  // namespace
