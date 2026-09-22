/**
 * \file    Bsw_DcmStack_SID14_ClearDtc_test.cpp
 * \brief   UDS SID 0x14 ClearDiagnosticInformation の、物理層（Can_Hw
 *          フェイク）を起点・終点とするフルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト・応答とも Single Frame に
 *          収まるため、CanTp のマルチフレーム分割は関与しない。
 *
 *          0x14 は extendedSession かつ SecurityAccess Level1 のアンロック
 *          が必須のため、`UnlockSecurityAccessLevel1()` で
 *          [0x10,0x03]→[0x27,0x01](requestSeed)→[0x27,0x02](sendKey) を
 *          実際に Can_Hw から受信させ、seed/key のやり取りも含めて実チェーン
 *          経由でアンロックする（旧 Bsw_Dcm_ReadDtcInfo_test.cpp の同名
 *          ヘルパーをチェーン化したもの。seed は物理応答フレームから読み
 *          取る）。
 *
 *          旧 Bsw_Dcm_ReadDtcInfo_test.cpp の
 *          `ClearDtc_NG_ExtraByteReturnsIncorrectMessageLength`
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

class Bsw_DcmStack_SID14_ClearDtc_Test : public ::testing::Test
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
         * （Bsw_ComStack_Rx_test.cpp と同じ理由）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM 経由で FULL_COM を確立しないため、CanIf_Init()
         * 直後の既定値 CANIF_OFFLINE のままだと応答側の CanIf_Transmit() が
         * 常に E_NOT_OK になってしまう（Bsw_ComStack_Tx_SendSignal_test.cpp
         * と同じ理由）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        /* CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため、
         * CanSM 未初期化のままだと DET_E_UNINIT が毎回報告される
         * （Bsw_ComStack_Rx_test.cpp と同じ理由。CanSM 自体の状態機械は
         * 本テストの対象外）。 */
        CanSM_Init(NULL);
        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);
        UnlockSecurityAccessLevel1();  // 0x14 は extendedSession かつ Level1 アンロック必須

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
     * のアンロックを行う（seed/key の計算式は本番コードと同じ
     * `seed ^ DCM_SECURITY_KEY_MASK`。ファイル冒頭コメント参照）。 */
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

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// NG: groupOfDTC(3byte)の後に余分な1バイトが付いた要求
// （[0x14, 0xFF,0xFF,0xFF, 0x00]、4バイト厳密一致のため上限超過）は
// incorrectMessageLength (NRC 0x13) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID14_ClearDtc_Test,
       ClearDtc_NG_ExtraByteProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): [0x14, 0xFF,0xFF,0xFF, 0x00] を 0x7E0 の受信バッファへ
     * セットする（SF: 05 14 FF FF FF 00）。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 5U;
    FakeCanHw_RxData[1] = DCM_SID_CLEAR_DTC;
    FakeCanHw_RxData[2] = 0xFFU;
    FakeCanHw_RxData[3] = 0xFFU;
    FakeCanHw_RxData[4] = 0xFFU;
    FakeCanHw_RxData[5] = 0x00U;
    FakeCanHw_RxData[6] = 0U;
    FakeCanHw_RxData[7] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x14, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_CLEAR_DTC);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

}  // namespace
