/**
 * \file    Wrap_Nm.c
 * \brief   `src/Bsw/Nm/Nm.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Nm.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_CanNm.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Nm.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Nm"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Nm_Init                     = 0U;
uint32 CallCount_Nm_NetworkRequest           = 0U;
uint32 CallCount_Nm_NetworkRelease           = 0U;
uint32 CallCount_Nm_DisableCommunication     = 0U;
uint32 CallCount_Nm_EnableCommunication      = 0U;
uint32 CallCount_Nm_RepeatMessageRequest     = 0U;
uint32 CallCount_Nm_GetNodeIdentifier        = 0U;
uint32 CallCount_Nm_GetLocalNodeIdentifier   = 0U;
uint32 CallCount_Nm_GetState                 = 0U;
uint32 CallCount_Nm_GetVersionInfo           = 0U;
uint32 CallCount_Nm_NetworkStartIndication   = 0U;
uint32 CallCount_Nm_NetworkMode              = 0U;
uint32 CallCount_Nm_BusSleepMode             = 0U;
uint32 CallCount_Nm_PrepareBusSleepMode      = 0U;

uint32 FailFromCallCount_Nm_NetworkRequest         = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Nm_NetworkRelease         = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Nm_DisableCommunication   = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Nm_EnableCommunication    = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Nm_RepeatMessageRequest   = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Nm_GetNodeIdentifier      = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Nm_GetLocalNodeIdentifier = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Nm_GetState               = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_Nm_NetworkRequest         = E_NOT_OK;
Std_ReturnType ForcedReturn_Nm_NetworkRelease         = E_NOT_OK;
Std_ReturnType ForcedReturn_Nm_DisableCommunication   = E_NOT_OK;
Std_ReturnType ForcedReturn_Nm_EnableCommunication    = E_NOT_OK;
Std_ReturnType ForcedReturn_Nm_RepeatMessageRequest   = E_NOT_OK;
Std_ReturnType ForcedReturn_Nm_GetNodeIdentifier      = E_NOT_OK;
Std_ReturnType ForcedReturn_Nm_GetLocalNodeIdentifier = E_NOT_OK;
Std_ReturnType ForcedReturn_Nm_GetState               = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapNm_Reset — 14関数すべての状態を一括で初期化する（Wrap_Nm.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapNm_Reset(void)
{
    CallCount_Nm_Init                   = 0U;
    CallCount_Nm_NetworkRequest         = 0U;
    CallCount_Nm_NetworkRelease         = 0U;
    CallCount_Nm_DisableCommunication   = 0U;
    CallCount_Nm_EnableCommunication    = 0U;
    CallCount_Nm_RepeatMessageRequest   = 0U;
    CallCount_Nm_GetNodeIdentifier      = 0U;
    CallCount_Nm_GetLocalNodeIdentifier = 0U;
    CallCount_Nm_GetState               = 0U;
    CallCount_Nm_GetVersionInfo         = 0U;
    CallCount_Nm_NetworkStartIndication = 0U;
    CallCount_Nm_NetworkMode            = 0U;
    CallCount_Nm_BusSleepMode           = 0U;
    CallCount_Nm_PrepareBusSleepMode    = 0U;

    FailFromCallCount_Nm_NetworkRequest         = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Nm_NetworkRelease         = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Nm_DisableCommunication   = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Nm_EnableCommunication    = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Nm_RepeatMessageRequest   = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Nm_GetNodeIdentifier      = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Nm_GetLocalNodeIdentifier = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Nm_GetState               = WRAP_NM_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_Nm_NetworkRequest         = E_NOT_OK;
    ForcedReturn_Nm_NetworkRelease         = E_NOT_OK;
    ForcedReturn_Nm_DisableCommunication   = E_NOT_OK;
    ForcedReturn_Nm_EnableCommunication    = E_NOT_OK;
    ForcedReturn_Nm_RepeatMessageRequest   = E_NOT_OK;
    ForcedReturn_Nm_GetNodeIdentifier      = E_NOT_OK;
    ForcedReturn_Nm_GetLocalNodeIdentifier = E_NOT_OK;
    ForcedReturn_Nm_GetState               = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_Init
 * ---------------------------------------------------------------------- */

extern
void __real_Nm_Init(const Nm_ConfigType* ConfigPtr);
void __wrap_Nm_Init(const Nm_ConfigType* ConfigPtr)
{
    CallCount_Nm_Init++;
    Log_Write(LOG_T, TAG, "Nm_Init", "called %u times", CallCount_Nm_Init);

    __real_Nm_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * Nm_PassiveStartUp
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_NetworkRequest
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Nm_NetworkRequest(NetworkHandleType Channel);
Std_ReturnType __wrap_Nm_NetworkRequest(NetworkHandleType Channel)
{
    CallCount_Nm_NetworkRequest++;
    Log_Write(LOG_T, TAG, "Nm_NetworkRequest", "called %u times", CallCount_Nm_NetworkRequest);

    if (CallCount_Nm_NetworkRequest >= FailFromCallCount_Nm_NetworkRequest)
    {
        return ForcedReturn_Nm_NetworkRequest;
    }

    return __real_Nm_NetworkRequest(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_NetworkRelease
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Nm_NetworkRelease(NetworkHandleType Channel);
Std_ReturnType __wrap_Nm_NetworkRelease(NetworkHandleType Channel)
{
    CallCount_Nm_NetworkRelease++;
    Log_Write(LOG_T, TAG, "Nm_NetworkRelease", "called %u times", CallCount_Nm_NetworkRelease);

    if (CallCount_Nm_NetworkRelease >= FailFromCallCount_Nm_NetworkRelease)
    {
        return ForcedReturn_Nm_NetworkRelease;
    }

    return __real_Nm_NetworkRelease(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_DisableCommunication
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Nm_DisableCommunication(NetworkHandleType Channel);
Std_ReturnType __wrap_Nm_DisableCommunication(NetworkHandleType Channel)
{
    CallCount_Nm_DisableCommunication++;
    Log_Write(LOG_T, TAG, "Nm_DisableCommunication", "called %u times", CallCount_Nm_DisableCommunication);

    if (CallCount_Nm_DisableCommunication >= FailFromCallCount_Nm_DisableCommunication)
    {
        return ForcedReturn_Nm_DisableCommunication;
    }

    return __real_Nm_DisableCommunication(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_EnableCommunication
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Nm_EnableCommunication(NetworkHandleType Channel);
Std_ReturnType __wrap_Nm_EnableCommunication(NetworkHandleType Channel)
{
    CallCount_Nm_EnableCommunication++;
    Log_Write(LOG_T, TAG, "Nm_EnableCommunication", "called %u times", CallCount_Nm_EnableCommunication);

    if (CallCount_Nm_EnableCommunication >= FailFromCallCount_Nm_EnableCommunication)
    {
        return ForcedReturn_Nm_EnableCommunication;
    }

    return __real_Nm_EnableCommunication(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_SetUserData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_GetUserData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_GetPduData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_RepeatMessageRequest
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Nm_RepeatMessageRequest(NetworkHandleType Channel);
Std_ReturnType __wrap_Nm_RepeatMessageRequest(NetworkHandleType Channel)
{
    CallCount_Nm_RepeatMessageRequest++;
    Log_Write(LOG_T, TAG, "Nm_RepeatMessageRequest", "called %u times", CallCount_Nm_RepeatMessageRequest);

    if (CallCount_Nm_RepeatMessageRequest >= FailFromCallCount_Nm_RepeatMessageRequest)
    {
        return ForcedReturn_Nm_RepeatMessageRequest;
    }

    return __real_Nm_RepeatMessageRequest(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_GetNodeIdentifier
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Nm_GetNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr);
Std_ReturnType __wrap_Nm_GetNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr)
{
    CallCount_Nm_GetNodeIdentifier++;
    Log_Write(LOG_T, TAG, "Nm_GetNodeIdentifier", "called %u times", CallCount_Nm_GetNodeIdentifier);

    if (CallCount_Nm_GetNodeIdentifier >= FailFromCallCount_Nm_GetNodeIdentifier)
    {
        return ForcedReturn_Nm_GetNodeIdentifier;
    }

    return __real_Nm_GetNodeIdentifier(Channel, nmNodeIdPtr);
}

/* ----------------------------------------------------------------------
 * Nm_GetLocalNodeIdentifier
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Nm_GetLocalNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr);
Std_ReturnType __wrap_Nm_GetLocalNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr)
{
    CallCount_Nm_GetLocalNodeIdentifier++;
    Log_Write(LOG_T, TAG, "Nm_GetLocalNodeIdentifier", "called %u times", CallCount_Nm_GetLocalNodeIdentifier);

    if (CallCount_Nm_GetLocalNodeIdentifier >= FailFromCallCount_Nm_GetLocalNodeIdentifier)
    {
        return ForcedReturn_Nm_GetLocalNodeIdentifier;
    }

    return __real_Nm_GetLocalNodeIdentifier(Channel, nmNodeIdPtr);
}

/* ----------------------------------------------------------------------
 * Nm_CheckRemoteSleepIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_GetState
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Nm_GetState(NetworkHandleType Channel, Nm_StateType* nmStatePtr, Nm_ModeType* nmModePtr);
Std_ReturnType __wrap_Nm_GetState(NetworkHandleType Channel, Nm_StateType* nmStatePtr, Nm_ModeType* nmModePtr)
{
    CallCount_Nm_GetState++;
    Log_Write(LOG_T, TAG, "Nm_GetState", "called %u times", CallCount_Nm_GetState);

    if (CallCount_Nm_GetState >= FailFromCallCount_Nm_GetState)
    {
        return ForcedReturn_Nm_GetState;
    }

    return __real_Nm_GetState(Channel, nmStatePtr, nmModePtr);
}

/* ----------------------------------------------------------------------
 * Nm_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_Nm_GetVersionInfo(Std_VersionInfoType* nmVerInfoPtr);
void __wrap_Nm_GetVersionInfo(Std_VersionInfoType* nmVerInfoPtr)
{
    CallCount_Nm_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "Nm_GetVersionInfo", "called %u times", CallCount_Nm_GetVersionInfo);

    __real_Nm_GetVersionInfo(nmVerInfoPtr);
}

/* ======================================================================
 * Call-back Notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_NetworkStartIndication
 * ---------------------------------------------------------------------- */

extern
void __real_Nm_NetworkStartIndication(NetworkHandleType Channel);
void __wrap_Nm_NetworkStartIndication(NetworkHandleType Channel)
{
    CallCount_Nm_NetworkStartIndication++;
    Log_Write(LOG_T, TAG, "Nm_NetworkStartIndication", "called %u times", CallCount_Nm_NetworkStartIndication);

    __real_Nm_NetworkStartIndication(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_NetworkMode
 * ---------------------------------------------------------------------- */

extern
void __real_Nm_NetworkMode(NetworkHandleType Channel);
void __wrap_Nm_NetworkMode(NetworkHandleType Channel)
{
    CallCount_Nm_NetworkMode++;
    Log_Write(LOG_T, TAG, "Nm_NetworkMode", "called %u times", CallCount_Nm_NetworkMode);

    __real_Nm_NetworkMode(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_BusSleepMode
 * ---------------------------------------------------------------------- */

extern
void __real_Nm_BusSleepMode(NetworkHandleType Channel);
void __wrap_Nm_BusSleepMode(NetworkHandleType Channel)
{
    CallCount_Nm_BusSleepMode++;
    Log_Write(LOG_T, TAG, "Nm_BusSleepMode", "called %u times", CallCount_Nm_BusSleepMode);

    __real_Nm_BusSleepMode(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_PrepareBusSleepMode
 * ---------------------------------------------------------------------- */

extern
void __real_Nm_PrepareBusSleepMode(NetworkHandleType Channel);
void __wrap_Nm_PrepareBusSleepMode(NetworkHandleType Channel)
{
    CallCount_Nm_PrepareBusSleepMode++;
    Log_Write(LOG_T, TAG, "Nm_PrepareBusSleepMode", "called %u times", CallCount_Nm_PrepareBusSleepMode);

    __real_Nm_PrepareBusSleepMode(Channel);
}

/* ----------------------------------------------------------------------
 * Nm_RemoteSleepIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_RemoteSleepCancellation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_SynchronizationPoint
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_CoordReadyToSleepIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_CoordReadyToSleepCancellation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_PduRxIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_StateChangeNotification
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_RepeatMessageIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_TxTimeoutException
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Nm_CarWakeUpIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Scheduled Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Nm_MainFunction
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
