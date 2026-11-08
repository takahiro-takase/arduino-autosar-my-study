/**
 * \file    Can_SetControllerMode_test.cpp
 * \brief   Can_SetControllerMode() の単体テスト
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
class Can_SetControllerMode_Test : public ::testing::Test
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

TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_OK_StartedDiscardsOverflowFromListenOnly)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STOPPED), E_OK);   // Listen-Only（起床の検証中など）
    FakeCanHw_TakeRxOverflowReturn = CAN_HW_OK;                   // Listen-Only の間に、受信バッファがあふれていた
    FakeCanHw_TakeRxOverflowCount  = 0U;
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeCanHw_TakeRxOverflowCount, 1U);   // STARTED へ変える時点で、あふれの記録を捨てる
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);           // 捨てるだけで、CAN_E_DATALOST は報告しない
}

TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_OK_StartedToStartedKeepsOverflow)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
    FakeCanHw_TakeRxOverflowCount = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeCanHw_TakeRxOverflowCount, 0U);   // すでに STARTED なら、本当の欠落の記録を捨てない
}

TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_SetControllerMode(0U, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
}

TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_NG_InvalidController)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_SetControllerMode(1U, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_CONTROLLER);
}

TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_NG_Start_On_Sleep)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_CS_SLEEP);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_SetControllerMode(0U, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_TRANSITION);
}

TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_OK_StopFromSleepWakesUp)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_SetControllerMode(0U, CAN_CS_STOPPED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // SLEEP → STOPPED はウェイクアップ（旧 CAN_T_WAKEUP）。受信専用モードへ戻る
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    EXPECT_EQ(FakeCanHw_LastMode, CAN_HW_MODE_LISTEN_ONLY);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_OK_StopFromStarted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_SetControllerMode(0U, CAN_CS_STOPPED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    EXPECT_EQ(FakeCanHw_LastMode, CAN_HW_MODE_LISTEN_ONLY);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_NG_UninitAsTarget)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_SetControllerMode(0U, CAN_CS_UNINIT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_TRANSITION);
}

#if 0 // do not test this case 
TEST_F(Can_SetControllerMode_Test, Can_SetControllerMode_NG_OtherTransition)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_SetControllerMode(0U, (Can_ControllerStateType)0xFF);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
}
#endif

}  // namespace
