/**
 * \file    Adc_StartGroupConversion_test.cpp
 * \brief   Adc_StartGroupConversion() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Adc_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Adc_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Adc.h"
#include "Adc_Cfg.h"
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

/* Adc の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。Adc_Init() は各テストが必要に応じて呼ぶ。 */
class Adc_StartGroupConversion_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Adc_StartGroupConversion_Test, Adc_StartGroupConversion_NG_BufferUninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_StartGroupConversion(ADC_GROUP_SENSOR);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_BUFFER_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_START_GROUP_CONVERSION);
    EXPECT_EQ(Adc_GetGroupStatus(ADC_GROUP_SENSOR), ADC_IDLE);   // 変換は始まっていない
}

TEST_F(Adc_StartGroupConversion_Test, Adc_StartGroupConversion_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);
    Adc_ValueGroupType buf = 0U;
    ASSERT_EQ(Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &buf), E_OK);
    Adc_DeInit();
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_StartGroupConversion(ADC_GROUP_SENSOR);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_START_GROUP_CONVERSION);
}

TEST_F(Adc_StartGroupConversion_Test, Adc_StartGroupConversion_NG_InvalidGroup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_StartGroupConversion(ADC_GROUP_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_GROUP);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_START_GROUP_CONVERSION);
}

TEST_F(Adc_StartGroupConversion_Test, Adc_StartGroupConversion_NG_Busy)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);
    Adc_ValueGroupType buf = 0U;
    ASSERT_EQ(Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &buf), E_OK);
    FakeDetHw_Reset();
    Adc_StartGroupConversion(ADC_GROUP_SENSOR);   // ADC_STREAM_COMPLETED（結果を読み出していない）
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_StartGroupConversion(ADC_GROUP_SENSOR);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_BUSY);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_START_GROUP_CONVERSION);
    EXPECT_EQ(Adc_GetGroupStatus(ADC_GROUP_SENSOR), ADC_STREAM_COMPLETED);   // 状態は変わらない
}

}  // namespace
