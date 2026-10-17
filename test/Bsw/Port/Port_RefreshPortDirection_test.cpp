/**
 * \file    Port_RefreshPortDirection_test.cpp
 * \brief   Port_RefreshPortDirection() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Port_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Port_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Port.h"
#include "Fake_Port_Hw.h"
#include "Fake_Det_Hw.h"
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

/* Port の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Port_Hw と DET の記録を初期化する。 */
class Port_RefreshPortDirection_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakePortHw_Reset();
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Port_RefreshPortDirection_Test, Port_RefreshPortDirection_OK_ReappliesAllConfiguredPinDirections)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Port_Init(NULL);
    FakePortHw_Reset();  /* Port_Init 自身の適用呼び出しを後続の検証対象から除く */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Port_RefreshPortDirection();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_LED_RUNNING), PORT_PIN_OUT);
    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_LED_FAULT),   PORT_PIN_OUT);
    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_LED_WARNING), PORT_PIN_OUT);
    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_BUTTON),      PORT_PIN_IN_PULLUP);
    EXPECT_EQ(FakePortHw_SetPinDirectionCount, PORT_PIN_COUNT);
}

TEST_F(Port_RefreshPortDirection_Test, Port_RefreshPortDirection_OK_OverridesDirectionChangedByRuntimeApi)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Port_Init(NULL);
    Port_SetPinDirection(PORT_PIN_LED_RUNNING, PORT_PIN_IN);  /* 実行時に方向を変更してしまった状態を模擬 */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Port_RefreshPortDirection();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_LED_RUNNING), PORT_PIN_OUT);
}

}  // namespace
