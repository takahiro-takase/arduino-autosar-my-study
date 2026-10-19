/**
 * \file    StbM_BusSetGlobalTime_test.cpp
 * \brief   StbM_BusSetGlobalTime() の単体テスト
 * \details 時間ベースの ID が範囲外なら STBM_E_PARAM、時刻または追加情報が NULL なら STBM_E_PARAM_POINTER を報告して E_NOT_OK を返す。
 *          正常時は、受信した時刻に伝搬遅延を足した値が全域時間になり、GLOBAL_TIME_BASE が立って TIMEOUT が下りる。
 *          時刻の SYNC_TO_GATEWAY は状態へ引き継ぐ。
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

/* StbM_BusSetGlobalTime() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_BusSetGlobalTime_Test : public ::testing::Test
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

TEST_F(StbM_BusSetGlobalTime_Test, StbM_BusSetGlobalTime_NG_InvalidTimeBaseId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType ts = { 0U, 0U, 0U, 0U };
    StbM_MeasurementType meas = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_BusSetGlobalTime(STBM_TIME_BASE_COUNT, &ts, NULL, &meas);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_BUS_SET_GLOBAL_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
}

TEST_F(StbM_BusSetGlobalTime_Test, StbM_BusSetGlobalTime_NG_NullTimeStamp)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_MeasurementType meas = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, NULL, NULL, &meas);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_BUS_SET_GLOBAL_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_BusSetGlobalTime_Test, StbM_BusSetGlobalTime_NG_NullMeasureData)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType ts = { 0U, 0U, 0U, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &ts, NULL, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastModuleId, STBM_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_BUS_SET_GLOBAL_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM_POINTER);
}

TEST_F(StbM_BusSetGlobalTime_Test, StbM_BusSetGlobalTime_NG_TimeStampOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType   ts      = { 0U, 0U, 0x00000000U, 0x7FFFU };   // 秒が保持できる範囲を超える
    StbM_MeasurementType measure = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &ts, NULL, &measure);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastApiId, STBM_API_ID_BUS_SET_GLOBAL_TIME);
    EXPECT_EQ(FakeDetHw_LastErrorId, STBM_E_PARAM);
    StbM_TimeBaseStatusType st = 0U;
    StbM_TimeBaseStatusType off = 0U;
    EXPECT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &st, &off), E_OK);
    EXPECT_EQ(st & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE, 0U);   // 時間ベースは更新されない
}

TEST_F(StbM_BusSetGlobalTime_Test, StbM_BusSetGlobalTime_OK_AddsPathDelayAndClearsTimeout)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 100000000U, 50U, 0U };   // 50.1 秒
    StbM_MeasurementType meas = { 20000000U };              // 伝搬遅延 20ms
    StbM_TimeStampType got;
    StbM_UserDataType ud;
    ASSERT_EQ(StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL, &meas), E_OK);
    FakeMillis_Value += STBM_SYNC_LOSS_TIMEOUT_MS_SLAVE;
    StbM_MainFunction();   // 同期喪失のタイムアウトを立てる
    StbM_TimeBaseStatusType status = 0U;
    StbM_TimeBaseStatusType offStatus = 0U;
    ASSERT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &status, &offStatus), E_OK);
    ASSERT_NE(status & STBM_TIMEBASE_STATUS_TIMEOUT, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL, &meas);
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_SLAVE, &got, &ud), E_OK);
    ASSERT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &status, &offStatus), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(got.seconds, 50U);                  // 50.1 秒 + 遅延 20ms = 50.12 秒
    EXPECT_EQ(got.nanoseconds, 120000000U);
    EXPECT_NE(status & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE, 0U);
    EXPECT_EQ(status & STBM_TIMEBASE_STATUS_TIMEOUT, 0U);   // 更新でタイムアウトが下りる
}

TEST_F(StbM_BusSetGlobalTime_Test, StbM_BusSetGlobalTime_OK_SyncToGatewayBitFollowsTimeStamp)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY, 0U, 10U, 0U };
    StbM_MeasurementType meas = { 0U };
    StbM_TimeBaseStatusType status = 0U;
    StbM_TimeBaseStatusType offStatus = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL, &meas), E_OK);
    ASSERT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &status, &offStatus), E_OK);
    bool withGateway = (status & STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY) != 0U;
    set.timeBaseStatus = 0U;   // 次の更新ではゲートウェイ経由でない
    ASSERT_EQ(StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL, &meas), E_OK);
    ASSERT_EQ(StbM_GetTimeBaseStatus(STBM_TIME_BASE_SLAVE, &status, &offStatus), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_TRUE(withGateway);                                              // ゲートウェイ経由の更新で立つ
    EXPECT_EQ(status & STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY, 0U);          // そうでない更新で下りる
}

}  // namespace
