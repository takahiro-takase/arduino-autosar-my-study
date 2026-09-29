/**
 * \file    Fake_Adc_Hw.c
 * \brief   Adc_Hw.h の呼び出し記録フェイク実装（Fake_Adc_Hw.h 参照）。
 */

#include "Adc_Hw.h"
#include "Fake_Adc_Hw.h"

uint32 FakeAdcHw_ReadChannelCount = 0U;
uint8  FakeAdcHw_LastChannel      = 0U;
uint16 FakeAdcHw_ReturnValue      = 0U;

void FakeAdcHw_Reset(void)
{
    FakeAdcHw_ReadChannelCount = 0U;
    FakeAdcHw_LastChannel      = 0U;
    FakeAdcHw_ReturnValue      = 0U;
}

uint16 Adc_Hw_ReadChannel(uint8 channel)
{
    FakeAdcHw_ReadChannelCount++;
    FakeAdcHw_LastChannel = channel;
    return FakeAdcHw_ReturnValue;
}
