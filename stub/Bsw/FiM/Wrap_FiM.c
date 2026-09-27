/**
 * \file    Wrap_FiM.c
 * \brief   `src/Bsw/FiM/FiM.c` 内の関数を対象とした wrap 実体
 *          （Wrap_FiM.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_FiM.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "FiM"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_FiM_Init                     = 0U;
uint32 CallCount_FiM_GetFunctionPermission    = 0U;
uint32 CallCount_FiM_SetFunctionAvailable     = 0U;
uint32 CallCount_FiM_GetVersionInfo           = 0U;
uint32 CallCount_FiM_MainFunction             = 0U;

uint32 FailFromCallCount_FiM_GetFunctionPermission = WRAP_FIM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_FiM_SetFunctionAvailable  = WRAP_FIM_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_FiM_GetFunctionPermission = E_NOT_OK;
Std_ReturnType ForcedReturn_FiM_SetFunctionAvailable  = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapFiM_Reset — 5関数すべての状態を一括で初期化する（Wrap_FiM.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapFiM_Reset(void)
{
    CallCount_FiM_Init                  = 0U;
    CallCount_FiM_GetFunctionPermission = 0U;
    CallCount_FiM_SetFunctionAvailable  = 0U;
    CallCount_FiM_GetVersionInfo        = 0U;
    CallCount_FiM_MainFunction          = 0U;

    FailFromCallCount_FiM_GetFunctionPermission = WRAP_FIM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_FiM_SetFunctionAvailable  = WRAP_FIM_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_FiM_GetFunctionPermission = E_NOT_OK;
    ForcedReturn_FiM_SetFunctionAvailable  = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * FiM_Init
 * ---------------------------------------------------------------------- */

extern
void __real_FiM_Init(const FiM_ConfigType* ConfigPtr);
void __wrap_FiM_Init(const FiM_ConfigType* ConfigPtr)
{
    CallCount_FiM_Init++;
    Log_Write(LOG_T, TAG, "FiM_Init", "called %u times", CallCount_FiM_Init);

    __real_FiM_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * FiM_GetFunctionPermission
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_FiM_GetFunctionPermission(FiM_FunctionIdType FunctionId, boolean* Permission);
Std_ReturnType __wrap_FiM_GetFunctionPermission(FiM_FunctionIdType FunctionId, boolean* Permission)
{
    CallCount_FiM_GetFunctionPermission++;
    Log_Write(LOG_T, TAG, "FiM_GetFunctionPermission", "called %u times", CallCount_FiM_GetFunctionPermission);

    if (CallCount_FiM_GetFunctionPermission >= FailFromCallCount_FiM_GetFunctionPermission)
    {
        return ForcedReturn_FiM_GetFunctionPermission;
    }

    return __real_FiM_GetFunctionPermission(FunctionId, Permission);
}

/* ----------------------------------------------------------------------
 * FiM_SetFunctionAvailable
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_FiM_SetFunctionAvailable(FiM_FunctionIdType FID, boolean Availability);
Std_ReturnType __wrap_FiM_SetFunctionAvailable(FiM_FunctionIdType FID, boolean Availability)
{
    CallCount_FiM_SetFunctionAvailable++;
    Log_Write(LOG_T, TAG, "FiM_SetFunctionAvailable", "called %u times", CallCount_FiM_SetFunctionAvailable);

    if (CallCount_FiM_SetFunctionAvailable >= FailFromCallCount_FiM_SetFunctionAvailable)
    {
        return ForcedReturn_FiM_SetFunctionAvailable;
    }

    return __real_FiM_SetFunctionAvailable(FID, Availability);
}

/* ----------------------------------------------------------------------
 * FiM_DemTriggerOnMonitorStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * FiM_DemTriggerOnComponentStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * FiM_DemInit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * FiM_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_FiM_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_FiM_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_FiM_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "FiM_GetVersionInfo", "called %u times", CallCount_FiM_GetVersionInfo);

    __real_FiM_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * FiM_MainFunction
 * ---------------------------------------------------------------------- */

extern
void __real_FiM_MainFunction(void);
void __wrap_FiM_MainFunction(void)
{
    CallCount_FiM_MainFunction++;
    Log_Write(LOG_T, TAG, "FiM_MainFunction", "called %u times", CallCount_FiM_MainFunction);

    __real_FiM_MainFunction();
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
