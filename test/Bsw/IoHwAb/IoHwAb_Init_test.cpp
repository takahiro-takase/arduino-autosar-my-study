/**
 * \file    IoHwAb_Init_test.cpp
 * \brief   IoHwAb_Init() の単体テスト
 * \details 3 つの LED を消灯し、ボタン・ADC の確定値を初期値（解放・0 mV）に戻す。ADC の結果バッファは Adc_SetupResultBuffer() で
 *          登録する（IoHwAb_MainFunction() が変換できる状態になる。IoHwAb_AdcLowVoltage_test.cpp 参照）。
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

/* IoHwAb_Init() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Dio_Hw・Adc_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Adc_Init(NULL)・IoHwAb_Init(NULL) を
 * EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。 */
class IoHwAb_Init_Test : public ::testing::Test
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

TEST_F(IoHwAb_Init_Test, IoHwAb_Init_OK_TurnsAllLedsOffAndStartsWithDefaults)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dio_WriteChannel(DIO_CHANNEL_LED_RUNNING, DIO_HIGH);
    Dio_WriteChannel(DIO_CHANNEL_LED_FAULT, DIO_HIGH);
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_HIGH);
    const uint32 writesBefore = FakeDioHw_WriteCount;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    IoHwAb_Init(NULL);
    uint8 level = 0xFFU;
    uint16 mv = 0xFFFFU;
    const Std_ReturnType retButton = IoHwAb_Button_GetLevel(&level);
    const Std_ReturnType retAdc = IoHwAb_Adc_GetValue_mV(&mv);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_RUNNING), DIO_LOW);   // 3 灯とも消灯状態で起動する
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_FAULT), DIO_LOW);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_WARNING), DIO_LOW);
    EXPECT_EQ(FakeDioHw_WriteCount - writesBefore, 3U);
    EXPECT_EQ(retButton, E_OK);
    EXPECT_EQ(level, 0U);                                           // ボタンは解放
    EXPECT_EQ(retAdc, E_OK);
    EXPECT_EQ(mv, 0U);                                              // ADC はまだ読んでいない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
