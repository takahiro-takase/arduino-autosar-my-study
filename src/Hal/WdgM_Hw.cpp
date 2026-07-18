/**
 * \file    WdgM_Hw.cpp
 * \brief   WdgM ハードウェア依存層 実装 (AVR wdt_* / Renesas RA WDT ライブラリ)
 * \details タイムアウト値は WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS (4000ms) に
 *          対応する。設定を変更する場合は両方を一致させること。
 *
 *          Renesas RA 側が .cpp である理由:
 *          RA の WDT ライブラリ (WDTimer クラス、グローバルインスタンス WDT) は
 *          C++ API のため、本ファイルは Can_Hw.cpp / Dio_Hw.cpp 等と同じ理由で
 *          C++ として実装する (WdgM.c からは WdgM_Hw.h の extern "C" 経由で
 *          呼び出せる)。
 *
 *          Renesas RA4M1 の IWDT 制約:
 *          最大タイムアウトは約 5592ms (WDT.getMaxTimeout() 相当) しかなく、
 *          旧 WDGM_SUPERVISION_CYCLE_MS (6000ms) 直結のリフレッシュ設計とは
 *          両立できない。このため WdgM 側でリフレッシュ (trigger) 周期を
 *          判定周期から分離した (WdgM_TriggerHwWatchdog、1000ms 周期)。
 *          本ファイルは「一定タイムアウトで Enable/Disable/Refresh する」
 *          という MCU 非依存の役割のみを持ち、周期分離は関与しない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "WdgM_Hw.h"
#include "Det.h"

#define TAG "WdgM_Hw"

#if defined(__AVR__)

#include <avr/wdt.h>

void WdgM_Hw_Enable(void)
{
    wdt_enable(WDTO_4S);
}

void WdgM_Hw_Disable(void)
{
    wdt_disable();
}

void WdgM_Hw_Refresh(void)
{
    wdt_reset();
}

#else /* Renesas RA */

#include <WDT.h>

void WdgM_Hw_Enable(void)
{
    if (!WDT.begin(4000))
    {
        DET_LOGE(TAG, "WDT.begin failed - HW watchdog NOT active");
    }
}

void WdgM_Hw_Disable(void)
{
    /* Renesas RA の IWDT は一度有効化すると FSP からの無効化手段がない。
     * WdgM 側の WdgM_SupervisionSuppressed フラグで代替する
     * (詳細は WdgM.c の HW ウォッチドッグ連携コメントを参照)。 */
}

void WdgM_Hw_Refresh(void)
{
    WDT.refresh();
}

#endif
