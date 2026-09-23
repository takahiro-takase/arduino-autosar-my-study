/**
 * \file    Bsw_DcmStack_SID36_TransferData_test.cpp
 * \brief   UDS SID 0x36 TransferData の、物理層（Can_Hw フェイク）を
 *          起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト・応答とも Single Frame に
 *          収まるため、CanTp のマルチフレーム分割は関与しない。
 *
 *          0x36 は DOWNLOADING 状態（RequestDownload 受理済み）でなければ
 *          意味を持たないため、`StartDownload()` ヘルパーで
 *          extendedSession→SecurityAccess Level1アンロック→RequestDownload
 *          を実チェーン経由で行い、前提状態を作る
 *          （Bsw_DcmStack_SID34_RequestDownload_test.cpp 参照）。
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

class Bsw_DcmStack_SID36_TransferData_Test : public ::testing::Test
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
        StartDownload(0x40U);  // 0x36 は DOWNLOADING 状態が前提（64バイト転送予定）

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
     * （Bsw_DcmStack_SID34_RequestDownload_test.cpp と同じ手順の統合版）。 */
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
     * チェーン全体を駆動する（Act）。 */
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

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// OK: DOWNLOADING 中、期待どおりの blockSequenceCounter(0x01) での
// TransferData は正応答 [0x76, 0x01] を返す。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID36_TransferData_Test,
       TransferData_OK_ExpectedBlockCounterProducesPositiveResponseOnCanHw)
{
    /* 実行 (Act) */
    const uint8 kData[4] = { 0x01U, 0x02U, 0x03U, 0x04U };
    SendTransferData(0x01U, kData, 4U);

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x76U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x01U);
}

// ------------------------------------------------------------
// OK: 連続する2ブロック（counter 0x01→0x02）はいずれも正応答を返し、
// カウンタが正しくインクリメントされる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID36_TransferData_Test,
       TransferData_OK_ConsecutiveBlocksIncrementCounterOnCanHw)
{
    const uint8 kBlock1[4] = { 0x01U, 0x02U, 0x03U, 0x04U };
    SendTransferData(0x01U, kBlock1, 4U);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x76U);  // 前提確認

    const uint8 kBlock2[4] = { 0x05U, 0x06U, 0x07U, 0x08U };
    SendTransferData(0x02U, kBlock2, 4U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x76U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x02U);
}

// ------------------------------------------------------------
// NG: 期待値と異なる blockSequenceCounter は
// NRC 0x73 wrongBlockSequenceCounter になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID36_TransferData_Test,
       TransferData_NG_WrongBlockCounterReturnsWrongBlockSequenceCounterOnCanHw)
{
    const uint8 kData[4] = { 0x01U, 0x02U, 0x03U, 0x04U };
    SendTransferData(0x02U /* 期待値は0x01 */, kData, 4U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_TRANSFER_DATA);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_WRONG_BLOCK_SEQUENCE_COUNTER);
}

// ------------------------------------------------------------
// NG: RequestDownload で宣言した memorySize(0x40=64バイト) を超える累計
// データは NRC 0x71 transferDataSuspended になる（各ブロックは CanTp の
// SF 上限(7バイト、UDSペイロードでは data 5バイトまで)に収める）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID36_TransferData_Test,
       TransferData_NG_ExceedsDeclaredSizeReturnsTransferDataSuspendedOnCanHw)
{
    /* 準備 (Arrange): SetUp() の StartDownload(0x40) は64バイト宣言のため、
     * 5バイト×12回=60バイトを送っておく（残り4バイトのみ許容）。 */
    const uint8 kBlock[5] = { 0U, 0U, 0U, 0U, 0U };
    uint8 counter = 0x01U;
    for (uint8 i = 0U; i < 12U; i++)
    {
        SendTransferData(counter, kBlock, 5U);
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x76U) << "block " << (unsigned)i << " must succeed";
        counter++;
    }

    /* 実行 (Act): 5バイト送ると 60+5=65 > 64 で超過する。 */
    SendTransferData(counter, kBlock, 5U);

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_TRANSFER_DATA);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_TRANSFER_DATA_SUSPENDED);
}

// ------------------------------------------------------------
// NG: 転送完了（RequestTransferExit 済み）で IDLE に戻った後の TransferData
// は NRC 0x24 requestSequenceError になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID36_TransferData_Test,
       TransferData_NG_AfterTransferCompleteReturnsRequestSequenceErrorOnCanHw)
{
    /* 準備 (Arrange): SetUp() の StartDownload(0x40) が宣言した64バイトを
     * ちょうど送り切り（5バイト×12回+4バイト×1回=64）、
     * [0x37] RequestTransferExit で IDLE へ戻す。 */
    const uint8 kBlock5[5] = { 0U, 0U, 0U, 0U, 0U };
    const uint8 kBlock4[4] = { 0U, 0U, 0U, 0U };
    uint8 counter = 0x01U;
    for (uint8 i = 0U; i < 12U; i++)
    {
        SendTransferData(counter, kBlock5, 5U);
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x76U) << "block " << (unsigned)i << " must succeed";
        counter++;
    }
    SendTransferData(counter, kBlock4, 4U);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x76U) << "final block must succeed";

    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 1U;
    FakeCanHw_RxData[1] = DCM_SID_REQUEST_TRANSFER_EXIT;
    for (uint8 i = 2U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x77U) << "RequestTransferExit must succeed as a test precondition";

    /* 実行 (Act): IDLE に戻った状態で TransferData を再送する。 */
    const uint8 kData[4] = { 0x01U, 0x02U, 0x03U, 0x04U };
    SendTransferData(0x01U, kData, 4U);

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_TRANSFER_DATA);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_SEQUENCE_ERROR);
}

}  // namespace
