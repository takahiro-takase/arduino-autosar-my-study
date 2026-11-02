/**
 * \file    Fake_Bsw_EcuM.c
 * \brief   EcuM.h のテスト用スパイ実装
 * \details Fake_Bsw_EcuM.h 冒頭のコメント参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Fake_Bsw_EcuM.h"
#include "CanSM.h"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint8 FakeEcuM_PassThrough = 0U;

uint32 FakeEcuM_RequestRUNCount = 0U;
uint32 FakeEcuM_ReleaseRUNCount = 0U;
uint32 FakeEcuM_CheckWakeupCount = 0U;
uint32 FakeEcuM_SetWakeupEventCount = 0U;

EcuM_UserType FakeEcuM_LastRequestUser = 0xFFU;
EcuM_UserType FakeEcuM_LastReleaseUser = 0xFFU;
EcuM_WakeupSourceType FakeEcuM_LastWakeupSource = 0U;
EcuM_WakeupSourceType FakeEcuM_LastSetWakeupEventSources = 0U;

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

/* --wrap 用の本物（EcuM.c の実体）。FakeEcuM_PassThrough が TRUE のときだけ呼ぶ。 */
Std_ReturnType __real_EcuM_RequestRUN(EcuM_UserType user);
Std_ReturnType __real_EcuM_ReleaseRUN(EcuM_UserType user);
void __real_EcuM_CheckWakeup(EcuM_WakeupSourceType wakeupSource);
void __real_EcuM_SetWakeupEvent(EcuM_WakeupSourceType sources);

/* ======================================================================
 * Functions
 * ====================================================================== */

void FakeEcuM_Reset(void)
{
    FakeEcuM_RequestRUNCount  = 0U;
    FakeEcuM_ReleaseRUNCount  = 0U;
    FakeEcuM_CheckWakeupCount = 0U;
    FakeEcuM_SetWakeupEventCount = 0U;
    FakeEcuM_LastRequestUser  = 0xFFU;
    FakeEcuM_LastReleaseUser  = 0xFFU;
    FakeEcuM_LastWakeupSource = 0U;
    FakeEcuM_LastSetWakeupEventSources = 0U;
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
 * ウェイクアップ検証チェーンを途切れさせないよう実 CanSM_StartWakeupSource()
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

    (void)CanSM_StartWakeupSource(0U);
}

/* EcuM_SetWakeupEvent() は、Can_CheckWakeup() が起床を検出したときの通知先。素通しでないときは、
 * 呼び出しの記録だけを残す（検証の開始は、上の __wrap_EcuM_CheckWakeup() が既に CanSM へ委譲している）。 */
void __wrap_EcuM_SetWakeupEvent(EcuM_WakeupSourceType sources)
{
    if (FakeEcuM_PassThrough != 0U)
    {
        __real_EcuM_SetWakeupEvent(sources);
        return;
    }

    FakeEcuM_SetWakeupEventCount++;
    FakeEcuM_LastSetWakeupEventSources = sources;
}
