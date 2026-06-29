/**
 * \file    WdgM_Hw.c
 * \brief   WdgM ハードウェア依存層 実装 (AVR wdt_* ラッパー)
 * \details タイムアウト値 WDTO_8S は WdgM_Cfg.h の
 *          WDGM_HW_WATCHDOG_TIMEOUT_MS (8000ms) に対応する。
 *          設定を変更する場合は両者を一致させること。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "WdgM_Hw.h"
#include <avr/wdt.h>

void WdgM_Hw_Enable(void)
{
    wdt_enable(WDTO_8S);
}

void WdgM_Hw_Disable(void)
{
    wdt_disable();
}

void WdgM_Hw_Refresh(void)
{
    wdt_reset();
}
