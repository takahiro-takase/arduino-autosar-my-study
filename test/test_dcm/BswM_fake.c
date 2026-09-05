/**
 * \file    BswM_fake.c
 * \brief   BswM_fake.h の実体。
 */
#include "BswM_fake.h"
#include "BswM.h"

NetworkHandleType         FakeBswM_LastNetwork;
Dcm_CommunicationModeType FakeBswM_LastMode;
uint32                    FakeBswM_CallCount;

void FakeBswM_Reset(void)
{
    FakeBswM_LastNetwork = 0U;
    FakeBswM_LastMode    = 0U;
    FakeBswM_CallCount   = 0U;
}

void BswM_Dcm_CommunicationMode_CurrentState(NetworkHandleType Network, Dcm_CommunicationModeType RequestedMode)
{
    FakeBswM_LastNetwork = Network;
    FakeBswM_LastMode    = RequestedMode;
    FakeBswM_CallCount++;
}
