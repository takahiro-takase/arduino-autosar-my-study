/**
 * \file    Wrap_CanIf.c
 * \brief   `src/Bsw/CanIf/CanIf.c` 内の関数を対象とした wrap 実体
 *          （Wrap_CanIf.h 参照）。
 */
#include "Wrap_CanIf.h"
#include "Det.h"

#define TAG "CanIf"

/* ======================================================================
 * External Variables
 * ====================================================================== */
uint32 CallCount_CanIf_Init                    = 0U;
uint32 CallCount_CanIf_DeInit                  = 0U;
uint32 CallCount_CanIf_SetControllerMode       = 0U;
uint32 CallCount_CanIf_GetControllerMode       = 0U;
uint32 CallCount_CanIf_GetControllerErrorState = 0U;
uint32 CallCount_CanIf_Transmit                = 0U;
uint32 CallCount_CanIf_ReadRxPduData           = 0U;
uint32 CallCount_CanIf_ReadTxNotifStatus       = 0U;
uint32 CallCount_CanIf_ReadRxNotifStatus       = 0U;
uint32 CallCount_CanIf_SetPduMode              = 0U;
uint32 CallCount_CanIf_GetPduMode              = 0U;
uint32 CallCount_CanIf_GetVersionInfo          = 0U;
uint32 CallCount_CanIf_RxIndication            = 0U;
uint32 CallCount_CanIf_TxConfirmation          = 0U;
uint32 CallCount_CanIf_ControllerBusOff        = 0U;
uint32 CallCount_CanIf_GetTxConfirmationState  = 0U;

uint32 FailFromCallCount_CanIf_SetControllerMode       = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_GetControllerMode       = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_GetControllerErrorState = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_Transmit                = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_ReadRxPduData           = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_ReadTxNotifStatus       = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_ReadRxNotifStatus       = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_SetPduMode              = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_GetPduMode              = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanIf_GetTxConfirmationState  = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_CanIf_SetControllerMode        = E_NOT_OK;
Std_ReturnType ForcedReturn_CanIf_GetControllerMode        = E_NOT_OK;
Std_ReturnType ForcedReturn_CanIf_GetControllerErrorState  = E_NOT_OK;
Std_ReturnType ForcedReturn_CanIf_Transmit                 = E_NOT_OK;
Std_ReturnType ForcedReturn_CanIf_ReadRxPduData            = E_NOT_OK;
CanIf_NotifStatusType ForcedReturn_CanIf_ReadTxNotifStatus = CANIF_NO_NOTIFICATION;
CanIf_NotifStatusType ForcedReturn_CanIf_ReadRxNotifStatus = CANIF_NO_NOTIFICATION;
Std_ReturnType ForcedReturn_CanIf_SetPduMode               = E_NOT_OK;
Std_ReturnType ForcedReturn_CanIf_GetPduMode               = E_NOT_OK;
CanIf_NotifStatusType ForcedReturn_CanIf_GetTxConfirmationState  = CANIF_NO_NOTIFICATION;

uint8         LastData_CanIf_RxIndication[8]      = { 0U };
Can_HwType    LastMailbox_CanIf_RxIndication      = { 0U, 0U, 0U };
PduLengthType LastLength_CanIf_RxIndication       = 0U;
PduIdType     LastPduId_CanIf_TxConfirmation      = 0U;
uint8         LastControllerId_CanIf_ControllerBusOff = 0U;

/* ----------------------------------------------------------------------
 * WrapCanIf_Reset — すべての関数状態を一括で初期化する（Wrap_CanIf.h 参照）。
 * ---------------------------------------------------------------------- */
void WrapCanIf_Reset(void)
{
    CallCount_CanIf_Init                    = 0U;
    CallCount_CanIf_DeInit                  = 0U;
    CallCount_CanIf_SetControllerMode       = 0U;
    CallCount_CanIf_GetControllerMode       = 0U;
    CallCount_CanIf_GetControllerErrorState = 0U;
    CallCount_CanIf_Transmit                = 0U;
    CallCount_CanIf_ReadRxPduData           = 0U;
    CallCount_CanIf_ReadRxNotifStatus       = 0U;
    CallCount_CanIf_ReadTxNotifStatus       = 0U;
    CallCount_CanIf_SetPduMode              = 0U;
    CallCount_CanIf_GetPduMode              = 0U;
    CallCount_CanIf_GetVersionInfo          = 0U;
    CallCount_CanIf_RxIndication            = 0U;
    CallCount_CanIf_TxConfirmation          = 0U;
    CallCount_CanIf_ControllerBusOff        = 0U;
    CallCount_CanIf_GetTxConfirmationState  = 0U;

    FailFromCallCount_CanIf_SetControllerMode       = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanIf_GetControllerMode       = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanIf_GetControllerErrorState = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanIf_Transmit                = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanIf_ReadRxPduData           = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanIf_SetPduMode              = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanIf_GetPduMode              = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanIf_GetTxConfirmationState  = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_CanIf_SetControllerMode       = E_NOT_OK;
    ForcedReturn_CanIf_GetControllerMode       = E_NOT_OK;
    ForcedReturn_CanIf_GetControllerErrorState = E_NOT_OK;
    ForcedReturn_CanIf_Transmit                = E_NOT_OK;
    ForcedReturn_CanIf_ReadRxPduData           = E_NOT_OK;
    ForcedReturn_CanIf_ReadTxNotifStatus       = CANIF_NO_NOTIFICATION;
    ForcedReturn_CanIf_ReadRxNotifStatus       = CANIF_NO_NOTIFICATION;
    ForcedReturn_CanIf_SetPduMode              = E_NOT_OK;
    ForcedReturn_CanIf_GetPduMode              = E_NOT_OK;
    ForcedReturn_CanIf_GetTxConfirmationState  = CANIF_NO_NOTIFICATION;

    LastMailbox_CanIf_RxIndication.CanId        = 0U;
    LastMailbox_CanIf_RxIndication.Hoh          = 0U;
    LastMailbox_CanIf_RxIndication.ControllerId = 0U;
    for (uint8 i = 0U; i < 8U; i++)
        LastData_CanIf_RxIndication[i] = 0U;
    LastLength_CanIf_RxIndication          = 0U;
    LastPduId_CanIf_TxConfirmation         = 0U;
    LastControllerId_CanIf_ControllerBusOff = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanIf_Init
 * ---------------------------------------------------------------------- */
extern
void __real_CanIf_Init(const CanIf_ConfigType* ConfigPtr);
void __wrap_CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    CallCount_CanIf_Init++;
    Log_Write(LOG_T, TAG, "CanIf_Init", "called %u times", CallCount_CanIf_Init);

    __real_CanIf_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * CanIf_DeInit
 * ---------------------------------------------------------------------- */
extern
void __real_CanIf_DeInit(void);
void __wrap_CanIf_DeInit(void)
{
    CallCount_CanIf_DeInit++;
    Log_Write(LOG_T, TAG, "CanIf_DeInit", "called %u times", CallCount_CanIf_DeInit);

    __real_CanIf_DeInit();
}

/* ----------------------------------------------------------------------
 * CanIf_SetControllerMode
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode);
Std_ReturnType __wrap_CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    CallCount_CanIf_SetControllerMode++;
    Log_Write(LOG_T, TAG, "CanIf_SetControllerMode", "called %u times", CallCount_CanIf_SetControllerMode);

    if (CallCount_CanIf_SetControllerMode >= FailFromCallCount_CanIf_SetControllerMode)
    {
        return ForcedReturn_CanIf_SetControllerMode;
    }
    return __real_CanIf_SetControllerMode(ControllerId, ControllerMode);
}

/* ----------------------------------------------------------------------
 * CanIf_GetControllerMode
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType* ControllerModePtr);
Std_ReturnType __wrap_CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType* ControllerModePtr)
{
    CallCount_CanIf_GetControllerMode++;
    Log_Write(LOG_T, TAG, "CanIf_GetControllerMode", "called %u times", CallCount_CanIf_GetControllerMode);

    if (CallCount_CanIf_GetControllerMode >= FailFromCallCount_CanIf_GetControllerMode)
    {
        return ForcedReturn_CanIf_GetControllerMode;
    }
    return __real_CanIf_GetControllerMode(ControllerId, ControllerModePtr);
}

/* ----------------------------------------------------------------------
 * CanIf_GetControllerErrorState
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_CanIf_GetControllerErrorState(uint8 ControllerId, Can_ErrorStateType* ErrorStatePtr);
Std_ReturnType __wrap_CanIf_GetControllerErrorState(uint8 ControllerId, Can_ErrorStateType* ErrorStatePtr)
{
    CallCount_CanIf_GetControllerErrorState++;
    Log_Write(LOG_T, TAG, "CanIf_GetControllerErrorState", "called %u times", CallCount_CanIf_GetControllerErrorState);

    if (CallCount_CanIf_GetControllerErrorState >= FailFromCallCount_CanIf_GetControllerErrorState)
    {
        return ForcedReturn_CanIf_GetControllerErrorState;
    }
    return __real_CanIf_GetControllerErrorState(ControllerId, ErrorStatePtr);
}

/* ----------------------------------------------------------------------
 * CanIf_Transmit
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
Std_ReturnType __wrap_CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_CanIf_Transmit++;
    Log_Write(LOG_T, TAG, "CanIf_Transmit", "called %u times", CallCount_CanIf_Transmit);

    if (CallCount_CanIf_Transmit >= FailFromCallCount_CanIf_Transmit)
    {
        return ForcedReturn_CanIf_Transmit;
    }
    return __real_CanIf_Transmit(TxPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * CanIf_CancelTransmit
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_ReadRxPduData
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_CanIf_ReadRxPduData(PduIdType CanIfRxSduId, PduInfoType* CanIfRxInfoPtr);
Std_ReturnType __wrap_CanIf_ReadRxPduData(PduIdType CanIfRxSduId, PduInfoType* CanIfRxInfoPtr)
{
    CallCount_CanIf_ReadRxPduData++;
    Log_Write(LOG_T, TAG, "CanIf_ReadRxPduData", "called %u times", CallCount_CanIf_ReadRxPduData);

    if (CallCount_CanIf_ReadRxPduData >= FailFromCallCount_CanIf_ReadRxPduData)
    {
        return ForcedReturn_CanIf_ReadRxPduData;
    }
    return __real_CanIf_ReadRxPduData(CanIfRxSduId, CanIfRxInfoPtr);
}

/* ----------------------------------------------------------------------
 * CanIf_ReadTxNotifStatus
 * ---------------------------------------------------------------------- */
extern
CanIf_NotifStatusType __real_CanIf_ReadTxNotifStatus(PduIdType CanIfTxSduId);
CanIf_NotifStatusType __wrap_CanIf_ReadTxNotifStatus(PduIdType CanIfTxSduId)
{
    CallCount_CanIf_ReadTxNotifStatus++;
    Log_Write(LOG_T, TAG, "CanIf_ReadTxNotifStatus", "called %u times", CallCount_CanIf_ReadTxNotifStatus);

    if (CallCount_CanIf_ReadTxNotifStatus >= FailFromCallCount_CanIf_ReadTxNotifStatus)
    {
        return ForcedReturn_CanIf_ReadTxNotifStatus;
    }
    return __real_CanIf_ReadTxNotifStatus(CanIfTxSduId);
}

/* ----------------------------------------------------------------------
 * CanIf_ReadRxNotifStatus
 * ---------------------------------------------------------------------- */
extern
CanIf_NotifStatusType __real_CanIf_ReadRxNotifStatus(PduIdType CanIfRxSduId);
CanIf_NotifStatusType __wrap_CanIf_ReadRxNotifStatus(PduIdType CanIfRxSduId)
{
    CallCount_CanIf_ReadRxNotifStatus++;
    Log_Write(LOG_T, TAG, "CanIf_ReadRxNotifStatus", "called %u times", CallCount_CanIf_ReadRxNotifStatus);

    if (CallCount_CanIf_ReadRxNotifStatus >= FailFromCallCount_CanIf_ReadTxNotifStatus)
    {
        return ForcedReturn_CanIf_ReadTxNotifStatus;
    }
    return __real_CanIf_ReadRxNotifStatus(CanIfRxSduId);
}

/* ----------------------------------------------------------------------
 * CanIf_SetPduMode
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_CanIf_SetPduMode(uint8 ControllerId, CanIf_PduModeType PduModeRequest);
Std_ReturnType __wrap_CanIf_SetPduMode(uint8 ControllerId, CanIf_PduModeType PduModeRequest)
{
    CallCount_CanIf_SetPduMode++;
    Log_Write(LOG_T, TAG, "CanIf_SetPduMode", "called %u times", CallCount_CanIf_SetPduMode);

    if (CallCount_CanIf_SetPduMode >= FailFromCallCount_CanIf_SetPduMode)
    {
        return ForcedReturn_CanIf_SetPduMode;
    }

    return __real_CanIf_SetPduMode(ControllerId, PduModeRequest);
}

/* ----------------------------------------------------------------------
 * CanIf_GetPduMode
 * ---------------------------------------------------------------------- */
extern
Std_ReturnType __real_CanIf_GetPduMode(uint8 ControllerId, CanIf_PduModeType* PduModePtr);
Std_ReturnType __wrap_CanIf_GetPduMode(uint8 ControllerId, CanIf_PduModeType* PduModePtr)
{
    CallCount_CanIf_GetPduMode++;
    Log_Write(LOG_T, TAG, "CanIf_GetPduMode", "called %u times", CallCount_CanIf_GetPduMode);

    if (CallCount_CanIf_GetPduMode >= FailFromCallCount_CanIf_GetPduMode)
    {
        return ForcedReturn_CanIf_GetPduMode;
    }
    return __real_CanIf_GetPduMode(ControllerId, PduModePtr);
}

/* ----------------------------------------------------------------------
 * CanIf_GetVersionInfo
 * ---------------------------------------------------------------------- */
extern 
void __real_CanIf_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_CanIf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_CanIf_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "CanIf_GetVersionInfo", "called %u times", CallCount_CanIf_GetVersionInfo);

    __real_CanIf_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * CanIf_SetDynamicTxId
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_SetTrcvMode
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_GetTrcvMode
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_GetTrcvWakeupReason
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_SetTrcvWakeupMode
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_CheckWakeup
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_CheckValidation
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_GetTxConfirmationState
 * ---------------------------------------------------------------------- */
extern 
CanIf_NotifStatusType __real_CanIf_GetTxConfirmationState(uint8 ControllerId);
CanIf_NotifStatusType __wrap_CanIf_GetTxConfirmationState(uint8 ControllerId)
{
    CallCount_CanIf_GetTxConfirmationState++;
    Log_Write(LOG_T, TAG, "CanIf_GetTxConfirmationState", "called %u times", CallCount_CanIf_GetTxConfirmationState);

    if (CallCount_CanIf_GetTxConfirmationState >= FailFromCallCount_CanIf_GetTxConfirmationState)
    {
        return ForcedReturn_CanIf_GetTxConfirmationState;
    }
    return __real_CanIf_GetTxConfirmationState(ControllerId);
}

/* ----------------------------------------------------------------------
 * CanIf_ClearTrcvWufFlag
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_CheckTrcvWakeFlag
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_SetBaudrate
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_SetIcomConfiguration
 * ---------------------------------------------------------------------- */

/* ======================================================================
 * Callback notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanIf_TriggerTransmit
 * ---------------------------------------------------------------------- */

 /* ----------------------------------------------------------------------
 * CanIf_TxConfirmation
 * ---------------------------------------------------------------------- */
extern
void __real_CanIf_TxConfirmation(PduIdType CanTxPduId);
void __wrap_CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    CallCount_CanIf_TxConfirmation++;
    Log_Write(LOG_T, TAG, "CanIf_TxConfirmation", "called %u times", CallCount_CanIf_TxConfirmation);

    LastPduId_CanIf_TxConfirmation = CanTxPduId;

    __real_CanIf_TxConfirmation(CanTxPduId);
}

/* ----------------------------------------------------------------------
 * CanIf_RxIndication
 * ---------------------------------------------------------------------- */
extern
void __real_CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr);
void __wrap_CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)
{
    CallCount_CanIf_RxIndication++;
    Log_Write(LOG_T, TAG, "CanIf_RxIndication", "called %u times", CallCount_CanIf_RxIndication);

    LastMailbox_CanIf_RxIndication = *Mailbox;
    LastLength_CanIf_RxIndication  = PduInfoPtr->SduLength;
    for (uint8 i = 0U; i < PduInfoPtr->SduLength && i < 8U; i++)
        LastData_CanIf_RxIndication[i] = PduInfoPtr->SduDataPtr[i];

    __real_CanIf_RxIndication(Mailbox, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * CanIf_ControllerBusOff
 * ---------------------------------------------------------------------- */
extern
void __real_CanIf_ControllerBusOff(uint8 ControllerId);
void __wrap_CanIf_ControllerBusOff(uint8 ControllerId)
{
    CallCount_CanIf_ControllerBusOff++;
    Log_Write(LOG_T, TAG, "CanIf_ControllerBusOff", "called %u times", CallCount_CanIf_ControllerBusOff);

    LastControllerId_CanIf_ControllerBusOff = ControllerId;

    __real_CanIf_ControllerBusOff(ControllerId);
}

/* ----------------------------------------------------------------------
 * CanIf_ConfirmPnAvailability
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_ClearTrcvWufFlagIndication
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_CheckTrcvWakeFlagIndication
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_ControllerModeIndication
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_TrcvModeIndication
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * CanIf_CurrentIcomConfiguration
 * ---------------------------------------------------------------------- */
