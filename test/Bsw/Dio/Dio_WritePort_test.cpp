/**
 * \file    Dio_WritePort_test.cpp
 * \brief   Dio_WritePort() の単体テスト
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
class Dio_WritePort_Test : public ::testing::Test
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

TEST_F(Dio_WritePort_Test, WritePort_NG_InvalidPortIdHasNoEffectAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 writeCountBefore = FakeDioHw_WriteCount;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_WritePort((Dio_PortType)0xFFU, 0x07U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_PORT_ID);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Dio_WritePort_Test, WritePort_OK_WritesEachChannelFromLsbFirst)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_WritePort(DIO_PORT_LED_GROUP, 0x06U);  /* 0b110 */

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_RUNNING), DIO_LOW);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_FAULT), DIO_HIGH);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_WARNING), DIO_HIGH);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
