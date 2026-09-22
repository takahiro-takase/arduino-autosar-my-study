/**
 * \file    Bsw_DcmStack_SID22_ReadDataByIdChain_test.cpp
 * \brief   UDS SID 0x22 ReadDataByIdentifier（VIN、DID 0xF190）の、物理層
 *          （Can_Hw フェイク）を起点・終点とするフルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF04_ReadDtcSnapshotChain_test.cpp 系列で
 *          確立したマルチフレームの型を適用する。応答は
 *          `3 + DCM_VIN_LENGTH = 20` バイトで Single Frame の7バイト制限を
 *          超えるため、FF（6バイト）+ CF 2本（残り14バイトを7バイトずつ、
 *          ちょうど割り切れる）というマルチフレームになる。リクエスト
 *          （3バイト）は Single Frame に収まる。
 *
 *          extendedSession は不要（VIN 読み出しはどのセッションでも許可）。
 *          期待値は Bsw_Dcm_ReadDtcInfo_test.cpp の ReadDataById_* と同じ
 *          もの（境界は `Wrap_CanTp.h` でキャプチャ）だが、本ファイルは
 *          さらに物理送信（Can_Hw への実フレーム到達）まで検証する。
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
// テスト専用の最小 CanIf/PduR 設定（Bsw_DcmStack_SID19_SF01_ReadDtcCountChain_test.cpp
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

class Bsw_DcmStack_SID22_ReadDataByIdChain_Test : public ::testing::Test
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
         * （Bsw_ComStack_RxChain_test.cpp と同じ理由）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM 経由で FULL_COM を確立しないため、CanIf_Init()
         * 直後の既定値 CANIF_OFFLINE のままだと応答側の CanIf_Transmit() が
         * 常に E_NOT_OK になってしまう（Bsw_ComStack_TxChain_SendSignal_test.cpp
         * と同じ理由）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        /* CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため、
         * CanSM 未初期化のままだと DET_E_UNINIT が毎回報告される
         * （Bsw_ComStack_RxChain_test.cpp と同じ理由。CanSM 自体の状態機械は
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
TEST_F(Bsw_DcmStack_SID22_ReadDataByIdChain_Test,
       ReadDataById_OK_VinMultiFrameResponseReassemblesToExpectedPayloadOnCanHw)
{
    /* 準備 (Arrange): [0x22, 0xF1, 0x90] を 0x7E0 の受信バッファへセットする
     * （SF: 03 22 F1 90）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DATA;
    FakeCanHw_RxData[2] = (uint8)(DCM_DID_VIN >> 8U);
    FakeCanHw_RxData[3] = (uint8)(DCM_DID_VIN & 0xFFU);
    for (uint8 i = 4U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 1): リクエスト受信 → Dcm 応答生成 → CanTp_Transmit(20バイト)
     * → First Frame 送信（WAIT_FC へ遷移）まで同期的に進む。 */
    Can_MainFunction_Read();

    /* 評価 (Assert 1): Dcm が生成した UDS ペイロード自体は
     * Bsw_Dcm_ReadDtcInfo_test.cpp と同じ期待値。 */
    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, (uint8)(3U + DCM_VIN_LENGTH));  // 20
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x62U);
    EXPECT_EQ(LastData_CanTp_Transmit[1], (uint8)(DCM_DID_VIN >> 8U));
    EXPECT_EQ(LastData_CanTp_Transmit[2], (uint8)(DCM_DID_VIN & 0xFFU));

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x10U);  // FF PCI（len=20<256）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 20U);    // len 下位8bit

    uint8 reassembled[24] = { 0U };
    uint8 pos = 0U;
    for (uint8 i = 0U; i < 6U; i++)
        reassembled[pos++] = FakeCanHw_LastSendData[2U + i];

    /* 準備 (Arrange 2): テスター役として Flow Control（CTS, BS=0, STmin=0）を
     * 0x7E0 から追加で受信させる。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 0x30U;
    FakeCanHw_RxData[1] = 0x00U;
    FakeCanHw_RxData[2] = 0x00U;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 2) */
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);  // FC 受信自体は Can_Hw への送信を生まない

    /* 実行 (Act 3): 残り14バイトは Consecutive Frame 2本（7+7バイト）で
     * ちょうど運びきれる（ceil((20-6)/7)=2）。 */
    for (uint8 cf = 0U; cf < 2U; cf++)
    {
        CanTp_MainFunction();

        ASSERT_EQ(FakeCanHw_SendCount, (uint32)(2U + cf))
            << "CF #" << (unsigned)(cf + 1U) << " が送信されていない";
        EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
        EXPECT_EQ(FakeCanHw_LastSendData[0], (uint8)(0x20U | ((cf + 1U) & 0x0FU)))
            << "CF #" << (unsigned)(cf + 1U) << " の SN 不一致";

        uint16 remaining = 20U - pos;
        uint8  copyLen   = (remaining > 7U) ? 7U : (uint8)remaining;
        for (uint8 i = 0U; i < copyLen; i++)
            reassembled[pos++] = FakeCanHw_LastSendData[1U + i];
    }

    /* 評価 (Assert 2): CanTp が IDLE へ戻り、CAN フレームへ分割・送出された
     * 内容を結合すると Dcm が生成した元の20バイト UDS ペイロードと完全
     * 一致すること。 */
    ASSERT_EQ(pos, 20U);
    EXPECT_EQ(FakeCanHw_SendCount, 3U);  // FF 1 + CF 2
    EXPECT_EQ(CanTp_IsTxBusy(), (boolean)0U);
    for (uint8 i = 0U; i < 20U; i++)
    {
        EXPECT_EQ(reassembled[i], LastData_CanTp_Transmit[i]) << "byte " << (unsigned)i;
    }
}

// ------------------------------------------------------------
// NG: DID の下位バイトが無い（[0x22, 0xF1] のみ、2バイト。3バイト必須）
// リクエストは incorrectMessageLength (NRC 0x13) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID22_ReadDataByIdChain_Test,
       ReadDataById_NG_TooShortRequestProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): [0x22, 0xF1] を 0x7E0 の受信バッファへセットする
     * （SF: 02 22 F1）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DATA;
    FakeCanHw_RxData[2] = (uint8)(DCM_DID_VIN >> 8U);
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x22, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_READ_DATA);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// ------------------------------------------------------------
// NG: 2件目のDID（[0x22, 0xF1,0x90, 0xF1,0x90]、5バイト。本実装は単一DIDのみ
// 対応）は incorrectMessageLength (NRC 0x13) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID22_ReadDataByIdChain_Test,
       ReadDataById_NG_MultipleDidRequestProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): [0x22, 0xF1,0x90, 0xF1,0x90] を 0x7E0 の受信バッファへ
     * セットする（SF: 05 22 F1 90 F1）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 5U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DATA;
    FakeCanHw_RxData[2] = (uint8)(DCM_DID_VIN >> 8U);
    FakeCanHw_RxData[3] = (uint8)(DCM_DID_VIN & 0xFFU);
    FakeCanHw_RxData[4] = (uint8)(DCM_DID_VIN >> 8U);
    FakeCanHw_RxData[5] = (uint8)(DCM_DID_VIN & 0xFFU);
    FakeCanHw_RxData[6] = 0U;
    FakeCanHw_RxData[7] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x22, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_READ_DATA);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

}  // namespace
