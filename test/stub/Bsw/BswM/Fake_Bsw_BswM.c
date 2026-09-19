/**
 * \file    Fake_Bsw_BswM.c
 * \brief   BswM.h のテスト用スパイ実装
 * \details Fake_Bsw_BswM.h 冒頭のコメント参照。
 */
#include "Fake_Bsw_BswM.h"

uint32 FakeBswM_ComM_CurrentModeCount = 0U;

uint8         FakeBswM_LastChannel = 0xFFU;
ComM_ModeType FakeBswM_LastMode    = 0xFFU;

uint32 FakeBswM_DcmCommunicationModeCurrentStateCount = 0U;

NetworkHandleType         FakeBswM_LastNetwork              = 0xFFU;
Dcm_CommunicationModeType FakeBswM_LastDcmCommunicationMode = 0xFFU;

void FakeBswM_Reset(void)
{
    FakeBswM_ComM_CurrentModeCount = 0U;
    FakeBswM_LastChannel           = 0xFFU;
    FakeBswM_LastMode              = 0xFFU;

    FakeBswM_DcmCommunicationModeCurrentStateCount = 0U;
    FakeBswM_LastNetwork               = 0xFFU;
    FakeBswM_LastDcmCommunicationMode  = 0xFFU;
}

void BswM_ComM_CurrentMode(uint8 channel, ComM_ModeType mode)
{
    FakeBswM_ComM_CurrentModeCount++;
    FakeBswM_LastChannel = channel;
    FakeBswM_LastMode    = mode;
}

void BswM_Dcm_CommunicationMode_CurrentState(NetworkHandleType Network, Dcm_CommunicationModeType RequestedMode)
{
    FakeBswM_DcmCommunicationModeCurrentStateCount++;
    FakeBswM_LastNetwork              = Network;
    FakeBswM_LastDcmCommunicationMode = RequestedMode;
}
