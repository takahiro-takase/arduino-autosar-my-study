/**
 * \file    Can.c
 * \brief   CAN ドライバ (AUTOSAR SWS_Can 準拠)
 * \file    Bsw_Can_test.cpp
 * \brief   Can.c（src/Bsw/Can/Can.c）の単体テスト（GoogleTest / PlatformIO native環境）
 * \details Can.c は実 HW 依存の無い自己完結したロジックのため、フェイク
 *          実装は不要で公開 API (Can_Write/Can_Read とその Init) を
 *          直接呼んで検証する。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Cfg.h"
#include "Can_Hw.h"
#include "Hal_Can_Hw_fake.h"
#include "Bsw_CanIf_fake.h"
}

namespace
{

class Bsw_Can_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        FakeCanIf_Reset();
        Can_Test_SetConfigPtr(NULL);  // 初期化前状態に戻す
        Can_Test_SetControllerState(CAN_CS_UNINIT);  // 初期化前状態に戻す
        Can_Test_ResetTxErrCount();  // Can_Init() ではリセットされないため明示的に戻す

        config.filter.filterId = 0x0220U;
        config.filter.mask     = 0x1FFFU;
        config.csPin           = 10U;
        config.intPin          = 2U;
        config.baudrate        = 500000U;
        config.crystalFreq     = CAN_CRYSTAL_16MHZ;
    }
    Can_ConfigType config;
};

// ------------------------------------------------------------
// Can_Init() の単体テスト
// ------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_Init_NG_NullConfig)
{
    /* 準備 (Arrange) */

    /* 実行 (Act) */
    Can_Init(NULL);

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
}

TEST_F(Bsw_Can_Test, Can_Init_Ok)
{
    /* 準備 (Arrange) */

    /* 実行 (Act) */
    Can_Init(&config);

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
}

// ------------------------------------------------------------
// Can_SetControllerMode() の単体テスト
// ------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_SetControllerMode_NG_NullConfig)
{
    /* 準備 (Arrange) */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, CAN_T_START);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
}

TEST_F(Bsw_Can_Test, Can_SetControllerMode_NG_Start)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(1U, CAN_T_START);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
}

TEST_F(Bsw_Can_Test, Can_SetControllerMode_OK_Start)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, CAN_T_START);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
}

TEST_F(Bsw_Can_Test, Can_SetControllerMode_NG_Start_On_Sleep)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, CAN_T_START);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

TEST_F(Bsw_Can_Test, Can_SetControllerMode_OK_Stop)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, CAN_T_STOP);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
}

TEST_F(Bsw_Can_Test, Can_SetControllerMode_NG_Stop_On_Sleep)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, CAN_T_STOP);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

TEST_F(Bsw_Can_Test, Can_SetControllerMode_OK_Wakeup_On_Sleep)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, CAN_T_WAKEUP);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
}

TEST_F(Bsw_Can_Test, Can_SetControllerMode_NG_Wakeup)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, CAN_T_WAKEUP);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
}

TEST_F(Bsw_Can_Test, Can_SetControllerMode_OK_Sleep)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, CAN_T_SLEEP);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

#if 0 // do not test this case 
TEST_F(Bsw_Can_Test, Can_SetControllerMode_NG_OtherTransition)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_SetControllerMode(0U, (Can_ControllerStateType)0xFF);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
}
#endif

// ------------------------------------------------------------
// Can_DisableControllerInterrupts()/Can_EnableControllerInterrupts() の
// 単体テスト（[SWS_Can_00202] のネストカウント仕様を検証する）
// ------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_DisableControllerInterrupts_NG_NullConfig)
{
    Can_DisableControllerInterrupts(0U);

    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 0U);
}

TEST_F(Bsw_Can_Test, Can_DisableControllerInterrupts_NG_InvalidController)
{
    Can_Init(&config);

    Can_DisableControllerInterrupts(1U);

    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 0U);
}

TEST_F(Bsw_Can_Test, Can_DisableControllerInterrupts_OK_FirstCallDisablesHw)
{
    Can_Init(&config);

    Can_DisableControllerInterrupts(0U);

    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 1U);
}

TEST_F(Bsw_Can_Test, Can_DisableControllerInterrupts_OK_NestedCallOnlyDisablesHwOnce)
{
    Can_Init(&config);

    Can_DisableControllerInterrupts(0U);
    Can_DisableControllerInterrupts(0U);

    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 1U);
}

TEST_F(Bsw_Can_Test, Can_EnableControllerInterrupts_NG_NullConfig)
{
    Can_EnableControllerInterrupts(0U);

    EXPECT_EQ(FakeCanHw_EnableRxIsrCount, 0U);
}

TEST_F(Bsw_Can_Test, Can_EnableControllerInterrupts_NG_InvalidController)
{
    Can_Init(&config);
    Can_DisableControllerInterrupts(0U);

    Can_EnableControllerInterrupts(1U);

    EXPECT_EQ(FakeCanHw_EnableRxIsrCount, 0U);
}

TEST_F(Bsw_Can_Test, Can_EnableControllerInterrupts_NG_UnmatchedCallIsNoOp)
{
    /* 対応する Disable が無い状態で Enable を呼んでも、ネストカウンタを
     * 0 未満へアンダーフローさせず、HW も一切呼ばない ([SWS_Can_00202])。 */
    Can_Init(&config);

    Can_EnableControllerInterrupts(0U);

    EXPECT_EQ(FakeCanHw_EnableRxIsrCount, 0U);
}

TEST_F(Bsw_Can_Test, Can_EnableControllerInterrupts_OK_OnlyReEnablesHwAfterMatchingDisableCount)
{
    Can_Init(&config);
    Can_DisableControllerInterrupts(0U);
    Can_DisableControllerInterrupts(0U);  // ネスト2重

    Can_EnableControllerInterrupts(0U);   // 1回目: まだ再有効化しない
    EXPECT_EQ(FakeCanHw_EnableRxIsrCount, 0U);

    Can_EnableControllerInterrupts(0U);   // 2回目: ここで再有効化
    EXPECT_EQ(FakeCanHw_EnableRxIsrCount, 1U);
}

// ------------------------------------------------------------
// Can_Write() の単体テスト
// ------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_Write_NG_NullConfig)
{
    /* 準備 (Arrange) */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* 実行 (Act) */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
}

TEST_F(Bsw_Can_Test, Can_Write_NG_NullPduInfo)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Can_ReturnType ret = Can_Write(0U, NULL);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
}

TEST_F(Bsw_Can_Test, Can_Write_NG_NullPduInfoSdu)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    pdu.sdu = NULL;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
}

TEST_F(Bsw_Can_Test, Can_Write_NG_LengthExceeds)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 9U;  // 8 バイトを超える
    uint8_t sdu[9] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
}

TEST_F(Bsw_Can_Test, Can_Write_NG_notStarted)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    // Can_SetControllerMode(0U, CAN_T_START);  // コントローラを START しない

    /* 実行 (Act) */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
}

TEST_F(Bsw_Can_Test, Can_Write_NG_HwSendFails)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* 実行 (Act) */
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

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetTxErrCount(), 1U);
    EXPECT_EQ(FakeCanIf_ControllerBusOffCount, 0U);  // 閾値未満のため BusOff 通知はまだ来ない
}

TEST_F(Bsw_Can_Test, Can_Write_NG_HwSendFails_ReachesBusOffThreshold)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;
    FakeCanHw_SendReturn = CAN_HW_FAIL;

    /* 実行 (Act): CAN_BUSOFF_TX_ERR_THRESHOLD (5) 回連続で送信失敗させる */
    Can_ReturnType ret = CAN_OK;
    for (int i = 0; i < 5; i++)
    {
        ret = Can_Write(0U, &pdu);
    }

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_NOT_OK);
    EXPECT_EQ(Can_Test_GetTxErrCount(), 0U);  // 閾値到達時に 0 へリセットされる（Can.c 実装参照）
    EXPECT_EQ(FakeCanIf_ControllerBusOffCount, 1U);
    EXPECT_EQ(FakeCanIf_LastControllerBusOffId, 0U);
}

TEST_F(Bsw_Can_Test, Can_Write_OK)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* 実行 (Act) */
    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 0U;

    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x123U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
}

TEST_F(Bsw_Can_Test, Can_Write_OK_ResetsTxErrCountAfterPriorFailure)
{
    /* 準備 (Arrange) */
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

    /* 実行 (Act) */
    FakeCanHw_SendReturn = CAN_HW_OK;
    Can_ReturnType ret = Can_Write(0U, &pdu);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, CAN_OK);
    EXPECT_EQ(Can_Test_GetTxErrCount(), 0U);  // 成功で 0 にリセットされる
}

//------------------------------------------------------------
// Can_MainFunction_Write() の単体テスト
//------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_MainFunction_Write_NG_NullConfig)
{
    /* 準備 (Arrange) */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* 実行 (Act) */
    Can_MainFunction_Write();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
}

TEST_F(Bsw_Can_Test, Can_MainFunction_Write_OK)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    Can_PduType pdu;
    pdu.id = 0x123U;
    pdu.length = 8U;
    uint8_t sdu[8] = {0};
    pdu.sdu = sdu;
    pdu.swPduHandle = 42U;

    Can_Write(0U, &pdu);
    ASSERT_EQ(FakeCanIf_TxConfirmationCount, 0U);  // Can_Write 単体ではまだ通知されない

    /* 実行 (Act) */
    Can_MainFunction_Write();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanIf_TxConfirmationCount, 1U);
    EXPECT_EQ(FakeCanIf_LastTxConfirmationPduId, 42U);
}

//------------------------------------------------------------
// Can_MainFunction_Read() の単体テスト
//------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_MainFunction_Read_NG_NullConfig)
{
    /* 準備 (Arrange) */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
}

TEST_F(Bsw_Can_Test, Can_MainFunction_Read_NG_On_Sleep)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

TEST_F(Bsw_Can_Test, Can_MainFunction_Read_NG_Can_Hw_CheckReceive_Fails)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* 実行 (Act) */
    FakeCanHw_RxPendingCount = 0U;  // 受信フレームがない状態にする
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanIf_RxIndicationCount, 0U);  // 受信なしなので上位層通知もされない
}

TEST_F(Bsw_Can_Test, Can_MainFunction_Read_OK)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    FakeCanHw_RxPendingCount = 1U;  // 受信フレーム 1 件を模擬
    FakeCanHw_RxId  = 0x100U;
    FakeCanHw_RxDlc = 4U;
    FakeCanHw_RxData[0] = 0xDEU;
    FakeCanHw_RxData[1] = 0xADU;
    FakeCanHw_RxData[2] = 0xBEU;
    FakeCanHw_RxData[3] = 0xEFU;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanHw_RxPendingCount, 0U);  // ドレインし尽くしたこと
    EXPECT_EQ(FakeCanIf_RxIndicationCount, 1U);
    EXPECT_EQ(FakeCanIf_LastRxMailbox.CanId, 0x100U);
    EXPECT_EQ(FakeCanIf_LastRxLength, 4U);
    EXPECT_EQ(FakeCanIf_LastRxData[0], 0xDEU);
    EXPECT_EQ(FakeCanIf_LastRxData[1], 0xADU);
    EXPECT_EQ(FakeCanIf_LastRxData[2], 0xBEU);
    EXPECT_EQ(FakeCanIf_LastRxData[3], 0xEFU);
}

//------------------------------------------------------------
// Can_MainFunction_Wakeup() の単体テスト
//------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_MainFunction_Wakeup_NG_NullConfig)
{
    /* 準備 (Arrange) */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* 実行 (Act) */
    Can_MainFunction_Wakeup();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
}

TEST_F(Bsw_Can_Test, Can_MainFunction_Wakeup_NG_NotOnSleep)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* 実行 (Act) */
    Can_MainFunction_Wakeup();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
}

TEST_F(Bsw_Can_Test, Can_MainFunction_Wakeup_OK)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);
    FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;  // INT ピンでウェイクアップ要因を検出させる

    /* 実行 (Act) */
    Can_MainFunction_Wakeup();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);  // 遷移自体は CanSM が行うため状態は変化しない
    EXPECT_EQ(FakeCanIf_ControllerWakeupCount, 1U);
    EXPECT_EQ(FakeCanIf_LastControllerWakeupId, 0U);
}

TEST_F(Bsw_Can_Test, Can_MainFunction_Wakeup_NG_NoWakeupDetected)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);
    // FakeCanHw_IsWakeupPendingReturn は既定 CAN_HW_FAIL のまま（ウェイクアップ要因なし）

    /* 実行 (Act) */
    Can_MainFunction_Wakeup();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeCanIf_ControllerWakeupCount, 0U);
}

//------------------------------------------------------------
// Can_MainFunction_BusOff() の単体テスト
//------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_MainFunction_BusOff_NG_NullConfig)
{
    /* 準備 (Arrange) */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* 実行 (Act) */
    Can_MainFunction_BusOff();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
}

TEST_F(Bsw_Can_Test, Can_MainFunction_BusOff_NG_NotOnStarted)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);

    /* 実行 (Act) */
    Can_MainFunction_BusOff();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP); 
}

TEST_F(Bsw_Can_Test, Can_MainFunction_BusOff_OK)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);
    // Can_Hw_IsBusOff が Bus-Off 検出（CAN_HW_OK）を返すようにする
    FakeCanHw_IsBusOffReturn = CAN_HW_OK;

    /* 実行 (Act) */
    Can_MainFunction_BusOff();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);  // 遷移自体は CanSM が行うため状態は変化しない
    EXPECT_EQ(FakeCanIf_ControllerBusOffCount, 1U);
    EXPECT_EQ(FakeCanIf_LastControllerBusOffId, 0U);
}

TEST_F(Bsw_Can_Test, Can_MainFunction_BusOff_NG_NoBusOffDetected)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);
    // FakeCanHw_IsBusOffReturn は既定 CAN_HW_FAIL のまま（Bus-Off 未検出）

    /* 実行 (Act) */
    Can_MainFunction_BusOff();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanIf_ControllerBusOffCount, 0U);
}

//------------------------------------------------------------
// Can_GetVersionInfo() の単体テスト
//------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_GetVersionInfo_NG_NullPointer)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Can_GetVersionInfo(NULL);

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
}

TEST_F(Bsw_Can_Test, Can_GetVersionInfo_OK)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Std_VersionInfoType versioninfo;
    Can_GetVersionInfo(&versioninfo);

    /* 評価 (Assert) */
    EXPECT_EQ(versioninfo.vendorID, CAN_VENDOR_ID);
    EXPECT_EQ(versioninfo.moduleID, CAN_MODULE_ID);
    EXPECT_EQ(versioninfo.sw_major_version, CAN_SW_MAJOR_VERSION);
    EXPECT_EQ(versioninfo.sw_minor_version, CAN_SW_MINOR_VERSION);
    EXPECT_EQ(versioninfo.sw_patch_version, CAN_SW_PATCH_VERSION);
}

// ------------------------------------------------------------
// Can_GetControllerErrorState() の単体テスト（AUTOSAR 非標準の拡張。
// Can.c の Can_GetControllerErrorState() ドキュメント参照）
// ------------------------------------------------------------
TEST_F(Bsw_Can_Test, Can_GetControllerErrorState_NG_NullConfig)
{
    /* 実行 (Act) */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Bsw_Can_Test, Can_GetControllerErrorState_NG_InvalidController)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(1U, &state);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Bsw_Can_Test, Can_GetControllerErrorState_NG_NullPointer)
{
    /* 準備 (Arrange) */
    Can_Init(&config);

    /* 実行 (Act) */
    Std_ReturnType ret = Can_GetControllerErrorState(0U, NULL);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Bsw_Can_Test, Can_GetControllerErrorState_OK_ReturnsActiveByDefault)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    FakeCanHw_ErrorState = 0U;  /* CAN_ERRORSTATE_ACTIVE */

    /* 実行 (Act) */
    Can_ErrorStateType state = CAN_ERRORSTATE_BUSOFF;  /* 未更新を検出できる初期値 */
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CAN_ERRORSTATE_ACTIVE);
}

TEST_F(Bsw_Can_Test, Can_GetControllerErrorState_OK_ReflectsPassiveFromHw)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    FakeCanHw_ErrorState = 1U;  /* CAN_ERRORSTATE_PASSIVE */

    /* 実行 (Act) */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CAN_ERRORSTATE_PASSIVE);
}

TEST_F(Bsw_Can_Test, Can_GetControllerErrorState_OK_ReflectsBusOffFromHw)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    FakeCanHw_ErrorState = 2U;  /* CAN_ERRORSTATE_BUSOFF */

    /* 実行 (Act) */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CAN_ERRORSTATE_BUSOFF);
}

TEST_F(Bsw_Can_Test, Can_GetControllerErrorState_NG_HwFailurePropagates)
{
    /* 準備 (Arrange) */
    Can_Init(&config);
    FakeCanHw_GetErrorStateReturn = CAN_HW_FAIL;

    /* 実行 (Act) */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_NOT_OK);
}

}  // namespace