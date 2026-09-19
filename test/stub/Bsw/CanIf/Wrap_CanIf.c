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

/* ----------------------------------------------------------------------
 * CanIf_RxIndication / CanIf_TxConfirmation / CanIf_ControllerBusOff
 * ---------------------------------------------------------------------- */
extern void __real_CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr);
extern void __real_CanIf_TxConfirmation(PduIdType CanTxPduId);
extern void __real_CanIf_ControllerBusOff(uint8 ControllerId);

uint32        WrapCanIfRxIndication_CallCount  = 0U;
Can_HwType    WrapCanIfRxIndication_LastMailbox = { 0U, 0U, 0U };
uint8         WrapCanIfRxIndication_LastData[8] = { 0U };
PduLengthType WrapCanIfRxIndication_LastLength  = 0U;

void WrapCanIfRxIndication_Reset(void)
{
    WrapCanIfRxIndication_CallCount         = 0U;
    WrapCanIfRxIndication_LastMailbox.CanId        = 0U;
    WrapCanIfRxIndication_LastMailbox.Hoh          = 0U;
    WrapCanIfRxIndication_LastMailbox.ControllerId = 0U;
    for (uint8 i = 0U; i < 8U; i++)
        WrapCanIfRxIndication_LastData[i] = 0U;
    WrapCanIfRxIndication_LastLength = 0U;
}

void __wrap_CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)
{
    WrapCanIfRxIndication_CallCount++;
    WrapCanIfRxIndication_LastMailbox = *Mailbox;
    WrapCanIfRxIndication_LastLength  = PduInfoPtr->SduLength;
    for (uint8 i = 0U; i < PduInfoPtr->SduLength && i < 8U; i++)
        WrapCanIfRxIndication_LastData[i] = PduInfoPtr->SduDataPtr[i];

    __real_CanIf_RxIndication(Mailbox, PduInfoPtr);
}

uint32    WrapCanIfTxConfirmation_CallCount  = 0U;
PduIdType WrapCanIfTxConfirmation_LastPduId  = 0U;

void WrapCanIfTxConfirmation_Reset(void)
{
    WrapCanIfTxConfirmation_CallCount = 0U;
    WrapCanIfTxConfirmation_LastPduId = 0U;
}

void __wrap_CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    WrapCanIfTxConfirmation_CallCount++;
    WrapCanIfTxConfirmation_LastPduId = CanTxPduId;

    __real_CanIf_TxConfirmation(CanTxPduId);
}

uint32 WrapCanIfControllerBusOff_CallCount        = 0U;
uint8  WrapCanIfControllerBusOff_LastControllerId = 0U;

void WrapCanIfControllerBusOff_Reset(void)
{
    WrapCanIfControllerBusOff_CallCount        = 0U;
    WrapCanIfControllerBusOff_LastControllerId = 0U;
}

void __wrap_CanIf_ControllerBusOff(uint8 ControllerId)
{
    WrapCanIfControllerBusOff_CallCount++;
    WrapCanIfControllerBusOff_LastControllerId = ControllerId;

    __real_CanIf_ControllerBusOff(ControllerId);
}
