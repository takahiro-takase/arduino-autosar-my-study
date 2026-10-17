/**
 * \file    Can_MainFunction_Read_test.cpp
 * \brief   Can_MainFunction_Read() の単体テスト
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
class Can_MainFunction_Read_Test : public ::testing::Test
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

TEST_F(Can_MainFunction_Read_Test, Can_MainFunction_Read_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
}

TEST_F(Can_MainFunction_Read_Test, Can_MainFunction_Read_NG_On_Sleep)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

TEST_F(Can_MainFunction_Read_Test, Can_MainFunction_Read_NG_Can_Hw_CheckReceive_Fails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeCanHw_RxPendingCount = 0U;  // 受信フレームがない状態にする
    Can_MainFunction_Read();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(CallCount_CanIf_RxIndication, 0U);  // 受信なしなので上位層通知もされない
}

TEST_F(Can_MainFunction_Read_Test, Can_MainFunction_Read_OK)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    FakeCanHw_RxPendingCount = 1U;  // 受信フレーム 1 件を模擬
    FakeCanHw_RxId  = 0x100U;
    FakeCanHw_RxDlc = 4U;
    FakeCanHw_RxData[0] = 0xDEU;
    FakeCanHw_RxData[1] = 0xADU;
    FakeCanHw_RxData[2] = 0xBEU;
    FakeCanHw_RxData[3] = 0xEFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanHw_RxPendingCount, 0U);  // ドレインし尽くしたこと
    EXPECT_EQ(CallCount_CanIf_RxIndication, 1U);
    EXPECT_EQ(LastMailbox_CanIf_RxIndication.CanId, 0x100U);
    EXPECT_EQ(LastLength_CanIf_RxIndication, 4U);
    EXPECT_EQ(LastData_CanIf_RxIndication[0], 0xDEU);
    EXPECT_EQ(LastData_CanIf_RxIndication[1], 0xADU);
    EXPECT_EQ(LastData_CanIf_RxIndication[2], 0xBEU);
    EXPECT_EQ(LastData_CanIf_RxIndication[3], 0xEFU);
}

}  // namespace
