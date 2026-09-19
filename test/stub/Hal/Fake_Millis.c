/**
 * \file    Fake_Millis.c
 * \brief   Arduino の millis() のテスト用フェイク実装。
 * \details Fake_Millis.h 冒頭のコメント参照。
 */
#include "Fake_Millis.h"

unsigned long FakeMillis_Value = 0UL;

void FakeMillis_Reset(void)
{
    FakeMillis_Value = 0UL;
}

unsigned long millis(void)
{
    return FakeMillis_Value;
}
