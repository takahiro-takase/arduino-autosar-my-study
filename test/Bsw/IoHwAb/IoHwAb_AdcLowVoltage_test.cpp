/**
 * \file    IoHwAb_AdcLowVoltage_test.cpp
 * \brief   IoHwAb_MainFunction() の ADC 読み取りと電圧低下の検出の単体テスト
 * \details IoHwAb_MainFunction() は、ADC のグループ変換（Adc_StartGroupConversion → Adc_ReadGroup）の結果を、mV へ換算し
 *          （raw * 5000 / 1023）、1000 mV 未満なら ADC_VOLT_LOW を FAILED、以上なら PASSED で Dem へ報告する。
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

/* IoHwAb_MainFunction() の ADC 読み取りと電圧低下の検出 の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Dio_Hw・Adc_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Adc_Init(NULL)・IoHwAb_Init(NULL) を
 * EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。 */
class IoHwAb_AdcLowVoltage_Test : public ::testing::Test
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

TEST_F(IoHwAb_AdcLowVoltage_Test, IoHwAb_MainFunction_OK_ConvertsRawToMvAndReportsLowVoltage)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeAdcHw_ReturnValue = 100U;   // 100 * 5000 / 1023 = 488 mV（しきい値 1000 mV 未満）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    IoHwAb_MainFunction();
    uint16 mvLow = 0U;
    (void)IoHwAb_Adc_GetValue_mV(&mvLow);
    const Dem_EventStatusType statusLow = LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_ADC_VOLT_LOW];

    FakeAdcHw_ReturnValue = 512U;   // 512 * 5000 / 1023 = 2502 mV（しきい値以上）
    IoHwAb_MainFunction();
    uint16 mvOk = 0U;
    (void)IoHwAb_Adc_GetValue_mV(&mvOk);
    const Dem_EventStatusType statusOk = LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_ADC_VOLT_LOW];

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(mvLow, 488U);
    EXPECT_EQ(statusLow, DEM_EVENT_STATUS_FAILED);   // 電圧低下
    EXPECT_EQ(mvOk, 2502U);
    EXPECT_EQ(statusOk, DEM_EVENT_STATUS_PASSED);    // 回復
    EXPECT_EQ(FakeAdcHw_LastChannel, ADC_CHANNEL_SENSOR);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);            // ADC のエラー（バッファ未登録など）が出ていない
}

}  // namespace
