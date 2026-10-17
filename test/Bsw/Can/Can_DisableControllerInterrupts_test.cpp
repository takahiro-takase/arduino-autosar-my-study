/**
 * \file    Can_DisableControllerInterrupts_test.cpp
 * \brief   Can_DisableControllerInterrupts() の単体テスト
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
class Can_DisableControllerInterrupts_Test : public ::testing::Test
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
// Can_DisableControllerInterrupts()/Can_EnableControllerInterrupts() の
// 単体テスト（[SWS_Can_00202] のネストカウント仕様を検証する）
// ------------------------------------------------------------
TEST_F(Can_DisableControllerInterrupts_Test, Can_DisableControllerInterrupts_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DisableControllerInterrupts(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
}

TEST_F(Can_DisableControllerInterrupts_Test, Can_DisableControllerInterrupts_NG_InvalidController)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DisableControllerInterrupts(1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_CONTROLLER);
}

TEST_F(Can_DisableControllerInterrupts_Test, Can_DisableControllerInterrupts_OK_FirstCallDisablesHw)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DisableControllerInterrupts(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 1U);
}

TEST_F(Can_DisableControllerInterrupts_Test, Can_DisableControllerInterrupts_OK_NestedCallOnlyDisablesHwOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_DisableControllerInterrupts(0U);
    Can_DisableControllerInterrupts(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeCanHw_DisableRxIsrCount, 1U);
}

}  // namespace
