/**
 * \file    Dio_ReadChannelGroup_test.cpp
 * \brief   Dio_ReadChannelGroup() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Dio_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Dio_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dio.h"
#include "Fake_Dio_Hw.h"
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

/* Dio の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Dio_Hw と DET の記録を初期化する。 */
class Dio_ReadChannelGroup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDioHw_Reset();
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dio_ReadChannelGroup_Test, ReadChannelGroup_NG_NullPointerReturnsZeroAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_PortLevelType level = Dio_ReadChannelGroup(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(level, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_POINTER);
}

TEST_F(Dio_ReadChannelGroup_Test, ReadChannelGroup_NG_InvalidPortReturnsZeroAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dio_ChannelGroupType group = { (Dio_PortType)0xFFU, 0x01U, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_PortLevelType level = Dio_ReadChannelGroup(&group);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(level, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_GROUP);
}

TEST_F(Dio_ReadChannelGroup_Test, ReadChannelGroup_OK_MasksAndShiftsToLsb)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dio_WriteChannel(DIO_CHANNEL_LED_RUNNING, DIO_HIGH);  /* bit0 */
    Dio_WriteChannel(DIO_CHANNEL_LED_FAULT, DIO_LOW);     /* bit1 */
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_HIGH);  /* bit2、グループ外 */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_PortLevelType level = Dio_ReadChannelGroup(&Dio_ChannelGroupRunFault);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(level, 0x01U);  /* bit2(WARNING)はマスク外なので反映されない */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
