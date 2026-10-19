/**
 * \file    StbM_GetCurrentTimeRaw_test.cpp
 * \brief   StbM_GetCurrentTimeRaw() の単体テスト
 * \details 時間ベースの ID が範囲外なら STBM_E_PARAM、ポインタが NULL なら STBM_E_PARAM_POINTER を報告して E_NOT_OK を返す。
 *          正常時は、仮想ローカル時間（Os カウンタ [ms] × 10^6）の ns の下位 32bit を返す。
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

/* StbM_GetCurrentTimeRaw() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_GetCurrentTimeRaw_Test : public ::testing::Test
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

TEST_F(StbM_GetCurrentTimeRaw_Test, StbM_GetCurrentTimeRaw_NG_InvalidTimeBaseId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampRawType raw;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTimeRaw(STBM_TIME_BASE_COUNT, &raw);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_CURRENT_TIME_RAW);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
}

TEST_F(StbM_GetCurrentTimeRaw_Test, StbM_GetCurrentTimeRaw_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTimeRaw(STBM_TIME_BASE_MASTER, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_CURRENT_TIME_RAW);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_GetCurrentTimeRaw_Test, StbM_GetCurrentTimeRaw_OK_ReturnsLocalTimeNs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampRawType raw = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetCurrentTimeRaw(STBM_TIME_BASE_MASTER, &raw);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(raw, static_cast<StbM_TimeStampRawType>(12345000000ULL & 0xFFFFFFFFULL));   // 12345ms = 12345000000ns の下位 32bit
}

}  // namespace
