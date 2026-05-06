#include "PduR.h"
#include "CanIf.h"
#include "Det.h"

static const PduR_ConfigType* PduR_ConfigPtr = NULL;

void PduR_Init(const PduR_ConfigType* Config)
{
    if (Config == NULL)
    {
        Det_LogP(PSTR("[PduR_Init] E: Config NULL"));
        return;
    }

    for (uint8 i = 0; i < Config->RxPathCount; i++)
    {
        if (Config->RxPaths[i].Dests == NULL || Config->RxPaths[i].DestCount == 0)
        {
            Det_PrintP(PSTR("[PduR_Init] E: RxPath["));
            Det_PrintDec(i);
            Det_LogP(PSTR("] no dests"));
            return;
        }
    }

    PduR_ConfigPtr = Config;
    Det_LogP(PSTR("[PduR_Init] initialized"));

    Det_PrintP(PSTR("  RX paths: "));
    Det_PrintDec(Config->RxPathCount);
    Det_Newline();
    for (uint8 i = 0; i < Config->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &Config->RxPaths[i];
        Det_PrintP(PSTR("    SrcPduId="));
        Det_PrintDec(path->SrcPduId);
        Det_PrintP(PSTR(" dests="));
        Det_PrintDec(path->DestCount);
        Det_Newline();
        for (uint8 d = 0; d < path->DestCount; d++)
        {
            Det_PrintP(PSTR("      Module="));
            Det_PrintDec(path->Dests[d].Module);
            Det_PrintP(PSTR(" DestPduId="));
            Det_PrintDec(path->Dests[d].DestPduId);
            Det_Newline();
        }
    }

    Det_PrintP(PSTR("  TX paths: "));
    Det_PrintDec(Config->TxPathCount);
    Det_Newline();
    for (uint8 i = 0; i < Config->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &Config->TxPaths[i];
        Det_PrintP(PSTR("    SrcPduId="));
        Det_PrintDec(path->SrcPduId);
        Det_PrintP(PSTR(" -> CanIfTxPduId="));
        Det_PrintDec(path->CanIfTxPduId);
        Det_Newline();
    }
}

void PduR_CanIfRxIndication(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < PduR_ConfigPtr->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &PduR_ConfigPtr->RxPaths[i];

        if (path->SrcPduId != SrcPduId)
            continue;

        for (uint8 d = 0; d < path->DestCount; d++)
        {
            const PduR_RxDestType* dest = &path->Dests[d];

            Det_PrintP(PSTR("[PduR_RxInd] Src="));
            Det_PrintDec(SrcPduId);
            Det_PrintP(PSTR(" Mod="));
            Det_PrintDec(dest->Module);
            Det_PrintP(PSTR(" Dst="));
            Det_PrintDec(dest->DestPduId);
            Det_Newline();

            if (dest->RxIndFct != NULL)
                dest->RxIndFct(dest->DestPduId, PduInfoPtr);
        }
        return;
    }

    Det_PrintP(PSTR("[PduR_RxInd] no route Src="));
    Det_PrintDec(SrcPduId);
    Det_Newline();
}

void PduR_CanIfTxConfirmation(PduIdType SrcPduId)
{
    if (PduR_ConfigPtr == NULL)
        return;

    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &PduR_ConfigPtr->TxPaths[i];

        if (path->SrcPduId != SrcPduId)
            continue;

        Det_PrintP(PSTR("[PduR_TxConf] Src="));
        Det_PrintDec(SrcPduId);
        Det_PrintP(PSTR(" ConfDst="));
        Det_PrintDec(path->ConfDestPduId);
        Det_Newline();

        if (path->ConfFct != NULL)
            path->ConfFct(path->ConfDestPduId);

        return;
    }

    Det_PrintP(PSTR("[PduR_TxConf] no route Src="));
    Det_PrintDec(SrcPduId);
    Det_Newline();
}

Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL || PduInfoPtr == NULL)
        return E_NOT_OK;

    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &PduR_ConfigPtr->TxPaths[i];

        if (path->SrcPduId != SrcPduId)
            continue;

        Det_PrintP(PSTR("[PduR_Transmit] Src="));
        Det_PrintDec(SrcPduId);
        Det_PrintP(PSTR(" -> CanIf="));
        Det_PrintDec(path->CanIfTxPduId);
        Det_Newline();

        return CanIf_Transmit(path->CanIfTxPduId, PduInfoPtr);
    }

    Det_PrintP(PSTR("[PduR_Transmit] no route Src="));
    Det_PrintDec(SrcPduId);
    Det_Newline();
    return E_NOT_OK;
}
