/**
 * \file    Wrap_WdgM.c
 * \brief   `src/Bsw/WdgM/WdgM.c` 内の関数を対象とした wrap 実体
 *          （Wrap_WdgM.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_WdgM.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "WdgM"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_WdgM_Init                   = 0U;
uint32 CallCount_WdgM_DeInit                 = 0U;
uint32 CallCount_WdgM_GetVersionInfo         = 0U;
uint32 CallCount_WdgM_SetMode                = 0U;
uint32 CallCount_WdgM_GetMode                = 0U;
uint32 CallCount_WdgM_EnableHwWatchdog       = 0U;
uint32 CallCount_WdgM_DisableHwWatchdog      = 0U;
uint32 CallCount_WdgM_ResumeSupervision      = 0U;
uint32 CallCount_WdgM_CheckpointReached      = 0U;
uint32 CallCount_WdgM_GetLocalStatus         = 0U;
uint32 CallCount_WdgM_GetGlobalStatus        = 0U;
uint32 CallCount_WdgM_MainFunction           = 0U;
uint32 CallCount_WdgM_TriggerHwWatchdog      = 0U;
uint32 CallCount_WdgM_PerformReset           = 0U;
uint32 CallCount_WdgM_GetFirstExpiredSEID    = 0U;

uint32 FailFromCallCount_WdgM_SetMode             = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_WdgM_GetMode             = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_WdgM_CheckpointReached   = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_WdgM_GetLocalStatus      = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_WdgM_GetGlobalStatus     = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_WdgM_GetFirstExpiredSEID = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_WdgM_SetMode             = E_NOT_OK;
Std_ReturnType ForcedReturn_WdgM_GetMode             = E_NOT_OK;
Std_ReturnType ForcedReturn_WdgM_CheckpointReached   = E_NOT_OK;
Std_ReturnType ForcedReturn_WdgM_GetLocalStatus      = E_NOT_OK;
Std_ReturnType ForcedReturn_WdgM_GetGlobalStatus     = E_NOT_OK;
Std_ReturnType ForcedReturn_WdgM_GetFirstExpiredSEID = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapWdgM_Reset — 15関数すべての状態を一括で初期化する（Wrap_WdgM.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapWdgM_Reset(void)
{
    CallCount_WdgM_Init                = 0U;
    CallCount_WdgM_DeInit              = 0U;
    CallCount_WdgM_GetVersionInfo      = 0U;
    CallCount_WdgM_SetMode             = 0U;
    CallCount_WdgM_GetMode             = 0U;
    CallCount_WdgM_EnableHwWatchdog    = 0U;
    CallCount_WdgM_DisableHwWatchdog   = 0U;
    CallCount_WdgM_ResumeSupervision   = 0U;
    CallCount_WdgM_CheckpointReached   = 0U;
    CallCount_WdgM_GetLocalStatus      = 0U;
    CallCount_WdgM_GetGlobalStatus     = 0U;
    CallCount_WdgM_MainFunction        = 0U;
    CallCount_WdgM_TriggerHwWatchdog   = 0U;
    CallCount_WdgM_PerformReset        = 0U;
    CallCount_WdgM_GetFirstExpiredSEID = 0U;

    FailFromCallCount_WdgM_SetMode             = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_WdgM_GetMode             = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_WdgM_CheckpointReached   = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_WdgM_GetLocalStatus      = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_WdgM_GetGlobalStatus     = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_WdgM_GetFirstExpiredSEID = WRAP_WDGM_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_WdgM_SetMode             = E_NOT_OK;
    ForcedReturn_WdgM_GetMode             = E_NOT_OK;
    ForcedReturn_WdgM_CheckpointReached   = E_NOT_OK;
    ForcedReturn_WdgM_GetLocalStatus      = E_NOT_OK;
    ForcedReturn_WdgM_GetGlobalStatus     = E_NOT_OK;
    ForcedReturn_WdgM_GetFirstExpiredSEID = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * WdgM_Init
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_Init(const WdgM_ConfigType* ConfigPtr);
void __wrap_WdgM_Init(const WdgM_ConfigType* ConfigPtr)
{
    CallCount_WdgM_Init++;
    Log_Write(LOG_T, TAG, "WdgM_Init", "called %u times", CallCount_WdgM_Init);

    __real_WdgM_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * WdgM_DeInit
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_DeInit(void);
void __wrap_WdgM_DeInit(void)
{
    CallCount_WdgM_DeInit++;
    Log_Write(LOG_T, TAG, "WdgM_DeInit", "called %u times", CallCount_WdgM_DeInit);

    __real_WdgM_DeInit();
}

/* ----------------------------------------------------------------------
 * WdgM_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_GetVersionInfo(Std_VersionInfoType* VersionInfo);
void __wrap_WdgM_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    CallCount_WdgM_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "WdgM_GetVersionInfo", "called %u times", CallCount_WdgM_GetVersionInfo);

    __real_WdgM_GetVersionInfo(VersionInfo);
}

/* ----------------------------------------------------------------------
 * WdgM_SetMode
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_WdgM_SetMode(WdgM_ModeType Mode);
Std_ReturnType __wrap_WdgM_SetMode(WdgM_ModeType Mode)
{
    CallCount_WdgM_SetMode++;
    Log_Write(LOG_T, TAG, "WdgM_SetMode", "called %u times", CallCount_WdgM_SetMode);

    if (CallCount_WdgM_SetMode >= FailFromCallCount_WdgM_SetMode)
    {
        return ForcedReturn_WdgM_SetMode;
    }

    return __real_WdgM_SetMode(Mode);
}

/* ----------------------------------------------------------------------
 * WdgM_GetMode
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_WdgM_GetMode(WdgM_ModeType* Mode);
Std_ReturnType __wrap_WdgM_GetMode(WdgM_ModeType* Mode)
{
    CallCount_WdgM_GetMode++;
    Log_Write(LOG_T, TAG, "WdgM_GetMode", "called %u times", CallCount_WdgM_GetMode);

    if (CallCount_WdgM_GetMode >= FailFromCallCount_WdgM_GetMode)
    {
        return ForcedReturn_WdgM_GetMode;
    }

    return __real_WdgM_GetMode(Mode);
}

/* ----------------------------------------------------------------------
 * WdgM_EnableHwWatchdog
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_EnableHwWatchdog(void);
void __wrap_WdgM_EnableHwWatchdog(void)
{
    CallCount_WdgM_EnableHwWatchdog++;
    Log_Write(LOG_T, TAG, "WdgM_EnableHwWatchdog", "called %u times", CallCount_WdgM_EnableHwWatchdog);

    __real_WdgM_EnableHwWatchdog();
}

/* ----------------------------------------------------------------------
 * WdgM_DisableHwWatchdog
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_DisableHwWatchdog(void);
void __wrap_WdgM_DisableHwWatchdog(void)
{
    CallCount_WdgM_DisableHwWatchdog++;
    Log_Write(LOG_T, TAG, "WdgM_DisableHwWatchdog", "called %u times", CallCount_WdgM_DisableHwWatchdog);

    __real_WdgM_DisableHwWatchdog();
}

/* ----------------------------------------------------------------------
 * WdgM_ResumeSupervision
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_ResumeSupervision(void);
void __wrap_WdgM_ResumeSupervision(void)
{
    CallCount_WdgM_ResumeSupervision++;
    Log_Write(LOG_T, TAG, "WdgM_ResumeSupervision", "called %u times", CallCount_WdgM_ResumeSupervision);

    __real_WdgM_ResumeSupervision();
}

/* ----------------------------------------------------------------------
 * WdgM_CheckpointReached
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID, WdgM_CheckpointIdType CheckpointId);
Std_ReturnType __wrap_WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID, WdgM_CheckpointIdType CheckpointId)
{
    CallCount_WdgM_CheckpointReached++;
    Log_Write(LOG_T, TAG, "WdgM_CheckpointReached", "called %u times", CallCount_WdgM_CheckpointReached);

    if (CallCount_WdgM_CheckpointReached >= FailFromCallCount_WdgM_CheckpointReached)
    {
        return ForcedReturn_WdgM_CheckpointReached;
    }

    return __real_WdgM_CheckpointReached(SEID, CheckpointId);
}

/* ----------------------------------------------------------------------
 * WdgM_GetLocalStatus
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID, WdgM_LocalStatusType* Status);
Std_ReturnType __wrap_WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID, WdgM_LocalStatusType* Status)
{
    CallCount_WdgM_GetLocalStatus++;
    Log_Write(LOG_T, TAG, "WdgM_GetLocalStatus", "called %u times", CallCount_WdgM_GetLocalStatus);

    if (CallCount_WdgM_GetLocalStatus >= FailFromCallCount_WdgM_GetLocalStatus)
    {
        return ForcedReturn_WdgM_GetLocalStatus;
    }

    return __real_WdgM_GetLocalStatus(SEID, Status);
}

/* ----------------------------------------------------------------------
 * WdgM_GetGlobalStatus
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_WdgM_GetGlobalStatus(WdgM_GlobalStatusType* Status);
Std_ReturnType __wrap_WdgM_GetGlobalStatus(WdgM_GlobalStatusType* Status)
{
    CallCount_WdgM_GetGlobalStatus++;
    Log_Write(LOG_T, TAG, "WdgM_GetGlobalStatus", "called %u times", CallCount_WdgM_GetGlobalStatus);

    if (CallCount_WdgM_GetGlobalStatus >= FailFromCallCount_WdgM_GetGlobalStatus)
    {
        return ForcedReturn_WdgM_GetGlobalStatus;
    }

    return __real_WdgM_GetGlobalStatus(Status);
}

/* ----------------------------------------------------------------------
 * WdgM_MainFunction
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_MainFunction(void);
void __wrap_WdgM_MainFunction(void)
{
    CallCount_WdgM_MainFunction++;
    Log_Write(LOG_T, TAG, "WdgM_MainFunction", "called %u times", CallCount_WdgM_MainFunction);

    __real_WdgM_MainFunction();
}

/* ----------------------------------------------------------------------
 * WdgM_TriggerHwWatchdog
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_TriggerHwWatchdog(void);
void __wrap_WdgM_TriggerHwWatchdog(void)
{
    CallCount_WdgM_TriggerHwWatchdog++;
    Log_Write(LOG_T, TAG, "WdgM_TriggerHwWatchdog", "called %u times", CallCount_WdgM_TriggerHwWatchdog);

    __real_WdgM_TriggerHwWatchdog();
}

/* ----------------------------------------------------------------------
 * WdgM_PerformReset
 * ---------------------------------------------------------------------- */

extern
void __real_WdgM_PerformReset(void);
void __wrap_WdgM_PerformReset(void)
{
    CallCount_WdgM_PerformReset++;
    Log_Write(LOG_T, TAG, "WdgM_PerformReset", "called %u times", CallCount_WdgM_PerformReset);

    __real_WdgM_PerformReset();
}

/* ----------------------------------------------------------------------
 * WdgM_GetFirstExpiredSEID
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_WdgM_GetFirstExpiredSEID(WdgM_SupervisedEntityIdType* SEID);
Std_ReturnType __wrap_WdgM_GetFirstExpiredSEID(WdgM_SupervisedEntityIdType* SEID)
{
    CallCount_WdgM_GetFirstExpiredSEID++;
    Log_Write(LOG_T, TAG, "WdgM_GetFirstExpiredSEID", "called %u times", CallCount_WdgM_GetFirstExpiredSEID);

    if (CallCount_WdgM_GetFirstExpiredSEID >= FailFromCallCount_WdgM_GetFirstExpiredSEID)
    {
        return ForcedReturn_WdgM_GetFirstExpiredSEID;
    }

    return __real_WdgM_GetFirstExpiredSEID(SEID);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
