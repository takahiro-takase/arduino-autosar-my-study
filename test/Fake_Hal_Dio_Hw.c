/**
 * \file    Fake_Hal_Dio_Hw.c
 * \brief   Dio_Hw.h のテスト用フェイク実装
 * \details Fake_Hal_Dio_Hw.h 冒頭のコメント参照。
 */
#include "Fake_Hal_Dio_Hw.h"
#include "Dio_Hw.h"

static Dio_LevelType FakeDioHw_ChannelLevel[256];

uint32 FakeDioHw_WriteCount = 0U;

void FakeDioHw_Reset(void)
{
    FakeDioHw_WriteCount = 0U;
    for (uint16 i = 0U; i < 256U; i++)
    {
        FakeDioHw_ChannelLevel[i] = DIO_LOW;
    }
}

void Dio_Hw_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level)
{
    FakeDioHw_WriteCount++;
    FakeDioHw_ChannelLevel[channelId] = level;
}

Dio_LevelType Dio_Hw_ReadChannel(Dio_ChannelType channelId)
{
    return FakeDioHw_ChannelLevel[channelId];
}
