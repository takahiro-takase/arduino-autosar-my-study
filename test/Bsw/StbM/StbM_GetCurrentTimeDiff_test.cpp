/**
 * \file    StbM_GetCurrentTimeDiff_test.cpp
 * \brief   StbM_GetCurrentTimeDiff() の単体テスト
 * \details 時間ベースの ID が範囲外なら STBM_E_PARAM、ポインタが NULL なら STBM_E_PARAM_POINTER を報告して E_NOT_OK を返す。
 *          正常時は、与えた時刻からの経過時間 [ns] を返し、仮想ローカル時間の ns の 32bit での循環をまたいでも正しい差になる。
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

/* StbM_GetCurrentTimeDiff() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_GetCurrentTimeDiff_Test : public ::testing::Test
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

TEST_F(StbM_GetCurrentTimeDiff_Test, StbM_GetCurrentTimeDiff_NG_InvalidTimeBaseId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampRawType diff;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTimeDiff(STBM_TIME_BASE_COUNT, 0U, &diff);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_CURRENT_TIME_DIFF);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
}

TEST_F(StbM_GetCurrentTimeDiff_Test, StbM_GetCurrentTimeDiff_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTimeDiff(STBM_TIME_BASE_MASTER, 0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_CURRENT_TIME_DIFF);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_GetCurrentTimeDiff_Test, StbM_GetCurrentTimeDiff_OK_ReturnsElapsedNs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampRawType start = 0U;
    StbM_TimeStampRawType diff  = 0U;
    ASSERT_EQ(StbM_GetCurrentTimeRaw(STBM_TIME_BASE_MASTER, &start), E_OK);
    FakeMillis_Value += 37UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTimeDiff(STBM_TIME_BASE_MASTER, start, &diff);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(diff, 37000000U);   // 37ms
}

TEST_F(StbM_GetCurrentTimeDiff_Test, StbM_GetCurrentTimeDiff_OK_DiffAcrossWrapOfNsRaw)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 仮想ローカル時間の ns 部分は約 4.29 秒（2^32 ns）で循環する。循環の直前に取った時刻から、またいだ先の差を測る。
    StbM_TimeStampRawType diff = 0U;
    FakeMillis_Value = 4290UL;                    // 4290ms = 4290000000ns（2^32 = 4294967296 の直前）
    StbM_TimeStampRawType start = 0U;
    ASSERT_EQ(StbM_GetCurrentTimeRaw(STBM_TIME_BASE_MASTER, &start), E_OK);
    FakeMillis_Value = 4300UL;                    // 4300000000ns は 2^32 を超えて循環する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTimeDiff(STBM_TIME_BASE_MASTER, start, &diff);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(diff, 10000000U);   // 10ms（循環をまたいでも正しい差）
}

}  // namespace
