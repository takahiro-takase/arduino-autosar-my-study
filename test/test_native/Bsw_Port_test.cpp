/**
 * \file    Bsw_Port_test.cpp
 * \brief   Port.c（src/Bsw/Port/Port.c）の単体テスト
 * \details AUTOSAR SWS_Port_00060/00061 が規定する「全ピンの方向を設定方向へ
 *          再適用する」挙動、および SWS_Port_00223 が規定する
 *          Port_SetPinMode() の PORT_E_MODE_UNCHANGEABLE 報告を検証する。
 *          Port.c 自体は実物をリンクし、実 HW 依存の Port_Hw.cpp のみを
 *          Hal_Port_Hw_fake.c（ピンごとに直近の設定方向を記録する簡易メモリ
 *          モデル）に差し替える。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Port.h"
#include "Hal_Port_Hw_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

class PortTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakePortHw_Reset();
        FakeDetHw_Reset();
    }
};

TEST_F(PortTest, RefreshPortDirectionReappliesAllConfiguredPinDirections)
{
    Port_Init(NULL);
    FakePortHw_Reset();  /* Port_Init 自身の適用呼び出しを後続の検証対象から除く */

    Port_RefreshPortDirection();

    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_LED_RUNNING), PORT_PIN_OUT);
    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_LED_FAULT),   PORT_PIN_OUT);
    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_LED_WARNING), PORT_PIN_OUT);
    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_BUTTON),      PORT_PIN_IN_PULLUP);
    EXPECT_EQ(FakePortHw_SetPinDirectionCount, PORT_PIN_COUNT);
}

TEST_F(PortTest, RefreshPortDirectionOverridesDirectionChangedByRuntimeApi)
{
    Port_Init(NULL);
    Port_SetPinDirection(PORT_PIN_LED_RUNNING, PORT_PIN_IN);  /* 実行時に方向を変更してしまった状態を模擬 */

    Port_RefreshPortDirection();

    EXPECT_EQ(FakePortHw_GetLastDirection(PORT_PIN_LED_RUNNING), PORT_PIN_OUT);
}

TEST_F(PortTest, SetPinModeAlwaysReportsModeUnchangeableAndHasNoEffect)
{
    Port_Init(NULL);
    FakePortHw_Reset();  /* Port_Init 自身の適用呼び出しを後続の検証対象から除く */

    Port_SetPinMode(PORT_PIN_LED_RUNNING, 0U);

    EXPECT_EQ(FakeDetHw_LastErrorId, PORT_E_MODE_UNCHANGEABLE);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
    /* [SWS_Port_00223]: エラー報告以外は何もしない（ピン方向は変化しない）。 */
    EXPECT_EQ(FakePortHw_SetPinDirectionCount, 0U);
}

}  // namespace
