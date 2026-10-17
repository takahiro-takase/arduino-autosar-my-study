/**
 * \file    Dio_FlipChannel_test.cpp
 * \brief   Dio_FlipChannel() の単体テスト
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

constexpr Dio_ChannelType kChannel = DIO_CHANNEL_LED_RUNNING;

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Dio の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Dio_Hw と DET の記録を初期化する。 */
class Dio_FlipChannel_Test : public ::testing::Test
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

TEST_F(Dio_FlipChannel_Test, Dio_FlipChannel_OK_TogglesLowToHighAndReturnsNewLevel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dio_WriteChannel(kChannel, DIO_LOW);
    uint32 writeCountBefore = FakeDioHw_WriteCount;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_LevelType ret = Dio_FlipChannel(kChannel);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DIO_HIGH);
    EXPECT_EQ(Dio_ReadChannel(kChannel), DIO_HIGH);
    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore + 1U);  /* 1回だけ書き込むこと（二重書き込み回帰の検出） */
}

TEST_F(Dio_FlipChannel_Test, Dio_FlipChannel_OK_TogglesHighToLowAndReturnsNewLevel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dio_WriteChannel(kChannel, DIO_HIGH);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_LevelType ret = Dio_FlipChannel(kChannel);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DIO_LOW);
    EXPECT_EQ(Dio_ReadChannel(kChannel), DIO_LOW);
}

TEST_F(Dio_FlipChannel_Test, Dio_FlipChannel_OK_CalledTwiceReturnsToOriginalLevel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dio_WriteChannel(kChannel, DIO_LOW);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_FlipChannel(kChannel);
    Dio_LevelType ret = Dio_FlipChannel(kChannel);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DIO_LOW);
}

}  // namespace
