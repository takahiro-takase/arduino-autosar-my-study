/**
 * \file    Bsw_DcmStack_SID11_EcuReset_test.cpp
 * \brief   UDS SID 0x11 ECUReset の、物理層（Can_Hw フェイク）を起点・終点と
 *          するフルコールチェーンテスト（GoogleTest / CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト・応答とも Single Frame に
 *          収まるため、CanTp のマルチフレーム分割は関与しない。
 *
 *          旧 Bsw_Dcm_ReadDtcInfo_test.cpp の
 *          `EcuReset_NG_ExtraByteReturnsIncorrectMessageLength`
 *          （固定長サービスの上限長チェック、2026-09 是正の回帰確認）を
 *          ここへ移植する。
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

class Bsw_DcmStack_SID11_EcuReset_Test : public ::testing::Test
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
// NG: 有効な subFunc に余分な1バイト（[0x11, 0x01, 0x00]、2バイト厳密一致の
// ため上限超過）は incorrectMessageLength (NRC 0x13) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID11_EcuReset_Test,
       EcuReset_NG_ExtraByteProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): [0x11, 0x01, 0x00] を 0x7E0 の受信バッファへセットする
     * （SF: 03 11 01 00）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_ECU_RESET;
    FakeCanHw_RxData[2] = DCM_RESET_HARD;
    FakeCanHw_RxData[3] = 0x00U;
    for (uint8 i = 4U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x11, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_ECU_RESET);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// ------------------------------------------------------------
// OK: [0x11, 0x01] hardReset は extendedSession 中でも正応答
// [0x51, 0x01] を返し、応答後にセッションが defaultSession へ自動的に
// 戻る（Dcm_ResetToDefaultSession() 経由。ファイル冒頭で参照している
// Dcm_HandleEcuReset() の \details 参照）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID11_EcuReset_Test,
       EcuReset_OK_HardResetFromExtendedSessionReturnsToDefaultSessionOnCanHw)
{
    /* 準備 (Arrange 1): 先に extendedSession へ遷移させておく（セッションが
     * 本当に defaultSession へ戻ったことを確認できるようにするため）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_SESSION_CTRL;
    FakeCanHw_RxData[2] = DCM_SESSION_EXTENDED;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x50U);  // 前提確認
    FakeCanHw_Reset();

    /* 準備 (Arrange 2): [0x11, 0x01] を 0x7E0 の受信バッファへセットする
     * （SF: 02 11 01）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_ECU_RESET;
    FakeCanHw_RxData[2] = DCM_RESET_HARD;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 正応答 [0x51, 0x01] が Can_Hw まで到達し、
     * セッションが defaultSession へ戻っていること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x51U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_RESET_HARD);

    Dcm_SesCtrlType session = 0U;
    ASSERT_EQ(Dcm_GetSesCtrlType(&session), E_OK);
    EXPECT_EQ(session, DCM_SESSION_DEFAULT);
}

// ------------------------------------------------------------
// OK: [0x11, 0x03] softReset も正応答 [0x51, 0x03] を返す
// （hardReset とは異なる subFunc 値が正しく echo されることの確認）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID11_EcuReset_Test,
       EcuReset_OK_SoftResetProducesPositiveResponseEchoingSubFuncOnCanHw)
{
    /* 準備 (Arrange): [0x11, 0x03] を 0x7E0 の受信バッファへセットする
     * （SF: 02 11 03）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_ECU_RESET;
    FakeCanHw_RxData[2] = DCM_RESET_SOFT;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x51U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_RESET_SOFT);
}

}  // namespace
