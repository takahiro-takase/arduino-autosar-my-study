/**
 * \file    Adc_ConvertAndRead_test.cpp
 * \brief   ADC のグループ変換（Adc_SetupResultBuffer → Adc_StartGroupConversion → Adc_ReadGroup）の単体テスト
 * \details 結果バッファを登録し、変換を開始すると、Adc_Hw_ReadChannel() の値が結果になり、
 *          グループの状態が ADC_STREAM_COMPLETED を経て、結果の読み出しで ADC_IDLE へ戻ることを確認する
 *          （[SWS_Adc_00330]）。IoHwAb_MainFunction() が 10ms ごとに行う読み取りの手順と同じ。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Adc.h"
#include "Adc_Cfg.h"
#include "Fake_Adc_Hw.h"
#include "Fake_Det_Hw.h"
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

/* ADC のグループ変換の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Adc_Hw と DET の記録を初期化し、Adc_Init(NULL) を呼ぶ。 */
class Adc_ConvertAndRead_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeAdcHw_Reset();
        FakeDetHw_Reset();
        Adc_Init(NULL);
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Adc_ConvertAndRead_Test, Adc_ReadGroup_OK_ReturnsConvertedValueAndGoesIdle)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_ValueGroupType buffer = 0U;
    ASSERT_EQ(Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &buffer), E_OK);
    FakeAdcHw_ReturnValue = 512U;
    Adc_ValueGroupType raw = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Adc_StatusType before = Adc_GetGroupStatus(ADC_GROUP_SENSOR);
    Adc_StartGroupConversion(ADC_GROUP_SENSOR);
    const Adc_StatusType afterStart = Adc_GetGroupStatus(ADC_GROUP_SENSOR);
    const Std_ReturnType ret = Adc_ReadGroup(ADC_GROUP_SENSOR, &raw);
    const Adc_StatusType afterRead = Adc_GetGroupStatus(ADC_GROUP_SENSOR);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(before, ADC_IDLE);
    EXPECT_EQ(afterStart, ADC_STREAM_COMPLETED);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(raw, 512U);
    EXPECT_EQ(afterRead, ADC_IDLE);
    EXPECT_EQ(FakeAdcHw_ReadChannelCount, 1U);
    EXPECT_EQ(FakeAdcHw_LastChannel, ADC_CHANNEL_SENSOR);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
