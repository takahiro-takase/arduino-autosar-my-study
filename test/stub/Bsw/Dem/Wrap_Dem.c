/**
 * \file    Wrap_Dem.c
 * \brief   `src/Bsw/Dem/Dem.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Dem.h 参照）。
 */
#include "Wrap_Dem.h"

/* ==================================================================== */
/*  External Variables                                                  */
/* ==================================================================== */
uint32              CallCount_Dem_SetEventStatus      = 0U;
Dem_EventIdType     LastEventId_Dem_SetEventStatus     = 0xFFU;
Dem_EventStatusType LastEventStatus_Dem_SetEventStatus = 0xFFU;

/* ----------------------------------------------------------------------
 * WrapDem_Reset — 呼び出し記録を一括で初期化する（Wrap_Dem.h 参照）。
 * ---------------------------------------------------------------------- */
void WrapDem_Reset(void)
{
    CallCount_Dem_SetEventStatus      = 0U;
    LastEventId_Dem_SetEventStatus     = 0xFFU;
    LastEventStatus_Dem_SetEventStatus = 0xFFU;
}

/* ==================================================================== */
/*  External Functions                                                  */
/* ==================================================================== */

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

    return __real_Dem_SetEventStatus(EventId, EventStatus);
}
