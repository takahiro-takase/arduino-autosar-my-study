/**
 * \file    Wrap_Gpt.h
 * \brief   `src/Bsw/Gpt/Gpt.c` 内の関数を対象とした `-Wl,--wrap=<symbol>`
 *          呼び出し記録のアクセサ群。
 *
 * \details `stub/` 配下の構成規則は `stub/Bsw/CanIf/Wrap_CanIf.h` 冒頭コメント
 *          参照（「wrap 対象の関数が定義されている元の src ファイル 1 つにつき
 *          1 ファイル」）。既定動作は `__real_...` へのパススルー。
 *
 *          `Gpt.h` が宣言する関数のうち実装済みの9関数を wrap 対象とする
 *          （`SetMode`/`DisableWakeup`/`EnableWakeup`/`CheckWakeup`/
 *          `GetPredefTimerValue` は未実装のため wrap 対象外）。
 *          全関数の戻り値は `Gpt_ValueType`（成功/失敗を表さない計測値）
 *          または `void` のため、故障注入は実装せず呼び出し回数のみ記録する。
 *
 *          変数名・Reset方針は
 *          `[[reference_wrap_stub_naming_convention]]` に統一する。
 */
#ifndef WRAP_GPT_H
#define WRAP_GPT_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "Gpt.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * 呼び出し回数（実装済み全9関数共通）
 * ---------------------------------------------------------------------- */
extern uint32 CallCount_Gpt_GetVersionInfo;
extern uint32 CallCount_Gpt_Init;
extern uint32 CallCount_Gpt_DeInit;
extern uint32 CallCount_Gpt_GetTimeElapsed;
extern uint32 CallCount_Gpt_GetTimeRemaining;
extern uint32 CallCount_Gpt_StartTimer;
extern uint32 CallCount_Gpt_StopTimer;
extern uint32 CallCount_Gpt_EnableNotification;
extern uint32 CallCount_Gpt_DisableNotification;

/* ======================================================================
 * Functions
 * ====================================================================== */

/** すべての関数呼び出し回数を初期状態へ戻す。
 *  各テストケースの開始時（SetUp()）に1回呼ぶ。 */
void WrapGpt_Reset(void);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* WRAP_GPT_H */
