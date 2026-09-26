/**
 * \file    Wrap_CanNm.c
 * \brief   `src/Bsw/CanNm/CanNm.c` 内の関数を対象とした wrap 実体
 *          （Wrap_CanNm.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_CanNm.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "CanNm"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_CanNm_Init                     = 0U;
uint32 CallCount_CanNm_DeInit                   = 0U;
uint32 CallCount_CanNm_NetworkRequest           = 0U;
uint32 CallCount_CanNm_NetworkRelease           = 0U;
uint32 CallCount_CanNm_DisableCommunication     = 0U;
uint32 CallCount_CanNm_EnableCommunication      = 0U;
uint32 CallCount_CanNm_GetNodeIdentifier        = 0U;
uint32 CallCount_CanNm_GetLocalNodeIdentifier   = 0U;
uint32 CallCount_CanNm_RepeatMessageRequest     = 0U;
uint32 CallCount_CanNm_GetState                 = 0U;
uint32 CallCount_CanNm_GetVersionInfo           = 0U;
uint32 CallCount_CanNm_TxConfirmation           = 0U;
uint32 CallCount_CanNm_RxIndication             = 0U;
uint32 CallCount_CanNm_MainFunction             = 0U;

uint32 FailFromCallCount_CanNm_NetworkRequest         = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanNm_NetworkRelease         = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanNm_DisableCommunication   = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanNm_EnableCommunication    = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanNm_GetNodeIdentifier      = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanNm_GetLocalNodeIdentifier = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanNm_RepeatMessageRequest   = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanNm_GetState               = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_CanNm_NetworkRequest         = E_NOT_OK;
Std_ReturnType ForcedReturn_CanNm_NetworkRelease         = E_NOT_OK;
Std_ReturnType ForcedReturn_CanNm_DisableCommunication   = E_NOT_OK;
Std_ReturnType ForcedReturn_CanNm_EnableCommunication    = E_NOT_OK;
Std_ReturnType ForcedReturn_CanNm_GetNodeIdentifier      = E_NOT_OK;
Std_ReturnType ForcedReturn_CanNm_GetLocalNodeIdentifier = E_NOT_OK;
Std_ReturnType ForcedReturn_CanNm_RepeatMessageRequest   = E_NOT_OK;
Std_ReturnType ForcedReturn_CanNm_GetState               = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapCanNm_Reset — 14関数すべての状態を一括で初期化する（Wrap_CanNm.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapCanNm_Reset(void)
{
    CallCount_CanNm_Init                   = 0U;
    CallCount_CanNm_DeInit                 = 0U;
    CallCount_CanNm_NetworkRequest         = 0U;
    CallCount_CanNm_NetworkRelease         = 0U;
    CallCount_CanNm_DisableCommunication   = 0U;
    CallCount_CanNm_EnableCommunication    = 0U;
    CallCount_CanNm_GetNodeIdentifier      = 0U;
    CallCount_CanNm_GetLocalNodeIdentifier = 0U;
    CallCount_CanNm_RepeatMessageRequest   = 0U;
    CallCount_CanNm_GetState               = 0U;
    CallCount_CanNm_GetVersionInfo         = 0U;
    CallCount_CanNm_TxConfirmation         = 0U;
    CallCount_CanNm_RxIndication           = 0U;
    CallCount_CanNm_MainFunction           = 0U;

    FailFromCallCount_CanNm_NetworkRequest         = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanNm_NetworkRelease         = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanNm_DisableCommunication   = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanNm_EnableCommunication    = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanNm_GetNodeIdentifier      = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanNm_GetLocalNodeIdentifier = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanNm_RepeatMessageRequest   = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanNm_GetState               = WRAP_CANNM_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_CanNm_NetworkRequest         = E_NOT_OK;
    ForcedReturn_CanNm_NetworkRelease         = E_NOT_OK;
    ForcedReturn_CanNm_DisableCommunication   = E_NOT_OK;
    ForcedReturn_CanNm_EnableCommunication    = E_NOT_OK;
    ForcedReturn_CanNm_GetNodeIdentifier      = E_NOT_OK;
    ForcedReturn_CanNm_GetLocalNodeIdentifier = E_NOT_OK;
    ForcedReturn_CanNm_RepeatMessageRequest   = E_NOT_OK;
    ForcedReturn_CanNm_GetState               = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanNm_Init
 * ---------------------------------------------------------------------- */

extern
void __real_CanNm_Init(const CanNm_ConfigType* ConfigPtr);
void __wrap_CanNm_Init(const CanNm_ConfigType* ConfigPtr)
{
    CallCount_CanNm_Init++;
    Log_Write(LOG_T, TAG, "CanNm_Init", "called %u times", CallCount_CanNm_Init);

    __real_CanNm_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * CanNm_DeInit
 * ---------------------------------------------------------------------- */

extern
void __real_CanNm_DeInit(void);
void __wrap_CanNm_DeInit(void)
{
    CallCount_CanNm_DeInit++;
    Log_Write(LOG_T, TAG, "CanNm_DeInit", "called %u times", CallCount_CanNm_DeInit);

    __real_CanNm_DeInit();
}

/* ----------------------------------------------------------------------
 * CanNm_PassiveStartUp
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanNm_NetworkRequest
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanNm_NetworkRequest(NetworkHandleType Channel);
Std_ReturnType __wrap_CanNm_NetworkRequest(NetworkHandleType Channel)
{
    CallCount_CanNm_NetworkRequest++;
    Log_Write(LOG_T, TAG, "CanNm_NetworkRequest", "called %u times", CallCount_CanNm_NetworkRequest);

    if (CallCount_CanNm_NetworkRequest >= FailFromCallCount_CanNm_NetworkRequest)
    {
        return ForcedReturn_CanNm_NetworkRequest;
    }

    return __real_CanNm_NetworkRequest(Channel);
}

/* ----------------------------------------------------------------------
 * CanNm_NetworkRelease
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanNm_NetworkRelease(NetworkHandleType Channel);
Std_ReturnType __wrap_CanNm_NetworkRelease(NetworkHandleType Channel)
{
    CallCount_CanNm_NetworkRelease++;
    Log_Write(LOG_T, TAG, "CanNm_NetworkRelease", "called %u times", CallCount_CanNm_NetworkRelease);

    if (CallCount_CanNm_NetworkRelease >= FailFromCallCount_CanNm_NetworkRelease)
    {
        return ForcedReturn_CanNm_NetworkRelease;
    }

    return __real_CanNm_NetworkRelease(Channel);
}

/* ----------------------------------------------------------------------
 * CanNm_DisableCommunication
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanNm_DisableCommunication(NetworkHandleType Channel);
Std_ReturnType __wrap_CanNm_DisableCommunication(NetworkHandleType Channel)
{
    CallCount_CanNm_DisableCommunication++;
    Log_Write(LOG_T, TAG, "CanNm_DisableCommunication", "called %u times", CallCount_CanNm_DisableCommunication);

    if (CallCount_CanNm_DisableCommunication >= FailFromCallCount_CanNm_DisableCommunication)
    {
        return ForcedReturn_CanNm_DisableCommunication;
    }

    return __real_CanNm_DisableCommunication(Channel);
}

/* ----------------------------------------------------------------------
 * CanNm_EnableCommunication
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanNm_EnableCommunication(NetworkHandleType Channel);
Std_ReturnType __wrap_CanNm_EnableCommunication(NetworkHandleType Channel)
{
    CallCount_CanNm_EnableCommunication++;
    Log_Write(LOG_T, TAG, "CanNm_EnableCommunication", "called %u times", CallCount_CanNm_EnableCommunication);

    if (CallCount_CanNm_EnableCommunication >= FailFromCallCount_CanNm_EnableCommunication)
    {
        return ForcedReturn_CanNm_EnableCommunication;
    }

    return __real_CanNm_EnableCommunication(Channel);
}

/* ----------------------------------------------------------------------
 * CanNm_SetUserData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanNm_GetUserData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanNm_Transmit
 * ---------------------------------------------------------------------- */

 /* 未実装 */

/* ----------------------------------------------------------------------
 * CanNm_GetNodeIdentifier
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanNm_GetNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr);
Std_ReturnType __wrap_CanNm_GetNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr)
{
    CallCount_CanNm_GetNodeIdentifier++;
    Log_Write(LOG_T, TAG, "CanNm_GetNodeIdentifier", "called %u times", CallCount_CanNm_GetNodeIdentifier);

    if (CallCount_CanNm_GetNodeIdentifier >= FailFromCallCount_CanNm_GetNodeIdentifier)
    {
        return ForcedReturn_CanNm_GetNodeIdentifier;
    }

    return __real_CanNm_GetNodeIdentifier(Channel, nmNodeIdPtr);
}

/* ----------------------------------------------------------------------
 * CanNm_GetLocalNodeIdentifier
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanNm_GetLocalNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr);
Std_ReturnType __wrap_CanNm_GetLocalNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr)
{
    CallCount_CanNm_GetLocalNodeIdentifier++;
    Log_Write(LOG_T, TAG, "CanNm_GetLocalNodeIdentifier", "called %u times", CallCount_CanNm_GetLocalNodeIdentifier);

    if (CallCount_CanNm_GetLocalNodeIdentifier >= FailFromCallCount_CanNm_GetLocalNodeIdentifier)
    {
        return ForcedReturn_CanNm_GetLocalNodeIdentifier;
    }

    return __real_CanNm_GetLocalNodeIdentifier(Channel, nmNodeIdPtr);
}

/* ----------------------------------------------------------------------
 * CanNm_RepeatMessageRequest
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanNm_RepeatMessageRequest(NetworkHandleType Channel);
Std_ReturnType __wrap_CanNm_RepeatMessageRequest(NetworkHandleType Channel)
{
    CallCount_CanNm_RepeatMessageRequest++;
    Log_Write(LOG_T, TAG, "CanNm_RepeatMessageRequest", "called %u times", CallCount_CanNm_RepeatMessageRequest);

    if (CallCount_CanNm_RepeatMessageRequest >= FailFromCallCount_CanNm_RepeatMessageRequest)
    {
        return ForcedReturn_CanNm_RepeatMessageRequest;
    }

    return __real_CanNm_RepeatMessageRequest(Channel);
}

/* ----------------------------------------------------------------------
 * CanNm_GetPduData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanNm_GetState
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanNm_GetState(NetworkHandleType Channel, CanNm_StateType* StatePtr, CanNm_ModeType* ModePtr);
Std_ReturnType __wrap_CanNm_GetState(NetworkHandleType Channel, CanNm_StateType* StatePtr, CanNm_ModeType* ModePtr)
{
    CallCount_CanNm_GetState++;
    Log_Write(LOG_T, TAG, "CanNm_GetState", "called %u times", CallCount_CanNm_GetState);

    if (CallCount_CanNm_GetState >= FailFromCallCount_CanNm_GetState)
    {
        return ForcedReturn_CanNm_GetState;
    }

    return __real_CanNm_GetState(Channel, StatePtr, ModePtr);
}

/* ----------------------------------------------------------------------
 * CanNm_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_CanNm_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_CanNm_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_CanNm_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "CanNm_GetVersionInfo", "called %u times", CallCount_CanNm_GetVersionInfo);

    __real_CanNm_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * CanNm_RequestBusSynchronization
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanNm_CheckRemoteSleepIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanNm_SetSleepReadyBit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Call-back Notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanNm_TxConfirmation
 * ---------------------------------------------------------------------- */

extern
void __real_CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
void __wrap_CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    CallCount_CanNm_TxConfirmation++;
    Log_Write(LOG_T, TAG, "CanNm_TxConfirmation", "called %u times", CallCount_CanNm_TxConfirmation);

    __real_CanNm_TxConfirmation(TxPduId, result);
}

/* ----------------------------------------------------------------------
 * CanNm_RxIndication
 * ---------------------------------------------------------------------- */

extern
void __real_CanNm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
void __wrap_CanNm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_CanNm_RxIndication++;
    Log_Write(LOG_T, TAG, "CanNm_RxIndication", "called %u times", CallCount_CanNm_RxIndication);

    __real_CanNm_RxIndication(RxPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * CanNm_ConfirmPnAvailability
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanNm_TriggerTransmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Scheduled Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanNm_MainFunction
 * ---------------------------------------------------------------------- */

extern
void __real_CanNm_MainFunction(void);
void __wrap_CanNm_MainFunction(void)
{
    CallCount_CanNm_MainFunction++;
    Log_Write(LOG_T, TAG, "CanNm_MainFunction", "called %u times", CallCount_CanNm_MainFunction);

    __real_CanNm_MainFunction();
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
