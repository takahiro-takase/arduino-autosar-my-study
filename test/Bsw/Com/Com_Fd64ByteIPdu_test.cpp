/**
 * \file    Com_Fd64ByteIPdu_test.cpp
 * \brief   64 byte の I-PDU（CAN FD）の、8 byte を超える位置のシグナルの単体テスト
 *
 * \details CAN FD 対応の段階 1（通信スタックの PDU 長を 64 byte まで通す）の確認用。
 *          シグナルのビット位置（BitPosition）が 8 bit（0〜255）に収まらない位置（byte 50 の
 *          bit 400 など）にあるときも、Com_RxIndication() で受けた 64 byte のフレームから、
 *          Com_ReceiveSignal() が正しい値を取り出せることを確認する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "Fake_Det_Hw.h"
}

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

/* 64 byte の RX I-PDU（IPduId=0、PduRId=0）。update-bit なし。 */
const Com_IPduConfigType kFdRxIPdu = {
    /* IPduId */            0U,
    /* DLC */               64U,
    /* PduRId */            0U,
    /* FirstTimeoutMs */    0U,
    /* TimeoutMs */         0U,
    /* IsSignalGroup */     0U,
    /* TxModeMode */        COM_TX_MODE_DIRECT,  // RX I-PDU では未使用
    /* TxPeriodMs */        0U,
    /* TxModeModeTrue */    COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */    0U,
    /* MinDelayMs */        0U,
    /* UpdateBitPosition */ COM_UPDATE_BIT_NONE,
    /* IpduGroupId */       COM_IPDU_GROUP_NONE
};

/* 先頭（bit 0、8 bit）と、byte 50（bit 400、16 bit、ビッグエンディアン）の 2 つの RX シグナル（SignalId=0、1）。 */
const Com_SignalConfigType kFdRxSignals[2] = {
    {
        /* SignalId */  0U, COM_SIGNAL_DIRECTION_RX, 0U,
        /* BitPosition, BitSize, Endian, InitValue */ 0U, 8U, COM_BIG_ENDIAN, 0U,
        COM_FILTER_ALWAYS, 0xFFU, 0U, 0U, 0U, NULL,
        0U, 0U, COM_TRANSFER_PROPERTY_PENDING, COM_RX_TIMEOUT_ACTION_NONE, 0U,
        COM_DATA_INVALID_ACTION_NONE, 0U, NULL, 0U, 0U, NULL, NULL, NULL, NULL, NULL, 0U
    },
    {
        /* SignalId */  1U, COM_SIGNAL_DIRECTION_RX, 0U,
        /* BitPosition, BitSize, Endian, InitValue */ 400U, 16U, COM_BIG_ENDIAN, 0U,
        COM_FILTER_ALWAYS, 0xFFFFU, 0U, 0U, 0U, NULL,
        0U, 0U, COM_TRANSFER_PROPERTY_PENDING, COM_RX_TIMEOUT_ACTION_NONE, 0U,
        COM_DATA_INVALID_ACTION_NONE, 0U, NULL, 0U, 0U, NULL, NULL, NULL, NULL, NULL, 0U
    }
};

/* 64 byte の TX I-PDU（IPduId=0、PduRId=0）。update-bit なし。 */
const Com_IPduConfigType kFdTxIPdu = {
    /* IPduId */            0U,
    /* DLC */               64U,
    /* PduRId */            0U,
    /* FirstTimeoutMs */    0U,
    /* TimeoutMs */         0U,
    /* IsSignalGroup */     0U,
    /* TxModeMode */        COM_TX_MODE_DIRECT,
    /* TxPeriodMs */        0U,
    /* TxModeModeTrue */    COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */    0U,
    /* MinDelayMs */        0U,
    /* UpdateBitPosition */ COM_UPDATE_BIT_NONE,
    /* IpduGroupId */       COM_IPDU_GROUP_NONE
};

/* RX の 2 つに、byte 50（bit 400、16 bit、ビッグエンディアン）の TX シグナル（SignalId=2。Com は SignalId を表の添字として引くため、RX の 2 つに続く番号にする）を加えた設定。 */
const Com_SignalConfigType kFdSignals[3] = {
    kFdRxSignals[0],
    kFdRxSignals[1],
    {
        /* SignalId */  2U, COM_SIGNAL_DIRECTION_TX, 0U,
        /* BitPosition, BitSize, Endian, InitValue */ 400U, 16U, COM_BIG_ENDIAN, 0U,
        COM_FILTER_ALWAYS, 0xFFFFU, 0U, 0U, 0U, NULL,
        0U, 0U, COM_TRANSFER_PROPERTY_PENDING, COM_RX_TIMEOUT_ACTION_NONE, 0U,
        COM_DATA_INVALID_ACTION_NONE, 0U, NULL, 0U, 0U, NULL, NULL, NULL, NULL, NULL, 0U
    }
};

const Com_ConfigType kFdConfig = {
    &kFdRxIPdu, 1U,
    &kFdTxIPdu, 1U,
    kFdSignals, 3U,
    NULL, 0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* 64 byte の I-PDU の単体テスト用フィクスチャ。
 * SetUp(): Com_Init(&kFdConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): Com_DeInit() で未初期化へ戻す。 */
class Com_Fd64ByteIPdu_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Com_Init(&kFdConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
    }
};

}  // namespace

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Com_Fd64ByteIPdu_Test, Com_ReceiveSignal_OK_SignalBeyond8BitPositionRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[64] = { 0U };
    frame[0]  = 0x11U;   // 先頭のシグナル（bit 0）
    frame[50] = 0xABU;   // bit 400〜407
    frame[51] = 0xCDU;   // bit 408〜415
    PduInfoType info = { frame, 64U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_RxIndication(0U, &info);
    uint8  first  = 0U;
    uint16 beyond = 0U;
    const uint8 retFirst  = Com_ReceiveSignal(0U, &first);
    const uint8 retBeyond = Com_ReceiveSignal(1U, &beyond);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retFirst, E_OK);
    EXPECT_EQ(retBeyond, E_OK);
    EXPECT_EQ(first, 0x11U);
    EXPECT_EQ(beyond, 0xABCDU);   // 以前は bit 400 が 8 bit に収まらず、別の位置を読んでいた
}

TEST_F(Com_Fd64ByteIPdu_Test, Com_SendSignal_OK_SignalBeyond8BitPositionRangeIsPackedIntoTxBuffer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint16 value = 0xABCDU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_SendSignal(2U, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[50], 0xABU);   // bit 400〜407
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[51], 0xCDU);   // bit 408〜415
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0],  0x00U);   // 先頭の byte は、書き換わっていない（位置が折り返していない）
}
