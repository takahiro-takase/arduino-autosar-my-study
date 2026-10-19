/**
 * \file    StbM_SetGlobalTime_test.cpp
 * \brief   StbM_SetGlobalTime() の単体テスト
 * \details 時間ベースの ID が範囲外なら STBM_E_PARAM、時刻が NULL なら STBM_E_PARAM_POINTER を報告して E_NOT_OK を返す。
 *          正常時は、設定した時点の全域時間が与えた値になり、そのあと時刻が進むと全域時間も同じだけ進む。GLOBAL_TIME_BASE が立つ。
 *          48bit の秒（secondsHi）も正しく扱う。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "StbM.h"
#include "StbM_Cfg.h"
#include "Fake_Millis.h"
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

/* StbM_SetGlobalTime() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_SetGlobalTime_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeMillis_Value = 12345UL;
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        StbM_Init(NULL);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(StbM_SetGlobalTime_Test, StbM_SetGlobalTime_NG_InvalidTimeBaseId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType ts = { 0U, 0U, 0U, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_SetGlobalTime(STBM_TIME_BASE_COUNT, &ts, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_SET_GLOBAL_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
}

TEST_F(StbM_SetGlobalTime_Test, StbM_SetGlobalTime_NG_NullTimeStamp)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_SetGlobalTime(STBM_TIME_BASE_MASTER, NULL, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_SET_GLOBAL_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_SetGlobalTime_Test, StbM_SetGlobalTime_NG_SecondsOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 0U, 0x00000000U, 0x7FFFU };   // 秒が保持できる範囲（約 146 年分）を超える

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_SetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_SET_GLOBAL_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
}

TEST_F(StbM_SetGlobalTime_Test, StbM_SetGlobalTime_NG_NanosecondsOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 1000000000U, 1U, 0U };   // ns が 10^9 以上

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_SetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
}

TEST_F(StbM_SetGlobalTime_Test, StbM_SetGlobalTime_OK_SetsGlobalTimeAndItAdvances)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 500000000U, 1000U, 0U };   // 1000.5 秒
    StbM_TimeStampType got;
    StbM_UserDataType ud;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_SetGlobalTime(STBM_TIME_BASE_MASTER, &set, NULL);
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, &got, &ud), E_OK);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(got.seconds, 1000U);                 // 設定した瞬間の全域時間
    EXPECT_EQ(got.nanoseconds, 500000000U);
    FakeMillis_Value += 1700UL;                    // 1.7 秒進める
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, &got, &ud), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(got.seconds, 1002U);                 // 1000.5 + 1.7 = 1002.2 秒
    EXPECT_EQ(got.nanoseconds, 200000000U);
    EXPECT_NE(got.timeBaseStatus & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE, 0U);
}

TEST_F(StbM_SetGlobalTime_Test, StbM_SetGlobalTime_OK_SecondsHigh48bit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 0U, 0x00000005U, 0x0001U };   // 秒 = 0x0001_0000_0005
    StbM_TimeStampType got;
    StbM_UserDataType ud;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(StbM_SetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL), E_OK);
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_SLAVE, &got, &ud), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(got.secondsHi, 0x0001U);
    EXPECT_EQ(got.seconds, 5U);
}

}  // namespace
