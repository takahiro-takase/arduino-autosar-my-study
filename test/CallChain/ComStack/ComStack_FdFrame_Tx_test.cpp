/**
 * \file    ComStack_FdFrame_Tx_test.cpp
 * \brief   CAN FD の長いフレーム（64 byte）の送信（CanIf_Transmit → Can_Write）のコールチェーンのテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details CAN FD 対応の段階 1（通信スタックの PDU 長を 64 byte まで通す）の確認用。
 *          設定 Dlc=64 の TX PDU に対し、CanIf_Transmit() が 64 byte の送信要求を受理し、
 *          Can_Write() を経て、模擬 Can_Hw_Send() に 64 byte のデータが渡ることを確認する。
 *          上位層（PduR/Com）は使わず、CanIf_Transmit() を直接呼ぶ。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "Can.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
}

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

/* CAN FD の 64 byte のフレームを送る TX PDU（CanIfTxSduId=0、CAN ID=0x300、Dlc=64）。 */
const CanIf_TxPduConfigType kFdTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x300U,
    /* Dlc */               64U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_ConfigType kFdTxConfig = {
    /* TxPduConfig */ &kFdTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CAN FD の 64 byte のフレームの送信のコールチェーンのフィクスチャ。
 * SetUp(): 模擬 Can_Hw と wrap をリセットし、Can_Init()（CAN FD 有効）→ CanIf_Init(&kFdTxConfig) → コントローラを STARTED → PDU を ONLINE の順に設定する（Init 中の DET ログは抑制する）。
 * TearDown(): CanIf を未初期化へ戻す。 */
class ComStack_FdFrame_Tx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;
        canConfig.fdEnabled       = 1U;

        Can_Init(&canConfig);
        CanIf_Init(&kFdTxConfig);
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        CanIf_SetPduMode(0U, CANIF_ONLINE);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

}  // namespace

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComStack_FdFrame_Tx_Test, OK_Step01_64ByteFrameReachesCanHw)
{
    /* ----------------------------------- */
    /* ---- 準備 (Arrange) --------------- */
    /* ----------------------------------- */
    uint8 sdu[64];
    for (uint8 b = 0U; b < 64U; b++)
        sdu[b] = (uint8)(0x80U + b);
    PduInfoType info = { sdu, 64U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_Transmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x300U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 64U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x80U);
    EXPECT_EQ(FakeCanHw_LastSendData[63], 0xBFU);
}
