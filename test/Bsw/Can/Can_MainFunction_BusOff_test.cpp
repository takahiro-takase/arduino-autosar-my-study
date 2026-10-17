/**
 * \file    Can_MainFunction_BusOff_test.cpp
 * \brief   Can_MainFunction_BusOff() の単体テスト
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
class Can_MainFunction_BusOff_Test : public ::testing::Test
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

TEST_F(Can_MainFunction_BusOff_Test, Can_MainFunction_BusOff_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_BusOff();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
}

TEST_F(Can_MainFunction_BusOff_Test, Can_MainFunction_BusOff_NG_NotOnStarted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_BusOff();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP); 
}

TEST_F(Can_MainFunction_BusOff_Test, Can_MainFunction_BusOff_NG_NoBusOffDetected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);
    // FakeCanHw_IsBusOffReturn は既定 CAN_HW_FAIL のまま（Bus-Off 未検出）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_BusOff();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(CallCount_CanIf_ControllerBusOff, 0U);
}

TEST_F(Can_MainFunction_BusOff_Test, Can_MainFunction_BusOff_OK)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);
    // Can_Hw_IsBusOff が Bus-Off 検出（CAN_HW_OK）を返すようにする
    FakeCanHw_IsBusOffReturn = CAN_HW_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_BusOff();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);  // 遷移自体は CanSM が行うため状態は変化しない
    EXPECT_EQ(CallCount_CanIf_ControllerBusOff, 1U);
    EXPECT_EQ(LastControllerId_CanIf_ControllerBusOff, 0U);
}

}  // namespace
