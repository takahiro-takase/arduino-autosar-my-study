/**
 * \file    Fake_Hal_Port_Hw.c
 * \brief   Port_Hw.h のテスト用フェイク実装
 * \details Fake_Hal_Port_Hw.h 冒頭のコメント参照。
 */
#include "Fake_Hal_Port_Hw.h"
#include "Port_Hw.h"

#define FAKE_PORT_HW_UNSET  0xFFU

static Port_PinDirectionType FakePortHw_LastDirection[256];

uint32 FakePortHw_SetPinDirectionCount = 0U;

void FakePortHw_Reset(void)
{
    FakePortHw_SetPinDirectionCount = 0U;
    for (uint16 i = 0U; i < 256U; i++)
    {
        FakePortHw_LastDirection[i] = FAKE_PORT_HW_UNSET;
    }
}

Port_PinDirectionType FakePortHw_GetLastDirection(Port_PinType pin)
{
    return FakePortHw_LastDirection[pin];
}

void Port_Hw_SetPinDirection(Port_PinType pin, Port_PinDirectionType direction)
{
    FakePortHw_SetPinDirectionCount++;
    FakePortHw_LastDirection[pin] = direction;
}
