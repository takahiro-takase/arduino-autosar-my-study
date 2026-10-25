/**
 * \file    Can_Write_test.cpp
 * \brief   Can_Write() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Can_test.cpp）。
 *
 *          Can.c は実 HW 依存の無い自己完結したロジックのため、フェイク
 *          実装は不要で公開 API (Can_Write/Can_Read とその Init) を
 *          直接呼んで検証する。
 *
 *          分割前の共通の説明は、旧 Can_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Cfg.h"
#include "Can_Hw.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
#include "Fake_Bsw_EcuM.h"
}

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Can の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw・wrap・DET の記録をリセットし、Can を未初期化の状態（設定なし・UNINIT・TX エラー数 0）へ戻したうえで、Can_Init() に渡す設定（config）を用意する。Can_Init() は各テストが呼ぶ。 */
class Can_Write_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeEcuM_Reset();
        FakeDetHw_Reset();
        Can_Test_SetConfigPtr(NULL);  // 初期化前状態に戻す
        Can_Test_SetControllerState(CAN_CS_UNINIT);  // 初期化前状態に戻す
        Can_Test_ResetTxErrCount();  // Can_Init() ではリセットされないため明示的に戻す

        config.filter.filterId = 0x0220U;
        config.filter.mask     = 0x1FFFU;
        config.csPin           = 10U;
        config.intPin          = 2U;
        config.baudrate        = 500000U;
        config.crystalFreq     = CAN_CRYSTAL_16MHZ;
        config.fdEnabled       = 0U;  // 既定はクラシック CAN（CAN FD のテストは自分で 1 にする）
    }
    Can_ConfigType config;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Can_Write_Test, Can_Write_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
}

TEST_F(Can_Write_Test, Can_Write_NG_NullPduInfo)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ReturnType ret = Can_Write(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_POINTER);
}

TEST_F(Can_Write_Test, Can_Write_NG_NullPduInfoSdu)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    pdu.sdu = NULL;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_POINTER);
}

TEST_F(Can_Write_Test, Can_Write_NG_LengthExceeds)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 9U;  // 8 バイトを超える
    uint8_t sdu[9] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_DLC);
}

TEST_F(Can_Write_Test, Can_Write_OK_FdEnabledAcceptsUpTo64Bytes)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    config.fdEnabled = 1U;
    Can_Init(&config);
    Can_Test_SetControllerState(CAN_CS_STARTED);

    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 64U;  // CAN FD の最大データ長
    uint8_t sdu[64] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_NE(FakeDetHw_LastErrorId, CAN_E_PARAM_DLC);
    EXPECT_EQ(ret, CAN_OK);
}

TEST_F(Can_Write_Test, Can_Write_NG_FdEnabledRejectsOver64Bytes)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    config.fdEnabled = 1U;
    Can_Init(&config);

    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 65U;  // CAN FD の最大データ長を超える
    uint8_t sdu[65] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_DLC);
}

TEST_F(Can_Write_Test, Can_Write_OK_FdEnabledRoundsUpInvalidLengthAndPads)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    config.fdEnabled = 1U;
    Can_Init(&config);
    Can_Test_SetControllerState(CAN_CS_STARTED);

    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 13U;  // CAN FD で有効でない長さ（次の有効な長さは 16）
    uint8_t sdu[13];
    for (uint8_t b = 0U; b < 13U; b++)
        sdu[b] = (uint8_t)(0x10U + b);
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 16U);                 // 16 byte へ切り上げて送る
    EXPECT_EQ(FakeCanHw_LastSendData[12], 0x1CU);          // 元のデータの末尾
    EXPECT_EQ(FakeCanHw_LastSendData[13], CAN_FD_PADDING_VALUE);  // 足りない部分は埋める
    EXPECT_EQ(FakeCanHw_LastSendData[15], CAN_FD_PADDING_VALUE);
}

TEST_F(Can_Write_Test, Can_Write_OK_FdDisabledDoesNotPad)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);   // fdEnabled=0（クラシック CAN）
    Can_Test_SetControllerState(CAN_CS_STARTED);

    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 5U;
    uint8_t sdu[5] = {1, 2, 3, 4, 5};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 5U);   // クラシック CAN は、長さをそのまま送る
}

TEST_F(Can_Write_Test, Can_Write_NG_notStarted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    // Can_SetControllerMode(0U, CAN_T_START);  // コントローラを START しない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_NOT_OK);
}

TEST_F(Can_Write_Test, Can_Write_NG_HwSendFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    // MCP2515 の送信を強制的に失敗させるため、Can_Hw_Send をモック化する
    // （ここでは簡略化のため、Can_Hw_Send が常に CAN_HW_FAIL を返すようにする）
    FakeCanHw_SendReturn = CAN_HW_FAIL;
    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetTxErrCount(), 1U);
    EXPECT_EQ(CallCount_CanIf_ControllerBusOff, 0U);  // 閾値未満のため BusOff 通知はまだ来ない
}

TEST_F(Can_Write_Test, Can_Write_NG_HwSendFails_ReachesBusOffThreshold)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;
    FakeCanHw_SendReturn = CAN_HW_FAIL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // CAN_BUSOFF_TX_ERR_THRESHOLD (5) 回連続で送信失敗させる
    Can_ReturnType ret = CAN_OK;
    for (int i = 0; i < 5; i++)
    {
        ret = Can_Write(0U, &pdu);
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetTxErrCount(), 0U);  // 閾値到達時に 0 へリセットされる（Can.c 実装参照）
    EXPECT_EQ(CallCount_CanIf_ControllerBusOff, 1U);
    EXPECT_EQ(LastControllerId_CanIf_ControllerBusOff, 0U);
}

TEST_F(Can_Write_Test, Can_Write_OK_ResetsTxErrCountAfterPriorFailure)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    FakeCanHw_SendReturn = CAN_HW_FAIL;
    Can_Write(0U, &pdu);
    ASSERT_EQ(Can_Test_GetTxErrCount(), 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeCanHw_SendReturn = CAN_HW_OK;
    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(Can_Test_GetTxErrCount(), 0U);  // 成功で 0 にリセットされる
}

}  // namespace
