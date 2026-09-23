/**
 * \file    Wrap_E2E_P01.c
 * \brief   `src/Bsw/E2E/E2E_P01.c` 内の関数を対象とした wrap 実体
 *          （Wrap_E2E_P01.h 参照）。
 *
 * \details 変数を先頭の External Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c`/`Wrap_E2E_P05.c` と同じ構成。
 *          `[[reference_wrap_stub_naming_convention]]` 参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_E2E_P01.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "E2E_P01"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_E2E_P01Protect        = 0U;
uint32 CallCount_E2E_P01ProtectInit    = 0U;
uint32 CallCount_E2E_P01Check          = 0U;
uint32 CallCount_E2E_P01CheckInit      = 0U;
uint32 CallCount_E2E_P01MapStatusToSM  = 0U;

uint32 FailFromCallCount_E2E_P01Protect       = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_P01ProtectInit   = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_P01Check         = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_P01CheckInit     = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_P01MapStatusToSM = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType       ForcedReturn_E2E_P01Protect       = E2E_E_INPUTERR_NULL;
Std_ReturnType       ForcedReturn_E2E_P01ProtectInit   = E2E_E_INPUTERR_NULL;
Std_ReturnType       ForcedReturn_E2E_P01Check         = E2E_E_INPUTERR_NULL;
Std_ReturnType       ForcedReturn_E2E_P01CheckInit     = E2E_E_INPUTERR_NULL;
E2E_PCheckStatusType ForcedReturn_E2E_P01MapStatusToSM = E2E_P_ERROR;

/* ----------------------------------------------------------------------
 * WrapE2EP01_Reset — 5関数すべての状態を一括で初期化する（Wrap_E2E_P01.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapE2EP01_Reset(void)
{
    CallCount_E2E_P01Protect       = 0U;
    CallCount_E2E_P01ProtectInit   = 0U;
    CallCount_E2E_P01Check         = 0U;
    CallCount_E2E_P01CheckInit     = 0U;
    CallCount_E2E_P01MapStatusToSM = 0U;

    FailFromCallCount_E2E_P01Protect       = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_P01ProtectInit   = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_P01Check         = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_P01CheckInit     = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_P01MapStatusToSM = WRAP_E2E_P01_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_E2E_P01Protect       = E2E_E_INPUTERR_NULL;
    ForcedReturn_E2E_P01ProtectInit   = E2E_E_INPUTERR_NULL;
    ForcedReturn_E2E_P01Check         = E2E_E_INPUTERR_NULL;
    ForcedReturn_E2E_P01CheckInit     = E2E_E_INPUTERR_NULL;
    ForcedReturn_E2E_P01MapStatusToSM = E2E_P_ERROR;
}

/* ======================================================================
 * Functions
 * ====================================================================== */
 
/* ----------------------------------------------------------------------
 * E2E_P01Protect
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_E2E_P01Protect(const E2E_P01ConfigType *Config, E2E_P01ProtectStateType *State,
                                      uint8 *Data);
Std_ReturnType __wrap_E2E_P01Protect(const E2E_P01ConfigType *Config, E2E_P01ProtectStateType *State,
                                      uint8 *Data)
{
    CallCount_E2E_P01Protect++;
    Log_Write(LOG_T, TAG, "E2E_P01Protect", "called %u times", CallCount_E2E_P01Protect);

    if (CallCount_E2E_P01Protect >= FailFromCallCount_E2E_P01Protect)
    {
        return ForcedReturn_E2E_P01Protect;
    }

    return __real_E2E_P01Protect(Config, State, Data);
}

/* ----------------------------------------------------------------------
 * E2E_P01ProtectInit
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_E2E_P01ProtectInit(E2E_P01ProtectStateType *State);
Std_ReturnType __wrap_E2E_P01ProtectInit(E2E_P01ProtectStateType *State)
{
    CallCount_E2E_P01ProtectInit++;
    Log_Write(LOG_T, TAG, "E2E_P01ProtectInit", "called %u times", CallCount_E2E_P01ProtectInit);

    if (CallCount_E2E_P01ProtectInit >= FailFromCallCount_E2E_P01ProtectInit)
    {
        return ForcedReturn_E2E_P01ProtectInit;
    }

    return __real_E2E_P01ProtectInit(State);
}

/* ----------------------------------------------------------------------
 * E2E_P01Check
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_E2E_P01Check(const E2E_P01ConfigType *Config, E2E_P01CheckStateType *State,
                                    const uint8 *Data);
Std_ReturnType __wrap_E2E_P01Check(const E2E_P01ConfigType *Config, E2E_P01CheckStateType *State,
                                    const uint8 *Data)
{
    CallCount_E2E_P01Check++;
    Log_Write(LOG_T, TAG, "E2E_P01Check", "called %u times", CallCount_E2E_P01Check);

    if (CallCount_E2E_P01Check >= FailFromCallCount_E2E_P01Check)
    {
        return ForcedReturn_E2E_P01Check;
    }

    return __real_E2E_P01Check(Config, State, Data);
}

/* ----------------------------------------------------------------------
 * E2E_P01CheckInit
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_E2E_P01CheckInit(E2E_P01CheckStateType *State);
Std_ReturnType __wrap_E2E_P01CheckInit(E2E_P01CheckStateType *State)
{
    CallCount_E2E_P01CheckInit++;
    Log_Write(LOG_T, TAG, "E2E_P01CheckInit", "called %u times", CallCount_E2E_P01CheckInit);

    if (CallCount_E2E_P01CheckInit >= FailFromCallCount_E2E_P01CheckInit)
    {
        return ForcedReturn_E2E_P01CheckInit;
    }

    return __real_E2E_P01CheckInit(State);
}

/* ----------------------------------------------------------------------
 * E2E_P01MapStatusToSM
 * ---------------------------------------------------------------------- */

extern
E2E_PCheckStatusType __real_E2E_P01MapStatusToSM(Std_ReturnType CheckReturn, E2E_P01StatusType Status,
                                                  boolean profileBehavior);
E2E_PCheckStatusType __wrap_E2E_P01MapStatusToSM(Std_ReturnType CheckReturn, E2E_P01StatusType Status,
                                                  boolean profileBehavior)
{
    CallCount_E2E_P01MapStatusToSM++;
    Log_Write(LOG_T, TAG, "E2E_P01MapStatusToSM", "called %u times", CallCount_E2E_P01MapStatusToSM);

    if (CallCount_E2E_P01MapStatusToSM >= FailFromCallCount_E2E_P01MapStatusToSM)
    {
        return ForcedReturn_E2E_P01MapStatusToSM;
    }

    return __real_E2E_P01MapStatusToSM(CheckReturn, Status, profileBehavior);
}
