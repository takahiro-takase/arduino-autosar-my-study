/**
 * \file    IoHwAb_LedSetLevel_test.cpp
 * \brief   IoHwAb の LED 出力（IoHwAb_Led_SetLevel / IoHwAb_LedRunning_SetLevel / IoHwAb_LedFault_SetLevel）の単体テスト
 * \details 各関数が、対応する Dio のチャネル（WARNING / RUNNING / FAULT の LED）へ、レベル（1=点灯、0=消灯）を書く。
 *          3 関数は同じ形の薄い委譲のため、1 ファイルにまとめている。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "IoHwAb.h"
#include "Dio.h"
#include "Dio_Cfg.h"
#include "Adc.h"
#include "Adc_Cfg.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Fake_Dio_Hw.h"
#include "Fake_Adc_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

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

/* IoHwAb の LED 出力 の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Dio_Hw・Adc_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Adc_Init(NULL)・IoHwAb_Init(NULL) を
 * EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。 */
class IoHwAb_LedSetLevel_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDioHw_Reset();
        FakeAdcHw_Reset();
        FakeDetHw_Reset();
        WrapDem_Reset();
        Dem_Init(NULL);
        Adc_Init(NULL);
        IoHwAb_Init(NULL);
        WrapDem_Reset();
        FakeDetHw_Reset();
    }

    /* ボタンの生レベルを設定する（押下 = DIO_LOW、IoHwAb は論理反転して 押下=1 にする）。 */
    static void SetButtonPressed(bool pressed)
    {
        Dio_WriteChannel(DIO_CHANNEL_BUTTON, pressed ? DIO_LOW : DIO_HIGH);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(IoHwAb_LedSetLevel_Test, IoHwAb_Led_SetLevel_OK_WritesLevelToDioChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_WARNING), DIO_LOW);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType retHigh = IoHwAb_Led_SetLevel(1U);
    const Dio_LevelType afterHigh = Dio_ReadChannel(DIO_CHANNEL_LED_WARNING);
    const Std_ReturnType retLow = IoHwAb_Led_SetLevel(0U);
    const Dio_LevelType afterLow = Dio_ReadChannel(DIO_CHANNEL_LED_WARNING);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retHigh, E_OK);
    EXPECT_EQ(afterHigh, DIO_HIGH);   // 1 = 点灯
    EXPECT_EQ(retLow, E_OK);
    EXPECT_EQ(afterLow, DIO_LOW);     // 0 = 消灯
}

TEST_F(IoHwAb_LedSetLevel_Test, IoHwAb_LedRunning_SetLevel_OK_WritesLevelToDioChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_RUNNING), DIO_LOW);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType retHigh = IoHwAb_LedRunning_SetLevel(1U);
    const Dio_LevelType afterHigh = Dio_ReadChannel(DIO_CHANNEL_LED_RUNNING);
    const Std_ReturnType retLow = IoHwAb_LedRunning_SetLevel(0U);
    const Dio_LevelType afterLow = Dio_ReadChannel(DIO_CHANNEL_LED_RUNNING);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retHigh, E_OK);
    EXPECT_EQ(afterHigh, DIO_HIGH);   // 1 = 点灯
    EXPECT_EQ(retLow, E_OK);
    EXPECT_EQ(afterLow, DIO_LOW);     // 0 = 消灯
}

TEST_F(IoHwAb_LedSetLevel_Test, IoHwAb_LedFault_SetLevel_OK_WritesLevelToDioChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_FAULT), DIO_LOW);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType retHigh = IoHwAb_LedFault_SetLevel(1U);
    const Dio_LevelType afterHigh = Dio_ReadChannel(DIO_CHANNEL_LED_FAULT);
    const Std_ReturnType retLow = IoHwAb_LedFault_SetLevel(0U);
    const Dio_LevelType afterLow = Dio_ReadChannel(DIO_CHANNEL_LED_FAULT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retHigh, E_OK);
    EXPECT_EQ(afterHigh, DIO_HIGH);   // 1 = 点灯
    EXPECT_EQ(retLow, E_OK);
    EXPECT_EQ(afterLow, DIO_LOW);     // 0 = 消灯
}

}  // namespace
