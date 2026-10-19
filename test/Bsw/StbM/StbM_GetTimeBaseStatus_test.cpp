/**
 * \file    StbM_GetTimeBaseStatus_test.cpp
 * \brief   StbM_GetTimeBaseStatus() の単体テスト
 * \details 時間ベースの ID が範囲外なら STBM_E_PARAM、ポインタが NULL なら STBM_E_PARAM_POINTER を報告して E_NOT_OK を返す。
 *          正常時は、同期時間ベースの状態を返し、オフセット時間ベースの状態は常に 0。
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

/* StbM_GetTimeBaseStatus() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_GetTimeBaseStatus_Test : public ::testing::Test
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

TEST_F(StbM_GetTimeBaseStatus_Test, StbM_GetTimeBaseStatus_NG_InvalidTimeBaseId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeBaseStatusType a, b;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetTimeBaseStatus(STBM_TIME_BASE_COUNT, &a, &b);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_TIME_BASE_STATUS);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
}

TEST_F(StbM_GetTimeBaseStatus_Test, StbM_GetTimeBaseStatus_NG_NullSyncStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeBaseStatusType b;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetTimeBaseStatus(STBM_TIME_BASE_MASTER, NULL, &b);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_TIME_BASE_STATUS);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_GetTimeBaseStatus_Test, StbM_GetTimeBaseStatus_NG_NullOffsetStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeBaseStatusType a;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_GetTimeBaseStatus(STBM_TIME_BASE_MASTER, &a, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_GET_TIME_BASE_STATUS);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_GetTimeBaseStatus_Test, StbM_GetTimeBaseStatus_OK_MasterSyncedSlaveNotYet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeBaseStatusType master = 0xFFU, slave = 0xFFU, offset = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret1 = StbM_GetTimeBaseStatus(STBM_TIME_BASE_MASTER, &master, &offset);
    Std_ReturnType ret2 = StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &slave, &offset);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret1, E_OK);
    EXPECT_EQ(ret2, E_OK);
    EXPECT_EQ(master, STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE);
    EXPECT_EQ(slave, 0U);
    EXPECT_EQ(offset, 0U);   // オフセット時間ベースは持たない
}

}  // namespace
