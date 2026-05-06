#include "Com.h"
#include "PduR.h"
#include "Det.h"

#define COM_IPDU_MAX_DLC  8U
#define COM_RX_IPDU_MAX   1U
#define COM_TX_IPDU_MAX   1U

static const Com_ConfigType* Com_ConfigPtr = NULL;
static uint8 Com_RxBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8 Com_TxBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];

void Com_Init(const Com_ConfigType* Config)
{
    if (Config == NULL)
    {
        Det_LogP(PSTR("[Com_Init] E: Config NULL"));
        return;
    }
    if (Config->RxIPduCount > COM_RX_IPDU_MAX)
    {
        Det_LogP(PSTR("[Com_Init] E: RxIPduCount>max"));
        return;
    }
    if (Config->TxIPduCount > COM_TX_IPDU_MAX)
    {
        Det_LogP(PSTR("[Com_Init] E: TxIPduCount>max"));
        return;
    }

    Com_ConfigPtr = Config;

    for (uint8 i = 0; i < COM_RX_IPDU_MAX; i++)
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
            Com_RxBuffer[i][j] = 0U;

    for (uint8 i = 0; i < COM_TX_IPDU_MAX; i++)
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
            Com_TxBuffer[i][j] = 0U;

    Det_LogP(PSTR("[Com_Init] OK"));

    Det_PrintP(PSTR("  RxIPdus="));
    Det_PrintDec(Config->RxIPduCount);
    Det_PrintP(PSTR(" TxIPdus="));
    Det_PrintDec(Config->TxIPduCount);
    Det_PrintP(PSTR(" Signals="));
    Det_PrintDec(Config->SignalCount);
    Det_Newline();

    for (uint8 s = 0; s < Config->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Config->Signals[s];
        Det_PrintP(PSTR("  Sig["));
        Det_PrintDec(sig->SignalId);
        Det_PrintP(PSTR("] IPdu="));
        Det_PrintDec(sig->IPduId);
        Det_PrintP(PSTR(" Bit="));
        Det_PrintDec(sig->BitPosition);
        Det_PrintP(PSTR("/"));
        Det_PrintDec(sig->BitSize);
        Det_PrintP(PSTR(" "));
        Det_LogP(sig->Endian == COM_BIG_ENDIAN ? PSTR("BIG") : PSTR("LITTLE"));
    }
}

void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    if (Com_ConfigPtr == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->PduRId != PduId)
            continue;

        const uint8 copyLen = (PduInfoPtr->SduLength < ipdu->DLC)
                              ? (uint8)PduInfoPtr->SduLength : ipdu->DLC;

        for (uint8 b = 0; b < copyLen; b++)
            Com_RxBuffer[ipdu->IPduId][b] = PduInfoPtr->SduDataPtr[b];

        Det_PrintP(PSTR("[Com_RxInd] IPdu="));
        Det_PrintDec(ipdu->IPduId);
        Det_PrintP(PSTR(" raw=["));
        for (uint8 b = 0; b < copyLen; b++)
        {
            if (b > 0U) Det_Print(" ");
            if (Com_RxBuffer[ipdu->IPduId][b] < 0x10U) Det_Print("0");
            Det_PrintHex(Com_RxBuffer[ipdu->IPduId][b]);
        }
        Det_LogP(PSTR("]"));
        return;
    }

    Det_PrintP(PSTR("[Com_RxInd] no IPdu PduRId="));
    Det_PrintDec(PduId);
    Det_Newline();
}

static uint32 Com_UnpackSignal(const uint8* buf,
                                uint8 bitPos,
                                uint8 bitSize,
                                Com_SignalEndianType endian)
{
    uint32 value = 0U;
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 pos = bitPos + i;
        const uint8 bit = (buf[pos / 8U] >> (7U - (pos % 8U))) & 1U;
        if (endian == COM_BIG_ENDIAN)
            value = (value << 1U) | bit;
        else
            value |= ((uint32)bit << i);
    }
    return value;
}

static void Com_PackSignal(uint8* buf,
                            uint8 bitPos,
                            uint8 bitSize,
                            Com_SignalEndianType endian,
                            uint32 value)
{
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 bit   = (endian == COM_BIG_ENDIAN)
                            ? (uint8)((value >> (bitSize - 1U - i)) & 1U)
                            : (uint8)((value >> i) & 1U);
        const uint8 pos   = bitPos + i;
        const uint8 shift = 7U - (pos % 8U);
        if (bit)
            buf[pos / 8U] |=  (uint8)(1U << shift);
        else
            buf[pos / 8U] &= (uint8)~(1U << shift);
    }
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr)
{
    if (Com_ConfigPtr == NULL || SignalDataPtr == NULL)
        return E_NOT_OK;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

        const uint32 value = Com_UnpackSignal(
            Com_RxBuffer[sig->IPduId],
            sig->BitPosition, sig->BitSize, sig->Endian);

        SignalDataPtr[0] = (uint8)(value);
        SignalDataPtr[1] = (uint8)(value >>  8U);
        SignalDataPtr[2] = (uint8)(value >> 16U);
        SignalDataPtr[3] = (uint8)(value >> 24U);
        return E_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr)
{
    if (Com_ConfigPtr == NULL || SignalDataPtr == NULL)
        return E_NOT_OK;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

        const uint32 value = (uint32)SignalDataPtr[0]
                           | ((uint32)SignalDataPtr[1] <<  8U)
                           | ((uint32)SignalDataPtr[2] << 16U)
                           | ((uint32)SignalDataPtr[3] << 24U);

        Com_PackSignal(Com_TxBuffer[sig->IPduId],
                       sig->BitPosition, sig->BitSize, sig->Endian, value);
        return E_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType Com_TriggerIPDUSend(Com_IPduIdType IPduId)
{
    if (Com_ConfigPtr == NULL)
        return E_NOT_OK;

    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        if (ipdu->IPduId != IPduId)
            continue;

        Det_PrintP(PSTR("[Com_TxSend] IPdu="));
        Det_PrintDec(IPduId);
        Det_PrintP(PSTR(" data=["));
        for (uint8 b = 0; b < ipdu->DLC; b++)
        {
            if (b > 0U) Det_Print(" ");
            if (Com_TxBuffer[IPduId][b] < 0x10U) Det_Print("0");
            Det_PrintHex(Com_TxBuffer[IPduId][b]);
        }
        Det_LogP(PSTR("]"));

        PduInfoType pduInfo = {
            .SduDataPtr = Com_TxBuffer[IPduId],
            .SduLength  = ipdu->DLC
        };
        return PduR_Transmit(ipdu->PduRId, &pduInfo);
    }

    Det_PrintP(PSTR("[Com_TxSend] no TX IPdu="));
    Det_PrintDec(IPduId);
    Det_Newline();
    return E_NOT_OK;
}

void Com_TxConfirmation(PduIdType PduId)
{
    Det_PrintP(PSTR("[Com_TxConf] PduId="));
    Det_PrintDec(PduId);
    Det_Newline();
}
