/**
 * \file    Can_MainFunction_Write_test.cpp
 * \brief   Can_MainFunction_Write() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Can_test.cpp）。
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
class Can_MainFunction_Write_Test : public ::testing::Test
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
    }
    Can_ConfigType config;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Can_MainFunction_Write_Test, Can_MainFunction_Write_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Write();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
}

TEST_F(Can_MainFunction_Write_Test, Can_MainFunction_Write_OK)
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
    pdu.swPduHandle = 42U;

    Can_Write(0U, &pdu);
    ASSERT_EQ(CallCount_CanIf_TxConfirmation, 0U);  // Can_Write 単体ではまだ通知されない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Write();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(CallCount_CanIf_TxConfirmation, 1U);
    EXPECT_EQ(LastPduId_CanIf_TxConfirmation, 42U);
}

TEST_F(Can_MainFunction_Write_Test, Can_MainFunction_Write_OK_BurstOfDiagResponsesAllConfirmed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* 周期フレーム（TX PDU 数）に加えて、診断応答（同じ PDU）が 8 件続いた状態を想定する。
     * 実機では、診断要求 5 件の連続処理中に確認が 7 件たまり、旧サイズ（TX PDU 数）では取りこぼした。 */
    const uint8_t burst = (uint8_t)(CANIF_TX_PDU_COUNT + 8U);
    uint8_t sdu[8] = {0};
    Can_PduType pdu;
    pdu.id = 0x7E8U;
    pdu.length = 8U;
    pdu.sdu = sdu;

    for (uint8_t i = 0U; i < burst; i++)
    {
        pdu.swPduHandle = (PduIdType)(i % 2U);
        Can_Write(0U, &pdu);
    }
    ASSERT_EQ(CallCount_CanIf_TxConfirmation, 0U);  // Can_Write 単体ではまだ通知されない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Write();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_TxConfirmation, (uint32_t)burst);  // 1 件も取りこぼさない
}

}  // namespace
