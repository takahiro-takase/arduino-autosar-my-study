/**
 * \file    Wrap_E2E.c
 * \brief   `src/Bsw/E2E/E2E.c` 内の関数を対象とした wrap 実体
 *          （Wrap_E2E.h 参照）。
 */
#include "Wrap_E2E.h"

/* ======================================================================
 *  External Variables
 * ====================================================================== */
uint32         CallCount_E2E_SMCheck         = 0U;
uint32         FailFromCallCount_E2E_SMCheck = WRAP_E2E_FAIL_FROM_CALL_COUNT_DISABLED;
Std_ReturnType ForcedReturn_E2E_SMCheck      = E2E_E_WRONGSTATE;

/* ----------------------------------------------------------------------
 * WrapE2E_Reset — すべての関数状態を一括で初期化する（Wrap_E2E.h 参照）。
 * ---------------------------------------------------------------------- */
void WrapE2E_Reset(void)
{
    CallCount_E2E_SMCheck         = 0U;
    FailFromCallCount_E2E_SMCheck = WRAP_E2E_FAIL_FROM_CALL_COUNT_DISABLED;
    ForcedReturn_E2E_SMCheck      = E2E_E_WRONGSTATE;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * E2E_SMCheck
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_E2E_SMCheck(E2E_PCheckStatusType ProfileStatus, const E2E_SMConfigType* ConfigPtr,
                                   E2E_SMCheckStateType* StatePtr);
Std_ReturnType __wrap_E2E_SMCheck(E2E_PCheckStatusType ProfileStatus, const E2E_SMConfigType* ConfigPtr,
                                   E2E_SMCheckStateType* StatePtr)
{
    CallCount_E2E_SMCheck++;

    if (CallCount_E2E_SMCheck >= FailFromCallCount_E2E_SMCheck)
    {
        return ForcedReturn_E2E_SMCheck;
    }

    return __real_E2E_SMCheck(ProfileStatus, ConfigPtr, StatePtr);
}
