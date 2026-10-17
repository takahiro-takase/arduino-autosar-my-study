/**
 * \file    Adc_ReadGroup_test.cpp
 * \brief   Adc_ReadGroup() の単体テスト
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
class Adc_ReadGroup_Test : public ::testing::Test
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

TEST_F(Adc_ReadGroup_Test, Adc_ReadGroup_NG_Uninit)
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
    Adc_ValueGroupType raw = 0U;
    Std_ReturnType ret = Adc_ReadGroup(ADC_GROUP_SENSOR, &raw);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_READ_GROUP);
}

TEST_F(Adc_ReadGroup_Test, Adc_ReadGroup_NG_InvalidGroup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_ValueGroupType raw = 0U;
    Std_ReturnType ret = Adc_ReadGroup(ADC_GROUP_COUNT, &raw);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_GROUP);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_READ_GROUP);
}

TEST_F(Adc_ReadGroup_Test, Adc_ReadGroup_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Adc_ReadGroup(ADC_GROUP_SENSOR, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_POINTER);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_READ_GROUP);
}

TEST_F(Adc_ReadGroup_Test, Adc_ReadGroup_NG_NoResult)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);
    Adc_ValueGroupType buf = 0U;
    ASSERT_EQ(Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &buf), E_OK);
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_ValueGroupType raw = 0xFFFFU;
    Std_ReturnType ret = Adc_ReadGroup(ADC_GROUP_SENSOR, &raw);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 変換を始めていない（ADC_IDLE）
    EXPECT_EQ(raw, 0xFFFFU);    // 書き込まない
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_IDLE);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_READ_GROUP);
}

}  // namespace
