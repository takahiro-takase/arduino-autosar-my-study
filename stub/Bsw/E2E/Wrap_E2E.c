/**
 * \file    Wrap_E2E.c
 * \brief   `src/Bsw/E2E/E2E.c` 内の関数を対象とした wrap 実体
 *          （Wrap_E2E.h 参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_E2E.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "E2E"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_E2E_SMCheck         = 0U;
uint32 CallCount_E2E_SMCheckInit     = 0U;
uint32 CallCount_E2E_GetVersionInfo  = 0U;

uint32 FailFromCallCount_E2E_SMCheck     = WRAP_E2E_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_SMCheckInit = WRAP_E2E_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_E2E_SMCheck      = E2E_E_WRONGSTATE;
Std_ReturnType ForcedReturn_E2E_SMCheckInit  = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapE2E_Reset — すべての関数状態を一括で初期化する（Wrap_E2E.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapE2E_Reset(void)
{
    CallCount_E2E_SMCheck         = 0U;
    CallCount_E2E_SMCheckInit     = 0U;
    CallCount_E2E_GetVersionInfo  = 0U;

    FailFromCallCount_E2E_SMCheck     = WRAP_E2E_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_SMCheckInit = WRAP_E2E_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_E2E_SMCheck      = E2E_E_WRONGSTATE;
    ForcedReturn_E2E_SMCheckInit  = E_NOT_OK;
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
    Log_Write(LOG_T, TAG, "E2E_SMCheck", "called %u times", CallCount_E2E_SMCheck);

    if (CallCount_E2E_SMCheck >= FailFromCallCount_E2E_SMCheck)
    {
        return ForcedReturn_E2E_SMCheck;
    }

    return __real_E2E_SMCheck(ProfileStatus, ConfigPtr, StatePtr);
}

/* ----------------------------------------------------------------------
 * E2E_SMCheckInit
 * ---------------------------------------------------------------------- */

extern 
Std_ReturnType __real_E2E_SMCheckInit(E2E_SMCheckStateType* StatePtr, const E2E_SMConfigType* ConfigPtr);
Std_ReturnType __wrap_E2E_SMCheckInit(E2E_SMCheckStateType* StatePtr, const E2E_SMConfigType* ConfigPtr)
{
    CallCount_E2E_SMCheckInit++;
    Log_Write(LOG_T, TAG, "E2E_SMCheckInit", "called %u times", CallCount_E2E_SMCheckInit);

    if (CallCount_E2E_SMCheckInit >= FailFromCallCount_E2E_SMCheckInit)
    {
        return ForcedReturn_E2E_SMCheckInit;
    }

    return __real_E2E_SMCheckInit(StatePtr, ConfigPtr);
}

/* ----------------------------------------------------------------------
 * E2E_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern 
void __real_E2E_GetVersionInfo(Std_VersionInfoType* VersionInfo);
void __wrap_E2E_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    CallCount_E2E_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "E2E_GetVersionInfo", "called %u times", CallCount_E2E_GetVersionInfo);

    __real_E2E_GetVersionInfo(VersionInfo);
}

/* ======================================================================
 * Callback notifications
 * ====================================================================== */

/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
