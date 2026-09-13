/**
 * \file    Bsw_BswM_fake.c
 * \brief   BswM.h のテスト用スパイ実装
 * \details Bsw_BswM_fake.h 冒頭のコメント参照。
 */
#include "Bsw_BswM_fake.h"

uint32 FakeBswM_ComM_CurrentModeCount = 0U;

uint8         FakeBswM_LastChannel = 0xFFU;
ComM_ModeType FakeBswM_LastMode    = 0xFFU;

void FakeBswM_Reset(void)
{
    FakeBswM_ComM_CurrentModeCount = 0U;
    FakeBswM_LastChannel           = 0xFFU;
    FakeBswM_LastMode              = 0xFFU;
}

void BswM_ComM_CurrentMode(uint8 channel, ComM_ModeType mode)
{
    FakeBswM_ComM_CurrentModeCount++;
    FakeBswM_LastChannel = channel;
    FakeBswM_LastMode    = mode;
}
