/**
 * \file    StbM_Init_test.cpp
 * \brief   StbM_Init() の単体テスト
 * \details マスターの時間ベース（0）は、全域時間 = 仮想ローカル時間（ECU の稼働時間）で同期済み（GLOBAL_TIME_BASE）になり、スレーブの時間ベース（1）は未同期になる。
 *          再初期化で、スレーブに設定した時刻のオフセットと状態が初期値へ戻る。
 *          未初期化での API 呼び出しの DET 報告（STBM_E_NOT_INITIALIZED）は、StbM に DeInit が無く他のテストが先に初期化済みにするため、本テスト群からは検証できない（docs/unit_test.md 参照）。
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

/* StbM_Init() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms（= 12.345 秒）にして StbM_Init(NULL) を呼び、DET の記録を初期化する。 */
class StbM_Init_Test : public ::testing::Test
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

TEST_F(StbM_Init_Test, StbM_Init_OK_MasterSyncedSlaveNot)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で StbM_Init() 済み）
    StbM_TimeStampType master;
    StbM_TimeStampType slave;
    StbM_UserDataType ud;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_MASTER, &master, &ud), E_OK);
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_SLAVE, &slave, &ud), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(master.timeBaseStatus, STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE);
    EXPECT_EQ(master.seconds, 12U);                 // 12345ms = 12.345 秒
    EXPECT_EQ(master.nanoseconds, 345000000U);
    EXPECT_EQ(master.secondsHi, 0U);
    EXPECT_EQ(slave.timeBaseStatus, 0U);            // スレーブはまだ同期していない
}

TEST_F(StbM_Init_Test, StbM_Init_OK_ReinitializeResetsSlaveTime)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StbM_TimeStampType set = { 0U, 5U, 100U, 0U };
    StbM_MeasurementType meas = { 0U };
    ASSERT_EQ(StbM_BusSetGlobalTime(STBM_TIME_BASE_SLAVE, &set, NULL, &meas), E_OK);
    StbM_TimeStampType slave;
    StbM_UserDataType ud;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    StbM_Init(NULL);
    ASSERT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_SLAVE, &slave, &ud), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(slave.timeBaseStatus, 0U);            // 未同期へ戻る
    EXPECT_EQ(slave.seconds, 12U);                  // 全域時間 = 仮想ローカル時間
}

}  // namespace
