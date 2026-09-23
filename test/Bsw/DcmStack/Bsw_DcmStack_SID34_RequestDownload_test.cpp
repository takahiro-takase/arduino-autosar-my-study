/**
 * \file    Bsw_DcmStack_SID34_RequestDownload_test.cpp
 * \brief   UDS SID 0x34 RequestDownload の、物理層（Can_Hw フェイク）を
 *          起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト・応答とも Single Frame に
 *          収まるため、CanTp のマルチフレーム分割は関与しない。
 *
 *          0x34 は extendedSession かつ SecurityAccess Level1 の
 *          アンロックが必須のため、`UnlockSecurityAccessLevel1()`
 *          （Bsw_DcmStack_SID14_ClearDtc_test.cpp と同じ手順）で事前に
 *          アンロックする。
 *
 *          0x34/0x36/0x37（RequestDownload/TransferData/
 *          RequestTransferExit）は IDLE/DOWNLOADING の2状態を持つ一連の
 *          ソフトウェア転送シーケンスだが、命名規則（1 SID = 1 ファイル）に
 *          従い SID ごとに分割する。0x36/0x37 は
 *          Bsw_DcmStack_SID36_TransferData_test.cpp /
 *          Bsw_DcmStack_SID37_RequestTransferExit_test.cpp を参照。
 *
 *          本ファイルは本プロジェクトでこの SID を対象とする初めての
 *          テストファイル（従来は単体・チェーンいずれのテストも存在せず、
 *          移植元は無い。ゼロから新設）。
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

class Bsw_DcmStack_SID34_RequestDownload_Test : public ::testing::Test
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
        UnlockSecurityAccessLevel1();  // 0x34 は extendedSession かつ Level1 アンロック必須

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /* [0x10,0x03]→[0x27,0x01](requestSeed)→[0x27,0x02](sendKey) を実際に
     * Can_Hw から受信させ、extendedSession への遷移と SecurityAccess Level1
     * のアンロックを行う（Bsw_DcmStack_SID14_ClearDtc_test.cpp と同じ
     * ヘルパー）。 */
    void UnlockSecurityAccessLevel1()
    {
        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 2U;
        FakeCanHw_RxData[1] = DCM_SID_SESSION_CTRL;
        FakeCanHw_RxData[2] = DCM_SESSION_EXTENDED;
        for (uint8 i = 3U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;
        Can_MainFunction_Read();
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x50U);  // 正応答確認

        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 2U;
        FakeCanHw_RxData[1] = DCM_SID_SECURITY_ACCESS;
        FakeCanHw_RxData[2] = DCM_SEC_SUBFUNC_REQUEST_SEED;
        for (uint8 i = 3U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;
        Can_MainFunction_Read();
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x67U);
        uint16 seed = (uint16)(((uint16)FakeCanHw_LastSendData[3] << 8U) | (uint16)FakeCanHw_LastSendData[4]);
        uint16 key  = (uint16)(seed ^ DCM_SECURITY_KEY_MASK);

        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 4U;
        FakeCanHw_RxData[1] = DCM_SID_SECURITY_ACCESS;
        FakeCanHw_RxData[2] = DCM_SEC_SUBFUNC_SEND_KEY;
        FakeCanHw_RxData[3] = (uint8)(key >> 8U);
        FakeCanHw_RxData[4] = (uint8)(key & 0xFFU);
        for (uint8 i = 5U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;
        Can_MainFunction_Read();
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x67U) << "security unlock must succeed as a test precondition";
    }

    /* [0x34, 0x00, 0x11, addr(1byte), size(1byte)] を Can_Hw から受信させ、
     * チェーン全体を駆動する（Act）。addressAndLengthFormatIdentifier=0x11
     * は addrBytes=1, sizeBytes=1（下位/上位 nibble）を表す。 */
    void SendRequestDownload(uint8 addr, uint8 size)
    {
        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 5U;
        FakeCanHw_RxData[1] = DCM_SID_REQUEST_DOWNLOAD;
        FakeCanHw_RxData[2] = DCM_TRANSFER_DATA_FORMAT_RAW;
        FakeCanHw_RxData[3] = 0x11U;  // addrBytes=1, sizeBytes=1
        FakeCanHw_RxData[4] = addr;
        FakeCanHw_RxData[5] = size;
        for (uint8 i = 6U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;

        Can_MainFunction_Read();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// OK: 妥当な memorySize での RequestDownload は正応答
// [0x74, 0x20, maxNumberOfBlockLengthH, maxNumberOfBlockLengthL] を返す。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID34_RequestDownload_Test,
       RequestDownload_OK_ValidSizeProducesMaxBlockLengthResponseOnCanHw)
{
    /* 実行 (Act): addr=0x10, size=0x40（64バイト、DCM_TRANSFER_MAX_SIZE 以内） */
    SendRequestDownload(0x10U, 0x40U);

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x04U);  // SF PCI（UDSペイロード長=4）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x74U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x20U);  // lengthFormatIdentifier
    EXPECT_EQ(FakeCanHw_LastSendData[3], (uint8)(DCM_TRANSFER_MAX_BLOCK_LENGTH >> 8U));
    EXPECT_EQ(FakeCanHw_LastSendData[4], (uint8)(DCM_TRANSFER_MAX_BLOCK_LENGTH & 0xFFU));
}

// ------------------------------------------------------------
// NG: memorySize=0 は NRC 0x31 requestOutOfRange になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID34_RequestDownload_Test,
       RequestDownload_NG_ZeroSizeReturnsRequestOutOfRangeOnCanHw)
{
    SendRequestDownload(0x10U, 0x00U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_REQUEST_DOWNLOAD);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// ------------------------------------------------------------
// NG: DOWNLOADING 中（前回の RequestDownload 未完了）の再要求は
// NRC 0x22 conditionsNotCorrect になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID34_RequestDownload_Test,
       RequestDownload_NG_AlreadyDownloadingReturnsConditionsNotCorrectOnCanHw)
{
    /* 準備 (Arrange): 一度正常に RequestDownload を受理させておく。 */
    SendRequestDownload(0x10U, 0x40U);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x74U);  // 前提確認

    /* 実行 (Act): 再度 RequestDownload を送る。 */
    SendRequestDownload(0x20U, 0x10U);

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_REQUEST_DOWNLOAD);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_CONDITIONS_NOT_CORRECT);
}

// ------------------------------------------------------------
// NG: dataFormatIdentifier が RAW(0x00) 以外は NRC 0x31 requestOutOfRange
// になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID34_RequestDownload_Test,
       RequestDownload_NG_NonRawDataFormatReturnsRequestOutOfRangeOnCanHw)
{
    /* 準備 (Arrange): dataFormatIdentifier=0x01（非RAW）を 0x7E0 の受信
     * バッファへセットする（SF: 05 34 01 11 10 40）。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 5U;
    FakeCanHw_RxData[1] = DCM_SID_REQUEST_DOWNLOAD;
    FakeCanHw_RxData[2] = 0x01U;  // 非RAW
    FakeCanHw_RxData[3] = 0x11U;
    FakeCanHw_RxData[4] = 0x10U;
    FakeCanHw_RxData[5] = 0x40U;
    for (uint8 i = 6U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_REQUEST_DOWNLOAD);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

}  // namespace
