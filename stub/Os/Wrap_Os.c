/**
 * \file    Wrap_Os.c
 * \brief   `src/Os/Os.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Os.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Os.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Os"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Os_Init             = 0U;
uint32 CallCount_Os_SchedulerStep    = 0U;
uint32 CallCount_Os_SetTaskActive    = 0U;

/* ----------------------------------------------------------------------
 * WrapOs_Reset — 3関数すべての状態を一括で初期化する（Wrap_Os.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapOs_Reset(void)
{
    CallCount_Os_Init          = 0U;
    CallCount_Os_SchedulerStep = 0U;
    CallCount_Os_SetTaskActive = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Os_Init
 * ---------------------------------------------------------------------- */

extern
void __real_Os_Init(const Os_ConfigType* ConfigPtr);
void __wrap_Os_Init(const Os_ConfigType* ConfigPtr)
{
    CallCount_Os_Init++;
    Log_Write(LOG_T, TAG, "Os_Init", "called %u times", CallCount_Os_Init);

    __real_Os_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * Os_SchedulerStep
 * ---------------------------------------------------------------------- */

extern
void __real_Os_SchedulerStep(void);
void __wrap_Os_SchedulerStep(void)
{
    CallCount_Os_SchedulerStep++;
    Log_Write(LOG_T, TAG, "Os_SchedulerStep", "called %u times", CallCount_Os_SchedulerStep);

    __real_Os_SchedulerStep();
}

/* ----------------------------------------------------------------------
 * Os_SetTaskActive
 * ---------------------------------------------------------------------- */

extern
void __real_Os_SetTaskActive(uint8 TaskId, uint8 Active);
void __wrap_Os_SetTaskActive(uint8 TaskId, uint8 Active)
{
    CallCount_Os_SetTaskActive++;
    Log_Write(LOG_T, TAG, "Os_SetTaskActive", "called %u times", CallCount_Os_SetTaskActive);

    __real_Os_SetTaskActive(TaskId, Active);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
