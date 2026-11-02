/**
 * \file    Can_DeInit_test.cpp
 * \brief   Can_DeInit() の単体テスト
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
class Can_DeInit_Test : public ::testing::Test
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

// Can_DeInit() の単体テスト（[SWS_Can_91002]、[91009]〜[91012]。Can.c の Can_DeInit() ドキュメント参照）
TEST_F(Can_DeInit_Test, Can_DeInit_OK_ReturnsToUninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 1U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Can_DeInit_Test, Can_DeInit_OK_ReinitAfterDeInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    EXPECT_EQ(FakeCanHw_InitCount, 2U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Can_DeInit_Test, Can_DeInit_OK_FromSleep)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Can_DeInit_Test, Can_DeInit_NG_NotInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（Can_Init() を呼ばない未初期化の状態）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_TRANSITION);
    EXPECT_EQ(FakeDetHw_LastApiId, 0x10U);   // [SWS_Can_91002] の Service ID（定数ではなく、仕様の値で確かめる）
    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 0U);
}

TEST_F(Can_DeInit_Test, Can_DeInit_NG_ControllerStarted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_TRANSITION);
    EXPECT_EQ(FakeDetHw_LastApiId, 0x10U);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);   // 状態は変えない
    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 0U);
}


TEST_F(Can_DeInit_Test, Can_DeInit_OK_ClearsTxErrCount)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
    uint8 data[8] = {0U};
    Can_PduType pdu = {1U, 0x100U, 8U, data};
    FakeCanHw_SendReturn = CAN_HW_FAIL;
    ASSERT_EQ(Can_Write(0U, &pdu), E_NOT_OK);
    ASSERT_EQ(Can_Write(0U, &pdu), E_NOT_OK);
    ASSERT_EQ(Can_Test_GetTxErrCount(), 2U);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STOPPED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetTxErrCount(), 0U);
}

}  // namespace
