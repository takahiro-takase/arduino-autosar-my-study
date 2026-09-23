/**
 * \file    Bsw_DcmStack_SID2E_WriteDataById_test.cpp
 * \brief   UDS SID 0x2E WriteDataByIdentifier の、物理層（Can_Hw フェイク）を
 *          起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。DCM_DID_TEST_PATTERN(0x0104)の要求は
 *          11バイト（SID+DID2バイト+データ8バイト）で CanTp の SF 上限
 *          （7バイト）を超えるため、CanTp の複数フレーム要求受信（FF+CF）を
 *          実際に駆動する（Fake_Can_Hw への複数回のフレーム投入で模擬する）。
 *
 *          0x2E は extendedSession かつ SecurityAccess Level1 のアンロックが
 *          必須のため、`UnlockSecurityAccessLevel1()`
 *          （Bsw_DcmStack_SID14_ClearDtc_test.cpp と同じ手順）で事前に
 *          アンロックする。
 *
 *          本ファイルは本プロジェクトでこの SID を対象とする初めての
 *          テストファイル（従来は単体・チェーンいずれのテストも存在せず、
 *          移植元は無い。ゼロから新設）。
 *
 *          DCM_DID_CRYPTO_KEY_UPDATE(0x0108)は KeyM の鍵更新セッションを
 *          駆動するが、native_chain は KeyM を最小リンクスタブ
 *          （`stub/Bsw/KeyM/Fake_KeyM.c`）で満たしており、`KeyM_Start()`が
 *          常に `E_NOT_OK` を返す構造的な制約により、このチェーン上では
 *          正応答を実機到達できない（Dcm_UpdateCryptoKey()経由で常に
 *          NRC 0x31 requestOutOfRange になる）。そのため本DIDはNGケース
 *          （境界確認）としてのみ扱う。
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

class Bsw_DcmStack_SID2E_WriteDataById_Test : public ::testing::Test
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
        UnlockSecurityAccessLevel1();  // 0x2E は extendedSession かつ Level1 アンロック必須

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

    /* [0x2E, 0x01,0x04, data0..data7]（DCM_DID_TEST_PATTERN、11バイト）を
     * CanTp の FF(6バイト)+CF(5バイト)の2フレームに分けて Can_Hw から受信
     * させ、チェーン全体を駆動する（Act）。CanTp は FF 受信直後に自動で
     * Flow Control（CTS）を物理送信する（CanTp_SendFlowControl()、CanTp.c
     * 参照）ため、テスト側から FC を注入する必要はない——受信側 CanTp
     * 自身が「継続してよい」と送信する側だからである。FF 受信直後の
     * FakeCanHw_Reset() で、この自動 FC 送信を後続の応答検証から除外する。 */
    void SendWriteTestPattern(const uint8 (&data)[DCM_DID_TEST_PATTERN_LENGTH])
    {
        FakeCanHw_Reset();

        /* FF: PCI=[0x10, 0x0B]（DataLength=11）+ SID/DID/data0..data4 */
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 0x10U;
        FakeCanHw_RxData[1] = 0x0BU;
        FakeCanHw_RxData[2] = DCM_SID_WRITE_DATA;
        FakeCanHw_RxData[3] = (uint8)(DCM_DID_TEST_PATTERN >> 8U);
        FakeCanHw_RxData[4] = (uint8)(DCM_DID_TEST_PATTERN & 0xFFU);
        FakeCanHw_RxData[5] = data[0];
        FakeCanHw_RxData[6] = data[1];
        FakeCanHw_RxData[7] = data[2];
        FakeCanHw_RxPendingCount = 1U;
        Can_MainFunction_Read();
        FakeCanHw_Reset();  // CanTp の自動 Flow Control 送信を後続の検証から除外する

        /* CF#1: PCI=[0x21] + 残り data5..data7（3バイト） */
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 0x21U;
        FakeCanHw_RxData[1] = data[3];
        FakeCanHw_RxData[2] = data[4];
        FakeCanHw_RxData[3] = data[5];
        FakeCanHw_RxData[4] = data[6];
        FakeCanHw_RxData[5] = data[7];
        for (uint8 i = 6U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;
        Can_MainFunction_Read();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// OK: DCM_DID_TEST_PATTERN(0x0104) への8バイト書き込みは正応答
// [0x6E, 0x01,0x04] を返す。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2E_WriteDataById_Test,
       WriteDataById_OK_TestPatternWriteProducesPositiveResponseOnCanHw)
{
    /* 準備 (Arrange) + 実行 (Act) */
    const uint8 kPattern[DCM_DID_TEST_PATTERN_LENGTH] =
        { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U };
    SendWriteTestPattern(kPattern);

    /* 評価 (Assert): 正応答 [0x6E, 0x01, 0x04] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x6EU);
    EXPECT_EQ(FakeCanHw_LastSendData[2], (uint8)(DCM_DID_TEST_PATTERN >> 8U));
    EXPECT_EQ(FakeCanHw_LastSendData[3], (uint8)(DCM_DID_TEST_PATTERN & 0xFFU));
}

// ------------------------------------------------------------
// OK: 書き込んだ値が SID 0x22 ReadDataById での読み出しに反映される
// （0x2E/0x22 間の実際のデータ経路の確認）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2E_WriteDataById_Test,
       WriteDataById_OK_WrittenValueIsReflectedInSubsequentReadDataByIdOnCanHw)
{
    /* 準備 (Arrange 1) + 実行 (Act 1): TestPattern を書き込む。 */
    const uint8 kPattern[DCM_DID_TEST_PATTERN_LENGTH] =
        { 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU, 0x01U, 0x02U };
    SendWriteTestPattern(kPattern);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x6EU);  // 前提確認

    /* 準備 (Arrange 2): [0x22, 0x01,0x04] を 0x7E0 の受信バッファへセットする
     * （SF: 03 22 01 04）。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DATA;
    FakeCanHw_RxData[2] = (uint8)(DCM_DID_TEST_PATTERN >> 8U);
    FakeCanHw_RxData[3] = (uint8)(DCM_DID_TEST_PATTERN & 0xFFU);
    for (uint8 i = 4U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 2) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 正応答 [0x62, 0x01,0x04, 直前に書き込んだ8バイト] が
     * Can_Hw まで到達すること（SF: 0B 62 01 04 ... + FC/CF 経由で残り）。
     * 応答自体は12バイトで CanTp の SF 上限(7)を超えるためマルチフレーム
     * になる。ここでは初回フレーム（FF）の先頭のみ確認する。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);  // FF のみ（FC 待ち）
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x10U);  // FF PCI 上位
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x0BU);  // DataLength=11（SID+DID2+data8）
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x62U);
    EXPECT_EQ(FakeCanHw_LastSendData[3], (uint8)(DCM_DID_TEST_PATTERN >> 8U));
    EXPECT_EQ(FakeCanHw_LastSendData[4], (uint8)(DCM_DID_TEST_PATTERN & 0xFFU));
    EXPECT_EQ(FakeCanHw_LastSendData[5], kPattern[0]);
    EXPECT_EQ(FakeCanHw_LastSendData[6], kPattern[1]);
}

// ------------------------------------------------------------
// NG: DCM_DID_CRYPTO_KEY_UPDATE(0x0108) への書き込みは、native_chain が
// KeyM を最小スタブ（KeyM_Start() が常に E_NOT_OK）で満たしているため、
// 構造的に常に NRC 0x31 requestOutOfRange になる（ファイル冒頭コメント
// 参照）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2E_WriteDataById_Test,
       WriteDataById_NG_CryptoKeyUpdateFailsDueToKeyMStubBoundaryOnCanHw)
{
    /* 準備 (Arrange): [0x2E, 0x01,0x08, keyName, key0..key15]
     * （DCM_DID_CRYPTO_KEY_UPDATE、20バイト）を FF+CF×2 で受信させる。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 0x10U;
    FakeCanHw_RxData[1] = 0x14U;  // DataLength=20
    FakeCanHw_RxData[2] = DCM_SID_WRITE_DATA;
    FakeCanHw_RxData[3] = (uint8)(DCM_DID_CRYPTO_KEY_UPDATE >> 8U);
    FakeCanHw_RxData[4] = (uint8)(DCM_DID_CRYPTO_KEY_UPDATE & 0xFFU);
    FakeCanHw_RxData[5] = 0x01U;  // keyName
    FakeCanHw_RxData[6] = 0x00U;  // key0
    FakeCanHw_RxData[7] = 0x00U;  // key1
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();
    FakeCanHw_Reset();  // CanTp の自動 Flow Control 送信を後続の検証から除外する
                          // （SendWriteTestPattern() と同じ理由）。

    /* CF#1: key2..key8（7バイト） */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 0x21U;
    for (uint8 i = 1U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0x00U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();

    /* CF#2: key9..key15（7バイト、実際は残り7バイトのみ有効） */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 0x22U;
    for (uint8 i = 1U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0x00U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x2E, 0x31] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_WRITE_DATA);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// ------------------------------------------------------------
// NG: 未対応 DID への書き込みは NRC 0x31 requestOutOfRange になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2E_WriteDataById_Test,
       WriteDataById_NG_UnknownDidReturnsRequestOutOfRangeOnCanHw)
{
    /* 準備 (Arrange): [0x2E, 0x00,0x01, 0x00]（未対応DID）を 0x7E0 の
     * 受信バッファへセットする（SF: 04 2E 00 01 00）。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 4U;
    FakeCanHw_RxData[1] = DCM_SID_WRITE_DATA;
    FakeCanHw_RxData[2] = 0x00U;
    FakeCanHw_RxData[3] = 0x01U;
    FakeCanHw_RxData[4] = 0x00U;
    for (uint8 i = 5U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_WRITE_DATA);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

}  // namespace
