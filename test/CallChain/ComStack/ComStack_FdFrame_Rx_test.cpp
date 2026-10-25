/**
 * \file    ComStack_FdFrame_Rx_test.cpp
 * \brief   CAN FD の長いフレーム（64 byte）の受信（Can_MainFunction_Read → CanIf）のコールチェーンのテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details CAN FD 対応の段階 1（通信スタックの PDU 長を 64 byte まで通す）の確認用。
 *          Can_MainFunction_Read() が、模擬 Can_Hw から 64 byte のフレームを受け取り、
 *          CanIf_RxIndication() が設定 Dlc=64 の RX PDU として受理し、CanIf_ReadRxPduData() で
 *          64 byte のデータを読み出せることを確認する。上位層（PduR/Com）へのルーティングは不要
 *          （RxIndicationFct=NULL）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "CanSM.h"
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

/* CAN FD の 64 byte のフレームを受ける RX PDU（CanIfRxSduId=0、CAN ID=0x300、Dlc=64、受信データの読み出しを有効）。 */
const CanIf_RxPduConfigType kFdRxPdu = {
    /* CanId */                0x300U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  64U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 1U
};

/* 8 byte のフレームを期待する RX PDU（CanIfRxSduId=1、CAN ID=0x301、Dlc=8）。
 * 長さが 8 を超えるフレームを受けたとき、CAN FD で有効な長さ（12 など）は受理し、取りえない長さ（13 など）は棄却することの確認用。 */
const CanIf_RxPduConfigType kFdRxPdus[2] = {
    kFdRxPdu,
    {
        /* CanId */                0x301U,
        /* Hrh */                  0U,
        /* UpperLayerRxPduId */    1U,
        /* Dlc */                  8U,
        /* RxIndicationFct */      NULL,
        /* ReadRxPduDataEnabled */ 1U
    }
};

const CanIf_ConfigType kFdRxConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ kFdRxPdus,
    /* RxPduCount */  2U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CAN FD の 64 byte のフレームの受信のコールチェーンのフィクスチャ。
 * SetUp(): 模擬 Can_Hw と wrap をリセットし、Can_Init()（CAN FD 有効）→ CanIf_Init(&kFdRxConfig) → コントローラを STARTED → CanSM_Init(NULL) の順に初期化する（Init 中の DET ログは抑制する）。
 * TearDown(): CanSM・CanIf を未初期化へ戻す。 */
class ComStack_FdFrame_Rx_Test : public ::testing::Test
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
        CanIf_Init(&kFdRxConfig);
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        CanSM_Init(NULL);

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

}  // namespace

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComStack_FdFrame_Rx_Test, OK_Step01_64ByteFrameIsReadBackFromCanIf)
{
    /* ----------------------------------- */
    /* ---- 準備 (Arrange) --------------- */
    /* ----------------------------------- */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x300U;
    FakeCanHw_RxDlc = 64U;
    for (uint8_t b = 0U; b < 64U; b++)
        FakeCanHw_RxData[b] = (uint8_t)(0x40U + b);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    const Std_ReturnType ret = CanIf_ReadRxPduData(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    ASSERT_EQ(info.SduLength, 64U);
    EXPECT_EQ(buf[0], 0x40U);
    EXPECT_EQ(buf[63], 0x7FU);   // 先頭だけでなく末尾の byte まで届いている
}

TEST_F(ComStack_FdFrame_Rx_Test, OK_Step02_12ByteFrameIsAcceptedAsValidFdLength)
{
    /* ----------------------------------- */
    /* ---- 準備 (Arrange) --------------- */
    /* ----------------------------------- */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x301U;
    FakeCanHw_RxDlc = 12U;   // CAN FD で有効な長さ（Dlc=8 の PDU に、それより長いフレーム）
    for (uint8_t b = 0U; b < 12U; b++)
        FakeCanHw_RxData[b] = (uint8_t)(0x50U + b);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    const Std_ReturnType ret = CanIf_ReadRxPduData(1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(info.SduLength, 8U);   // 設定の Dlc(8) でクランプされる
    EXPECT_EQ(buf[0], 0x50U);
}

TEST_F(ComStack_FdFrame_Rx_Test, NG_Step02_13ByteFrameIsRejectedAsInvalidLength)
{
    /* ----------------------------------- */
    /* ---- 準備 (Arrange) --------------- */
    /* ----------------------------------- */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x301U;
    FakeCanHw_RxDlc = 13U;   // CAN で取りえない長さ（CAN FD は 12 の次が 16）
    for (uint8_t b = 0U; b < 13U; b++)
        FakeCanHw_RxData[b] = (uint8_t)(0x60U + b);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    const Std_ReturnType ret = CanIf_ReadRxPduData(1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 棄却されたので、受信データは保存されていない
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_DATA_LENGTH);
}
