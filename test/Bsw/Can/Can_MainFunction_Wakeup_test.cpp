/**
 * \file    Can_MainFunction_Wakeup_test.cpp
 * \brief   Can_MainFunction_Wakeup() の単体テスト
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
class Can_MainFunction_Wakeup_Test : public ::testing::Test
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

TEST_F(Can_MainFunction_Wakeup_Test, Can_MainFunction_Wakeup_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Wakeup();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_UNINIT);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
}

TEST_F(Can_MainFunction_Wakeup_Test, Can_MainFunction_Wakeup_NG_NotOnSleep)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_START);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Wakeup();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
}

TEST_F(Can_MainFunction_Wakeup_Test, Can_MainFunction_Wakeup_NG_NoWakeupDetected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);
    // FakeCanHw_IsWakeupPendingReturn は既定 CAN_HW_FAIL のまま（ウェイクアップ要因なし）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Wakeup();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeEcuM_CheckWakeupCount, 0U);
}

TEST_F(Can_MainFunction_Wakeup_Test, Can_MainFunction_Wakeup_OK)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);
    FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;  // INT ピンでウェイクアップ要因を検出させる

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Wakeup();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);  // 遷移自体は CanSM が行うため状態は変化しない
    EXPECT_EQ(FakeEcuM_CheckWakeupCount, 1U);
    EXPECT_EQ(FakeEcuM_LastWakeupSource, static_cast<EcuM_WakeupSourceType>(ECUM_WKSOURCE_CAN));
}

TEST_F(Can_MainFunction_Wakeup_Test, Can_MainFunction_Wakeup_OK_IsrFlagWithoutHwIndication)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    Can_SetControllerMode(0U, CAN_T_SLEEP);
    // FakeCanHw_IsWakeupPendingReturn は既定 CAN_HW_FAIL のまま（HW の確認では検出されない）
    ASSERT_NE(FakeCanHw_AttachedIsr, nullptr);
    FakeCanHw_AttachedIsr();   // SLEEP 中の INT ピンの割り込み（ウェイクアップのフラグだけが立つ）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Wakeup();
    const uint32 countAfterFirst = FakeEcuM_CheckWakeupCount;
    Can_MainFunction_Wakeup();   // フラグは読み出したときに消えている

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(countAfterFirst, 1U);                  // HW の確認なしに、フラグだけで EcuM へ通知する
    EXPECT_EQ(FakeEcuM_CheckWakeupCount, 1U);        // 2 回目は通知しない
    EXPECT_EQ(FakeEcuM_LastWakeupSource, static_cast<EcuM_WakeupSourceType>(ECUM_WKSOURCE_CAN));
}

}  // namespace
