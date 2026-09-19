/**
 * \file    WdgIf_fake.c
 * \brief   WdgIf.h のテスト用フェイク実装（WdgIf_fake.h 冒頭のコメント参照）
 */
#include "WdgIf_fake.h"

uint32 FakeWdgIf_SetModeCount             = 0U;
uint32 FakeWdgIf_SetTriggerConditionCount = 0U;

WdgIf_DeviceType FakeWdgIf_LastSetModeDevice = 0U;
WdgIf_ModeType   FakeWdgIf_LastSetModeMode   = WDGIF_OFF_MODE;
WdgIf_DeviceType FakeWdgIf_LastTriggerDevice = 0U;
uint16           FakeWdgIf_LastTriggerTimeout = 0U;

Std_ReturnType FakeWdgIf_SetModeReturn = E_OK;

void FakeWdgIf_Reset(void)
{
    FakeWdgIf_SetModeCount             = 0U;
    FakeWdgIf_SetTriggerConditionCount = 0U;
    FakeWdgIf_LastSetModeDevice        = 0U;
    FakeWdgIf_LastSetModeMode          = WDGIF_OFF_MODE;
    FakeWdgIf_LastTriggerDevice        = 0U;
    FakeWdgIf_LastTriggerTimeout       = 0U;
    FakeWdgIf_SetModeReturn            = E_OK;
}

Std_ReturnType WdgIf_SetMode(WdgIf_DeviceType Device, WdgIf_ModeType WdgMode)
{
    FakeWdgIf_SetModeCount++;
    FakeWdgIf_LastSetModeDevice = Device;
    FakeWdgIf_LastSetModeMode   = WdgMode;
    return FakeWdgIf_SetModeReturn;
}

void WdgIf_SetTriggerCondition(WdgIf_DeviceType Device, uint16 Timeout)
{
    FakeWdgIf_SetTriggerConditionCount++;
    FakeWdgIf_LastTriggerDevice  = Device;
    FakeWdgIf_LastTriggerTimeout = Timeout;
}
