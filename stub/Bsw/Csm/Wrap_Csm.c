/**
 * \file    Wrap_Csm.c
 * \brief   `src/Bsw/Csm/Csm.c` 内の `Csm_MacGenerate()` を対象とした wrap 実体（Wrap_Csm.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に Functions セクションで
 *          `__wrap_...` の実装をまとめる（`Wrap_Nm.c` と同じ構成）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Csm.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Csm"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32         CallCount_Csm_MacGenerate     = 0U;
uint32         FailFromCallCount_Csm_MacGenerate = WRAP_CSM_FAIL_FROM_CALL_COUNT_DISABLED;
Std_ReturnType ForcedReturn_Csm_MacGenerate  = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapCsm_Reset — 状態を一括で初期化する（Wrap_Csm.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapCsm_Reset(void)
{
    CallCount_Csm_MacGenerate         = 0U;
    FailFromCallCount_Csm_MacGenerate = WRAP_CSM_FAIL_FROM_CALL_COUNT_DISABLED;
    ForcedReturn_Csm_MacGenerate      = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Csm_MacGenerate
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Csm_MacGenerate(uint32 jobId, Crypto_OperationModeType mode,
                                      const uint8* dataPtr, uint32 dataLength,
                                      uint8* macPtr, uint32* macLengthPtr);
Std_ReturnType __wrap_Csm_MacGenerate(uint32 jobId, Crypto_OperationModeType mode,
                                      const uint8* dataPtr, uint32 dataLength,
                                      uint8* macPtr, uint32* macLengthPtr)
{
    CallCount_Csm_MacGenerate++;
    Log_Write(LOG_T, TAG, "Csm_MacGenerate", "called %u times", CallCount_Csm_MacGenerate);

    if (CallCount_Csm_MacGenerate >= FailFromCallCount_Csm_MacGenerate)
    {
        return ForcedReturn_Csm_MacGenerate;
    }

    return __real_Csm_MacGenerate(jobId, mode, dataPtr, dataLength, macPtr, macLengthPtr);
}
