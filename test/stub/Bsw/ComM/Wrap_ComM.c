/**
 * \file    Wrap_ComM.c
 * \brief   `src/Bsw/ComM/ComM.c` 内の関数を対象とした wrap 実体
 *          （Wrap_ComM.h 参照）。
 */
#include "Wrap_ComM.h"
#include "ComM.h"

extern void __real_ComM_DCM_ActiveDiagnostic(NetworkHandleType Channel);
extern void __real_ComM_DCM_InactiveDiagnostic(NetworkHandleType Channel);

uint8 WrapComM_DcmDiagnosticSuppressed = 0U;

void WrapComM_Reset(void)
{
    WrapComM_DcmDiagnosticSuppressed = 0U;
}

void __wrap_ComM_DCM_ActiveDiagnostic(NetworkHandleType Channel)
{
    if (WrapComM_DcmDiagnosticSuppressed)
        return;

    __real_ComM_DCM_ActiveDiagnostic(Channel);
}

void __wrap_ComM_DCM_InactiveDiagnostic(NetworkHandleType Channel)
{
    if (WrapComM_DcmDiagnosticSuppressed)
        return;

    __real_ComM_DCM_InactiveDiagnostic(Channel);
}
