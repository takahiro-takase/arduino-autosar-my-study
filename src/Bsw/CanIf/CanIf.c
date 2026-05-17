#include "CanIf.h"
#include "Can.h"
#include "Det.h"

static const CanIf_ConfigType* CanIf_ConfigPtr = NULL;

void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    CanIf_ConfigPtr = ConfigPtr;
    Det_LogP(PSTR("[CanIf_Init] initialized"));
    Det_PrintP(PSTR("  TX PDU count: "));
    Det_PrintDec(ConfigPtr->TxPduCount);
    Det_Newline();
    Det_PrintP(PSTR("  RX PDU count: "));
    Det_PrintDec(ConfigPtr->RxPduCount);
    Det_Newline();
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (TxPduId >= CanIf_ConfigPtr->TxPduCount)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: invalid TxPduId"));
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: PduInfoPtr NULL"));
        return E_NOT_OK;
    }

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[TxPduId];

    if (PduInfoPtr->SduLength > txCfg->Dlc)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: SduLength>DLC"));
        return E_NOT_OK;
    }

    Can_PduType canPdu = {
        .swPduHandle = TxPduId,
        .id          = txCfg->CanId,
        .length      = (uint8)PduInfoPtr->SduLength,
        .sdu         = PduInfoPtr->SduDataPtr
    };

    Det_PrintP(PSTR("[CanIf_Transmit] TxPduId="));
    Det_PrintDec(TxPduId);
    Det_PrintP(PSTR(" CanId=0x"));
    Det_PrintHex(txCfg->CanId);
    Det_Newline();

    Can_ReturnType ret = Can_Write(txCfg->Hth, &canPdu);

    if (ret == CAN_BUSY)
        Det_LogP(PSTR("[CanIf_Transmit] BUSY"));

    return (ret == CAN_OK) ? E_OK : E_NOT_OK;
}

void CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)
{
    if (CanIf_ConfigPtr == NULL || Mailbox == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < CanIf_ConfigPtr->RxPduCount; i++)
    {
        const CanIf_RxPduConfigType* rxCfg = &CanIf_ConfigPtr->RxPduConfig[i];

        if (rxCfg->Hrh == Mailbox->Hoh && rxCfg->CanId == Mailbox->CanId)
        {
            Det_PrintP(PSTR("[CanIf_RxInd] CanId=0x"));
            Det_PrintHex(Mailbox->CanId);
            Det_PrintP(PSTR(" -> RxPduId="));
            Det_PrintDec(rxCfg->UpperLayerRxPduId);
            Det_Newline();

            if (rxCfg->RxIndicationFct != NULL)
                rxCfg->RxIndicationFct(rxCfg->UpperLayerRxPduId, PduInfoPtr);

            return;
        }
    }

    Det_PrintP(PSTR("[CanIf_RxInd] no match CanId=0x"));
    Det_PrintHex(Mailbox->CanId);
    Det_Newline();
}

void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    if (CanIf_ConfigPtr == NULL)
        return;

    if (CanTxPduId >= CanIf_ConfigPtr->TxPduCount)
        return;

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[CanTxPduId];

    Det_PrintP(PSTR("[CanIf_TxConf] TxPduId="));
    Det_PrintDec(CanTxPduId);
    Det_Newline();

    if (txCfg->TxConfirmFct != NULL)
        txCfg->TxConfirmFct(txCfg->UpperLayerTxPduId);
}
