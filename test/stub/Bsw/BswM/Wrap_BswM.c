/**
 * \file    Wrap_BswM.c
 * \brief   `src/Bsw/BswM/BswM.c` 内の関数を対象とした wrap 実体
 *          （Wrap_BswM.h 参照）。
 */
#include "Wrap_BswM.h"
#include "Det.h"

#define TAG "BswM"

/* ======================================================================
 * External Variables
 * ====================================================================== */
uint32 CallCount_BswM_Init                                = 0U;
uint32 CallCount_BswM_Deinit                              = 0U;
uint32 CallCount_BswM_GetVersionInfo                      = 0U;
uint32 CallCount_BswM_EcuM_CurrentState                   = 0U;
uint32 CallCount_BswM_ComM_CurrentMode                    = 0U;
uint32 CallCount_BswM_Dcm_CommunicationMode_CurrentState  = 0U;

NetworkHandleType LastChannel_BswM_ComM_CurrentMode = 0U;
ComM_ModeType     LastMode_BswM_ComM_CurrentMode    = COMM_NO_COMMUNICATION;

NetworkHandleType         LastNetwork_BswM_Dcm_CommunicationMode_CurrentState       = 0U;
Dcm_CommunicationModeType LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState = DCM_ENABLE_RX_TX_NORM_NM;

/* ----------------------------------------------------------------------
 * WrapBswM_Reset — 6関数すべての状態を一括で初期化する（Wrap_BswM.h 参照）。
 * ---------------------------------------------------------------------- */
void WrapBswM_Reset(void)
{
    CallCount_BswM_Init                               = 0U;
    CallCount_BswM_Deinit                             = 0U;
    CallCount_BswM_GetVersionInfo                     = 0U;
    CallCount_BswM_EcuM_CurrentState                  = 0U;
    CallCount_BswM_ComM_CurrentMode                   = 0U;
    CallCount_BswM_Dcm_CommunicationMode_CurrentState = 0U;

    LastChannel_BswM_ComM_CurrentMode = 0U;
    LastMode_BswM_ComM_CurrentMode    = COMM_NO_COMMUNICATION;

    LastNetwork_BswM_Dcm_CommunicationMode_CurrentState       = 0U;
    LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState = DCM_ENABLE_RX_TX_NORM_NM;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * BswM_Init
 * ---------------------------------------------------------------------- */
extern
void __real_BswM_Init(const BswM_ConfigType* ConfigPtr);
void __wrap_BswM_Init(const BswM_ConfigType* ConfigPtr)
{
    CallCount_BswM_Init++;
    Log_Write(LOG_T, TAG, "BswM_Init", "called %u times", CallCount_BswM_Init);

    __real_BswM_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * BswM_Deinit
 * ---------------------------------------------------------------------- */
extern
void __real_BswM_Deinit(void);
void __wrap_BswM_Deinit(void)
{
    CallCount_BswM_Deinit++;
    Log_Write(LOG_T, TAG, "BswM_Deinit", "called %u times", CallCount_BswM_Deinit);

    __real_BswM_Deinit();
}

/* ----------------------------------------------------------------------
 * BswM_GetVersionInfo
 * ---------------------------------------------------------------------- */
extern
void __real_BswM_GetVersionInfo(Std_VersionInfoType* VersionInfo);
void __wrap_BswM_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    CallCount_BswM_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "BswM_GetVersionInfo", "called %u times", CallCount_BswM_GetVersionInfo);

    __real_BswM_GetVersionInfo(VersionInfo);
}

/* ----------------------------------------------------------------------
 * BswM_EcuM_CurrentState
 * ---------------------------------------------------------------------- */
extern
void __real_BswM_EcuM_CurrentState(EcuM_StateType state);
void __wrap_BswM_EcuM_CurrentState(EcuM_StateType state)
{
    CallCount_BswM_EcuM_CurrentState++;
    Log_Write(LOG_T, TAG, "BswM_EcuM_CurrentState", "called %u times", CallCount_BswM_EcuM_CurrentState);

    __real_BswM_EcuM_CurrentState(state);
}

/* ----------------------------------------------------------------------
 * BswM_ComM_CurrentMode
 * ---------------------------------------------------------------------- */
extern
void __real_BswM_ComM_CurrentMode(NetworkHandleType channel, ComM_ModeType mode);
void __wrap_BswM_ComM_CurrentMode(NetworkHandleType channel, ComM_ModeType mode)
{
    CallCount_BswM_ComM_CurrentMode++;
    LastChannel_BswM_ComM_CurrentMode = channel;
    LastMode_BswM_ComM_CurrentMode    = mode;
    Log_Write(LOG_T, TAG, "BswM_ComM_CurrentMode", "called %u times", CallCount_BswM_ComM_CurrentMode);

    __real_BswM_ComM_CurrentMode(channel, mode);
}

/* ----------------------------------------------------------------------
 * BswM_Dcm_CommunicationMode_CurrentState
 * ---------------------------------------------------------------------- */
extern
void __real_BswM_Dcm_CommunicationMode_CurrentState(NetworkHandleType Network, Dcm_CommunicationModeType RequestedMode);
void __wrap_BswM_Dcm_CommunicationMode_CurrentState(NetworkHandleType Network, Dcm_CommunicationModeType RequestedMode)
{
    CallCount_BswM_Dcm_CommunicationMode_CurrentState++;
    LastNetwork_BswM_Dcm_CommunicationMode_CurrentState       = Network;
    LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState = RequestedMode;
    Log_Write(LOG_T, TAG, "BswM_Dcm_CommunicationMode_CurrentState", "called %u times", CallCount_BswM_Dcm_CommunicationMode_CurrentState);

    __real_BswM_Dcm_CommunicationMode_CurrentState(Network, RequestedMode);
}
