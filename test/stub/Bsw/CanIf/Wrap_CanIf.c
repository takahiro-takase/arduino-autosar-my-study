/**
 * \file    Wrap_CanIf.c
 * \brief   `src/Bsw/CanIf/CanIf.c` 内の関数を対象とした wrap 実体
 *          （Wrap_CanIf.h 参照）。
 */
#include "Wrap_CanIf.h"

/* ----------------------------------------------------------------------
 * CanIf_SetControllerMode
 * ---------------------------------------------------------------------- */
extern Std_ReturnType __real_CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode);

uint32 WrapCanIfSetControllerMode_CallCount = 0U;
uint8  WrapCanIfSetControllerMode_ForceFail = 0U;

void WrapCanIfSetControllerMode_Reset(void)
{
    WrapCanIfSetControllerMode_CallCount = 0U;
    WrapCanIfSetControllerMode_ForceFail = 0U;
}

Std_ReturnType __wrap_CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    WrapCanIfSetControllerMode_CallCount++;

    if (WrapCanIfSetControllerMode_ForceFail)
        return E_NOT_OK;

    return __real_CanIf_SetControllerMode(ControllerId, ControllerMode);
}
