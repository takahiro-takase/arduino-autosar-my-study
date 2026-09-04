/**
 * \file    Bsw_Dem_fake.c
 * \brief   Dem.h のテスト用スパイ実装
 * \details Bsw_Dem_fake.h 冒頭のコメント参照。
 */
#include "Bsw_Dem_fake.h"

uint32              FakeDem_SetEventStatusCount = 0U;
Dem_EventIdType     FakeDem_LastEventId            = 0xFFU;
Dem_EventStatusType FakeDem_LastEventStatus         = 0xFFU;

void FakeDem_Reset(void)
{
    FakeDem_SetEventStatusCount = 0U;
    FakeDem_LastEventId            = 0xFFU;
    FakeDem_LastEventStatus        = 0xFFU;
}

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    FakeDem_SetEventStatusCount++;
    FakeDem_LastEventId     = EventId;
    FakeDem_LastEventStatus = EventStatus;
    return E_OK;
}
