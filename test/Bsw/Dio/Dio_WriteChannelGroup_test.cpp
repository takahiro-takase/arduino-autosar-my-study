/**
 * \file    Dio_WriteChannelGroup_test.cpp
 * \brief   Dio_WriteChannelGroup() の単体テスト
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
class Dio_WriteChannelGroup_Test : public ::testing::Test
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

TEST_F(Dio_WriteChannelGroup_Test, WriteChannelGroup_NG_NullPointerHasNoEffectAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 writeCountBefore = FakeDioHw_WriteCount;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_WriteChannelGroup(NULL, 0x01U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_POINTER);
}

TEST_F(Dio_WriteChannelGroup_Test, WriteChannelGroup_NG_OffsetBeyondPortWidthHasNoEffectAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 writeCountBefore = FakeDioHw_WriteCount;
    Dio_ChannelGroupType group = { DIO_PORT_LED_GROUP, 0x01U, 3U };  /* ポート幅3(bit0-2)を超える */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_WriteChannelGroup(&group, 0x01U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_GROUP);
}

TEST_F(Dio_WriteChannelGroup_Test, WriteChannelGroup_NG_MaskExtendsBeyondPortWidthHasNoEffectAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 writeCountBefore = FakeDioHw_WriteCount;
    /* offset(0) 単体はポート幅3内だが、mask=0x0F(4bit)は bit3 まで届き
     * ポート幅3(bit0-2)を超える（offsetだけの旧チェックでは検出できなかった不具合）。 */
    Dio_ChannelGroupType group = { DIO_PORT_LED_GROUP, 0x0FU, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_WriteChannelGroup(&group, 0x0FU);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_GROUP);
}

TEST_F(Dio_WriteChannelGroup_Test, WriteChannelGroup_OK_LeavesChannelsOutsideMaskUnchanged)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_HIGH);  /* グループ外、変化しないはず */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dio_WriteChannelGroup(&Dio_ChannelGroupRunFault, 0x02U);  /* bit1(FAULT)のみ立てる */

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_RUNNING), DIO_LOW);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_FAULT), DIO_HIGH);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_WARNING), DIO_HIGH);  /* 維持される */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
