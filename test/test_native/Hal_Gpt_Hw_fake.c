/**
 * \file    Hal_Gpt_Hw_fake.c
 * \brief   Gpt_Hw.h のテスト用フェイク実装
 * \details Hal_Gpt_Hw_fake.h 冒頭のコメント参照。
 */
#include "Hal_Gpt_Hw_fake.h"
#include "Gpt_Hw.h"

uint32          FakeGptHw_StartCount          = 0U;
uint32          FakeGptHw_StopCount           = 0U;
Gpt_ChannelType FakeGptHw_LastStartChannel    = 0U;
uint32          FakeGptHw_LastTickFrequencyHz = 0U;
uint8           FakeGptHw_StartShouldFail     = 0U;

void FakeGptHw_Reset(void)
{
    FakeGptHw_StartCount          = 0U;
    FakeGptHw_StopCount           = 0U;
    FakeGptHw_LastStartChannel    = 0U;
    FakeGptHw_LastTickFrequencyHz = 0U;
    FakeGptHw_StartShouldFail     = 0U;
}

void Gpt_Hw_Init(void)
{
}

void Gpt_Hw_DeInit(void)
{
}

Std_ReturnType Gpt_Hw_StartTimer(Gpt_ChannelType Channel, uint32 TickFrequencyHz)
{
    FakeGptHw_StartCount++;
    FakeGptHw_LastStartChannel    = Channel;
    FakeGptHw_LastTickFrequencyHz = TickFrequencyHz;
    return (FakeGptHw_StartShouldFail != 0U) ? E_NOT_OK : E_OK;
}

void Gpt_Hw_StopTimer(Gpt_ChannelType Channel)
{
    (void)Channel;
    FakeGptHw_StopCount++;
}
