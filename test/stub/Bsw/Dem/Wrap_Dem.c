/**
 * \file    Wrap_Dem.c
 * \brief   `src/Bsw/Dem/Dem.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Dem.h 参照）。
 */
#include "Wrap_Dem.h"

/* ----------------------------------------------------------------------
 * Dem_SetEventStatus
 * ---------------------------------------------------------------------- */
extern Std_ReturnType __real_Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);

uint32              WrapDemSetEventStatus_CallCount     = 0U;
Dem_EventIdType     WrapDemSetEventStatus_LastEventId     = 0xFFU;
Dem_EventStatusType WrapDemSetEventStatus_LastEventStatus = 0xFFU;

void WrapDemSetEventStatus_Reset(void)
{
    WrapDemSetEventStatus_CallCount     = 0U;
    WrapDemSetEventStatus_LastEventId     = 0xFFU;
    WrapDemSetEventStatus_LastEventStatus = 0xFFU;
}

Std_ReturnType __wrap_Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    WrapDemSetEventStatus_CallCount++;
    WrapDemSetEventStatus_LastEventId     = EventId;
    WrapDemSetEventStatus_LastEventStatus = EventStatus;

    return __real_Dem_SetEventStatus(EventId, EventStatus);
}
