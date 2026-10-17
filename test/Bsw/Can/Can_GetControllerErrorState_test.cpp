/**
 * \file    Can_GetControllerErrorState_test.cpp
 * \brief   Can_GetControllerErrorState() の単体テスト
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
class Can_GetControllerErrorState_Test : public ::testing::Test
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

// ------------------------------------------------------------
// Can_GetControllerErrorState() の単体テスト（AUTOSAR 非標準の拡張。
// Can.c の Can_GetControllerErrorState() ドキュメント参照）
// ------------------------------------------------------------
TEST_F(Can_GetControllerErrorState_Test, Can_GetControllerErrorState_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（Can_Init() を呼ばない未初期化の状態）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
}

TEST_F(Can_GetControllerErrorState_Test, Can_GetControllerErrorState_NG_InvalidController)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(1U, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_CONTROLLER);
}

TEST_F(Can_GetControllerErrorState_Test, Can_GetControllerErrorState_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Can_GetControllerErrorState(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_POINTER);
}

TEST_F(Can_GetControllerErrorState_Test, Can_GetControllerErrorState_NG_HwFailurePropagates)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    FakeCanHw_GetErrorStateReturn = CAN_HW_FAIL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Can_GetControllerErrorState_Test, Can_GetControllerErrorState_OK_ReturnsActiveByDefault)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    FakeCanHw_ErrorState = 0U;  /* CAN_ERRORSTATE_ACTIVE */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ErrorStateType state = CAN_ERRORSTATE_BUSOFF;  /* 未更新を検出できる初期値 */
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CAN_ERRORSTATE_ACTIVE);
}

TEST_F(Can_GetControllerErrorState_Test, Can_GetControllerErrorState_OK_ReflectsPassiveFromHw)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    FakeCanHw_ErrorState = 1U;  /* CAN_ERRORSTATE_PASSIVE */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CAN_ERRORSTATE_PASSIVE);
}

TEST_F(Can_GetControllerErrorState_Test, Can_GetControllerErrorState_OK_ReflectsBusOffFromHw)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    FakeCanHw_ErrorState = 2U;  /* CAN_ERRORSTATE_BUSOFF */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_ErrorStateType state;
    Std_ReturnType ret = Can_GetControllerErrorState(0U, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CAN_ERRORSTATE_BUSOFF);
}

}  // namespace
