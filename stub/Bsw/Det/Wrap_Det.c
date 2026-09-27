/**
 * \file    Wrap_Det.c
 * \brief   `src/Bsw/Det/Det.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Det.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Det"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Det_Init                   = 0U;
uint32 CallCount_Det_ReportError            = 0U;
uint32 CallCount_Det_ReportRuntimeError     = 0U;
uint32 CallCount_Det_ReportTransientFault   = 0U;
uint32 CallCount_Det_Start                  = 0U;
uint32 CallCount_Det_GetVersionInfo         = 0U;

uint16 LastModuleId_Det_ReportError   = 0xFFFFU;
uint8  LastInstanceId_Det_ReportError = 0xFFU;
uint8  LastApiId_Det_ReportError      = 0xFFU;
uint8  LastErrorId_Det_ReportError    = 0xFFU;

uint32 FailFromCallCount_Det_ReportError          = WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Det_ReportRuntimeError   = WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Det_ReportTransientFault = WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_Det_ReportError          = E_NOT_OK;
Std_ReturnType ForcedReturn_Det_ReportRuntimeError   = E_NOT_OK;
Std_ReturnType ForcedReturn_Det_ReportTransientFault = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapDet_Reset — 6関数すべての状態を一括で初期化する（Wrap_Det.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapDet_Reset(void)
{
    CallCount_Det_Init                 = 0U;
    CallCount_Det_ReportError          = 0U;
    CallCount_Det_ReportRuntimeError   = 0U;
    CallCount_Det_ReportTransientFault = 0U;
    CallCount_Det_Start                = 0U;
    CallCount_Det_GetVersionInfo       = 0U;

    LastModuleId_Det_ReportError   = 0xFFFFU;
    LastInstanceId_Det_ReportError = 0xFFU;
    LastApiId_Det_ReportError      = 0xFFU;
    LastErrorId_Det_ReportError    = 0xFFU;

    FailFromCallCount_Det_ReportError          = WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Det_ReportRuntimeError   = WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Det_ReportTransientFault = WRAP_DET_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_Det_ReportError          = E_NOT_OK;
    ForcedReturn_Det_ReportRuntimeError   = E_NOT_OK;
    ForcedReturn_Det_ReportTransientFault = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Det_Init
 * ---------------------------------------------------------------------- */

extern
void __real_Det_Init(const Det_ConfigType* ConfigPtr);
void __wrap_Det_Init(const Det_ConfigType* ConfigPtr)
{
    CallCount_Det_Init++;
    Log_Write(LOG_T, TAG, "Det_Init", "called %u times", CallCount_Det_Init);

    __real_Det_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * Det_ReportError
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);
Std_ReturnType __wrap_Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    CallCount_Det_ReportError++;
    LastModuleId_Det_ReportError   = ModuleId;
    LastInstanceId_Det_ReportError = InstanceId;
    LastApiId_Det_ReportError      = ApiId;
    LastErrorId_Det_ReportError    = ErrorId;
    Log_Write(LOG_T, TAG, "Det_ReportError", "called %u times", CallCount_Det_ReportError);

    if (CallCount_Det_ReportError >= FailFromCallCount_Det_ReportError)
    {
        return ForcedReturn_Det_ReportError;
    }

    return __real_Det_ReportError(ModuleId, InstanceId, ApiId, ErrorId);
}

/* ----------------------------------------------------------------------
 * Det_Start
 * ---------------------------------------------------------------------- */

extern
void __real_Det_Start(void);
void __wrap_Det_Start(void)
{
    CallCount_Det_Start++;
    Log_Write(LOG_T, TAG, "Det_Start", "called %u times", CallCount_Det_Start);

    __real_Det_Start();
}

/* ----------------------------------------------------------------------
 * Det_ReportRuntimeError
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId);
Std_ReturnType __wrap_Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    CallCount_Det_ReportRuntimeError++;
    Log_Write(LOG_T, TAG, "Det_ReportRuntimeError", "called %u times", CallCount_Det_ReportRuntimeError);

    if (CallCount_Det_ReportRuntimeError >= FailFromCallCount_Det_ReportRuntimeError)
    {
        return ForcedReturn_Det_ReportRuntimeError;
    }

    return __real_Det_ReportRuntimeError(ModuleId, InstanceId, ApiId, ErrorId);
}

/* ----------------------------------------------------------------------
 * Det_ReportTransientFault
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Det_ReportTransientFault(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 FaultId);
Std_ReturnType __wrap_Det_ReportTransientFault(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 FaultId)
{
    CallCount_Det_ReportTransientFault++;
    Log_Write(LOG_T, TAG, "Det_ReportTransientFault", "called %u times", CallCount_Det_ReportTransientFault);

    if (CallCount_Det_ReportTransientFault >= FailFromCallCount_Det_ReportTransientFault)
    {
        return ForcedReturn_Det_ReportTransientFault;
    }

    return __real_Det_ReportTransientFault(ModuleId, InstanceId, ApiId, FaultId);
}

/* ----------------------------------------------------------------------
 * Det_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_Det_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_Det_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_Det_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "Det_GetVersionInfo", "called %u times", CallCount_Det_GetVersionInfo);

    __real_Det_GetVersionInfo(versioninfo);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
