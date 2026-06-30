/**
 * \file    Mcu_Hw.c
 * \brief   Mcu ハードウェア依存層 実装
 * \details AVR (MCUSR / wdt_disable) と Renesas RA (RSTSR0/RSTSR1 / WDT)
 *          の両方に対応する。ビルドターゲットに応じて自動的に切り替わる。
 *
 *          Renesas RA 側の制約 (現時点でハードウェア未検証):
 *            - RSTSR0/RSTSR1 の外部リセットピン専用フラグが無いため、
 *              Mcu_Hw_ResetReasonType.External は常に 0 を返す。
 *            - 各フラグのクリア手順 (1 であることを確認して 0 を書く) は
 *              デバイスヘッダのレジスタコメントに基づく。PRCR (レジスタ
 *              保護) のロック対象にはリセットステータスレジスタは含まれて
 *              いないと判断したが、実機未検証である。
 *            - Renesas RA の WDT (WDTimer ライブラリ) は begin() を呼ぶ
 *              まで動作しないため、Mcu_Hw_DisableWatchdogAtBoot() は no-op。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Mcu_Hw.h"

#if defined(__AVR__)

#include <avr/io.h>
#include <avr/wdt.h>

Mcu_Hw_ResetReasonType Mcu_Hw_ReadAndClearResetReason(void)
{
    const uint8 resetFlags = MCUSR;
    MCUSR = 0;

    Mcu_Hw_ResetReasonType reason;
    reason.Watchdog = (resetFlags >> 3) & 1U;  /* WDRF  */
    reason.BrownOut = (resetFlags >> 2) & 1U;  /* BORF  */
    reason.External = (resetFlags >> 1) & 1U;  /* EXTRF */
    reason.PowerOn  = resetFlags & 1U;         /* PORF  */
    return reason;
}

void Mcu_Hw_DisableWatchdogAtBoot(void)
{
    wdt_disable();
}

#else /* Renesas RA */

#include "bsp_api.h"

Mcu_Hw_ResetReasonType Mcu_Hw_ReadAndClearResetReason(void)
{
    Mcu_Hw_ResetReasonType reason;
    reason.Watchdog = (uint8)(R_SYSTEM->RSTSR1_b.WDTRF | R_SYSTEM->RSTSR1_b.IWDTRF);
    reason.BrownOut = (uint8)(R_SYSTEM->RSTSR0_b.LVD0RF | R_SYSTEM->RSTSR0_b.LVD1RF
                               | R_SYSTEM->RSTSR0_b.LVD2RF);
    reason.External = 0U;  /* RA4M1 に外部リセットピン専用の検出フラグは無い */
    reason.PowerOn  = R_SYSTEM->RSTSR0_b.PORF;

    /* 各フラグは「1であることを確認してから0を書く」とクリアされる
     * (デバイスヘッダのレジスタコメントに基づく)。 */
    if (R_SYSTEM->RSTSR1_b.WDTRF)  R_SYSTEM->RSTSR1_b.WDTRF  = 0U;
    if (R_SYSTEM->RSTSR1_b.IWDTRF) R_SYSTEM->RSTSR1_b.IWDTRF = 0U;
    if (R_SYSTEM->RSTSR0_b.LVD0RF) R_SYSTEM->RSTSR0_b.LVD0RF = 0U;
    if (R_SYSTEM->RSTSR0_b.LVD1RF) R_SYSTEM->RSTSR0_b.LVD1RF = 0U;
    if (R_SYSTEM->RSTSR0_b.LVD2RF) R_SYSTEM->RSTSR0_b.LVD2RF = 0U;
    if (R_SYSTEM->RSTSR0_b.PORF)   R_SYSTEM->RSTSR0_b.PORF   = 0U;

    return reason;
}

void Mcu_Hw_DisableWatchdogAtBoot(void)
{
    /* Renesas RA の WDT は WDTimer::begin() を呼ぶまで動作しないため、
     * 起動直後に無効化すべき有効な WDT が存在しない。 */
}

#endif
