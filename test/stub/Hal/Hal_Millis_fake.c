/**
 * \file    Hal_Millis_fake.c
 * \brief   Arduino の millis() のテスト用フェイク実装。
 * \details Hal_Millis_fake.h 冒頭のコメント参照。
 */
#include "Hal_Millis_fake.h"

unsigned long FakeMillis_Value = 0UL;

void FakeMillis_Reset(void)
{
    FakeMillis_Value = 0UL;
}

unsigned long millis(void)
{
    return FakeMillis_Value;
}
