/**
 * \file    Wrap_MemIf.c
 * \brief   `src/Bsw/MemIf/MemIf.c` 内の関数を対象とした wrap 実体
 *          （Wrap_MemIf.h 参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_MemIf.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "MemIf"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_MemIf_Init           = 0U;
uint32 CallCount_MemIf_GetVersionInfo = 0U;
uint32 CallCount_MemIf_SetMode        = 0U;
uint32 CallCount_MemIf_Read           = 0U;
uint32 CallCount_MemIf_Write          = 0U;
uint32 CallCount_MemIf_Cancel         = 0U;
uint32 CallCount_MemIf_GetStatus      = 0U;
uint32 CallCount_MemIf_GetJobResult   = 0U;
uint32 CallCount_MemIf_MainFunction   = 0U;

uint32 FailFromCallCount_MemIf_Read         = WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_MemIf_Write        = WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_MemIf_GetStatus    = WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_MemIf_GetJobResult = WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType      ForcedReturn_MemIf_Read         = E_NOT_OK;
Std_ReturnType      ForcedReturn_MemIf_Write        = E_NOT_OK;
MemIf_StatusType    ForcedReturn_MemIf_GetStatus    = MEMIF_BUSY;
MemIf_JobResultType ForcedReturn_MemIf_GetJobResult = MEMIF_JOB_FAILED;

/* ======================================================================
 * Functions
 * ====================================================================== */

void WrapMemIf_Reset(void)
{
    CallCount_MemIf_Init           = 0U;
    CallCount_MemIf_GetVersionInfo = 0U;
    CallCount_MemIf_SetMode        = 0U;
    CallCount_MemIf_Read           = 0U;
    CallCount_MemIf_Write          = 0U;
    CallCount_MemIf_Cancel         = 0U;
    CallCount_MemIf_GetStatus      = 0U;
    CallCount_MemIf_GetJobResult   = 0U;
    CallCount_MemIf_MainFunction   = 0U;

    FailFromCallCount_MemIf_Read         = WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_MemIf_Write        = WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_MemIf_GetStatus    = WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_MemIf_GetJobResult = WRAP_MEMIF_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_MemIf_Read         = E_NOT_OK;
    ForcedReturn_MemIf_Write        = E_NOT_OK;
    ForcedReturn_MemIf_GetStatus    = MEMIF_BUSY;
    ForcedReturn_MemIf_GetJobResult = MEMIF_JOB_FAILED;
}

/* ----------------------------------------------------------------------
 * MemIf_Init
 * ---------------------------------------------------------------------- */
extern
void __real_MemIf_Init(void);
void __wrap_MemIf_Init(void)
{
    CallCount_MemIf_Init++;
    Log_Write(LOG_T, TAG, "MemIf_Init", "called %u times", CallCount_MemIf_Init);

    __real_MemIf_Init();
}

/* ----------------------------------------------------------------------
 * MemIf_GetVersionInfo
 * ---------------------------------------------------------------------- */
extern
void __real_MemIf_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_MemIf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_MemIf_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "MemIf_GetVersionInfo", "called %u times", CallCount_MemIf_GetVersionInfo);

    __real_MemIf_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * MemIf_SetMode
 * ---------------------------------------------------------------------- */
extern
void __real_MemIf_SetMode(MemIf_ModeType Mode);
void __wrap_MemIf_SetMode(MemIf_ModeType Mode)
{
    CallCount_MemIf_SetMode++;
    Log_Write(LOG_T, TAG, "MemIf_SetMode", "called %u times", CallCount_MemIf_SetMode);

    __real_MemIf_SetMode(Mode);
}

/* ----------------------------------------------------------------------
 * MemIf_Read
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_MemIf_Read(MemIf_DeviceType Device, uint16 Address, uint8* DataBufferPtr, uint16 Length);
Std_ReturnType __wrap_MemIf_Read(MemIf_DeviceType Device, uint16 Address, uint8* DataBufferPtr, uint16 Length)
{
    CallCount_MemIf_Read++;
    Log_Write(LOG_T, TAG, "MemIf_Read", "called %u times", CallCount_MemIf_Read);

    if (CallCount_MemIf_Read >= FailFromCallCount_MemIf_Read)
    {
        return ForcedReturn_MemIf_Read;
    }
    return __real_MemIf_Read(Device, Address, DataBufferPtr, Length);
}

/* ----------------------------------------------------------------------
 * MemIf_Write
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_MemIf_Write(MemIf_DeviceType Device, uint16 Address, const uint8* DataBufferPtr, uint16 Length);
Std_ReturnType __wrap_MemIf_Write(MemIf_DeviceType Device, uint16 Address, const uint8* DataBufferPtr, uint16 Length)
{
    CallCount_MemIf_Write++;
    Log_Write(LOG_T, TAG, "MemIf_Write", "called %u times", CallCount_MemIf_Write);

    if (CallCount_MemIf_Write >= FailFromCallCount_MemIf_Write)
    {
        return ForcedReturn_MemIf_Write;
    }
    return __real_MemIf_Write(Device, Address, DataBufferPtr, Length);
}

/* ----------------------------------------------------------------------
 * MemIf_Cancel
 * ---------------------------------------------------------------------- */
extern
void __real_MemIf_Cancel(MemIf_DeviceType Device);
void __wrap_MemIf_Cancel(MemIf_DeviceType Device)
{
    CallCount_MemIf_Cancel++;
    Log_Write(LOG_T, TAG, "MemIf_Cancel", "called %u times", CallCount_MemIf_Cancel);

    __real_MemIf_Cancel(Device);
}

/* ----------------------------------------------------------------------
 * MemIf_GetStatus
 * ---------------------------------------------------------------------- */
extern
MemIf_StatusType __real_MemIf_GetStatus(MemIf_DeviceType Device);
MemIf_StatusType __wrap_MemIf_GetStatus(MemIf_DeviceType Device)
{
    CallCount_MemIf_GetStatus++;

    if (CallCount_MemIf_GetStatus >= FailFromCallCount_MemIf_GetStatus)
    {
        return ForcedReturn_MemIf_GetStatus;
    }
    return __real_MemIf_GetStatus(Device);
}

/* ----------------------------------------------------------------------
 * MemIf_GetJobResult
 * ---------------------------------------------------------------------- */
extern
MemIf_JobResultType __real_MemIf_GetJobResult(MemIf_DeviceType Device);
MemIf_JobResultType __wrap_MemIf_GetJobResult(MemIf_DeviceType Device)
{
    CallCount_MemIf_GetJobResult++;

    if (CallCount_MemIf_GetJobResult >= FailFromCallCount_MemIf_GetJobResult)
    {
        return ForcedReturn_MemIf_GetJobResult;
    }
    return __real_MemIf_GetJobResult(Device);
}

/* ----------------------------------------------------------------------
 * MemIf_MainFunction
 * ---------------------------------------------------------------------- */
extern
void __real_MemIf_MainFunction(void);
void __wrap_MemIf_MainFunction(void)
{
    CallCount_MemIf_MainFunction++;

    __real_MemIf_MainFunction();
}
