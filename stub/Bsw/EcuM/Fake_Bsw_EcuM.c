/**
 * \file    Fake_Bsw_EcuM.c
 * \brief   EcuM.h のテスト用スパイ実装
 * \details Fake_Bsw_EcuM.h 冒頭のコメント参照。
 */
#include "Fake_Bsw_EcuM.h"
#include "CanSM.h"

/* --wrap 用の本物（EcuM.c の実体）。FakeEcuM_PassThrough が TRUE のときだけ呼ぶ。 */
Std_ReturnType __real_EcuM_RequestRUN(EcuM_UserType user);
Std_ReturnType __real_EcuM_ReleaseRUN(EcuM_UserType user);
void __real_EcuM_CheckWakeup(EcuM_WakeupSourceType wakeupSource);

uint8 FakeEcuM_PassThrough = 0U;

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
    FakeEcuM_PassThrough      = 0U;
}

Std_ReturnType __wrap_EcuM_RequestRUN(EcuM_UserType user)
{
    if (FakeEcuM_PassThrough != 0U)
        return __real_EcuM_RequestRUN(user);

    FakeEcuM_RequestRUNCount++;
    FakeEcuM_LastRequestUser = user;
    return E_OK;
}

Std_ReturnType __wrap_EcuM_ReleaseRUN(EcuM_UserType user)
{
    if (FakeEcuM_PassThrough != 0U)
        return __real_EcuM_ReleaseRUN(user);

    FakeEcuM_ReleaseRUNCount++;
    FakeEcuM_LastReleaseUser = user;
    return E_OK;
}

/* Fake_Bsw_EcuM.h 冒頭コメント参照: 他の RUN/POST_RUN 系スパイと異なり、
 * ウェイクアップ検証チェーンを途切れさせないよう実 CanSM_ControllerModeIndication()
 * へ委譲する（EcuM_CheckWakeup() 自体の実装が一行委譲のみのため、フェイク側で
 * 同じ委譲を再現しても実装の分岐ロジックが二重管理になる心配はない）。 */
void __wrap_EcuM_CheckWakeup(EcuM_WakeupSourceType wakeupSource)
{
    if (FakeEcuM_PassThrough != 0U)
    {
        __real_EcuM_CheckWakeup(wakeupSource);
        return;
    }

    FakeEcuM_CheckWakeupCount++;
    FakeEcuM_LastWakeupSource = wakeupSource;

    if (wakeupSource != ECUM_WKSOURCE_CAN)
        return;

    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);
}
