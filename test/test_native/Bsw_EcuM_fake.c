/**
 * \file    Bsw_EcuM_fake.c
 * \brief   EcuM.h のテスト用フェイク実装（Can.c から見た上位層コールバック）
 * \details Bsw_EcuM_fake.h 冒頭のコメント参照。
 */
#include "Bsw_EcuM_fake.h"

uint32 FakeEcuM_CheckWakeupCount = 0U;
EcuM_WakeupSourceType FakeEcuM_LastWakeupSource = 0U;

void FakeEcuM_Reset(void)
{
    FakeEcuM_CheckWakeupCount = 0U;
    FakeEcuM_LastWakeupSource = 0U;
}

void EcuM_CheckWakeup(EcuM_WakeupSourceType wakeupSource)
{
    FakeEcuM_CheckWakeupCount++;
    FakeEcuM_LastWakeupSource = wakeupSource;
}
