/**
 * \file    Bsw_CanIf_fake.c
 * \brief   CanIf.h のテスト用フェイク実装（Can.c から見た上位層コールバック）
 * \details Bsw_CanIf_fake.h 冒頭のコメント参照。
 */
#include "Bsw_CanIf_fake.h"

uint32 FakeCanIf_TxConfirmationCount   = 0U;
uint32 FakeCanIf_ControllerBusOffCount = 0U;
uint32 FakeCanIf_RxIndicationCount     = 0U;

PduIdType     FakeCanIf_LastTxConfirmationPduId = 0U;
uint8         FakeCanIf_LastControllerBusOffId  = 0U;
Can_HwType    FakeCanIf_LastRxMailbox           = { 0U, 0U, 0U };
uint8         FakeCanIf_LastRxData[8]           = { 0U };
PduLengthType FakeCanIf_LastRxLength            = 0U;

void FakeCanIf_Reset(void)
{
    FakeCanIf_TxConfirmationCount   = 0U;
    FakeCanIf_ControllerBusOffCount = 0U;
    FakeCanIf_RxIndicationCount     = 0U;

    FakeCanIf_LastTxConfirmationPduId = 0U;
    FakeCanIf_LastControllerBusOffId  = 0U;
    FakeCanIf_LastRxMailbox.CanId        = 0U;
    FakeCanIf_LastRxMailbox.Hoh          = 0U;
    FakeCanIf_LastRxMailbox.ControllerId = 0U;
    for (uint8 i = 0U; i < 8U; i++)
        FakeCanIf_LastRxData[i] = 0U;
    FakeCanIf_LastRxLength = 0U;
}

void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    FakeCanIf_TxConfirmationCount++;
    FakeCanIf_LastTxConfirmationPduId = CanTxPduId;
}

void CanIf_ControllerBusOff(uint8 ControllerId)
{
    FakeCanIf_ControllerBusOffCount++;
    FakeCanIf_LastControllerBusOffId = ControllerId;
}

void CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)
{
    FakeCanIf_RxIndicationCount++;
    FakeCanIf_LastRxMailbox = *Mailbox;
    FakeCanIf_LastRxLength  = PduInfoPtr->SduLength;
    for (uint8 i = 0U; i < PduInfoPtr->SduLength && i < 8U; i++)
        FakeCanIf_LastRxData[i] = PduInfoPtr->SduDataPtr[i];
}

