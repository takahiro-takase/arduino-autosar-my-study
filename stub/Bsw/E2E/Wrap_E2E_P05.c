/**
 * \file    Wrap_E2E_P05.c
 * \brief   `src/Bsw/E2E/E2E_P05.c` 内の関数を対象とした wrap 実体
 *          （Wrap_E2E_P05.h 参照）。
 *
 * \details 変数を先頭の External Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_E2E_P05.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "E2E_P05"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_E2E_P05Protect        = 0U;
uint32 CallCount_E2E_P05ProtectInit    = 0U;
uint32 CallCount_E2E_P05Check          = 0U;
uint32 CallCount_E2E_P05CheckInit      = 0U;
uint32 CallCount_E2E_P05MapStatusToSM  = 0U;

uint32 FailFromCallCount_E2E_P05Protect       = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_P05ProtectInit   = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_P05Check         = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_P05CheckInit     = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2E_P05MapStatusToSM = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType       ForcedReturn_E2E_P05Protect       = E2E_E_INPUTERR_NULL;
Std_ReturnType       ForcedReturn_E2E_P05ProtectInit   = E2E_E_INPUTERR_NULL;
Std_ReturnType       ForcedReturn_E2E_P05Check         = E2E_E_INPUTERR_NULL;
Std_ReturnType       ForcedReturn_E2E_P05CheckInit     = E2E_E_INPUTERR_NULL;
E2E_PCheckStatusType ForcedReturn_E2E_P05MapStatusToSM = E2E_P_ERROR;

/* ----------------------------------------------------------------------
 * WrapE2EP05_Reset — 5関数すべての状態を一括で初期化する（Wrap_E2E_P05.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapE2EP05_Reset(void)
{
    CallCount_E2E_P05Protect       = 0U;
    CallCount_E2E_P05ProtectInit   = 0U;
    CallCount_E2E_P05Check         = 0U;
    CallCount_E2E_P05CheckInit     = 0U;
    CallCount_E2E_P05MapStatusToSM = 0U;

    FailFromCallCount_E2E_P05Protect       = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_P05ProtectInit   = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_P05Check         = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_P05CheckInit     = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2E_P05MapStatusToSM = WRAP_E2E_P05_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_E2E_P05Protect       = E2E_E_INPUTERR_NULL;
    ForcedReturn_E2E_P05ProtectInit   = E2E_E_INPUTERR_NULL;
    ForcedReturn_E2E_P05Check         = E2E_E_INPUTERR_NULL;
    ForcedReturn_E2E_P05CheckInit     = E2E_E_INPUTERR_NULL;
    ForcedReturn_E2E_P05MapStatusToSM = E2E_P_ERROR;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * E2E_P05Protect
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_E2E_P05Protect(const E2E_P05ConfigType *Config, E2E_P05ProtectStateType *State,
                                      uint8 *Data, uint16 Length);
Std_ReturnType __wrap_E2E_P05Protect(const E2E_P05ConfigType *Config, E2E_P05ProtectStateType *State,
                                      uint8 *Data, uint16 Length)
{
    CallCount_E2E_P05Protect++;
    Log_Write(LOG_T, TAG, "E2E_P05Protect", "called %u times", CallCount_E2E_P05Protect);

    if (CallCount_E2E_P05Protect >= FailFromCallCount_E2E_P05Protect)
    {
        return ForcedReturn_E2E_P05Protect;
    }

    return __real_E2E_P05Protect(Config, State, Data, Length);
}

/* ----------------------------------------------------------------------
 * E2E_P05ProtectInit
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_E2E_P05ProtectInit(E2E_P05ProtectStateType *State);
Std_ReturnType __wrap_E2E_P05ProtectInit(E2E_P05ProtectStateType *State)
{
    CallCount_E2E_P05ProtectInit++;
    Log_Write(LOG_T, TAG, "E2E_P05ProtectInit", "called %u times", CallCount_E2E_P05ProtectInit);

    if (CallCount_E2E_P05ProtectInit >= FailFromCallCount_E2E_P05ProtectInit)
    {
        return ForcedReturn_E2E_P05ProtectInit;
    }

    return __real_E2E_P05ProtectInit(State);
}

/* ----------------------------------------------------------------------
 * E2E_P05Check
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_E2E_P05Check(const E2E_P05ConfigType *Config, E2E_P05CheckStateType *State,
                                    const uint8 *Data, uint16 Length);
Std_ReturnType __wrap_E2E_P05Check(const E2E_P05ConfigType *Config, E2E_P05CheckStateType *State,
                                    const uint8 *Data, uint16 Length)
{
    CallCount_E2E_P05Check++;
    Log_Write(LOG_T, TAG, "E2E_P05Check", "called %u times", CallCount_E2E_P05Check);

    if (CallCount_E2E_P05Check >= FailFromCallCount_E2E_P05Check)
    {
        return ForcedReturn_E2E_P05Check;
    }

    return __real_E2E_P05Check(Config, State, Data, Length);
}

/* ----------------------------------------------------------------------
 * E2E_P05CheckInit
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_E2E_P05CheckInit(E2E_P05CheckStateType *State);
Std_ReturnType __wrap_E2E_P05CheckInit(E2E_P05CheckStateType *State)
{
    CallCount_E2E_P05CheckInit++;
    Log_Write(LOG_T, TAG, "E2E_P05CheckInit", "called %u times", CallCount_E2E_P05CheckInit);

    if (CallCount_E2E_P05CheckInit >= FailFromCallCount_E2E_P05CheckInit)
    {
        return ForcedReturn_E2E_P05CheckInit;
    }

    return __real_E2E_P05CheckInit(State);
}

/* ----------------------------------------------------------------------
 * E2E_P05MapStatusToSM
 * ---------------------------------------------------------------------- */

extern
E2E_PCheckStatusType __real_E2E_P05MapStatusToSM(Std_ReturnType CheckReturn, E2E_P05StatusType Status);
E2E_PCheckStatusType __wrap_E2E_P05MapStatusToSM(Std_ReturnType CheckReturn, E2E_P05StatusType Status)
{
    CallCount_E2E_P05MapStatusToSM++;
    Log_Write(LOG_T, TAG, "E2E_P05MapStatusToSM", "called %u times", CallCount_E2E_P05MapStatusToSM);

    if (CallCount_E2E_P05MapStatusToSM >= FailFromCallCount_E2E_P05MapStatusToSM)
    {
        return ForcedReturn_E2E_P05MapStatusToSM;
    }

    return __real_E2E_P05MapStatusToSM(CheckReturn, Status);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
