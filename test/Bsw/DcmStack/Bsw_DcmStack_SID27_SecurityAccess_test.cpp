/**
 * \file    Bsw_DcmStack_SID27_SecurityAccess_test.cpp
 * \brief   UDS SID 0x27 SecurityAccess の、物理層（Can_Hw フェイク）を
 *          起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト・応答とも Single Frame に
 *          収まるため、CanTp のマルチフレーム分割は関与しない。
 *
 *          旧 Bsw_Dcm_ReadDtcInfo_test.cpp の
 *          `SecuritySendKey_NG_ExtraByteReturnsIncorrectMessageLength`/
 *          `SecurityRequestSeed_NG_ExtraByteReturnsIncorrectMessageLength`
 *          （固定長サービスの上限長チェック、2026-09 是正の回帰確認）を
 *          ここへ移植する。同じ SID のため1ファイルに同居させる。
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

class Bsw_DcmStack_SID27_SecurityAccess_Test : public ::testing::Test
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
        EnterExtendedSession();  // 0x27 は extendedSession 限定

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /* [0x10, 0x03] extendedDiagnosticSession を実際に Can_Hw から受信させ、
     * 物理応答が正応答であることまで確認する前提確立専用のヘルパー
     * （Bsw_DcmStack_SID31_RoutineControl_test.cpp と同じ理由）。 */
    void EnterExtendedSession()
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

        ASSERT_EQ(FakeCanHw_SendCount, 1U);
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x50U);  // 正応答確認（前提が崩れていないこと）
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// NG: requestSeed に余分な1バイト（[0x27, 0x01, 0x00]、2バイト厳密一致の
// ため上限超過）は incorrectMessageLength (NRC 0x13) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID27_SecurityAccess_Test,
       SecurityRequestSeed_NG_ExtraByteProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): [0x27, 0x01, 0x00] を 0x7E0 の受信バッファへセットする
     * （SF: 03 27 01 00）。SetUp() の EnterExtendedSession() が残した送信
     * カウントをリセットしてから使う。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_SECURITY_ACCESS;
    FakeCanHw_RxData[2] = DCM_SEC_SUBFUNC_REQUEST_SEED;
    FakeCanHw_RxData[3] = 0x00U;
    for (uint8 i = 4U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x27, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_SECURITY_ACCESS);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// ------------------------------------------------------------
// NG: requestSeed 済みの状態で、sendKey に余分な1バイト
// （[0x27, 0x02, keyH, keyL, 0x00]、4バイト厳密一致のため上限超過）は
// incorrectMessageLength (NRC 0x13) になる。長さチェックがキー値の妥当性
// より先に効くため、キー値自体は不正でも構わない。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID27_SecurityAccess_Test,
       SecuritySendKey_NG_ExtraByteProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange 1): [0x27, 0x01] requestSeed を送っておく（sendKey の
     * 前提）。SetUp() の EnterExtendedSession() が残した送信カウントを
     * リセットしてから使う。 */
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
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x67U);  // requestSeed が正応答であること（前提確認）
    FakeCanHw_Reset();

    /* 準備 (Arrange 2): 余分な1バイト付きの [0x27, 0x02, 0x00,0x00, 0x00]
     * を 0x7E0 の受信バッファへセットする（SF: 05 27 02 00 00 00）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 5U;
    FakeCanHw_RxData[1] = DCM_SID_SECURITY_ACCESS;
    FakeCanHw_RxData[2] = DCM_SEC_SUBFUNC_SEND_KEY;
    FakeCanHw_RxData[3] = 0x00U;
    FakeCanHw_RxData[4] = 0x00U;
    FakeCanHw_RxData[5] = 0x00U;
    FakeCanHw_RxData[6] = 0U;
    FakeCanHw_RxData[7] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x27, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_SECURITY_ACCESS);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// ------------------------------------------------------------
// OK: requestSeed→sendKey の実チェーンで SecurityAccess Level1 が
// アンロックされる（key = seed ^ DCM_SECURITY_KEY_MASK、本番コードと同じ
// 計算式。seed は requestSeed の物理応答フレームから読み取る）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID27_SecurityAccess_Test,
       SecurityAccess_OK_RequestSeedThenSendKeyUnlocksLevel1OnCanHw)
{
    /* 準備 (Arrange): [0x27, 0x01] requestSeed を 0x7E0 の受信バッファへ
     * セットする（SF: 02 27 01）。SetUp() の EnterExtendedSession() が
     * 残した送信カウントをリセットしてから使う。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_SECURITY_ACCESS;
    FakeCanHw_RxData[2] = DCM_SEC_SUBFUNC_REQUEST_SEED;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 1) */
    Can_MainFunction_Read();

    /* 評価 (Assert 1): requestSeed の正応答 [0x67, 0x01, seedH, seedL] が
     * Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x04U);  // SF PCI（UDSペイロード長=4）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x67U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SEC_SUBFUNC_REQUEST_SEED);
    uint16 seed = (uint16)(((uint16)FakeCanHw_LastSendData[3] << 8U) | (uint16)FakeCanHw_LastSendData[4]);
    uint16 key  = (uint16)(seed ^ DCM_SECURITY_KEY_MASK);
    FakeCanHw_Reset();

    /* 準備 (Arrange 2): [0x27, 0x02, keyH, keyL] を 0x7E0 の受信バッファへ
     * セットする（SF: 04 27 02 keyH keyL）。 */
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

    /* 実行 (Act 2) */
    Can_MainFunction_Read();

    /* 評価 (Assert 2): sendKey の正応答 [0x67, 0x02] が Can_Hw まで到達し、
     * SecurityAccess Level1 が実際にアンロックされていること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x67U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SEC_SUBFUNC_SEND_KEY);

    Dcm_SecLevelType secLevel = 0U;
    ASSERT_EQ(Dcm_GetSecurityLevel(&secLevel), E_OK);
    EXPECT_EQ(secLevel, 1U);  // Unlocked
}

// ------------------------------------------------------------
// OK: 既にアンロック済みの状態で requestSeed を再送すると、ISO 14229-1 の
// 作法どおり allZeroSeed（[0x67, 0x01, 0x00, 0x00]）を返す（sendKey 不要の
// 合図）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID27_SecurityAccess_Test,
       SecurityAccess_OK_RequestSeedWhenAlreadyUnlockedReturnsAllZeroSeedOnCanHw)
{
    /* 準備 (Arrange 1): 一度 requestSeed→sendKey でアンロックしておく。 */
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
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x67U);
    Dcm_SecLevelType secLevel = 0U;
    ASSERT_EQ(Dcm_GetSecurityLevel(&secLevel), E_OK);
    ASSERT_EQ(secLevel, 1U);  // アンロック済みであることの前提確認
    FakeCanHw_Reset();

    /* 準備 (Arrange 2): [0x27, 0x01] requestSeed を再送する。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_SECURITY_ACCESS;
    FakeCanHw_RxData[2] = DCM_SEC_SUBFUNC_REQUEST_SEED;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): allZeroSeed [0x67, 0x01, 0x00, 0x00] が返ること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x67U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SEC_SUBFUNC_REQUEST_SEED);
    EXPECT_EQ(FakeCanHw_LastSendData[3], 0x00U);
    EXPECT_EQ(FakeCanHw_LastSendData[4], 0x00U);
}

}  // namespace
