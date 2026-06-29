/**
 * \file    Mcu_Hw.c
 * \brief   Mcu ハードウェア依存層 実装 (AVR MCUSR / wdt_disable)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Mcu_Hw.h"
#include <avr/io.h>
#include <avr/wdt.h>

uint8 Mcu_Hw_ReadAndClearResetReason(void)
{
    const uint8 resetFlags = MCUSR;
    MCUSR = 0;
    return resetFlags;
}

void Mcu_Hw_DisableWatchdogAtBoot(void)
{
    wdt_disable();
}
