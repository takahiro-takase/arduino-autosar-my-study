/**
 * \file    Wrap_CanSM.c
 * \brief   `src/Bsw/CanSM/CanSM.c` 内の関数を対象とした wrap 実体
 *          （Wrap_CanSM.h 参照）。
 *
 * \details 変数を先頭の External Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_CanSM.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "CanSM"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * External Variables
 * ====================================================================== */

uint32 CallCount_CanSM_Init                       = 0U;
uint32 CallCount_CanSM_DeInit                     = 0U;
uint32 CallCount_CanSM_RequestComMode             = 0U;
uint32 CallCount_CanSM_GetCurrentComMode          = 0U;
uint32 CallCount_CanSM_ControllerBusOff           = 0U;
uint32 CallCount_CanSM_ControllerModeIndication   = 0U;
uint32 CallCount_CanSM_RxIndication               = 0U;
uint32 CallCount_CanSM_MainFunction               = 0U;
uint32 CallCount_CanSM_GetVersionInfo             = 0U;

uint32 FailFromCallCount_CanSM_RequestComMode    = WRAP_CANSM_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanSM_GetCurrentComMode = WRAP_CANSM_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_CanSM_RequestComMode    = E_NOT_OK;
Std_ReturnType ForcedReturn_CanSM_GetCurrentComMode = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapCanSM_Reset — 9関数すべての状態を一括で初期化する（Wrap_CanSM.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapCanSM_Reset(void)
{
    CallCount_CanSM_Init                     = 0U;
    CallCount_CanSM_DeInit                   = 0U;
    CallCount_CanSM_RequestComMode           = 0U;
    CallCount_CanSM_GetCurrentComMode        = 0U;
    CallCount_CanSM_ControllerBusOff         = 0U;
    CallCount_CanSM_ControllerModeIndication = 0U;
    CallCount_CanSM_RxIndication             = 0U;
    CallCount_CanSM_MainFunction             = 0U;
    CallCount_CanSM_GetVersionInfo           = 0U;

    FailFromCallCount_CanSM_RequestComMode    = WRAP_CANSM_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanSM_GetCurrentComMode = WRAP_CANSM_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_CanSM_RequestComMode    = E_NOT_OK;
    ForcedReturn_CanSM_GetCurrentComMode = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanSM_Init
 * ---------------------------------------------------------------------- */

extern
void __real_CanSM_Init(const CanSM_ConfigType* ConfigPtr);
void __wrap_CanSM_Init(const CanSM_ConfigType* ConfigPtr)
{
    CallCount_CanSM_Init++;
    Log_Write(LOG_T, TAG, "CanSM_Init", "called %u times", CallCount_CanSM_Init);

    __real_CanSM_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * CanSM_DeInit
 * ---------------------------------------------------------------------- */

extern
void __real_CanSM_DeInit(void);
void __wrap_CanSM_DeInit(void)
{
    CallCount_CanSM_DeInit++;
    Log_Write(LOG_T, TAG, "CanSM_DeInit", "called %u times", CallCount_CanSM_DeInit);

    __real_CanSM_DeInit();
}

/* ----------------------------------------------------------------------
 * CanSM_RequestComMode
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType mode);
Std_ReturnType __wrap_CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType mode)
{
    CallCount_CanSM_RequestComMode++;
    Log_Write(LOG_T, TAG, "CanSM_RequestComMode", "called %u times", CallCount_CanSM_RequestComMode);

    if (CallCount_CanSM_RequestComMode >= FailFromCallCount_CanSM_RequestComMode)
    {
        return ForcedReturn_CanSM_RequestComMode;
    }

    return __real_CanSM_RequestComMode(network, mode);
}

/* ----------------------------------------------------------------------
 * CanSM_GetCurrentComMode
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType* mode);
Std_ReturnType __wrap_CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType* mode)
{
    CallCount_CanSM_GetCurrentComMode++;
    Log_Write(LOG_T, TAG, "CanSM_GetCurrentComMode", "called %u times", CallCount_CanSM_GetCurrentComMode);

    if (CallCount_CanSM_GetCurrentComMode >= FailFromCallCount_CanSM_GetCurrentComMode)
    {
        return ForcedReturn_CanSM_GetCurrentComMode;
    }

    return __real_CanSM_GetCurrentComMode(network, mode);
}

/* ----------------------------------------------------------------------
 * CanSM_StartWakeupSource
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_StopWakeupSource
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_CanSM_GetVersionInfo(Std_VersionInfoType* VersionInfo);
void __wrap_CanSM_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    CallCount_CanSM_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "CanSM_GetVersionInfo", "called %u times", CallCount_CanSM_GetVersionInfo);

    __real_CanSM_GetVersionInfo(VersionInfo);
}

/* ----------------------------------------------------------------------
 * CanSM_SetBaudrate
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_SetIcomConfiguration
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_SetEcuPassive
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Call-back notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanSM_ControllerBusOff
 * ---------------------------------------------------------------------- */

extern
void __real_CanSM_ControllerBusOff(uint8 ControllerId);
void __wrap_CanSM_ControllerBusOff(uint8 ControllerId)
{
    CallCount_CanSM_ControllerBusOff++;
    Log_Write(LOG_T, TAG, "CanSM_ControllerBusOff", "called %u times", CallCount_CanSM_ControllerBusOff);

    __real_CanSM_ControllerBusOff(ControllerId);
}

/* ----------------------------------------------------------------------
 * CanSM_ControllerModeIndication
 * ---------------------------------------------------------------------- */

extern
void __real_CanSM_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode);
void __wrap_CanSM_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    CallCount_CanSM_ControllerModeIndication++;
    Log_Write(LOG_T, TAG, "CanSM_ControllerModeIndication", "called %u times", CallCount_CanSM_ControllerModeIndication);

    __real_CanSM_ControllerModeIndication(ControllerId, ControllerMode);
}

/* ----------------------------------------------------------------------
 * CanSM_TransceiverModeIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_TxTimeoutException
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_ClearTrcvWufFlagIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_CheckTransceiverWakeFlagIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_ConfirmPnAvailability
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanSM_CurrentIcomConfiguration
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanSM_MainFunction
 * ---------------------------------------------------------------------- */

extern
void __real_CanSM_MainFunction(void);
void __wrap_CanSM_MainFunction(void)
{
    CallCount_CanSM_MainFunction++;
    Log_Write(LOG_T, TAG, "CanSM_MainFunction", "called %u times", CallCount_CanSM_MainFunction);

    __real_CanSM_MainFunction();
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanSM_RxIndication
 * ---------------------------------------------------------------------- */

extern
void __real_CanSM_RxIndication(uint8 ControllerId);
void __wrap_CanSM_RxIndication(uint8 ControllerId)
{
    CallCount_CanSM_RxIndication++;
    Log_Write(LOG_T, TAG, "CanSM_RxIndication", "called %u times", CallCount_CanSM_RxIndication);

    __real_CanSM_RxIndication(ControllerId);
}
