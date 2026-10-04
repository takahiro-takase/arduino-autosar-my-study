/**
 * \file    Fake_Mcu_Hw.c
 * \brief   Mcu_Hw.h（Renesas RA レジスタ境界）のテスト用フェイク実装
 * \details Fake_Mcu_Hw.h 冒頭のコメント参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Fake_Mcu_Hw.h"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

Mcu_Hw_ResetReasonType FakeMcuHw_ResetReason = { 0U, 0U, 0U, 0U };

uint32 FakeMcuHw_ReadAndClearResetReasonCount = 0U;
uint32 FakeMcuHw_DisableWatchdogAtBootCount   = 0U;
uint32 FakeMcuHw_PerformResetCount            = 0U;

/* ======================================================================
 * Functions
 * ====================================================================== */

void FakeMcuHw_Reset(void)
{
    FakeMcuHw_ResetReason.Watchdog = 0U;
    FakeMcuHw_ResetReason.BrownOut = 0U;
    FakeMcuHw_ResetReason.External = 0U;
    FakeMcuHw_ResetReason.PowerOn  = 0U;

    FakeMcuHw_ReadAndClearResetReasonCount = 0U;
    FakeMcuHw_DisableWatchdogAtBootCount   = 0U;
    FakeMcuHw_PerformResetCount            = 0U;
}

Mcu_Hw_ResetReasonType Mcu_Hw_ReadAndClearResetReason(void)
{
    FakeMcuHw_ReadAndClearResetReasonCount++;
    return FakeMcuHw_ResetReason;
}

void Mcu_Hw_DisableWatchdogAtBoot(void)
{
    FakeMcuHw_DisableWatchdogAtBootCount++;
}

void Mcu_Hw_PerformReset(void)
{
    /* 実HW（NVIC_SystemReset()）と異なり戻ってくる（Fake_Mcu_Hw.h 参照）。 */
    FakeMcuHw_PerformResetCount++;
}
