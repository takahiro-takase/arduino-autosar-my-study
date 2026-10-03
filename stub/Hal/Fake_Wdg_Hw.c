/**
 * \file    Fake_Wdg_Hw.c
 * \brief   Wdg_Hw.h の呼び出し記録フェイク実装（Fake_Wdg_Hw.h 参照）。
 */

#include "Wdg_Hw.h"
#include "Fake_Wdg_Hw.h"

uint32 FakeWdgHw_EnableCount        = 0U;
uint32 FakeWdgHw_DisableCount       = 0U;
uint32 FakeWdgHw_RefreshCount       = 0U;
uint32 FakeWdgHw_ForceResetCount    = 0U;
uint16 FakeWdgHw_LastEnableTimeoutMs = 0U;

void FakeWdgHw_Reset(void)
{
    FakeWdgHw_EnableCount        = 0U;
    FakeWdgHw_DisableCount       = 0U;
    FakeWdgHw_RefreshCount       = 0U;
    FakeWdgHw_ForceResetCount    = 0U;
    FakeWdgHw_LastEnableTimeoutMs = 0U;
}

void Wdg_Hw_Enable(uint16 timeoutMs)
{
    FakeWdgHw_EnableCount++;
    FakeWdgHw_LastEnableTimeoutMs = timeoutMs;
}

void Wdg_Hw_Disable(void)
{
    FakeWdgHw_DisableCount++;
}

void Wdg_Hw_Refresh(void)
{
    FakeWdgHw_RefreshCount++;
}

void Wdg_Hw_ForceReset(void)
{
    FakeWdgHw_ForceResetCount++;
}
