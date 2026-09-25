/**
 * \file    Wrap_E2EXf.c
 * \brief   `src/Bsw/E2EXf/E2EXf.c` 内の関数を対象とした wrap 実体
 *          （Wrap_E2EXf.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c`/`Wrap_E2E_P05.c` と同じ構成。
 *          `[[reference_wrap_stub_naming_convention]]` 参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_E2EXf.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "E2EXf"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_E2EXf_E2EHealthStatus  = 0U;
uint32 CallCount_E2EXf_Inv_EngineInfo   = 0U;
uint32 CallCount_E2EXf_Inv_AbsInfo      = 0U;
uint32 CallCount_E2EXf_Init             = 0U;
uint32 CallCount_E2EXf_DeInit           = 0U;
uint32 CallCount_E2EXf_GetVersionInfo   = 0U;

uint32 FailFromCallCount_E2EXf_E2EHealthStatus = WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2EXf_Inv_EngineInfo  = WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_E2EXf_Inv_AbsInfo     = WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED;

uint8 ForcedReturn_E2EXf_E2EHealthStatus = E_SAFETY_HARD_RUNTIMEERROR;
uint8 ForcedReturn_E2EXf_Inv_EngineInfo  = E_SAFETY_HARD_RUNTIMEERROR;
uint8 ForcedReturn_E2EXf_Inv_AbsInfo     = E_SAFETY_HARD_RUNTIMEERROR;

/* ----------------------------------------------------------------------
 * WrapE2EXf_Reset — 7関数すべての状態を一括で初期化する（Wrap_E2EXf.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapE2EXf_Reset(void)
{
    CallCount_E2EXf_E2EHealthStatus = 0U;
    CallCount_E2EXf_Inv_EngineInfo  = 0U;
    CallCount_E2EXf_Inv_AbsInfo     = 0U;
    CallCount_E2EXf_Init            = 0U;
    CallCount_E2EXf_DeInit          = 0U;
    CallCount_E2EXf_GetVersionInfo  = 0U;

    FailFromCallCount_E2EXf_E2EHealthStatus = WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2EXf_Inv_EngineInfo  = WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_E2EXf_Inv_AbsInfo     = WRAP_E2EXF_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_E2EXf_E2EHealthStatus = E_SAFETY_HARD_RUNTIMEERROR;
    ForcedReturn_E2EXf_Inv_EngineInfo  = E_SAFETY_HARD_RUNTIMEERROR;
    ForcedReturn_E2EXf_Inv_AbsInfo     = E_SAFETY_HARD_RUNTIMEERROR;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * E2EXf_<transformerId>
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * E2EXf_E2EHealthStatus
 * ---------------------------------------------------------------------- */

extern
uint8 __real_E2EXf_E2EHealthStatus(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer,
                                    uint32 inputBufferLength);
uint8 __wrap_E2EXf_E2EHealthStatus(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer,
                                    uint32 inputBufferLength)
{
    CallCount_E2EXf_E2EHealthStatus++;
    Log_Write(LOG_T, TAG, "E2EXf_E2EHealthStatus", "called %u times", CallCount_E2EXf_E2EHealthStatus);

    if (CallCount_E2EXf_E2EHealthStatus >= FailFromCallCount_E2EXf_E2EHealthStatus)
    {
        return ForcedReturn_E2EXf_E2EHealthStatus;
    }

    return __real_E2EXf_E2EHealthStatus(buffer, bufferLength, inputBuffer, inputBufferLength);
}

/* ----------------------------------------------------------------------
 * E2EXf_Inv_<transformerId>
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * E2EXf_Inv_EngineInfo
 * ---------------------------------------------------------------------- */

extern
uint8 __real_E2EXf_Inv_EngineInfo(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer,
                                   uint32 inputBufferLength, E2E_P05StatusType* CheckStatus);
uint8 __wrap_E2EXf_Inv_EngineInfo(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer,
                                   uint32 inputBufferLength, E2E_P05StatusType* CheckStatus)
{
    CallCount_E2EXf_Inv_EngineInfo++;
    Log_Write(LOG_T, TAG, "E2EXf_Inv_EngineInfo", "called %u times", CallCount_E2EXf_Inv_EngineInfo);

    if (CallCount_E2EXf_Inv_EngineInfo >= FailFromCallCount_E2EXf_Inv_EngineInfo)
    {
        return ForcedReturn_E2EXf_Inv_EngineInfo;
    }

    return __real_E2EXf_Inv_EngineInfo(buffer, bufferLength, inputBuffer, inputBufferLength, CheckStatus);
}

/* ----------------------------------------------------------------------
 * E2EXf_Inv_AbsInfo
 * ---------------------------------------------------------------------- */

extern
uint8 __real_E2EXf_Inv_AbsInfo(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer,
                                uint32 inputBufferLength, E2E_P05StatusType* CheckStatus);
uint8 __wrap_E2EXf_Inv_AbsInfo(uint8* buffer, uint32* bufferLength, const uint8* inputBuffer,
                                uint32 inputBufferLength, E2E_P05StatusType* CheckStatus)
{
    CallCount_E2EXf_Inv_AbsInfo++;
    Log_Write(LOG_T, TAG, "E2EXf_Inv_AbsInfo", "called %u times", CallCount_E2EXf_Inv_AbsInfo);

    if (CallCount_E2EXf_Inv_AbsInfo >= FailFromCallCount_E2EXf_Inv_AbsInfo)
    {
        return ForcedReturn_E2EXf_Inv_AbsInfo;
    }

    return __real_E2EXf_Inv_AbsInfo(buffer, bufferLength, inputBuffer, inputBufferLength, CheckStatus);
}

/* ----------------------------------------------------------------------
 * E2EXf_Init
 * ---------------------------------------------------------------------- */

extern
void __real_E2EXf_Init(const E2EXf_ConfigType* ConfigPtr);
void __wrap_E2EXf_Init(const E2EXf_ConfigType* ConfigPtr)
{
    CallCount_E2EXf_Init++;
    Log_Write(LOG_T, TAG, "E2EXf_Init", "called %u times", CallCount_E2EXf_Init);

    __real_E2EXf_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * E2EXf_DeInit
 * ---------------------------------------------------------------------- */

extern
void __real_E2EXf_DeInit(void);
void __wrap_E2EXf_DeInit(void)
{
    CallCount_E2EXf_DeInit++;
    Log_Write(LOG_T, TAG, "E2EXf_DeInit", "called %u times", CallCount_E2EXf_DeInit);

    __real_E2EXf_DeInit();
}

/* ----------------------------------------------------------------------
 * E2EXf_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_E2EXf_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_E2EXf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_E2EXf_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "E2EXf_GetVersionInfo", "called %u times", CallCount_E2EXf_GetVersionInfo);

    __real_E2EXf_GetVersionInfo(versioninfo);
}

/* ======================================================================
 * Call-back notifications
 * ====================================================================== */

/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
