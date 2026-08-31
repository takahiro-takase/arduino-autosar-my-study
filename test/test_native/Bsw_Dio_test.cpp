/**
 * \file    Bsw_Dio_test.cpp
 * \brief   Dio.c（src/Bsw/Dio/Dio.c）の Dio_FlipChannel() 単体テスト
 * \details AUTOSAR SWS_Dio_00190/00191 が規定する「読み取り→反転→書き込みし、
 *          反転後の値を返す」挙動を検証する。Dio.c 自体は実物をリンクし、
 *          実 HW 依存の Dio_Hw.cpp のみを Hal_Dio_Hw_fake.c（チャネルごとの
 *          現在レベルを保持する簡易メモリモデル）に差し替える。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dio.h"
#include "Hal_Dio_Hw_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

constexpr Dio_ChannelType kChannel = DIO_CHANNEL_LED_RUNNING;

class DioTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDioHw_Reset();
        FakeDetHw_Reset();
    }
};

TEST_F(DioTest, FlipChannelTogglesLowToHighAndReturnsNewLevel)
{
    Dio_WriteChannel(kChannel, DIO_LOW);
    uint32 writeCountBefore = FakeDioHw_WriteCount;

    Dio_LevelType ret = Dio_FlipChannel(kChannel);

    EXPECT_EQ(ret, DIO_HIGH);
    EXPECT_EQ(Dio_ReadChannel(kChannel), DIO_HIGH);
    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore + 1U);  /* 1回だけ書き込むこと（二重書き込み回帰の検出） */
}

TEST_F(DioTest, FlipChannelTogglesHighToLowAndReturnsNewLevel)
{
    Dio_WriteChannel(kChannel, DIO_HIGH);

    Dio_LevelType ret = Dio_FlipChannel(kChannel);

    EXPECT_EQ(ret, DIO_LOW);
    EXPECT_EQ(Dio_ReadChannel(kChannel), DIO_LOW);
}

TEST_F(DioTest, FlipChannelCalledTwiceReturnsToOriginalLevel)
{
    Dio_WriteChannel(kChannel, DIO_LOW);

    Dio_FlipChannel(kChannel);
    Dio_LevelType ret = Dio_FlipChannel(kChannel);

    EXPECT_EQ(ret, DIO_LOW);
}

TEST_F(DioTest, ReadPort_OK_CombinesAllChannelsLsbFirst)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_RUNNING, DIO_HIGH);  /* bit0 */
    Dio_WriteChannel(DIO_CHANNEL_LED_FAULT, DIO_LOW);     /* bit1 */
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_HIGH);  /* bit2 */

    Dio_PortLevelType level = Dio_ReadPort(DIO_PORT_LED_GROUP);

    EXPECT_EQ(level, 0x05U);  /* 0b101 */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(DioTest, ReadPort_NG_InvalidPortIdReturnsZeroAndReportsDet)
{
    Dio_PortLevelType level = Dio_ReadPort((Dio_PortType)0xFFU);

    EXPECT_EQ(level, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_PORT_ID);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(DioTest, WritePort_OK_WritesEachChannelFromLsbFirst)
{
    Dio_WritePort(DIO_PORT_LED_GROUP, 0x06U);  /* 0b110 */

    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_RUNNING), DIO_LOW);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_FAULT), DIO_HIGH);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_WARNING), DIO_HIGH);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(DioTest, WritePort_NG_InvalidPortIdHasNoEffectAndReportsDet)
{
    uint32 writeCountBefore = FakeDioHw_WriteCount;

    Dio_WritePort((Dio_PortType)0xFFU, 0x07U);

    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_PORT_ID);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(DioTest, ReadChannelGroup_OK_MasksAndShiftsToLsb)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_RUNNING, DIO_HIGH);  /* bit0 */
    Dio_WriteChannel(DIO_CHANNEL_LED_FAULT, DIO_LOW);     /* bit1 */
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_HIGH);  /* bit2、グループ外 */

    Dio_PortLevelType level = Dio_ReadChannelGroup(&Dio_ChannelGroupRunFault);

    EXPECT_EQ(level, 0x01U);  /* bit2(WARNING)はマスク外なので反映されない */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(DioTest, WriteChannelGroup_OK_LeavesChannelsOutsideMaskUnchanged)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_HIGH);  /* グループ外、変化しないはず */

    Dio_WriteChannelGroup(&Dio_ChannelGroupRunFault, 0x02U);  /* bit1(FAULT)のみ立てる */

    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_RUNNING), DIO_LOW);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_FAULT), DIO_HIGH);
    EXPECT_EQ(Dio_ReadChannel(DIO_CHANNEL_LED_WARNING), DIO_HIGH);  /* 維持される */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(DioTest, ReadChannelGroup_NG_NullPointerReturnsZeroAndReportsDet)
{
    Dio_PortLevelType level = Dio_ReadChannelGroup(NULL);

    EXPECT_EQ(level, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_POINTER);
}

TEST_F(DioTest, WriteChannelGroup_NG_NullPointerHasNoEffectAndReportsDet)
{
    uint32 writeCountBefore = FakeDioHw_WriteCount;

    Dio_WriteChannelGroup(NULL, 0x01U);

    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_POINTER);
}

TEST_F(DioTest, ReadChannelGroup_NG_InvalidPortReturnsZeroAndReportsDet)
{
    Dio_ChannelGroupType group = { (Dio_PortType)0xFFU, 0x01U, 0U };

    Dio_PortLevelType level = Dio_ReadChannelGroup(&group);

    EXPECT_EQ(level, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_GROUP);
}

TEST_F(DioTest, WriteChannelGroup_NG_OffsetBeyondPortWidthHasNoEffectAndReportsDet)
{
    uint32 writeCountBefore = FakeDioHw_WriteCount;
    Dio_ChannelGroupType group = { DIO_PORT_LED_GROUP, 0x01U, 3U };  /* ポート幅3(bit0-2)を超える */

    Dio_WriteChannelGroup(&group, 0x01U);

    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_GROUP);
}

TEST_F(DioTest, WriteChannelGroup_NG_MaskExtendsBeyondPortWidthHasNoEffectAndReportsDet)
{
    uint32 writeCountBefore = FakeDioHw_WriteCount;
    /* offset(0) 単体はポート幅3内だが、mask=0x0F(4bit)は bit3 まで届き
     * ポート幅3(bit0-2)を超える（offsetだけの旧チェックでは検出できなかった不具合）。 */
    Dio_ChannelGroupType group = { DIO_PORT_LED_GROUP, 0x0FU, 0U };

    Dio_WriteChannelGroup(&group, 0x0FU);

    EXPECT_EQ(FakeDioHw_WriteCount, writeCountBefore);
    EXPECT_EQ(FakeDetHw_LastErrorId, DIO_E_PARAM_INVALID_GROUP);
}

}  // namespace
