/**
 * \file    Bsw_DcmStack_SID37_RequestTransferExit_test.cpp
 * \brief   UDS SID 0x37 RequestTransferExit の、物理層（Can_Hw フェイク）を
 *          起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト・応答とも Single Frame に
 *          収まるため、CanTp のマルチフレーム分割は関与しない。
 *
 *          `StartDownload()`（Bsw_DcmStack_SID36_TransferData_test.cpp と
 *          同じ手順）で DOWNLOADING 状態を確立し、`SendTransferData()`
 *          （同ファイル参照）で必要な累計バイト数を送ってから
 *          RequestTransferExit を検証する。
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

class Bsw_DcmStack_SID37_RequestTransferExit_Test : public ::testing::Test
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
        StartDownload(0x0AU);  // 0x37 は DOWNLOADING 状態が前提（10バイト転送予定）

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /* [0x10,0x03]→[0x27,0x01](requestSeed)→[0x27,0x02](sendKey)→
     * [0x34, 0x00,0x11, addr,size] を実際に Can_Hw から受信させ、
     * DOWNLOADING 状態を確立する
     * （Bsw_DcmStack_SID36_TransferData_test.cpp と同じ手順）。 */
    void StartDownload(uint8 size)
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
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x50U);

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

        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 5U;
        FakeCanHw_RxData[1] = DCM_SID_REQUEST_DOWNLOAD;
        FakeCanHw_RxData[2] = DCM_TRANSFER_DATA_FORMAT_RAW;
        FakeCanHw_RxData[3] = 0x11U;  // addrBytes=1, sizeBytes=1
        FakeCanHw_RxData[4] = 0x10U;  // addr
        FakeCanHw_RxData[5] = size;
        for (uint8 i = 6U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;
        Can_MainFunction_Read();
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x74U) << "RequestDownload must succeed as a test precondition";
    }

    /* [0x36, blockSequenceCounter, data...] を Can_Hw から受信させ、
     * チェーン全体を駆動する（Bsw_DcmStack_SID36_TransferData_test.cpp と
     * 同じヘルパー）。 */
    void SendTransferData(uint8 blockSequenceCounter, const uint8* data, uint8 dataLen)
    {
        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = (uint8)(2U + dataLen);
        FakeCanHw_RxData[1] = DCM_SID_TRANSFER_DATA;
        FakeCanHw_RxData[2] = blockSequenceCounter;
        for (uint8 i = 0U; i < dataLen; i++)
            FakeCanHw_RxData[3U + i] = data[i];
        for (uint8 i = (uint8)(3U + dataLen); i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;

        Can_MainFunction_Read();
    }

    /* [0x37] を Can_Hw から受信させ、チェーン全体を駆動する（Act）。 */
    void SendRequestTransferExit()
    {
        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 1U;
        FakeCanHw_RxData[1] = DCM_SID_REQUEST_TRANSFER_EXIT;
        for (uint8 i = 2U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;

        Can_MainFunction_Read();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// OK: 宣言どおりの累計バイト数（10バイト）を送り切った後の
// RequestTransferExit は正応答 [0x77, checksum] を返す。checksum は
// 送信した全バイトの XOR。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID37_RequestTransferExit_Test,
       RequestTransferExit_OK_CompletedTransferProducesChecksumResponseOnCanHw)
{
    /* 準備 (Arrange): SetUp() の StartDownload(0x0A) は10バイト宣言のため、
     * 5バイト×2回で送り切る。 */
    const uint8 kBlock1[5] = { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U };
    const uint8 kBlock2[5] = { 0x06U, 0x07U, 0x08U, 0x09U, 0x0AU };
    SendTransferData(0x01U, kBlock1, 5U);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x76U);  // 前提確認
    SendTransferData(0x02U, kBlock2, 5U);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x76U);  // 前提確認

    /* 実行 (Act) */
    SendRequestTransferExit();

    /* 評価 (Assert): checksum は 0x01^0x02^...^0x0A = 0x00
     * （0x01^0x02=0x03, ^0x03=0x00, ^0x04=0x04, ^0x05=0x01, ^0x06=0x07,
     *   ^0x07=0x00, ^0x08=0x08, ^0x09=0x01, ^0x0A=0x0B）。 */
    uint8 expectedChecksum = 0U;
    const uint8 kAllBytes[10] = { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U, 0x09U, 0x0AU };
    for (uint8 i = 0U; i < 10U; i++)
        expectedChecksum ^= kAllBytes[i];

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x77U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], expectedChecksum);
}

// ------------------------------------------------------------
// OK: RequestTransferExit 完了後は Dcm_TransferState が IDLE へ戻り、
// 新たに RequestDownload を再受理できる（1回の診断セッションで複数回の
// ソフトウェア転送シーケンスが行える確認）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID37_RequestTransferExit_Test,
       RequestTransferExit_OK_AllowsNewRequestDownloadAfterCompletionOnCanHw)
{
    /* 準備 (Arrange): 10バイトを送り切って RequestTransferExit する。 */
    const uint8 kData[10] = { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U };
    SendTransferData(0x01U, &kData[0], 5U);
    SendTransferData(0x02U, &kData[5], 5U);
    SendRequestTransferExit();
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x77U);  // 前提確認

    /* 実行 (Act): 新たに RequestDownload を送る。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 5U;
    FakeCanHw_RxData[1] = DCM_SID_REQUEST_DOWNLOAD;
    FakeCanHw_RxData[2] = DCM_TRANSFER_DATA_FORMAT_RAW;
    FakeCanHw_RxData[3] = 0x11U;
    FakeCanHw_RxData[4] = 0x20U;
    FakeCanHw_RxData[5] = 0x08U;
    for (uint8 i = 6U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();

    /* 評価 (Assert): 再度正応答 [0x74, ...] を受理できること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x74U);
}

// ------------------------------------------------------------
// NG: 累計受信サイズが宣言サイズ未満（転送未完了）での RequestTransferExit
// は NRC 0x22 conditionsNotCorrect になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID37_RequestTransferExit_Test,
       RequestTransferExit_NG_IncompleteTransferReturnsConditionsNotCorrectOnCanHw)
{
    /* 準備 (Arrange): SetUp() の StartDownload(0x0A) は10バイト宣言だが、
     * 5バイトしか送らない。 */
    const uint8 kBlock[5] = { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U };
    SendTransferData(0x01U, kBlock, 5U);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x76U);  // 前提確認

    /* 実行 (Act) */
    SendRequestTransferExit();

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_REQUEST_TRANSFER_EXIT);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_CONDITIONS_NOT_CORRECT);
}

// ------------------------------------------------------------
// NG: DOWNLOADING 状態でない（TransferData を一度も送らず、かつ既に
// 完了済みの状態からの再要求）RequestTransferExit は
// NRC 0x24 requestSequenceError になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID37_RequestTransferExit_Test,
       RequestTransferExit_NG_AlreadyCompletedReturnsRequestSequenceErrorOnCanHw)
{
    /* 準備 (Arrange): 一度正常に転送を完了させておく。 */
    const uint8 kData[10] = { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U };
    SendTransferData(0x01U, &kData[0], 5U);
    SendTransferData(0x02U, &kData[5], 5U);
    SendRequestTransferExit();
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x77U);  // 前提確認（IDLE へ遷移済み）

    /* 実行 (Act): IDLE 状態で RequestTransferExit を再送する。 */
    SendRequestTransferExit();

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_REQUEST_TRANSFER_EXIT);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_SEQUENCE_ERROR);
}

}  // namespace
