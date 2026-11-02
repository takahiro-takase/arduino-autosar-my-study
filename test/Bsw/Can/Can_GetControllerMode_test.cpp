/**
 * \file    Can_GetControllerMode_test.cpp
 * \brief   Can_GetControllerMode() の単体テスト
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
class Can_GetControllerMode_Test : public ::testing::Test
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

// Can_GetControllerMode() の単体テスト（[SWS_Can_91014]〜[91018]。Can.c の Can_GetControllerMode() ドキュメント参照）
TEST_F(Can_GetControllerMode_Test, Can_GetControllerMode_OK_Stopped)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_ControllerStateType mode = CAN_CS_UNINIT;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
    EXPECT_EQ(static_cast<int>(mode), 0x02);   // [SWS_Can_91014 等] 仕様の値
}

TEST_F(Can_GetControllerMode_Test, Can_GetControllerMode_OK_Started)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
    Can_ControllerStateType mode = CAN_CS_UNINIT;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, CAN_CS_STARTED);
    EXPECT_EQ(static_cast<int>(mode), 0x01);   // [SWS_Can_91014 等] 仕様の値
}

TEST_F(Can_GetControllerMode_Test, Can_GetControllerMode_OK_Sleep)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);
    Can_ControllerStateType mode = CAN_CS_UNINIT;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, CAN_CS_SLEEP);
    EXPECT_EQ(static_cast<int>(mode), 0x03);
}

TEST_F(Can_GetControllerMode_Test, Can_GetControllerMode_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_ControllerStateType mode = CAN_CS_STARTED;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, 0x12U);   // [SWS_Can_91014] の Service ID
    EXPECT_EQ(mode, CAN_CS_STARTED);   // 書き込まない
}

TEST_F(Can_GetControllerMode_Test, Can_GetControllerMode_NG_InvalidController)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_ControllerStateType mode = CAN_CS_STARTED;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_GetControllerMode(1U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_CONTROLLER);
    EXPECT_EQ(FakeDetHw_LastApiId, 0x12U);
    EXPECT_EQ(mode, CAN_CS_STARTED);
}

TEST_F(Can_GetControllerMode_Test, Can_GetControllerMode_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_GetControllerMode(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_POINTER);
    EXPECT_EQ(FakeDetHw_LastApiId, 0x12U);
}


}  // namespace
