#include <Arduino.h>
#include "Com.h"
#include "PduR.h"

#define COM_IPDU_MAX_DLC  8
#define COM_RX_IPDU_MAX   1
#define COM_TX_IPDU_MAX   1

static const Com_ConfigType* Com_ConfigPtr = nullptr;
static uint8 Com_RxBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8 Com_TxBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];

// -------------------------------------------------------
// Com_Init
// -------------------------------------------------------
void Com_Init(const Com_ConfigType* Config)
{
    if (Config == nullptr)
    {
        Serial.println(F("[Com_Init] ERROR: Config is NULL"));
        return;
    }
    if (Config->RxIPduCount > COM_RX_IPDU_MAX)
    {
        Serial.println(F("[Com_Init] ERROR: RxIPduCount exceeds max"));
        return;
    }
    if (Config->TxIPduCount > COM_TX_IPDU_MAX)
    {
        Serial.println(F("[Com_Init] ERROR: TxIPduCount exceeds max"));
        return;
    }

    Com_ConfigPtr = Config;

    for (uint8 i = 0; i < COM_RX_IPDU_MAX; i++)
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
            Com_RxBuffer[i][j] = 0;

    for (uint8 i = 0; i < COM_TX_IPDU_MAX; i++)
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
            Com_TxBuffer[i][j] = 0;

    Serial.println(F("[Com_Init] OK"));

    Serial.print(F("  RX I-PDUs: "));
    Serial.println(Config->RxIPduCount);
    for (uint8 i = 0; i < Config->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Config->RxIPdus[i];
        Serial.print(F("    IPduId="));
        Serial.print(ipdu->IPduId);
        Serial.print(F(" DLC="));
        Serial.print(ipdu->DLC);
        Serial.print(F(" PduRId="));
        Serial.println(ipdu->PduRId);
    }

    Serial.print(F("  TX I-PDUs: "));
    Serial.println(Config->TxIPduCount);
    for (uint8 i = 0; i < Config->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Config->TxIPdus[i];
        Serial.print(F("    IPduId="));
        Serial.print(ipdu->IPduId);
        Serial.print(F(" DLC="));
        Serial.print(ipdu->DLC);
        Serial.print(F(" PduRId="));
        Serial.println(ipdu->PduRId);
    }

    Serial.print(F("  Signals: "));
    Serial.println(Config->SignalCount);
    for (uint8 s = 0; s < Config->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Config->Signals[s];
        Serial.print(F("    SigId="));
        Serial.print(sig->SignalId);
        Serial.print(F(" IPduId="));
        Serial.print(sig->IPduId);
        Serial.print(F(" Bit="));
        Serial.print(sig->BitPosition);
        Serial.print(F("/"));
        Serial.print(sig->BitSize);
        Serial.print(F(" Endian="));
        Serial.println(sig->Endian == COM_BIG_ENDIAN ? F("BIG") : F("LITTLE"));
    }
}

// -------------------------------------------------------
// Com_RxIndication
// -------------------------------------------------------
void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    if (Com_ConfigPtr == nullptr || PduInfoPtr == nullptr)
        return;

    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->PduRId != PduId)
            continue;

        const uint8 copyLen = (PduInfoPtr->SduLength < ipdu->DLC)
                              ? PduInfoPtr->SduLength : ipdu->DLC;

        for (uint8 b = 0; b < copyLen; b++)
            Com_RxBuffer[ipdu->IPduId][b] = PduInfoPtr->SduDataPtr[b];

        Serial.print(F("[Com_RxInd] IPduId="));
        Serial.print(ipdu->IPduId);
        Serial.print(F(" raw=["));
        for (uint8 b = 0; b < copyLen; b++)
        {
            if (b > 0) Serial.print(' ');
            if (Com_RxBuffer[ipdu->IPduId][b] < 0x10) Serial.print('0');
            Serial.print(Com_RxBuffer[ipdu->IPduId][b], HEX);
        }
        Serial.println(']');
        return;
    }

    Serial.print(F("[Com_RxInd] no IPdu for PduRId="));
    Serial.println(PduId);
}

// -------------------------------------------------------
// [内部] Com_UnpackSignal / Com_PackSignal
// ビット番号: bit0 = byte[0] MSB (ネットワークビット順)
// -------------------------------------------------------
static uint32 Com_UnpackSignal(const uint8* buf,
                                uint8 bitPos,
                                uint8 bitSize,
                                Com_SignalEndianType endian)
{
    uint32 value = 0;
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 pos     = bitPos + i;
        const uint8 bit     = (buf[pos / 8] >> (7 - (pos % 8))) & 1U;
        if (endian == COM_BIG_ENDIAN)
            value = (value << 1) | bit;
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
        const uint8 bit     = (endian == COM_BIG_ENDIAN)
                              ? (uint8)((value >> (bitSize - 1U - i)) & 1U)
                              : (uint8)((value >> i) & 1U);
        const uint8 pos     = bitPos + i;
        const uint8 shift   = 7 - (pos % 8);
        if (bit)
            buf[pos / 8] |=  (uint8)(1U << shift);
        else
            buf[pos / 8] &= (uint8)~(1U << shift);
    }
}

// -------------------------------------------------------
// Com_ReceiveSignal
// -------------------------------------------------------
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr)
{
    if (Com_ConfigPtr == nullptr || SignalDataPtr == nullptr)
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
        SignalDataPtr[1] = (uint8)(value >>  8);
        SignalDataPtr[2] = (uint8)(value >> 16);
        SignalDataPtr[3] = (uint8)(value >> 24);
        return E_OK;
    }
    return E_NOT_OK;
}

// -------------------------------------------------------
// Com_SendSignal
// -------------------------------------------------------
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr)
{
    if (Com_ConfigPtr == nullptr || SignalDataPtr == nullptr)
        return E_NOT_OK;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

        const uint32 value = (uint32)SignalDataPtr[0]
                           | ((uint32)SignalDataPtr[1] <<  8)
                           | ((uint32)SignalDataPtr[2] << 16)
                           | ((uint32)SignalDataPtr[3] << 24);

        Com_PackSignal(Com_TxBuffer[sig->IPduId],
                       sig->BitPosition, sig->BitSize, sig->Endian, value);
        return E_OK;
    }
    return E_NOT_OK;
}

// -------------------------------------------------------
// Com_TriggerIPDUSend
// -------------------------------------------------------
Std_ReturnType Com_TriggerIPDUSend(Com_IPduIdType IPduId)
{
    if (Com_ConfigPtr == nullptr)
        return E_NOT_OK;

    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        if (ipdu->IPduId != IPduId)
            continue;

        Serial.print(F("[Com_TxSend] IPduId="));
        Serial.print(IPduId);
        Serial.print(F(" data=["));
        for (uint8 b = 0; b < ipdu->DLC; b++)
        {
            if (b > 0) Serial.print(' ');
            if (Com_TxBuffer[IPduId][b] < 0x10) Serial.print('0');
            Serial.print(Com_TxBuffer[IPduId][b], HEX);
        }
        Serial.println(']');

        PduInfoType pduInfo = {
            .SduDataPtr = Com_TxBuffer[IPduId],
            .SduLength  = ipdu->DLC
        };
        return PduR_Transmit(ipdu->PduRId, &pduInfo);
    }

    Serial.print(F("[Com_TxSend] no TX IPdu="));
    Serial.println(IPduId);
    return E_NOT_OK;
}

void Com_TxConfirmation(PduIdType PduId)
{
    Serial.print(F("[Com_TxConf] PduId="));
    Serial.println(PduId);
}
