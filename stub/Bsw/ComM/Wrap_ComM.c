/**
 * \file    Wrap_ComM.c
 * \brief   `src/Bsw/ComM/ComM.c` 内の関数を対象とした wrap 実体
 *          （Wrap_ComM.h 参照）。
 */
#include "Wrap_ComM.h"

/* ======================================================================
 * External Variables
 * ====================================================================== */
uint32 CallCount_ComM_DCM_ActiveDiagnostic   = 0U;
uint32 CallCount_ComM_DCM_InactiveDiagnostic = 0U;

uint8 Suppressed_ComM_DcmDiagnostic = 0U;

/* ----------------------------------------------------------------------
 * WrapComM_Reset — すべての関数状態を一括で初期化する（Wrap_ComM.h 参照）。
 * ---------------------------------------------------------------------- */
void WrapComM_Reset(void)
{
    CallCount_ComM_DCM_ActiveDiagnostic   = 0U;
    CallCount_ComM_DCM_InactiveDiagnostic = 0U;

    Suppressed_ComM_DcmDiagnostic = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * ComM_DCM_ActiveDiagnostic
 * ---------------------------------------------------------------------- */
extern
void __real_ComM_DCM_ActiveDiagnostic(NetworkHandleType Channel);
void __wrap_ComM_DCM_ActiveDiagnostic(NetworkHandleType Channel)
{
    CallCount_ComM_DCM_ActiveDiagnostic++;

    if (Suppressed_ComM_DcmDiagnostic)
    {
        return;
    }

    __real_ComM_DCM_ActiveDiagnostic(Channel);
}

/* ----------------------------------------------------------------------
 * ComM_DCM_InactiveDiagnostic
 * ---------------------------------------------------------------------- */
extern
void __real_ComM_DCM_InactiveDiagnostic(NetworkHandleType Channel);
void __wrap_ComM_DCM_InactiveDiagnostic(NetworkHandleType Channel)
{
    CallCount_ComM_DCM_InactiveDiagnostic++;

    if (Suppressed_ComM_DcmDiagnostic)
    {
        return;
    }

    __real_ComM_DCM_InactiveDiagnostic(Channel);
}
