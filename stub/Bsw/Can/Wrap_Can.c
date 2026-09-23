/**
 * \file    Wrap_Can.c
 * \brief   `src/Bsw/Can/Can.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Can.h 参照）。
 *
 * \details 変数を先頭の External Variables セクションへ集約し、その後に
 *          External Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （2026-09-20、`src/Bsw/Can/Can.c` の External/Internal/Test
 *          Functions バナー方式に倣った構成。今後新規追加する `Wrap_XXX.c` は
 *          本ファイルと同じ構成へ統一する）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Can.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Can"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Can_Init                         = 0U;
uint32 CallCount_Can_GetVersionInfo               = 0U;
uint32 CallCount_Can_SetControllerMode            = 0U;
uint32 CallCount_Can_GetControllerErrorState      = 0U;
uint32 CallCount_Can_DisableControllerInterrupts  = 0U;
uint32 CallCount_Can_EnableControllerInterrupts   = 0U;
uint32 CallCount_Can_Write                        = 0U;
uint32 CallCount_Can_MainFunctionRead             = 0U;
uint32 CallCount_Can_MainFunctionWrite            = 0U;
uint32 CallCount_Can_MainFunctionBusOff           = 0U;
uint32 CallCount_Can_MainFunctionWakeup           = 0U;

uint32 FailFromCallCount_Can_SetControllerMode       = WRAP_CAN_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Can_GetControllerErrorState = WRAP_CAN_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_Can_Write                   = WRAP_CAN_FAIL_FROM_CALL_COUNT_DISABLED;

Can_ReturnType ForcedReturn_Can_SetControllerMode       = CAN_NOT_OK;
Std_ReturnType ForcedReturn_Can_GetControllerErrorState = E_NOT_OK;
Can_ReturnType ForcedReturn_Can_Write                   = CAN_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapCan_Reset — 11関数すべての状態を一括で初期化する（Wrap_Can.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapCan_Reset(void)
{
    CallCount_Can_Init                        = 0U;
    CallCount_Can_GetVersionInfo              = 0U;
    CallCount_Can_SetControllerMode           = 0U;
    CallCount_Can_GetControllerErrorState     = 0U;
    CallCount_Can_DisableControllerInterrupts = 0U;
    CallCount_Can_EnableControllerInterrupts  = 0U;
    CallCount_Can_Write                       = 0U;
    CallCount_Can_MainFunctionRead            = 0U;
    CallCount_Can_MainFunctionWrite           = 0U;
    CallCount_Can_MainFunctionBusOff          = 0U;
    CallCount_Can_MainFunctionWakeup          = 0U;

    FailFromCallCount_Can_SetControllerMode       = WRAP_CAN_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Can_GetControllerErrorState = WRAP_CAN_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_Can_Write                   = WRAP_CAN_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_Can_SetControllerMode       = CAN_NOT_OK;
    ForcedReturn_Can_GetControllerErrorState = E_NOT_OK;
    ForcedReturn_Can_Write                   = CAN_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Can_Init
 * ---------------------------------------------------------------------- */

extern
void __real_Can_Init(const Can_ConfigType* Config);
void __wrap_Can_Init(const Can_ConfigType* Config)
{
    CallCount_Can_Init++;
    Log_Write(LOG_T, TAG, "Can_Init", "called %u times", CallCount_Can_Init);

    __real_Can_Init(Config);
}

/* ----------------------------------------------------------------------
 * Can_GetVersionInfo
 * ---------------------------------------------------------------------- */

 extern
void __real_Can_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_Can_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_Can_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "Can_GetVersionInfo", "called %u times", CallCount_Can_GetVersionInfo);

    __real_Can_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * Can_CheckBaudrate
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Can_ChangeBaudrate
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Can_SetBaudrate
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Can_SetControllerMode
 * ---------------------------------------------------------------------- */

extern
Can_ReturnType __real_Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition);
Can_ReturnType __wrap_Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition)
{
    CallCount_Can_SetControllerMode++;
    Log_Write(LOG_T, TAG, "Can_SetControllerMode", "called %u times", CallCount_Can_SetControllerMode);

    if (CallCount_Can_SetControllerMode >= FailFromCallCount_Can_SetControllerMode)
    {
        return ForcedReturn_Can_SetControllerMode;
    }

    return __real_Can_SetControllerMode(Controller, Transition);
}

/* ----------------------------------------------------------------------
 * Can_DisableControllerInterrupts
 * ---------------------------------------------------------------------- */

extern
void __real_Can_DisableControllerInterrupts(uint8 Controller);
void __wrap_Can_DisableControllerInterrupts(uint8 Controller)
{
    CallCount_Can_DisableControllerInterrupts++;
    Log_Write(LOG_T, TAG, "Can_DisableControllerInterrupts", "called %u times", CallCount_Can_DisableControllerInterrupts);

    __real_Can_DisableControllerInterrupts(Controller);
}

/* ----------------------------------------------------------------------
 * Can_EnableControllerInterrupts
 * ---------------------------------------------------------------------- */

extern
void __real_Can_EnableControllerInterrupts(uint8 Controller);
void __wrap_Can_EnableControllerInterrupts(uint8 Controller)
{
    CallCount_Can_EnableControllerInterrupts++;
    Log_Write(LOG_T, TAG, "Can_EnableControllerInterrupts", "called %u times", CallCount_Can_EnableControllerInterrupts);

    __real_Can_EnableControllerInterrupts(Controller);
}

/* ----------------------------------------------------------------------
 * Can_CheckWakeup
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Can_Write
 * ---------------------------------------------------------------------- */

extern
Can_ReturnType __real_Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
Can_ReturnType __wrap_Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)
{
    CallCount_Can_Write++;
    Log_Write(LOG_T, TAG, "Can_Write", "called %u times", CallCount_Can_Write);

    if (CallCount_Can_Write >= FailFromCallCount_Can_Write)
    {
        return ForcedReturn_Can_Write;
    }

    return __real_Can_Write(Hth, PduInfo);
}

/* ======================================================================
 * Callback notifications
 * ====================================================================== */

/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Can_MainFunction_Write
 * ---------------------------------------------------------------------- */

extern
void __real_Can_MainFunction_Write(void);
void __wrap_Can_MainFunction_Write(void)
{
    CallCount_Can_MainFunctionWrite++;
    Log_Write(LOG_T, TAG, "Can_MainFunction_Write", "called %u times", CallCount_Can_MainFunctionWrite);

    __real_Can_MainFunction_Write();
}

/* ----------------------------------------------------------------------
 * Can_MainFunction_Read
 * ---------------------------------------------------------------------- */

extern
void __real_Can_MainFunction_Read(void);
void __wrap_Can_MainFunction_Read(void)
{
    CallCount_Can_MainFunctionRead++;
    Log_Write(LOG_T, TAG, "Can_MainFunction_Read", "called %u times", CallCount_Can_MainFunctionRead);

    __real_Can_MainFunction_Read();
}

/* ----------------------------------------------------------------------
 * Can_MainFunction_BusOff
 * ---------------------------------------------------------------------- */

extern
void __real_Can_MainFunction_BusOff(void);
void __wrap_Can_MainFunction_BusOff(void)
{
    CallCount_Can_MainFunctionBusOff++;
    Log_Write(LOG_T, TAG, "Can_MainFunction_BusOff", "called %u times", CallCount_Can_MainFunctionBusOff);

    __real_Can_MainFunction_BusOff();
}

/* ----------------------------------------------------------------------
 * Can_MainFunction_Wakeup
 * ---------------------------------------------------------------------- */

extern
void __real_Can_MainFunction_Wakeup(void);
void __wrap_Can_MainFunction_Wakeup(void)
{
    CallCount_Can_MainFunctionWakeup++;
    Log_Write(LOG_T, TAG, "Can_MainFunction_Wakeup", "called %u times", CallCount_Can_MainFunctionWakeup);

    __real_Can_MainFunction_Wakeup();
}

/* ----------------------------------------------------------------------
 * Can_MainFunction_Mode
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Can_GetControllerErrorState
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_Can_GetControllerErrorState(uint8 Controller, Can_ErrorStateType* ErrorStatePtr);
Std_ReturnType __wrap_Can_GetControllerErrorState(uint8 Controller, Can_ErrorStateType* ErrorStatePtr)
{
    CallCount_Can_GetControllerErrorState++;
    Log_Write(LOG_T, TAG, "Can_GetControllerErrorState", "called %u times", CallCount_Can_GetControllerErrorState);

    if (CallCount_Can_GetControllerErrorState >= FailFromCallCount_Can_GetControllerErrorState)
    {
        return ForcedReturn_Can_GetControllerErrorState;
    }

    return __real_Can_GetControllerErrorState(Controller, ErrorStatePtr);
}
