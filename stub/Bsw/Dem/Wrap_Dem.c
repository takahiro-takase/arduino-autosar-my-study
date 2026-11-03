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
uint32 CallCount_Dem_GetMonitorStatus                 = 0U;
uint32 CallCount_Dem_GetDTCOfEvent                    = 0U;
uint32 CallCount_Dem_GetFaultDetectionCounter         = 0U;
uint32 CallCount_Dem_SetEventStatus                   = 0U;
uint32 CallCount_Dem_GetTranslationType               = 0U;
uint32 CallCount_Dem_GetDTCStatusAvailabilityMask     = 0U;
uint32 CallCount_Dem_DisableDTCSetting                = 0U;
uint32 CallCount_Dem_EnableDTCSetting                 = 0U;
uint32 CallCount_Dem_SetOperationCycleState           = 0U;
uint32 CallCount_Dem_GetOperationCycleState           = 0U;
uint32 CallCount_Dem_ResetEventStatus                 = 0U;
uint32 CallCount_Dem_ResetEventDebounceStatus         = 0U;
uint32 CallCount_Dem_GetDebouncingOfEvent             = 0U;
uint32 CallCount_Dem_ClearDTC                         = 0U;
uint32 CallCount_Dem_GetDTCSelectionResult                    = 0U;
uint32 CallCount_Dem_GetDTCSelectionResultForClearDTC         = 0U;
uint32 CallCount_Dem_SelectDTC                                = 0U;
uint32 CallCount_Dem_GetStatusOfDTC                           = 0U;
uint32 CallCount_Dem_SetDTCFilter                     = 0U;
uint32 CallCount_Dem_GetNumberOfFilteredDTC           = 0U;
uint32 CallCount_Dem_GetNextFilteredDTC               = 0U;
uint32 CallCount_Dem_GetNextFilteredDTCAndFDC         = 0U;
uint32 CallCount_Dem_SelectExtendedDataRecord                = 0U;
uint32 CallCount_Dem_GetSizeOfExtendedDataRecordSelection    = 0U;
uint32 CallCount_Dem_GetNextExtendedDataRecord               = 0U;
uint32 CallCount_Dem_SelectFreezeFrameData                    = 0U;
uint32 CallCount_Dem_GetSizeOfFreezeFrameSelection            = 0U;
uint32 CallCount_Dem_GetNextFreezeFrameData                   = 0U;

Dem_EventIdType     LastEventId_Dem_SetEventStatus     = 0xFFU;
Dem_EventStatusType LastEventStatus_Dem_SetEventStatus = 0xFFU;
Dem_EventStatusType LastStatusOfEvent_Dem_SetEventStatus[DEM_EVENT_COUNT];

uint32 FailFromCallCount_Dem_GetEventUdsStatus             = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetMonitorStatus              = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetDTCOfEvent                 = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetFaultDetectionCounter      = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetDTCStatusAvailabilityMask  = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_DisableDTCSetting             = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_EnableDTCSetting              = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_SetOperationCycleState        = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetOperationCycleState        = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_ResetEventStatus              = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_ResetEventDebounceStatus      = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetDebouncingOfEvent          = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_ClearDTC                      = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetDTCSelectionResult                    = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetDTCSelectionResultForClearDTC         = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_SelectDTC                                = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetStatusOfDTC                           = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_SelectExtendedDataRecord                = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetSizeOfExtendedDataRecordSelection    = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetNextExtendedDataRecord               = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_SelectFreezeFrameData            = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetSizeOfFreezeFrameSelection    = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetNextFreezeFrameData           = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_SetDTCFilter                 = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetNumberOfFilteredDTC       = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetNextFilteredDTC           = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Dem_GetNextFilteredDTCAndFDC     = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_Dem_GetEventUdsStatus            = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetMonitorStatus             = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetDTCOfEvent                = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetFaultDetectionCounter     = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetDTCStatusAvailabilityMask = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_DisableDTCSetting            = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_EnableDTCSetting             = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_SetOperationCycleState       = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetOperationCycleState       = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_ResetEventStatus             = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_ResetEventDebounceStatus     = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetDebouncingOfEvent         = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_ClearDTC                     = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetDTCSelectionResult        = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetDTCSelectionResultForClearDTC = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_SelectDTC                    = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetStatusOfDTC               = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_SelectExtendedDataRecord    = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetSizeOfExtendedDataRecordSelection = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetNextExtendedDataRecord   = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_SelectFreezeFrameData         = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetSizeOfFreezeFrameSelection = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetNextFreezeFrameData        = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_SetDTCFilter                = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetNumberOfFilteredDTC      = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetNextFilteredDTC          = E_NOT_OK;
Std_ReturnType ForcedReturn_Dem_GetNextFilteredDTCAndFDC    = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapDem_Reset — 25関数すべての状態を一括で初期化する（Wrap_Dem.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapDem_Reset(void)
{
    CallCount_Dem_GetVersionInfo               = 0U;
    CallCount_Dem_Init                         = 0U;
    CallCount_Dem_GetEventUdsStatus            = 0U;
    CallCount_Dem_GetMonitorStatus             = 0U;
    CallCount_Dem_GetDTCOfEvent                = 0U;
    CallCount_Dem_SetEventStatus               = 0U;
    CallCount_Dem_GetDTCStatusAvailabilityMask = 0U;
    CallCount_Dem_ClearDTC                     = 0U;
    CallCount_Dem_GetDTCSelectionResult                    = 0U;
    CallCount_Dem_GetDTCSelectionResultForClearDTC         = 0U;
    CallCount_Dem_SelectDTC                                = 0U;
    CallCount_Dem_GetStatusOfDTC                           = 0U;
    CallCount_Dem_SetDTCFilter                 = 0U;
    CallCount_Dem_GetNumberOfFilteredDTC       = 0U;
    CallCount_Dem_GetNextFilteredDTC           = 0U;
    CallCount_Dem_GetNextFilteredDTCAndFDC     = 0U;
    CallCount_Dem_SelectExtendedDataRecord                = 0U;
    CallCount_Dem_GetSizeOfExtendedDataRecordSelection    = 0U;
    CallCount_Dem_GetNextExtendedDataRecord               = 0U;
    CallCount_Dem_SelectFreezeFrameData               = 0U;
    CallCount_Dem_GetSizeOfFreezeFrameSelection       = 0U;
    CallCount_Dem_GetNextFreezeFrameData              = 0U;
    CallCount_Dem_GetTranslationType           = 0U;
    CallCount_Dem_GetFaultDetectionCounter     = 0U;
    CallCount_Dem_EnableDTCSetting             = 0U;
    CallCount_Dem_SetOperationCycleState       = 0U;
    CallCount_Dem_GetOperationCycleState       = 0U;
    CallCount_Dem_ResetEventStatus             = 0U;
    CallCount_Dem_ResetEventDebounceStatus     = 0U;
    CallCount_Dem_GetDebouncingOfEvent         = 0U;
    CallCount_Dem_DisableDTCSetting            = 0U;
    
    LastEventId_Dem_SetEventStatus     = 0xFFU;
    LastEventStatus_Dem_SetEventStatus = 0xFFU;
    for (uint8 e = 0U; e < DEM_EVENT_COUNT; e++)
    {
        LastStatusOfEvent_Dem_SetEventStatus[e] = 0xFFU;
    }

    FailFromCallCount_Dem_GetEventUdsStatus            = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetMonitorStatus             = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetDTCOfEvent                = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetDTCStatusAvailabilityMask = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_ClearDTC                     = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetDTCSelectionResult                    = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetDTCSelectionResultForClearDTC         = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_SelectDTC                                = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetStatusOfDTC                           = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_SelectExtendedDataRecord                = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetSizeOfExtendedDataRecordSelection    = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetNextExtendedDataRecord               = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_SelectFreezeFrameData       = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetSizeOfFreezeFrameSelection= WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetNextFreezeFrameData      = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_SetDTCFilter                = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetNumberOfFilteredDTC      = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetNextFilteredDTC          = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetNextFilteredDTCAndFDC    = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetFaultDetectionCounter     = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_EnableDTCSetting             = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_DisableDTCSetting            = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_SetOperationCycleState       = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetOperationCycleState       = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_ResetEventStatus             = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_ResetEventDebounceStatus     = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Dem_GetDebouncingOfEvent         = WRAP_DEM_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_Dem_GetEventUdsStatus            = E_NOT_OK;
    ForcedReturn_Dem_GetMonitorStatus             = E_NOT_OK;
    ForcedReturn_Dem_GetDTCOfEvent                = E_NOT_OK;
    ForcedReturn_Dem_GetDTCStatusAvailabilityMask = E_NOT_OK;
    ForcedReturn_Dem_ClearDTC                     = E_NOT_OK;
    ForcedReturn_Dem_GetDTCSelectionResult        = E_NOT_OK;
    ForcedReturn_Dem_GetDTCSelectionResultForClearDTC = E_NOT_OK;
    ForcedReturn_Dem_SelectDTC                    = E_NOT_OK;
    ForcedReturn_Dem_GetStatusOfDTC               = E_NOT_OK;
    ForcedReturn_Dem_SelectExtendedDataRecord    = E_NOT_OK;
    ForcedReturn_Dem_GetSizeOfExtendedDataRecordSelection = E_NOT_OK;
    ForcedReturn_Dem_GetNextExtendedDataRecord   = E_NOT_OK;
    ForcedReturn_Dem_SelectFreezeFrameData            = E_NOT_OK;
    ForcedReturn_Dem_GetSizeOfFreezeFrameSelection    = E_NOT_OK;
    ForcedReturn_Dem_GetNextFreezeFrameData           = E_NOT_OK;
    ForcedReturn_Dem_SetDTCFilter                = E_NOT_OK;
    ForcedReturn_Dem_GetNumberOfFilteredDTC      = E_NOT_OK;
    ForcedReturn_Dem_GetNextFilteredDTC          = E_NOT_OK;
    ForcedReturn_Dem_GetNextFilteredDTCAndFDC    = E_NOT_OK;
    ForcedReturn_Dem_GetFaultDetectionCounter     = E_NOT_OK;
    ForcedReturn_Dem_EnableDTCSetting             = E_NOT_OK;
    ForcedReturn_Dem_DisableDTCSetting            = E_NOT_OK;
    ForcedReturn_Dem_SetOperationCycleState       = E_NOT_OK;
    ForcedReturn_Dem_GetOperationCycleState       = E_NOT_OK;
    ForcedReturn_Dem_ResetEventStatus             = E_NOT_OK;
    ForcedReturn_Dem_ResetEventDebounceStatus     = E_NOT_OK;
    ForcedReturn_Dem_GetDebouncingOfEvent         = E_NOT_OK;
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

extern
Std_ReturnType __real_Dem_ClearDTC(uint8 ClientId);
Std_ReturnType __wrap_Dem_ClearDTC(uint8 ClientId)
{
    CallCount_Dem_ClearDTC++;
    Log_Write(LOG_T, TAG, "Dem_ClearDTC", "called %u times", CallCount_Dem_ClearDTC);

    if (CallCount_Dem_ClearDTC >= FailFromCallCount_Dem_ClearDTC)
    {
        return ForcedReturn_Dem_ClearDTC;
    }

    return __real_Dem_ClearDTC(ClientId);
}

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

extern
Std_ReturnType __real_Dem_GetDTCSelectionResult(uint8 ClientId);
Std_ReturnType __wrap_Dem_GetDTCSelectionResult(uint8 ClientId)
{
    CallCount_Dem_GetDTCSelectionResult++;
    Log_Write(LOG_T, TAG, "Dem_GetDTCSelectionResult", "called %u times", CallCount_Dem_GetDTCSelectionResult);

    if (CallCount_Dem_GetDTCSelectionResult >= FailFromCallCount_Dem_GetDTCSelectionResult)
    {
        return ForcedReturn_Dem_GetDTCSelectionResult;
    }

    return __real_Dem_GetDTCSelectionResult(ClientId);
}

/* ----------------------------------------------------------------------
 * Dem_GetDTCSelectionResultForClearDTC
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetDTCSelectionResultForClearDTC(uint8 ClientId);
Std_ReturnType __wrap_Dem_GetDTCSelectionResultForClearDTC(uint8 ClientId)
{
    CallCount_Dem_GetDTCSelectionResultForClearDTC++;
    Log_Write(LOG_T, TAG, "Dem_GetDTCSelectionResultForClearDTC", "called %u times", CallCount_Dem_GetDTCSelectionResultForClearDTC);

    if (CallCount_Dem_GetDTCSelectionResultForClearDTC >= FailFromCallCount_Dem_GetDTCSelectionResultForClearDTC)
    {
        return ForcedReturn_Dem_GetDTCSelectionResultForClearDTC;
    }

    return __real_Dem_GetDTCSelectionResultForClearDTC(ClientId);
}

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

extern
Std_ReturnType __real_Dem_GetMonitorStatus(Dem_EventIdType EventId, Dem_MonitorStatusType* MonitorStatus);
Std_ReturnType __wrap_Dem_GetMonitorStatus(Dem_EventIdType EventId, Dem_MonitorStatusType* MonitorStatus)
{
    CallCount_Dem_GetMonitorStatus++;
    Log_Write(LOG_T, TAG, "Dem_GetMonitorStatus", "called %u times", CallCount_Dem_GetMonitorStatus);

    if (CallCount_Dem_GetMonitorStatus >= FailFromCallCount_Dem_GetMonitorStatus)
    {
        return ForcedReturn_Dem_GetMonitorStatus;
    }

    return __real_Dem_GetMonitorStatus(EventId, MonitorStatus);
}

/* ----------------------------------------------------------------------
 * Dem_GetDebouncingOfEvent
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetDebouncingOfEvent(Dem_EventIdType EventId, Dem_DebouncingStateType* DebouncingState);
Std_ReturnType __wrap_Dem_GetDebouncingOfEvent(Dem_EventIdType EventId, Dem_DebouncingStateType* DebouncingState)
{
    CallCount_Dem_GetDebouncingOfEvent++;
    Log_Write(LOG_T, TAG, "Dem_GetDebouncingOfEvent", "called %u times", CallCount_Dem_GetDebouncingOfEvent);

    if (CallCount_Dem_GetDebouncingOfEvent >= FailFromCallCount_Dem_GetDebouncingOfEvent)
    {
        return ForcedReturn_Dem_GetDebouncingOfEvent;
    }

    return __real_Dem_GetDebouncingOfEvent(EventId, DebouncingState);
}

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

extern
Std_ReturnType __real_Dem_ResetEventDebounceStatus(Dem_EventIdType EventId, Dem_DebounceResetStatusType DebounceResetStatus);
Std_ReturnType __wrap_Dem_ResetEventDebounceStatus(Dem_EventIdType EventId, Dem_DebounceResetStatusType DebounceResetStatus)
{
    CallCount_Dem_ResetEventDebounceStatus++;
    Log_Write(LOG_T, TAG, "Dem_ResetEventDebounceStatus", "called %u times", CallCount_Dem_ResetEventDebounceStatus);

    if (CallCount_Dem_ResetEventDebounceStatus >= FailFromCallCount_Dem_ResetEventDebounceStatus)
    {
        return ForcedReturn_Dem_ResetEventDebounceStatus;
    }

    return __real_Dem_ResetEventDebounceStatus(EventId, DebounceResetStatus);
}

/* ----------------------------------------------------------------------
 * Dem_ResetEventStatus
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_ResetEventStatus(Dem_EventIdType EventId);
Std_ReturnType __wrap_Dem_ResetEventStatus(Dem_EventIdType EventId)
{
    CallCount_Dem_ResetEventStatus++;
    Log_Write(LOG_T, TAG, "Dem_ResetEventStatus", "called %u times", CallCount_Dem_ResetEventStatus);

    if (CallCount_Dem_ResetEventStatus >= FailFromCallCount_Dem_ResetEventStatus)
    {
        return ForcedReturn_Dem_ResetEventStatus;
    }

    return __real_Dem_ResetEventStatus(EventId);
}

/* ----------------------------------------------------------------------
 * Dem_PrestoreFreezeFrame
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Dem_SelectDTC
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_SelectDTC(uint8 ClientId, uint32 DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin);
Std_ReturnType __wrap_Dem_SelectDTC(uint8 ClientId, uint32 DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin)
{
    CallCount_Dem_SelectDTC++;
    Log_Write(LOG_T, TAG, "Dem_SelectDTC", "called %u times", CallCount_Dem_SelectDTC);

    if (CallCount_Dem_SelectDTC >= FailFromCallCount_Dem_SelectDTC)
    {
        return ForcedReturn_Dem_SelectDTC;
    }

    return __real_Dem_SelectDTC(ClientId, DTC, DTCFormat, DTCOrigin);
}

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
    if (EventId < DEM_EVENT_COUNT)
    {
        LastStatusOfEvent_Dem_SetEventStatus[EventId] = EventStatus;
    }
    Log_Write(LOG_T, TAG, "Dem_SetEventStatus", "called %u times", CallCount_Dem_SetEventStatus);

    /* 他の関数と異なり故障注入のトグルを持たない（Wrap_Dem.h 冒頭コメント参照）。 */
    return __real_Dem_SetEventStatus(EventId, EventStatus);
}

/* ----------------------------------------------------------------------
 * Dem_SetOperationCycleState
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_SetOperationCycleState(uint8 OperationCycleId, Dem_OperationCycleStateType CycleState);
Std_ReturnType __wrap_Dem_SetOperationCycleState(uint8 OperationCycleId, Dem_OperationCycleStateType CycleState)
{
    CallCount_Dem_SetOperationCycleState++;
    Log_Write(LOG_T, TAG, "Dem_SetOperationCycleState", "called %u times", CallCount_Dem_SetOperationCycleState);

    if (CallCount_Dem_SetOperationCycleState >= FailFromCallCount_Dem_SetOperationCycleState)
    {
        return ForcedReturn_Dem_SetOperationCycleState;
    }

    return __real_Dem_SetOperationCycleState(OperationCycleId, CycleState);
}

/* ----------------------------------------------------------------------
 * Dem_GetOperationCycleState
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetOperationCycleState(uint8 OperationCycleId, Dem_OperationCycleStateType* CycleState);
Std_ReturnType __wrap_Dem_GetOperationCycleState(uint8 OperationCycleId, Dem_OperationCycleStateType* CycleState)
{
    CallCount_Dem_GetOperationCycleState++;
    Log_Write(LOG_T, TAG, "Dem_GetOperationCycleState", "called %u times", CallCount_Dem_GetOperationCycleState);

    if (CallCount_Dem_GetOperationCycleState >= FailFromCallCount_Dem_GetOperationCycleState)
    {
        return ForcedReturn_Dem_GetOperationCycleState;
    }

    return __real_Dem_GetOperationCycleState(OperationCycleId, CycleState);
}

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

extern
Std_ReturnType __real_Dem_GetStatusOfDTC(uint8 ClientId, Dem_UdsStatusByteType* DTCStatus);
Std_ReturnType __wrap_Dem_GetStatusOfDTC(uint8 ClientId, Dem_UdsStatusByteType* DTCStatus)
{
    CallCount_Dem_GetStatusOfDTC++;
    Log_Write(LOG_T, TAG, "Dem_GetStatusOfDTC", "called %u times", CallCount_Dem_GetStatusOfDTC);

    if (CallCount_Dem_GetStatusOfDTC >= FailFromCallCount_Dem_GetStatusOfDTC)
    {
        return ForcedReturn_Dem_GetStatusOfDTC;
    }

    return __real_Dem_GetStatusOfDTC(ClientId, DTCStatus);
}

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

extern
Std_ReturnType __real_Dem_GetSizeOfExtendedDataRecordSelection(uint8 ClientId, uint16* SizeOfExtendedDataRecord);
Std_ReturnType __wrap_Dem_GetSizeOfExtendedDataRecordSelection(uint8 ClientId, uint16* SizeOfExtendedDataRecord)
{
    CallCount_Dem_GetSizeOfExtendedDataRecordSelection++;
    Log_Write(LOG_T, TAG, "Dem_GetSizeOfExtendedDataRecordSelection", "called %u times", CallCount_Dem_GetSizeOfExtendedDataRecordSelection);

    if (CallCount_Dem_GetSizeOfExtendedDataRecordSelection >= FailFromCallCount_Dem_GetSizeOfExtendedDataRecordSelection)
    {
        return ForcedReturn_Dem_GetSizeOfExtendedDataRecordSelection;
    }

    return __real_Dem_GetSizeOfExtendedDataRecordSelection(ClientId, SizeOfExtendedDataRecord);
}

/* ----------------------------------------------------------------------
 * Dem_GetSizeOfFreezeFrameSelection
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetSizeOfFreezeFrameSelection(uint8 ClientId, uint16* SizeOfFreezeFrame);
Std_ReturnType __wrap_Dem_GetSizeOfFreezeFrameSelection(uint8 ClientId, uint16* SizeOfFreezeFrame)
{
    CallCount_Dem_GetSizeOfFreezeFrameSelection++;
    Log_Write(LOG_T, TAG, "Dem_GetSizeOfFreezeFrameSelection", "called %u times", CallCount_Dem_GetSizeOfFreezeFrameSelection);

    if (CallCount_Dem_GetSizeOfFreezeFrameSelection >= FailFromCallCount_Dem_GetSizeOfFreezeFrameSelection)
    {
        return ForcedReturn_Dem_GetSizeOfFreezeFrameSelection;
    }

    return __real_Dem_GetSizeOfFreezeFrameSelection(ClientId, SizeOfFreezeFrame);
}

/* ----------------------------------------------------------------------
 * Dem_GetNextExtendedDataRecord
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetNextExtendedDataRecord(uint8 ClientId, uint8* DestBuffer, uint16* BufSize);
Std_ReturnType __wrap_Dem_GetNextExtendedDataRecord(uint8 ClientId, uint8* DestBuffer, uint16* BufSize)
{
    CallCount_Dem_GetNextExtendedDataRecord++;
    Log_Write(LOG_T, TAG, "Dem_GetNextExtendedDataRecord", "called %u times", CallCount_Dem_GetNextExtendedDataRecord);

    if (CallCount_Dem_GetNextExtendedDataRecord >= FailFromCallCount_Dem_GetNextExtendedDataRecord)
    {
        return ForcedReturn_Dem_GetNextExtendedDataRecord;
    }

    return __real_Dem_GetNextExtendedDataRecord(ClientId, DestBuffer, BufSize);
}

/* ----------------------------------------------------------------------
 * Dem_GetNextFreezeFrameData
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetNextFreezeFrameData(uint8 ClientId, uint8* DestBuffer, uint16* BufSize);
Std_ReturnType __wrap_Dem_GetNextFreezeFrameData(uint8 ClientId, uint8* DestBuffer, uint16* BufSize)
{
    CallCount_Dem_GetNextFreezeFrameData++;
    Log_Write(LOG_T, TAG, "Dem_GetNextFreezeFrameData", "called %u times", CallCount_Dem_GetNextFreezeFrameData);

    if (CallCount_Dem_GetNextFreezeFrameData >= FailFromCallCount_Dem_GetNextFreezeFrameData)
    {
        return ForcedReturn_Dem_GetNextFreezeFrameData;
    }

    return __real_Dem_GetNextFreezeFrameData(ClientId, DestBuffer, BufSize);
}

/* ----------------------------------------------------------------------
 * Dem_SelectExtendedDataRecord
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_SelectExtendedDataRecord(uint8 ClientId, uint8 ExtendedDataNumber);
Std_ReturnType __wrap_Dem_SelectExtendedDataRecord(uint8 ClientId, uint8 ExtendedDataNumber)
{
    CallCount_Dem_SelectExtendedDataRecord++;
    Log_Write(LOG_T, TAG, "Dem_SelectExtendedDataRecord", "called %u times", CallCount_Dem_SelectExtendedDataRecord);

    if (CallCount_Dem_SelectExtendedDataRecord >= FailFromCallCount_Dem_SelectExtendedDataRecord)
    {
        return ForcedReturn_Dem_SelectExtendedDataRecord;
    }

    return __real_Dem_SelectExtendedDataRecord(ClientId, ExtendedDataNumber);
}

/* ----------------------------------------------------------------------
 * Dem_SelectFreezeFrameData
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_SelectFreezeFrameData(uint8 ClientId, uint8 RecordNumber);
Std_ReturnType __wrap_Dem_SelectFreezeFrameData(uint8 ClientId, uint8 RecordNumber)
{
    CallCount_Dem_SelectFreezeFrameData++;
    Log_Write(LOG_T, TAG, "Dem_SelectFreezeFrameData", "called %u times", CallCount_Dem_SelectFreezeFrameData);

    if (CallCount_Dem_SelectFreezeFrameData >= FailFromCallCount_Dem_SelectFreezeFrameData)
    {
        return ForcedReturn_Dem_SelectFreezeFrameData;
    }

    return __real_Dem_SelectFreezeFrameData(ClientId, RecordNumber);
}

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
 * Dem_SetDTCFilter
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_SetDTCFilter(uint8 ClientId, uint8 DTCStatusMask, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin, boolean FilterWithSeverity, Dem_DTCSeverityType DTCSeverityMask, boolean FilterForFaultDetectionCounter);
Std_ReturnType __wrap_Dem_SetDTCFilter(uint8 ClientId, uint8 DTCStatusMask, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin, boolean FilterWithSeverity, Dem_DTCSeverityType DTCSeverityMask, boolean FilterForFaultDetectionCounter)
{
    CallCount_Dem_SetDTCFilter++;
    Log_Write(LOG_T, TAG, "Dem_SetDTCFilter", "called %u times", CallCount_Dem_SetDTCFilter);

    if (CallCount_Dem_SetDTCFilter >= FailFromCallCount_Dem_SetDTCFilter)
    {
        return ForcedReturn_Dem_SetDTCFilter;
    }

    return __real_Dem_SetDTCFilter(ClientId, DTCStatusMask, DTCFormat, DTCOrigin, FilterWithSeverity, DTCSeverityMask, FilterForFaultDetectionCounter);
}

/* ----------------------------------------------------------------------
 * Dem_GetNumberOfFilteredDTC
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetNumberOfFilteredDTC(uint8 ClientId, uint16* NumberOfFilteredDTC);
Std_ReturnType __wrap_Dem_GetNumberOfFilteredDTC(uint8 ClientId, uint16* NumberOfFilteredDTC)
{
    CallCount_Dem_GetNumberOfFilteredDTC++;
    Log_Write(LOG_T, TAG, "Dem_GetNumberOfFilteredDTC", "called %u times", CallCount_Dem_GetNumberOfFilteredDTC);

    if (CallCount_Dem_GetNumberOfFilteredDTC >= FailFromCallCount_Dem_GetNumberOfFilteredDTC)
    {
        return ForcedReturn_Dem_GetNumberOfFilteredDTC;
    }

    return __real_Dem_GetNumberOfFilteredDTC(ClientId, NumberOfFilteredDTC);
}

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredDTC
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetNextFilteredDTC(uint8 ClientId, uint32* DTC, uint8* DTCStatus);
Std_ReturnType __wrap_Dem_GetNextFilteredDTC(uint8 ClientId, uint32* DTC, uint8* DTCStatus)
{
    CallCount_Dem_GetNextFilteredDTC++;
    Log_Write(LOG_T, TAG, "Dem_GetNextFilteredDTC", "called %u times", CallCount_Dem_GetNextFilteredDTC);

    if (CallCount_Dem_GetNextFilteredDTC >= FailFromCallCount_Dem_GetNextFilteredDTC)
    {
        return ForcedReturn_Dem_GetNextFilteredDTC;
    }

    return __real_Dem_GetNextFilteredDTC(ClientId, DTC, DTCStatus);
}

/* ----------------------------------------------------------------------
 * Dem_GetNextFilteredDTCAndFDC
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Dem_GetNextFilteredDTCAndFDC(uint8 ClientId, uint32* DTC, sint8* DTCFaultDetectionCounter);
Std_ReturnType __wrap_Dem_GetNextFilteredDTCAndFDC(uint8 ClientId, uint32* DTC, sint8* DTCFaultDetectionCounter)
{
    CallCount_Dem_GetNextFilteredDTCAndFDC++;
    Log_Write(LOG_T, TAG, "Dem_GetNextFilteredDTCAndFDC", "called %u times", CallCount_Dem_GetNextFilteredDTCAndFDC);

    if (CallCount_Dem_GetNextFilteredDTCAndFDC >= FailFromCallCount_Dem_GetNextFilteredDTCAndFDC)
    {
        return ForcedReturn_Dem_GetNextFilteredDTCAndFDC;
    }

    return __real_Dem_GetNextFilteredDTCAndFDC(ClientId, DTC, DTCFaultDetectionCounter);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
