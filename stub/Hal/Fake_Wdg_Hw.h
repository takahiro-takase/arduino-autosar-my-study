/**
 * \file    Fake_Wdg_Hw.h
 * \brief   Wdg_Hw.h（Renesas RA IWDT 境界）のテスト用フェイク実装の宣言
 * \details Wdg.c のロジックだけを検証したいので、実 HW（IWDT レジスタ）は
 *          使わず、呼び出し回数を記録するだけのフェイクに差し替える。
 */
#ifndef FAKE_WDG_HW_H
#define FAKE_WDG_HW_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeWdgHw_EnableCount;
extern uint32 FakeWdgHw_DisableCount;
extern uint32 FakeWdgHw_RefreshCount;
extern uint32 FakeWdgHw_ForceResetCount;
extern uint16 FakeWdgHw_LastEnableTimeoutMs;

/** 各テストケースの開始時に呼び、記録をすべて初期状態に戻す。 */
void FakeWdgHw_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_WDG_HW_H */
