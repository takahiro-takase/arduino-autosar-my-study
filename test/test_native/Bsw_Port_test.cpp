/**
 * \file    Bsw_Port_test.cpp
 * \brief   Port.c（src/Bsw/Port/Port.c）の Port_RefreshPortDirection() 単体テスト
 * \details AUTOSAR SWS_Port_00060/00061 が規定する「全ピンの方向を設定方向へ
 *          再適用する」挙動を検証する。Port.c 自体は実物をリンクし、実 HW
 *          依存の Port_Hw.cpp のみを Hal_Port_Hw_fake.c（ピンごとに直近の
 *          設定方向を記録する簡易メモリモデル）に差し替える。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Port.h"
#include "Hal_Port_Hw_fake.h"
}

namespace
{

class PortTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakePortHw_Reset();
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

}  // namespace
