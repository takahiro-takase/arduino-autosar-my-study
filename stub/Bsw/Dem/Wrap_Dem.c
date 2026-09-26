/**
 * \file    Wrap_Dem.c
 * \brief   `src/Bsw/Dem/Dem.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Dem.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Dem.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Dem"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Dem_GetVersionInfo                   = 0U;
uint32 CallCount_Dem_Init                             = 0U;
uint32 CallCount_Dem_GetEventUdsStatus                = 0U;
uint32 CallCount_Dem_GetDTCOfEvent                    = 0U;
uint32 CallCount_Dem_GetFaultDetectionCounter         = 0U;
uint32 CallCount_Dem_SetEventStatus                   = 0U;
uint32 CallCount_Dem_GetTranslationType               = 0U;
uint32 CallCount_Dem_GetDTCStatusAvailabilityMask     = 0U;
uint32 CallCount_Dem_DisableDTCSetting                = 0U;
uint32 CallCount_Dem_EnableDTCSetting                 = 0U;
uint32 CallCount_Dem_ClearAllDTCs                     = 0U;
uint32 CallCount_Dem_ClearOneDtc                      = 0U;
uint32 CallCount_Dem_GetAllDTCs                       = 0U;
uint32 CallCount_Dem_GetSupportedDTCs                 = 0U;
uint32 CallCount_Dem_GetPrefailedDTCs                 = 0U;
uint32 CallCount_Dem_SetFreezeFrameContext            = 0U;
uint32 CallCount_Dem_GetFreezeFrameOfEvent            = 0U;
uint32 CallCount_Dem_GetEventIdOfDTC                  = 0U;
uint32 CallCount_Dem_GetOccurrenceCounterOfEvent      = 0U;

Dem_EventIdType     LastEventId_Dem_SetEventStatus     = 0xFFU;
Dem_EventStatusType LastEventStatus_Dem_SetEventStatus = 0xFFU;

uint32 FailFromCallCount_Dem_GetEventUdsStatus             = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetDTCOfEvent                 = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetFaultDetectionCounter      = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetDTCStatusAvailabilityMask  = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_DisableDTCSetting             = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_EnableDTCSetting              = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_ClearAllDTCs                  = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_ClearOneDtc                   = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetFreezeFrameOfEvent         = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetEventIdOfDTC               = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetOccurrenceCounterOfEvent   = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_Dem_GetEventUdsStatus            = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetDTCOfEvent                = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetFaultDetectionCounter     = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetDTCStatusAvailabilityMask = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_DisableDTCSetting            = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_EnableDTCSetting             = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_ClearAllDTCs                 = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_ClearOneDtc                  = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetFreezeFrameOfEvent        = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetEventIdOfDTC              = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetOccurrenceCounterOfEvent  = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapDem_Reset — 19関数すべての状態を一括で初期化する（Wrap_Dem.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapDem_Reset(void)
{
    CallCount_Dem_GetVersionInfo               = 0U;
    CallCount_Dem_Init                         = 0U;
    CallCount_Dem_GetEventUdsStatus            = 0U;
    CallCount_Dem_GetDTCOfEvent                = 0U;
    CallCount_Dem_SetEventStatus               = 0U;
    CallCount_Dem_GetDTCStatusAvailabilityMask = 0U;
    CallCount_Dem_ClearAllDTCs                 = 0U;
    CallCount_Dem_ClearOneDtc                  = 0U;
    CallCount_Dem_GetAllDTCs                   = 0U;
    CallCount_Dem_GetSupportedDTCs             = 0U;
    CallCount_Dem_GetPrefailedDTCs             = 0U;
    CallCount_Dem_SetFreezeFrameContext        = 0U;
    CallCount_Dem_GetFreezeFrameOfEvent        = 0U;
    CallCount_Dem_GetEventIdOfDTC              = 0U;
    CallCount_Dem_GetOccurrenceCounterOfEvent  = 0U;
    CallCount_Dem_GetTranslationType           = 0U;
    CallCount_Dem_GetFaultDetectionCounter     = 0U;
    CallCount_Dem_EnableDTCSetting             = 0U;
    CallCount_Dem_DisableDTCSetting            = 0U;
    
    LastEventId_Dem_SetEventStatus     = 0xFFU;
    LastEventStatus_Dem_SetEventStatus = 0xFFU;

    FailFromCallCount_Dem_GetEventUdsStatus            = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetDTCOfEvent                = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetDTCStatusAvailabilityMask = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_ClearAllDTCs                 = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_ClearOneDtc                  = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetFreezeFrameOfEvent        = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetEventIdOfDTC              = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetOccurrenceCounterOfEvent  = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetFaultDetectionCounter     = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_EnableDTCSetting             = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_DisableDTCSetting            = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_Dem_GetEventUdsStatus            = E_NOT_OK;
    ForcedReturn_Dem_GetDTCOfEvent                = E_NOT_OK;
    ForcedReturn_Dem_GetDTCStatusAvailabilityMask = E_NOT_OK;
    ForcedReturn_Dem_ClearAllDTCs                 = E_NOT_OK;
    ForcedReturn_Dem_ClearOneDtc                  = E_NOT_OK;
    ForcedReturn_Dem_GetFreezeFrameOfEvent        = E_NOT_OK;
    ForcedReturn_Dem_GetEventIdOfDTC              = E_NOT_OK;
    ForcedReturn_Dem_GetOccurrenceCounterOfEvent  = E_NOT_OK;
    ForcedReturn_Dem_GetFaultDetectionCounter     = E_NOT_OK;
    ForcedReturn_Dem_EnableDTCSetting             = E_NOT_OK;
    ForcedReturn_Dem_DisableDTCSetting            = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_Dem_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_Dem_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_Dem_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "Dem_GetVersionInfo", "called %u times", CallCount_Dem_GetVersionInfo);

    __real_Dem_GetVersionInfo(versioninfo);
}

/* ======================================================================
 * Interface ECU State Manager <=> Dem
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_PreInit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_Init
 * ---------------------------------------------------------------------- */

extern
void __real_Dem_Init(const Dem_ConfigType* ConfigPtr);
void __wrap_Dem_Init(const Dem_ConfigType* ConfigPtr)
{
    CallCount_Dem_Init++;
    Log_Write(LOG_T, TAG, "Dem_Init", "called %u times", CallCount_Dem_Init);

    __real_Dem_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * Dem_Shutdown
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Interface BSW modules / SW-Components <=> Dem
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_ClearDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_ClearPrestoredFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetComponentFailed
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDTCSelectionResult
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDTCSelectionResultForClearDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetEventUdsStatus
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetEventUdsStatus(Dem_EventIdType EventId, Dem_UdsStatusByteType* UDSStatusByte);
Std_ReturnType __wrap_Dem_GetEventUdsStatus(Dem_EventIdType EventId, Dem_UdsStatusByteType* UDSStatusByte)
{
    CallCount_Dem_GetEventUdsStatus++;
    Log_Write(LOG_T, TAG, "Dem_GetEventUdsStatus", "called %u times", CallCount_Dem_GetEventUdsStatus);

    if (CallCount_Dem_GetEventUdsStatus >= FailFromCallCount_Dem_GetEventUdsStatus)
    {
        return ForcedReturn_Dem_GetEventUdsStatus;
    }

    return __real_Dem_GetEventUdsStatus(EventId, UDSStatusByte);
}

/* ----------------------------------------------------------------------
 * Dem_GetMonitorStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDebouncingOfEvent
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDTCOfEvent
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetDTCOfEvent(Dem_EventIdType EventId, Dem_DTCFormatType DTCFormat, uint32* DTCOfEvent);
Std_ReturnType __wrap_Dem_GetDTCOfEvent(Dem_EventIdType EventId, Dem_DTCFormatType DTCFormat, uint32* DTCOfEvent)
{
    CallCount_Dem_GetDTCOfEvent++;
    Log_Write(LOG_T, TAG, "Dem_GetDTCOfEvent", "called %u times", CallCount_Dem_GetDTCOfEvent);

    if (CallCount_Dem_GetDTCOfEvent >= FailFromCallCount_Dem_GetDTCOfEvent)
    {
        return ForcedReturn_Dem_GetDTCOfEvent;
    }

    return __real_Dem_GetDTCOfEvent(EventId, DTCFormat, DTCOfEvent);
}

/* ----------------------------------------------------------------------
 * Dem_GetDTCSuppression
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetFaultDetectionCounter
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetFaultDetectionCounter(Dem_EventIdType EventId, sint8* FaultDetectionCounter);
Std_ReturnType __wrap_Dem_GetFaultDetectionCounter(Dem_EventIdType EventId, sint8* FaultDetectionCounter)
{
    CallCount_Dem_GetFaultDetectionCounter++;
    Log_Write(LOG_T, TAG, "Dem_GetFaultDetectionCounter", "called %u times", CallCount_Dem_GetFaultDetectionCounter);

    if (CallCount_Dem_GetFaultDetectionCounter >= FailFromCallCount_Dem_GetFaultDetectionCounter)
    {
        return ForcedReturn_Dem_GetFaultDetectionCounter;
    }

    return __real_Dem_GetFaultDetectionCounter(EventId, FaultDetectionCounter);
}

/* ----------------------------------------------------------------------
 * Dem_GetIndicatorStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetEventFreezeFrameDataEx
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetEventExtendedDataRecordEx
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetEventMemoryOverflow
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNumberOfEventMemoryEntries
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_ResetEventDebounceStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_ResetEventStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_PrestoreFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SelectDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetComponentAvailable
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDTCSuppression
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetEnableCondition
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetEventAvailable
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetEventFailureCycleCounterThreshold
 * ---------------------------------------------------------------------- */

/* 未実装 */


/* ----------------------------------------------------------------------
 * Dem_SetEventStatus
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);
Std_ReturnType __wrap_Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    CallCount_Dem_SetEventStatus++;
    LastEventId_Dem_SetEventStatus     = EventId;
    LastEventStatus_Dem_SetEventStatus = EventStatus;
    Log_Write(LOG_T, TAG, "Dem_SetEventStatus", "called %u times", CallCount_Dem_SetEventStatus);

    /* 他の関数と異なり故障注入のトグルを持たない（Wrap_Dem.h 冒頭コメント参照）。 */
    return __real_Dem_SetEventStatus(EventId, EventStatus);
}

/* ----------------------------------------------------------------------
 * Dem_SetOperationCycleState
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetOperationCycleState
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetStorageCondition
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetWIRStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Interface Dcm <=> Dem
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_GetTranslationType
 * ---------------------------------------------------------------------- */

extern
Dem_DTCTranslationFormatType __real_Dem_GetTranslationType(uint8 ClientId);
Dem_DTCTranslationFormatType __wrap_Dem_GetTranslationType(uint8 ClientId)
{
    CallCount_Dem_GetTranslationType++;
    Log_Write(LOG_T, TAG, "Dem_GetTranslationType", "called %u times", CallCount_Dem_GetTranslationType);

    /* 成功/失敗を表さない列挙型のため故障注入は実装しない（Wrap_Dem.h 冒頭コメント参照）。 */
    return __real_Dem_GetTranslationType(ClientId);
}

/* ----------------------------------------------------------------------
 * Dem_GetDTCStatusAvailabilityMask
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetDTCStatusAvailabilityMask(uint8 ClientId, Dem_UdsStatusByteType* DTCStatusMask);
Std_ReturnType __wrap_Dem_GetDTCStatusAvailabilityMask(uint8 ClientId, Dem_UdsStatusByteType* DTCStatusMask)
{
    CallCount_Dem_GetDTCStatusAvailabilityMask++;
    Log_Write(LOG_T, TAG, "Dem_GetDTCStatusAvailabilityMask", "called %u times", CallCount_Dem_GetDTCStatusAvailabilityMask);

    if (CallCount_Dem_GetDTCStatusAvailabilityMask >= FailFromCallCount_Dem_GetDTCStatusAvailabilityMask)
    {
        return ForcedReturn_Dem_GetDTCStatusAvailabilityMask;
    }

    return __real_Dem_GetDTCStatusAvailabilityMask(ClientId, DTCStatusMask);
}

/* ----------------------------------------------------------------------
 * Dem_GetStatusOfDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetSeverityOfDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetFunctionalUnitOfDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetDTCFilter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNumberOfFilteredDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredDTC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredDTCAndFDC
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredDTCAndSeverity
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SetFreezeFrameRecordFilter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredRecord
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetDTCByOccurrenceTime
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DisableDTCRecordUpdate
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_EnableDTCRecordUpdate
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetSizeOfExtendedDataRecordSelection
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetSizeOfFreezeFrameSelection
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNextExtendedDataRecord
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_GetNextFreezeFrameData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SelectExtendedDataRecord
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SelectFreezeFrameData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DisableDTCSetting
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_DisableDTCSetting(uint8 ClientId);
Std_ReturnType __wrap_Dem_DisableDTCSetting(uint8 ClientId)
{
    CallCount_Dem_DisableDTCSetting++;
    Log_Write(LOG_T, TAG, "Dem_DisableDTCSetting", "called %u times", CallCount_Dem_DisableDTCSetting);

    if (CallCount_Dem_DisableDTCSetting >= FailFromCallCount_Dem_DisableDTCSetting)
    {
        return ForcedReturn_Dem_DisableDTCSetting;
    }

    return __real_Dem_DisableDTCSetting(ClientId);
}

/* ----------------------------------------------------------------------
 * Dem_EnableDTCSetting
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_EnableDTCSetting(uint8 ClientId);
Std_ReturnType __wrap_Dem_EnableDTCSetting(uint8 ClientId)
{
    CallCount_Dem_EnableDTCSetting++;
    Log_Write(LOG_T, TAG, "Dem_EnableDTCSetting", "called %u times", CallCount_Dem_EnableDTCSetting);

    if (CallCount_Dem_EnableDTCSetting >= FailFromCallCount_Dem_EnableDTCSetting)
    {
        return ForcedReturn_Dem_EnableDTCSetting;
    }

    return __real_Dem_EnableDTCSetting(ClientId);
}

/* ======================================================================
 * OBD-specific Dcm <=> Dem Interfaces
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dem_DcmGetInfoTypeValue08
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetInfoTypeValue0B
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID01
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID1C
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID21
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID30
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID31
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID41
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID4D
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID4E
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfPID91
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmReadDataOfOBDFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetDTCOfOBDFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetAvailableOBDMIDs
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetNumTIDsOfOBDMID
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_DcmGetDTRData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_ClearAllDTCs
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_ClearAllDTCs(void);
Std_ReturnType __wrap_Dem_ClearAllDTCs(void)
{
    CallCount_Dem_ClearAllDTCs++;
    Log_Write(LOG_T, TAG, "Dem_ClearAllDTCs", "called %u times", CallCount_Dem_ClearAllDTCs);

    if (CallCount_Dem_ClearAllDTCs >= FailFromCallCount_Dem_ClearAllDTCs)
    {
        return ForcedReturn_Dem_ClearAllDTCs;
    }

    return __real_Dem_ClearAllDTCs();
}

/* ----------------------------------------------------------------------
 * Dem_ClearOneDtc
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_ClearOneDtc(Dem_EventIdType EventId);
Std_ReturnType __wrap_Dem_ClearOneDtc(Dem_EventIdType EventId)
{
    CallCount_Dem_ClearOneDtc++;
    Log_Write(LOG_T, TAG, "Dem_ClearOneDtc", "called %u times", CallCount_Dem_ClearOneDtc);

    if (CallCount_Dem_ClearOneDtc >= FailFromCallCount_Dem_ClearOneDtc)
    {
        return ForcedReturn_Dem_ClearOneDtc;
    }

    return __real_Dem_ClearOneDtc(EventId);
}

/* ----------------------------------------------------------------------
 * Dem_GetAllDTCs
 * ---------------------------------------------------------------------- */

extern
void __real_Dem_GetAllDTCs(uint32* dtcBuf, uint8* statusBuf, uint8* count, uint8 statusMask);
void __wrap_Dem_GetAllDTCs(uint32* dtcBuf, uint8* statusBuf, uint8* count, uint8 statusMask)
{
    CallCount_Dem_GetAllDTCs++;
    Log_Write(LOG_T, TAG, "Dem_GetAllDTCs", "called %u times", CallCount_Dem_GetAllDTCs);

    __real_Dem_GetAllDTCs(dtcBuf, statusBuf, count, statusMask);
}

/* ----------------------------------------------------------------------
 * Dem_GetSupportedDTCs
 * ---------------------------------------------------------------------- */

extern
void __real_Dem_GetSupportedDTCs(uint32* dtcBuf, uint8* statusBuf, uint8* count);
void __wrap_Dem_GetSupportedDTCs(uint32* dtcBuf, uint8* statusBuf, uint8* count)
{
    CallCount_Dem_GetSupportedDTCs++;
    Log_Write(LOG_T, TAG, "Dem_GetSupportedDTCs", "called %u times", CallCount_Dem_GetSupportedDTCs);

    __real_Dem_GetSupportedDTCs(dtcBuf, statusBuf, count);
}

/* ----------------------------------------------------------------------
 * Dem_GetPrefailedDTCs
 * ---------------------------------------------------------------------- */

extern
void __real_Dem_GetPrefailedDTCs(uint32* dtcBuf, uint8* fdcBuf, uint8* count);
void __wrap_Dem_GetPrefailedDTCs(uint32* dtcBuf, uint8* fdcBuf, uint8* count)
{
    CallCount_Dem_GetPrefailedDTCs++;
    Log_Write(LOG_T, TAG, "Dem_GetPrefailedDTCs", "called %u times", CallCount_Dem_GetPrefailedDTCs);

    __real_Dem_GetPrefailedDTCs(dtcBuf, fdcBuf, count);
}

/* ----------------------------------------------------------------------
 * Dem_SetFreezeFrameContext
 * ---------------------------------------------------------------------- */

extern
void __real_Dem_SetFreezeFrameContext(uint16 EngineSpeed, uint8 CoolantTemp, uint8 EngineState);
void __wrap_Dem_SetFreezeFrameContext(uint16 EngineSpeed, uint8 CoolantTemp, uint8 EngineState)
{
    CallCount_Dem_SetFreezeFrameContext++;
    Log_Write(LOG_T, TAG, "Dem_SetFreezeFrameContext", "called %u times", CallCount_Dem_SetFreezeFrameContext);

    __real_Dem_SetFreezeFrameContext(EngineSpeed, CoolantTemp, EngineState);
}

/* ----------------------------------------------------------------------
 * Dem_GetFreezeFrameOfEvent
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetFreezeFrameOfEvent(Dem_EventIdType EventId, Dem_FreezeFrameType* Frame);
Std_ReturnType __wrap_Dem_GetFreezeFrameOfEvent(Dem_EventIdType EventId, Dem_FreezeFrameType* Frame)
{
    CallCount_Dem_GetFreezeFrameOfEvent++;
    Log_Write(LOG_T, TAG, "Dem_GetFreezeFrameOfEvent", "called %u times", CallCount_Dem_GetFreezeFrameOfEvent);

    if (CallCount_Dem_GetFreezeFrameOfEvent >= FailFromCallCount_Dem_GetFreezeFrameOfEvent)
    {
        return ForcedReturn_Dem_GetFreezeFrameOfEvent;
    }

    return __real_Dem_GetFreezeFrameOfEvent(EventId, Frame);
}

/* ----------------------------------------------------------------------
 * Dem_GetEventIdOfDTC
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetEventIdOfDTC(uint32 DTC, Dem_EventIdType* EventId);
Std_ReturnType __wrap_Dem_GetEventIdOfDTC(uint32 DTC, Dem_EventIdType* EventId)
{
    CallCount_Dem_GetEventIdOfDTC++;
    Log_Write(LOG_T, TAG, "Dem_GetEventIdOfDTC", "called %u times", CallCount_Dem_GetEventIdOfDTC);

    if (CallCount_Dem_GetEventIdOfDTC >= FailFromCallCount_Dem_GetEventIdOfDTC)
    {
        return ForcedReturn_Dem_GetEventIdOfDTC;
    }

    return __real_Dem_GetEventIdOfDTC(DTC, EventId);
}

/* ----------------------------------------------------------------------
 * Dem_GetOccurrenceCounterOfEvent
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetOccurrenceCounterOfEvent(Dem_EventIdType EventId, uint8* Counter);
Std_ReturnType __wrap_Dem_GetOccurrenceCounterOfEvent(Dem_EventIdType EventId, uint8* Counter)
{
    CallCount_Dem_GetOccurrenceCounterOfEvent++;
    Log_Write(LOG_T, TAG, "Dem_GetOccurrenceCounterOfEvent", "called %u times", CallCount_Dem_GetOccurrenceCounterOfEvent);

    if (CallCount_Dem_GetOccurrenceCounterOfEvent >= FailFromCallCount_Dem_GetOccurrenceCounterOfEvent)
    {
        return ForcedReturn_Dem_GetOccurrenceCounterOfEvent;
    }

    return __real_Dem_GetOccurrenceCounterOfEvent(EventId, Counter);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
