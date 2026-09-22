/**
 * \file    Wrap_Dcm.c
 * \brief   `src/Bsw/Dcm/Dcm_Cbk.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Dcm.h 参照）。
 */
#include "Wrap_Dcm.h"
#include "Det.h"

#define TAG "Dcm"

/* ======================================================================
 * External Variables
 * ====================================================================== */
uint32 CallCount_Dcm_Init                  = 0U;
uint32 CallCount_Dcm_MainFunction          = 0U;
uint32 CallCount_Dcm_GetVin                = 0U;
uint32 CallCount_Dcm_GetSesCtrlType        = 0U;
uint32 CallCount_Dcm_GetSecurityLevel      = 0U;
uint32 CallCount_Dcm_GetActiveProtocol     = 0U;
uint32 CallCount_Dcm_ResetToDefaultSession = 0U;
uint32 CallCount_Dcm_GetVersionInfo        = 0U;
uint32 CallCount_Dcm_ComIndication         = 0U;

uint32 FailFromCallCount_Dcm_GetVin                = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dcm_GetSesCtrlType         = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dcm_GetSecurityLevel       = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dcm_GetActiveProtocol      = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dcm_ResetToDefaultSession  = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_Dcm_GetVin                = E_NOT_OK;
Std_ReturnType ForcedReturn_Dcm_GetSesCtrlType        = E_NOT_OK;
Std_ReturnType ForcedReturn_Dcm_GetSecurityLevel      = E_NOT_OK;
Std_ReturnType ForcedReturn_Dcm_GetActiveProtocol     = E_NOT_OK;
Std_ReturnType ForcedReturn_Dcm_ResetToDefaultSession = E_NOT_OK;

PduIdType LastRxPduId_Dcm_ComIndication = 0U;

/* ----------------------------------------------------------------------
 * WrapDcm_Reset — 9関数すべての状態を一括で初期化する（Wrap_Dcm.h 参照）。
 * ---------------------------------------------------------------------- */
void WrapDcm_Reset(void)
{
    CallCount_Dcm_Init                  = 0U;
    CallCount_Dcm_MainFunction          = 0U;
    CallCount_Dcm_GetVin                = 0U;
    CallCount_Dcm_GetSesCtrlType        = 0U;
    CallCount_Dcm_GetSecurityLevel      = 0U;
    CallCount_Dcm_GetActiveProtocol     = 0U;
    CallCount_Dcm_ResetToDefaultSession = 0U;
    CallCount_Dcm_GetVersionInfo        = 0U;
    CallCount_Dcm_ComIndication         = 0U;

    FailFromCallCount_Dcm_GetVin               = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dcm_GetSesCtrlType        = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dcm_GetSecurityLevel      = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dcm_GetActiveProtocol     = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dcm_ResetToDefaultSession = WRAP_DCM_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_Dcm_GetVin                = E_NOT_OK;
    ForcedReturn_Dcm_GetSesCtrlType        = E_NOT_OK;
    ForcedReturn_Dcm_GetSecurityLevel      = E_NOT_OK;
    ForcedReturn_Dcm_GetActiveProtocol     = E_NOT_OK;
    ForcedReturn_Dcm_ResetToDefaultSession = E_NOT_OK;

    LastRxPduId_Dcm_ComIndication = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dcm_Init
 * ---------------------------------------------------------------------- */
extern
void __real_Dcm_Init(const Dcm_ConfigType* ConfigPtr);
void __wrap_Dcm_Init(const Dcm_ConfigType* ConfigPtr)
{
    CallCount_Dcm_Init++;
    Log_Write(LOG_T, TAG, "Dcm_Init", "called %u times", CallCount_Dcm_Init);

    __real_Dcm_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * Dcm_MainFunction
 * ---------------------------------------------------------------------- */
extern
void __real_Dcm_MainFunction(void);
void __wrap_Dcm_MainFunction(void)
{
    CallCount_Dcm_MainFunction++;
    Log_Write(LOG_T, TAG, "Dcm_MainFunction", "called %u times", CallCount_Dcm_MainFunction);

    __real_Dcm_MainFunction();
}

/* ----------------------------------------------------------------------
 * Dcm_GetVin
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_Dcm_GetVin(uint8* Data);
Std_ReturnType __wrap_Dcm_GetVin(uint8* Data)
{
    CallCount_Dcm_GetVin++;
    Log_Write(LOG_T, TAG, "Dcm_GetVin", "called %u times", CallCount_Dcm_GetVin);

    if (CallCount_Dcm_GetVin >= FailFromCallCount_Dcm_GetVin)
    {
        return ForcedReturn_Dcm_GetVin;
    }

    return __real_Dcm_GetVin(Data);
}

/* ----------------------------------------------------------------------
 * Dcm_GetSesCtrlType
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_Dcm_GetSesCtrlType(Dcm_SesCtrlType* SesCtrlType);
Std_ReturnType __wrap_Dcm_GetSesCtrlType(Dcm_SesCtrlType* SesCtrlType)
{
    CallCount_Dcm_GetSesCtrlType++;
    Log_Write(LOG_T, TAG, "Dcm_GetSesCtrlType", "called %u times", CallCount_Dcm_GetSesCtrlType);

    if (CallCount_Dcm_GetSesCtrlType >= FailFromCallCount_Dcm_GetSesCtrlType)
    {
        return ForcedReturn_Dcm_GetSesCtrlType;
    }

    return __real_Dcm_GetSesCtrlType(SesCtrlType);
}

/* ----------------------------------------------------------------------
 * Dcm_GetSecurityLevel
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_Dcm_GetSecurityLevel(Dcm_SecLevelType* SecLevel);
Std_ReturnType __wrap_Dcm_GetSecurityLevel(Dcm_SecLevelType* SecLevel)
{
    CallCount_Dcm_GetSecurityLevel++;
    Log_Write(LOG_T, TAG, "Dcm_GetSecurityLevel", "called %u times", CallCount_Dcm_GetSecurityLevel);

    if (CallCount_Dcm_GetSecurityLevel >= FailFromCallCount_Dcm_GetSecurityLevel)
    {
        return ForcedReturn_Dcm_GetSecurityLevel;
    }

    return __real_Dcm_GetSecurityLevel(SecLevel);
}

/* ----------------------------------------------------------------------
 * Dcm_GetActiveProtocol
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_Dcm_GetActiveProtocol(Dcm_ProtocolType* ActiveProtocolType, uint16* ConnectionId, uint16* TesterSourceAddress);
Std_ReturnType __wrap_Dcm_GetActiveProtocol(Dcm_ProtocolType* ActiveProtocolType, uint16* ConnectionId, uint16* TesterSourceAddress)
{
    CallCount_Dcm_GetActiveProtocol++;
    Log_Write(LOG_T, TAG, "Dcm_GetActiveProtocol", "called %u times", CallCount_Dcm_GetActiveProtocol);

    if (CallCount_Dcm_GetActiveProtocol >= FailFromCallCount_Dcm_GetActiveProtocol)
    {
        return ForcedReturn_Dcm_GetActiveProtocol;
    }

    return __real_Dcm_GetActiveProtocol(ActiveProtocolType, ConnectionId, TesterSourceAddress);
}

/* ----------------------------------------------------------------------
 * Dcm_ResetToDefaultSession
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_Dcm_ResetToDefaultSession(void);
Std_ReturnType __wrap_Dcm_ResetToDefaultSession(void)
{
    CallCount_Dcm_ResetToDefaultSession++;
    Log_Write(LOG_T, TAG, "Dcm_ResetToDefaultSession", "called %u times", CallCount_Dcm_ResetToDefaultSession);

    if (CallCount_Dcm_ResetToDefaultSession >= FailFromCallCount_Dcm_ResetToDefaultSession)
    {
        return ForcedReturn_Dcm_ResetToDefaultSession;
    }

    return __real_Dcm_ResetToDefaultSession();
}

/* ----------------------------------------------------------------------
 * Dcm_GetVersionInfo
 * ---------------------------------------------------------------------- */
extern
void __real_Dcm_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_Dcm_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_Dcm_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "Dcm_GetVersionInfo", "called %u times", CallCount_Dcm_GetVersionInfo);

    __real_Dcm_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * Dcm_ComIndication
 * ---------------------------------------------------------------------- */
extern
void __real_Dcm_ComIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
void __wrap_Dcm_ComIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_Dcm_ComIndication++;
    LastRxPduId_Dcm_ComIndication = RxPduId;
    Log_Write(LOG_T, TAG, "Dcm_ComIndication", "called %u times", CallCount_Dcm_ComIndication);

    __real_Dcm_ComIndication(RxPduId, PduInfoPtr);
}
