#include "CanIf.h"
#include "Can.h"
#include "Det.h"

static const CanIf_ConfigType* CanIf_ConfigPtr = NULL;

void CanIf_Init(const CanIf_ConfigType* Config)
{
    CanIf_ConfigPtr = Config;
    Det_LogP(PSTR("[CanIf_Init] initialized"));
    Det_PrintP(PSTR("  TX PDU count: "));
    Det_PrintDec(Config->TxPduCount);
    Det_Newline();
    Det_PrintP(PSTR("  RX PDU count: "));
    Det_PrintDec(Config->RxPduCount);
    Det_Newline();
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfo)
{
    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (TxPduId >= CanIf_ConfigPtr->TxPduCount)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: invalid TxPduId"));
        return E_NOT_OK;
    }

    if (PduInfo == NULL || PduInfo->SduDataPtr == NULL)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: PduInfo NULL"));
        return E_NOT_OK;
    }

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[TxPduId];

    if (PduInfo->SduLength > txCfg->Dlc)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: SduLength>DLC"));
        return E_NOT_OK;
    }

    Can_PduType canPdu = {
        .swPduHandle = TxPduId,
        .id          = txCfg->CanId,
        .length      = (uint8)PduInfo->SduLength,
        .sdu         = PduInfo->SduDataPtr
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

void CanIf_RxIndication(Can_HwHandleType Hrh, Can_IdType CanId, uint8 Dlc, const uint8* Data)
{
    if (CanIf_ConfigPtr == NULL)
        return;

    for (uint8 i = 0; i < CanIf_ConfigPtr->RxPduCount; i++)
    {
        const CanIf_RxPduConfigType* rxCfg = &CanIf_ConfigPtr->RxPduConfig[i];

        if (rxCfg->Hrh == Hrh && rxCfg->CanId == CanId)
        {
            Det_PrintP(PSTR("[CanIf_RxInd] CanId=0x"));
            Det_PrintHex(CanId);
            Det_PrintP(PSTR(" -> RxPduId="));
            Det_PrintDec(rxCfg->UpperLayerRxPduId);
            Det_Newline();

            if (rxCfg->RxIndicationFct != NULL)
            {
                PduInfoType pduInfo = {
                    .SduDataPtr = (uint8*)Data,
                    .SduLength  = (PduLengthType)Dlc
                };
                rxCfg->RxIndicationFct(rxCfg->UpperLayerRxPduId, &pduInfo);
            }
            return;
        }
    }

    Det_PrintP(PSTR("[CanIf_RxInd] no match CanId=0x"));
    Det_PrintHex(CanId);
    Det_Newline();
}

void CanIf_TxConfirmation(PduIdType TxPduId)
{
    if (CanIf_ConfigPtr == NULL)
        return;

    if (TxPduId >= CanIf_ConfigPtr->TxPduCount)
        return;

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[TxPduId];

    Det_PrintP(PSTR("[CanIf_TxConf] TxPduId="));
    Det_PrintDec(TxPduId);
    Det_Newline();

    if (txCfg->TxConfirmFct != NULL)
        txCfg->TxConfirmFct(txCfg->UpperLayerTxPduId);
}
