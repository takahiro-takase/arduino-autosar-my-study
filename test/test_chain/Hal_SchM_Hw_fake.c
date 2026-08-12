/**
 * \file    Hal_SchM_Hw_fake.c
 * \brief   SchM_Hw.h（noInterrupts/interrupts）のテスト用フェイク実装
 * \details テストはシングルスレッドで実行されるため（テストコードが
 *          Gpt_OnTick() を直接呼んで割り込みを模擬する。Hal_Gpt_Hw_fake.h
 *          参照）、実際に割り込みを無効化する必要はない。
 */
#include "SchM_Hw.h"

void SchM_Hw_EnterExclusiveArea(void)
{
}

void SchM_Hw_ExitExclusiveArea(void)
{
}
