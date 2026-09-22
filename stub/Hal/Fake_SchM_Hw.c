/**
 * \file    Fake_SchM_Hw.c
 * \brief   SchM_Hw.h（noInterrupts/interrupts）のテスト用フェイク実装
 * \details テストはシングルスレッドで実行されるため（テストコードが
 *          Gpt_OnTick() を直接呼んで割り込みを模擬する。Fake_Gpt_Hw.h
 *          参照）、実際に割り込みを無効化する必要はない。
 *
 *          2026-09、`test_native`/`test_chain` の2envで内容が重複していた
 *          ため、`Fake_Det_Hw.h` と同じ理由で `stub/Hal/` へ
 *          集約した（テスト専用アクセサを持たないため .h は無い）。
 */
#include "SchM_Hw.h"

void SchM_Hw_EnterExclusiveArea(void)
{
}

void SchM_Hw_ExitExclusiveArea(void)
{
}
