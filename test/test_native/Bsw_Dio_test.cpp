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

}  // namespace
