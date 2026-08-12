/**
 * \file    Bsw_ComM_fake.c
 * \brief   ComM.h のテスト用スパイ実装
 * \details Bsw_ComM_fake.h 冒頭のコメント参照。
 */
#include "Bsw_ComM_fake.h"

uint32         FakeComM_BusSMIndicationCount = 0U;
uint8         FakeComM_LastNetwork          = 0xFFU;
ComM_ModeType FakeComM_LastMode             = 0xFFU;

void FakeComM_Reset(void)
{
    FakeComM_BusSMIndicationCount = 0U;
    FakeComM_LastNetwork          = 0xFFU;
    FakeComM_LastMode             = 0xFFU;
}

void ComM_BusSMIndication(uint8 Network, ComM_ModeType Mode)
{
    FakeComM_BusSMIndicationCount++;
    FakeComM_LastNetwork = Network;
    FakeComM_LastMode    = Mode;
}
