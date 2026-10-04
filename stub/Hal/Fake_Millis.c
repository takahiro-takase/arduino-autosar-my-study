/**
 * \file    Fake_Millis.c
 * \brief   Arduino の millis() のテスト用フェイク実装。
 * \details Fake_Millis.h 冒頭のコメント参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Fake_Millis.h"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

unsigned long FakeMillis_Value = 0UL;
uint32        FakeMillis_DelayCallCount = 0U;
unsigned long FakeMillis_LastDelayMs    = 0UL;

/* ======================================================================
 * Functions
 * ====================================================================== */

void FakeMillis_Reset(void)
{
    FakeMillis_Value         = 0UL;
    FakeMillis_DelayCallCount = 0U;
    FakeMillis_LastDelayMs    = 0UL;
}

unsigned long millis(void)
{
    return FakeMillis_Value;
}

void delay(unsigned long ms)
{
    /* Fake_Millis.h 冒頭のコメント参照: テストを実時間で待たせても意味が
     * ないため実際には待たない。呼ばれたことと引数だけ記録する。 */
    FakeMillis_DelayCallCount++;
    FakeMillis_LastDelayMs = ms;
}
