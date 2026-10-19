/**
 * \file    StbM_GetCurrentTime_test.cpp
 * \brief   StbM_GetCurrentTime() の単体テスト
 * \details 時間ベースの ID が範囲外なら STBM_E_PARAM、timeStamp または userData が NULL なら STBM_E_PARAM_POINTER を報告して E_NOT_OK を返す。
 *          正常時は、時間マスターの全域時間（秒・ナノ秒）と状態を返し、時刻が進めば全域時間も進む。
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

/* StbM_GetCurrentTime() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_GetCurrentTime_Test : public ::testing::Test
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

TEST_F(StbM_GetCurrentTime_Test, StbM_GetCurrentTime_NG_InvalidTimeBaseId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType ts;
    StbM_UserDataType ud;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTime(STBM_TIME_BASE_COUNT, &ts, &ud);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_CURRENT_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
}

TEST_F(StbM_GetCurrentTime_Test, StbM_GetCurrentTime_NG_NullTimeStamp)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_UserDataType ud;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, NULL, &ud);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_CURRENT_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_GetCurrentTime_Test, StbM_GetCurrentTime_NG_NullUserData)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType ts;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, &ts, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_CURRENT_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_GetCurrentTime_Test, StbM_GetCurrentTime_OK_ReturnsGlobalTimeAndAdvances)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType ts;
    StbM_UserDataType ud;
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, &ts, &ud), E_OK);
    ASSERT_EQ(ts.seconds, 12U);
    FakeMillis_Value += 1700UL;   // 1.7 秒進める → 14.045 秒

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, &ts, &ud);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(ts.seconds, 14U);
    EXPECT_EQ(ts.nanoseconds, 45000000U);
    EXPECT_EQ(ud.userDataLength, 0U);   // ユーザデータは保持しない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(StbM_GetCurrentTime_Test, StbM_GetCurrentTime_OK_ContinuesAcrossOsCounterWrap)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 0U, 1000U, 0U };   // 1000 秒
    StbM_TimeStampType got;
    StbM_UserDataType ud;
    FakeMillis_Value = 0xFFFFFFF0UL;                  // Os カウンタ（32bit）が一巡する 16ms 手前

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(StbM_SetGlobalTime(STBM_TIME_BASE_MASTER, &set, NULL), E_OK);
    FakeMillis_Value = 0x00000010UL;                  // 一巡して 32ms 後
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, &got, &ud), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(got.seconds, 1000U);                    // 全域時間が飛ばず、32ms だけ進む
    EXPECT_EQ(got.nanoseconds, 32000000U);
}

}  // namespace
