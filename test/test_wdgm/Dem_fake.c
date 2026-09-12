/**
 * \file    Dem_fake.c
 * \brief   Dem.h のテスト用フェイク実装（Dem_fake.h 冒頭のコメント参照）
 */
#include "Dem_fake.h"

uint32 FakeDem_SetEventStatusCount = 0U;

Dem_EventIdType     FakeDem_LastEventId     = 0U;
Dem_EventStatusType FakeDem_LastEventStatus = DEM_EVENT_STATUS_PASSED;

void FakeDem_Reset(void)
{
    FakeDem_SetEventStatusCount = 0U;
    FakeDem_LastEventId         = 0U;
    FakeDem_LastEventStatus     = DEM_EVENT_STATUS_PASSED;
}

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    FakeDem_SetEventStatusCount++;
    FakeDem_LastEventId     = EventId;
    FakeDem_LastEventStatus = EventStatus;
    return E_OK;
}
