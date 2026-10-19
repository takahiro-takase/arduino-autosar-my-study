/**
 * \file    StbM_MainFunction_test.cpp
 * \brief   StbM_MainFunction() の単体テスト
 * \details 同期済みでタイムアウトが設定された時間ベース（スレーブ）が、最後の更新から STBM_SYNC_LOSS_TIMEOUT_MS_SLAVE 以上更新されないと TIMEOUT ビットを立てる。
 *          設定時間に満たない間、未同期の間、タイムアウトを設定しない時間ベース（マスター）では立てない。
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

/* StbM_MainFunction() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_MainFunction_Test : public ::testing::Test
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

TEST_F(StbM_MainFunction_Test, StbM_MainFunction_NG_NoTimeoutBeforeThreshold)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 0U, 10U, 0U };
    StbM_MeasurementType meas = { 0U };
    ASSERT_EQ(StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL, &meas), E_OK);
    FakeMillis_Value += STBM_SYNC_LOSS_TIMEOUT_MS_SLAVE - 1UL;
    StbM_TimeBaseStatusType status = 0U;
    StbM_TimeBaseStatusType off = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StbM_MainFunction();
    ASSERT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &status, &off), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(status & STBM_TIMEBASE_STATUS_TIMEOUT, 0U);
}

TEST_F(StbM_MainFunction_Test, StbM_MainFunction_NG_NoTimeoutWhileUnsynchronized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeBaseStatusType status = 0U;
    StbM_TimeBaseStatusType off = 0U;
    FakeMillis_Value += STBM_SYNC_LOSS_TIMEOUT_MS_SLAVE * 3UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StbM_MainFunction();
    ASSERT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &status, &off), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(status, 0U);   // 一度も同期していないスレーブは、TIMEOUT も立たない
}

TEST_F(StbM_MainFunction_Test, StbM_MainFunction_NG_MasterNeverTimesOut)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeBaseStatusType status = 0U;
    StbM_TimeBaseStatusType off = 0U;
    FakeMillis_Value += 600000UL;   // 10 分

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StbM_MainFunction();
    ASSERT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_MASTER, &status, &off), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(status, STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE);   // マスターはタイムアウトを監視しない
}

TEST_F(StbM_MainFunction_Test, StbM_MainFunction_OK_TimeoutSetAtThreshold)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 0U, 10U, 0U };
    StbM_MeasurementType meas = { 0U };
    ASSERT_EQ(StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL, &meas), E_OK);
    FakeMillis_Value += STBM_SYNC_LOSS_TIMEOUT_MS_SLAVE;
    StbM_TimeBaseStatusType status = 0U;
    StbM_TimeBaseStatusType off = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StbM_MainFunction();
    ASSERT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &status, &off), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_NE(status & STBM_TIMEBASE_STATUS_TIMEOUT, 0U);
    EXPECT_NE(status & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE, 0U);   // 同期済みの印は残る
}

}  // namespace
