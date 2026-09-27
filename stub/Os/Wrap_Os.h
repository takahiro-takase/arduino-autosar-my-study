/**
 * \file    Wrap_Os.h
 * \brief   `src/Os/Os.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          呼び出し記録のアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h` 冒頭コメント
 *          参照（「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `Os.h` が宣言する公開API全3関数を wrap 対象とする。全関数の戻り値
 *          は `void` のため、故障注入は実装せず呼び出し回数のみ記録する。
 *
 *          変数名・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_OS_H
#define WRAP_OS_H

#include "Std_Types.h"
#include "Os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * 呼び出し回数（公開API全3関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_Os_Init;
extern uint32 CallCount_Os_SchedulerStep;
extern uint32 CallCount_Os_SetTaskActive;

/** すべての関数呼び出し回数を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapOs_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WRAP_OS_H */
