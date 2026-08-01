/**
 * \file    Wdg_Hw.cpp
 * \brief   Wdg ハードウェア依存層 実装 (Renesas RA WDT ライブラリ)
 * \details 本プロジェクトが対応する MCU は Renesas RA (Arduino UNO R4) のみ
 *          （AVR/UNO 無印は初代のプログラムサイズ制限により移行済み。旧
 *          AVR (wdt_* / avr/wdt.h) 分岐は削除済み）。
 *
 *          旧 WdgM_Hw.cpp との違い: タイムアウト値がハードコード（
 *          `WDT.begin(4000)`）ではなく Wdg_Hw_Enable() の引数として渡される
 *          ようになった。以前は WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS と
 *          この 4000 を手動で一致させる必要があったが、Wdg_PBCfg.c が
 *          WDGM_HW_WATCHDOG_TIMEOUT_MS を直接引用して Wdg_Config を組み立てる
 *          ため、値の実体は 1 か所だけになった（詳細は Wdg_PBCfg.c 参照）。
 *
 *          本ファイルが .cpp である理由:
 *          RA の WDT ライブラリ (WDTimer クラス、グローバルインスタンス WDT) は
 *          C++ API のため、本ファイルは Can_Hw.cpp / Dio_Hw.cpp 等と同じ理由で
 *          C++ として実装する (Wdg.c からは Wdg_Hw.h の extern "C" 経由で
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
#include "Wdg_Hw.h"
#include "Det.h"
#include <WDT.h>

#define TAG "Wdg_Hw"

void Wdg_Hw_Enable(uint16 timeoutMs)
{
    if (!WDT.begin(timeoutMs))
    {
        DET_LOGE(TAG, "WDT.begin failed - HW watchdog NOT active");
    }
}

void Wdg_Hw_Disable(void)
{
    /* Renesas RA の IWDT は一度有効化すると FSP からの無効化手段がない。
     * Wdg_SetMode(WDGIF_OFF_MODE) が E_NOT_OK を返すことで上位層へ伝える
     * （詳細は Wdg.c の Wdg_SetMode() コメントを参照）。 */
}

void Wdg_Hw_Refresh(void)
{
    WDT.refresh();
}
