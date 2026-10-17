/**
 * \file    Adc_DeInit_test.cpp
 * \brief   Adc_DeInit() の単体テスト
 * \details 全グループが ADC_IDLE のとき Adc_DeInit() は、結果バッファの登録を消し、未初期化の状態へ戻す。
 *          以降、他の API は ADC_E_UNINIT を報告する。Adc_Init() をやり直すと、結果バッファは未登録に戻っている
 *          （Adc_StartGroupConversion() が ADC_E_BUFFER_UNINIT を報告する）。
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Adc_DeInit() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。 */
class Adc_DeInit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_Reset();
    }
};

/* Adc の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。Adc_Init() は各テストが必要に応じて呼ぶ。 */
class Adc_DeInit_Base_Test : public ::testing::Test
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

TEST_F(Adc_DeInit_Base_Test, Adc_DeInit_NG_Uninit)
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
    Adc_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_DEINIT);
}

TEST_F(Adc_DeInit_Base_Test, Adc_DeInit_NG_BusyWhileResultNotRead)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);
    Adc_ValueGroupType buf = 0U;
    ASSERT_EQ(Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &buf), E_OK);
    Adc_StartGroupConversion(ADC_GROUP_SENSOR);   // ADC_STREAM_COMPLETED（結果を読み出していない）
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_DeInit();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_BUSY);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_DEINIT);
    EXPECT_EQ(Adc_GetGroupStatus(ADC_GROUP_SENSOR), ADC_STREAM_COMPLETED);   // 何もしない（初期化済みのまま）
}

TEST_F(Adc_DeInit_Test, Adc_DeInit_OK_ClearsInitializationAndBufferRegistration)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);
    Adc_ValueGroupType buffer = 0U;
    ASSERT_EQ(Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &buffer), E_OK);
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_DeInit();
    const uint32 detAfterDeInit = FakeDetHw_ReportCount;   // DeInit 自体は DET を報告しない
    Adc_StartGroupConversion(ADC_GROUP_SENSOR);            // 未初期化
    const uint8 uninitErr = FakeDetHw_LastErrorId;
    Adc_Init(NULL);
    FakeDetHw_Reset();
    Adc_StartGroupConversion(ADC_GROUP_SENSOR);            // 再初期化後は、結果バッファが未登録

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(detAfterDeInit, 0U);
    EXPECT_EQ(uninitErr, ADC_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_BUFFER_UNINIT);
    EXPECT_EQ(Adc_GetGroupStatus(ADC_GROUP_SENSOR), ADC_IDLE);
}

}  // namespace
