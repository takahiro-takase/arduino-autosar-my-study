/**
 * \file    Wrap_E2E.c
 * \brief   `src/Bsw/E2E/E2E.c` 内の関数を対象とした wrap 実体
 *          （Wrap_E2E.h 参照）。
 */
#include "Wrap_E2E.h"

/* ----------------------------------------------------------------------
 * E2E_SMCheck
 * ---------------------------------------------------------------------- */
extern Std_ReturnType __real_E2E_SMCheck(E2E_PCheckStatusType ProfileStatus, const E2E_SMConfigType* ConfigPtr,
                                          E2E_SMCheckStateType* StatePtr);

uint32          WrapE2ESMCheck_CallCount    = 0U;
uint8           WrapE2ESMCheck_ForceFail    = 0U;
Std_ReturnType  WrapE2ESMCheck_ForcedReturn = E2E_E_WRONGSTATE;

void WrapE2ESMCheck_Reset(void)
{
    WrapE2ESMCheck_CallCount    = 0U;
    WrapE2ESMCheck_ForceFail    = 0U;
    WrapE2ESMCheck_ForcedReturn = E2E_E_WRONGSTATE;
}

Std_ReturnType __wrap_E2E_SMCheck(E2E_PCheckStatusType ProfileStatus, const E2E_SMConfigType* ConfigPtr,
                                   E2E_SMCheckStateType* StatePtr)
{
    WrapE2ESMCheck_CallCount++;

    if (WrapE2ESMCheck_ForceFail)
        return WrapE2ESMCheck_ForcedReturn;

    return __real_E2E_SMCheck(ProfileStatus, ConfigPtr, StatePtr);
}
