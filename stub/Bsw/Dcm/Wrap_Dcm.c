/**
 * \file    Wrap_Dcm.c
 * \brief   `src/Bsw/Dcm/Dcm.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Dcm.h 参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Dcm.h"
#include "Det.h"
#include "Dcm_Cfg.h"
#include "Dcm_Cbk.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Dcm"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Dcm_Init                  = 0U;
uint32 CallCount_Dcm_MainFunction          = 0U;
uint32 CallCount_Dcm_GetVin                = 0U;
uint32 CallCount_Dcm_GetSesCtrlType        = 0U;
uint32 CallCount_Dcm_GetSecurityLevel      = 0U;
uint32 CallCount_Dcm_GetActiveProtocol     = 0U;
uint32 CallCount_Dcm_ResetToDefaultSession = 0U;
uint32 CallCount_Dcm_GetVersionInfo        = 0U;
uint32 CallCount_Dcm_StartOfReception     = 0U;
uint32 CallCount_Dcm_CopyRxData            = 0U;
uint32 CallCount_Dcm_TpRxIndication        = 0U;
uint32 CallCount_Dcm_CopyTxData            = 0U;
uint32 CallCount_Dcm_TpTxConfirmation      = 0U;

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

PduLengthType  LastTpSduLength_Dcm_StartOfReception = 0U;
Std_ReturnType LastResult_Dcm_TpRxIndication        = E_OK;
Std_ReturnType LastResult_Dcm_TpTxConfirmation      = E_OK;

/* ----------------------------------------------------------------------
 * WrapDcm_Reset — 全関数の状態を一括で初期化する（Wrap_Dcm.h 参照）。
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
    CallCount_Dcm_StartOfReception      = 0U;
    CallCount_Dcm_CopyRxData            = 0U;
    CallCount_Dcm_TpRxIndication        = 0U;
    CallCount_Dcm_CopyTxData            = 0U;
    CallCount_Dcm_TpTxConfirmation      = 0U;

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

    LastTpSduLength_Dcm_StartOfReception = 0U;
    LastResult_Dcm_TpRxIndication        = E_OK;
    LastResult_Dcm_TpTxConfirmation      = E_OK;
}

/* ----------------------------------------------------------------------
 * WrapDcm_InjectRequest — CanTp を介さずに UDS 要求を渡す（Wrap_Dcm.h 参照）。
 * ---------------------------------------------------------------------- */

Std_ReturnType WrapDcm_InjectRequest(const uint8* data, uint8 len)
{
    static uint8 work[DCM_RX_BUF_SIZE];
    PduInfoType   info;
    PduLengthType bufSize = 0U;
    uint8         i;

    if (len > DCM_RX_BUF_SIZE)
    {
        return E_NOT_OK;
    }
    for (i = 0U; i < len; i++)
    {
        work[i] = data[i];
    }

    if (Dcm_StartOfReception(0U, NULL, (PduLengthType)len, &bufSize) != BUFREQ_OK)
    {
        return E_NOT_OK;
    }

    info.SduDataPtr = work;
    info.SduLength  = (PduLengthType)len;
    if (Dcm_CopyRxData(0U, &info, &bufSize) != BUFREQ_OK)
    {
        Dcm_TpRxIndication(0U, E_NOT_OK);
        return E_NOT_OK;
    }

    Dcm_TpRxIndication(0U, E_OK);
    return E_OK;
}

/* ======================================================================
 * Functions provided for other BSW components
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

/* -----------------------------------------------------------------------
 * Dcm_DemTriggerOnDTCStatus
 * ----------------------------------------------------------------------- */

/* 未実装 */

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

/* ======================================================================
 * Functions provided to BSW modules and to SW-Cs
 * ====================================================================== */

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

/* -----------------------------------------------------------------------
 * Dcm_TriggerOnEvent
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_SetActiveDiagnostic
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Callback notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dcm_StartOfReception
 * ---------------------------------------------------------------------- */

extern
BufReq_ReturnType __real_Dcm_StartOfReception(PduIdType id, const PduInfoType* info, PduLengthType TpSduLength, PduLengthType* bufferSizePtr);
BufReq_ReturnType __wrap_Dcm_StartOfReception(PduIdType id, const PduInfoType* info, PduLengthType TpSduLength, PduLengthType* bufferSizePtr)
{
    CallCount_Dcm_StartOfReception++;
    LastTpSduLength_Dcm_StartOfReception = TpSduLength;
    Log_Write(LOG_T, TAG, "Dcm_StartOfReception", "called %u times", CallCount_Dcm_StartOfReception);

    return __real_Dcm_StartOfReception(id, info, TpSduLength, bufferSizePtr);
}

/* ----------------------------------------------------------------------
 * Dcm_CopyRxData
 * ---------------------------------------------------------------------- */

extern
BufReq_ReturnType __real_Dcm_CopyRxData(PduIdType id, const PduInfoType* info, PduLengthType* bufferSizePtr);
BufReq_ReturnType __wrap_Dcm_CopyRxData(PduIdType id, const PduInfoType* info, PduLengthType* bufferSizePtr)
{
    CallCount_Dcm_CopyRxData++;
    Log_Write(LOG_T, TAG, "Dcm_CopyRxData", "called %u times", CallCount_Dcm_CopyRxData);

    return __real_Dcm_CopyRxData(id, info, bufferSizePtr);
}

/* ----------------------------------------------------------------------
 * Dcm_TpRxIndication
 * ---------------------------------------------------------------------- */

extern
void __real_Dcm_TpRxIndication(PduIdType id, Std_ReturnType result);
void __wrap_Dcm_TpRxIndication(PduIdType id, Std_ReturnType result)
{
    CallCount_Dcm_TpRxIndication++;
    LastResult_Dcm_TpRxIndication = result;
    Log_Write(LOG_T, TAG, "Dcm_TpRxIndication", "called %u times", CallCount_Dcm_TpRxIndication);

    __real_Dcm_TpRxIndication(id, result);
}

/* ----------------------------------------------------------------------
 * Dcm_CopyTxData
 * ---------------------------------------------------------------------- */

extern
BufReq_ReturnType __real_Dcm_CopyTxData(PduIdType id, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr);
BufReq_ReturnType __wrap_Dcm_CopyTxData(PduIdType id, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr)
{
    CallCount_Dcm_CopyTxData++;
    Log_Write(LOG_T, TAG, "Dcm_CopyTxData", "called %u times", CallCount_Dcm_CopyTxData);

    return __real_Dcm_CopyTxData(id, info, retry, availableDataPtr);
}

/* ----------------------------------------------------------------------
 * Dcm_TpTxConfirmation
 * ---------------------------------------------------------------------- */

extern
void __real_Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result);
void __wrap_Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result)
{
    CallCount_Dcm_TpTxConfirmation++;
    LastResult_Dcm_TpTxConfirmation = result;
    Log_Write(LOG_T, TAG, "Dcm_TpTxConfirmation", "called %u times", CallCount_Dcm_TpTxConfirmation);

    __real_Dcm_TpTxConfirmation(id, result);
}


/* -----------------------------------------------------------------------
 * Dcm_TxConfirmation
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_ComM_NoComModeEntered
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_ComM_SilentComModeEntered
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_ComM_FullComModeEntered
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Callout Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * Dcm_ReadMemory
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_WriteMemory
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_SetProgConditions
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_GetProgConditions
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_ProcessRequestTransferExit
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_ProcessRequestUpload
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_ProcessRequestDownload
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * Dcm_ProcessRequestFileTransfer
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

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
