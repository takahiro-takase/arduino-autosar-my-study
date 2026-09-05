/**
 * \file    Bsw_EcuM_fake.c
 * \brief   EcuM.h のテスト用スパイ実装
 * \details Bsw_EcuM_fake.h 冒頭のコメント参照。
 */
#include "Bsw_EcuM_fake.h"
#include "CanSM.h"

uint32 FakeEcuM_RequestRUNCount = 0U;
uint32 FakeEcuM_ReleaseRUNCount = 0U;
uint32 FakeEcuM_CheckWakeupCount = 0U;

EcuM_UserType FakeEcuM_LastRequestUser = 0xFFU;
EcuM_UserType FakeEcuM_LastReleaseUser = 0xFFU;
EcuM_WakeupSourceType FakeEcuM_LastWakeupSource = 0U;

void FakeEcuM_Reset(void)
{
    FakeEcuM_RequestRUNCount  = 0U;
    FakeEcuM_ReleaseRUNCount  = 0U;
    FakeEcuM_CheckWakeupCount = 0U;
    FakeEcuM_LastRequestUser  = 0xFFU;
    FakeEcuM_LastReleaseUser  = 0xFFU;
    FakeEcuM_LastWakeupSource = 0U;
}

Std_ReturnType EcuM_RequestRUN(EcuM_UserType user)
{
    FakeEcuM_RequestRUNCount++;
    FakeEcuM_LastRequestUser = user;
    return E_OK;
}

Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user)
{
    FakeEcuM_ReleaseRUNCount++;
    FakeEcuM_LastReleaseUser = user;
    return E_OK;
}

/* Bsw_EcuM_fake.h 冒頭コメント参照: 他の RUN/POST_RUN 系スパイと異なり、
 * ウェイクアップ検証チェーンを途切れさせないよう実 CanSM_ControllerModeIndication()
 * へ委譲する（EcuM_CheckWakeup() 自体の実装が一行委譲のみのため、フェイク側で
 * 同じ委譲を再現しても実装の分岐ロジックが二重管理になる心配はない）。 */
void EcuM_CheckWakeup(EcuM_WakeupSourceType wakeupSource)
{
    FakeEcuM_CheckWakeupCount++;
    FakeEcuM_LastWakeupSource = wakeupSource;

    if (wakeupSource != ECUM_WKSOURCE_CAN)
        return;

    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);
}
