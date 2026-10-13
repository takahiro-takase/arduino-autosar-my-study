/**
 * \file    Adc_test.cpp
 * \brief   Adc.c（src/Bsw/Adc/Adc.c）の単体テスト（NG系）
 * \details Adc.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          各ケースは `Adc_Init()` を呼んでから行う（`Adc_Init()` を呼ぶと、全グループが
 *          ADC_IDLE・結果バッファ未登録の状態に戻る）。未初期化（`ADC_E_UNINIT`）の検証は、
 *          他のテストが先に初期化済みのことがあるため、行わない。
 *          実 HW（analogRead()）への依存は `stub/Hal/Fake_Adc_Hw.c` で満たす。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
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
class Adc_Test : public ::testing::Test
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

TEST_F(Adc_Test, Adc_SetupResultBuffer_NG_InvalidGroup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_ValueGroupType buf = 0U;
    Std_ReturnType ret = Adc_SetupResultBuffer(ADC_GROUP_COUNT, &buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_GROUP);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_SETUP_RESULT_BUFFER);
}

TEST_F(Adc_Test, Adc_SetupResultBuffer_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Adc_SetupResultBuffer(ADC_GROUP_SENSOR, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_POINTER);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_SETUP_RESULT_BUFFER);
}

// ------------------------------------------------------------
// Adc_StartGroupConversion()
// ------------------------------------------------------------

TEST_F(Adc_Test, Adc_StartGroupConversion_NG_InvalidGroup)
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

TEST_F(Adc_Test, Adc_StartGroupConversion_NG_BufferUninit)
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

TEST_F(Adc_Test, Adc_StartGroupConversion_NG_Busy)
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

// ------------------------------------------------------------
// Adc_ReadGroup()
// ------------------------------------------------------------

TEST_F(Adc_Test, Adc_ReadGroup_NG_InvalidGroup)
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

TEST_F(Adc_Test, Adc_ReadGroup_NG_NullPointer)
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

TEST_F(Adc_Test, Adc_ReadGroup_NG_NoResult)
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

// ------------------------------------------------------------
// Adc_GetGroupStatus()
// ------------------------------------------------------------

TEST_F(Adc_Test, Adc_GetGroupStatus_NG_InvalidGroup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Adc_Init(NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_StatusType st = Adc_GetGroupStatus(ADC_GROUP_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(st, ADC_IDLE);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_GROUP);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_GET_GROUP_STATUS);
}

// ------------------------------------------------------------
// 未初期化（Adc_DeInit() の後）
// ------------------------------------------------------------

TEST_F(Adc_Test, Adc_SetupResultBuffer_NG_Uninit)
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
    Std_ReturnType ret = Adc_SetupResultBuffer(ADC_GROUP_SENSOR, &buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_SETUP_RESULT_BUFFER);
}

TEST_F(Adc_Test, Adc_StartGroupConversion_NG_Uninit)
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

TEST_F(Adc_Test, Adc_ReadGroup_NG_Uninit)
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

TEST_F(Adc_Test, Adc_GetGroupStatus_NG_Uninit)
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
    Adc_StatusType st = Adc_GetGroupStatus(ADC_GROUP_SENSOR);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(st, ADC_IDLE);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, ADC_API_ID_GET_GROUP_STATUS);
}

// ------------------------------------------------------------
// Adc_DeInit()
// ------------------------------------------------------------

TEST_F(Adc_Test, Adc_DeInit_NG_Uninit)
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

TEST_F(Adc_Test, Adc_DeInit_NG_BusyWhileResultNotRead)
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

// ------------------------------------------------------------
// Adc_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Adc_Test, Adc_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_POINTER);
}

}  // namespace
