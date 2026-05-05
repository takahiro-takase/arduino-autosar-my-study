#include <Arduino.h>
#include "Com.h"
#include "PduR.h"

// -------------------------------------------------------
// 内部バッファ（Step 3 以降で実装する）
// -------------------------------------------------------
#define COM_IPDU_MAX_DLC  8
#define COM_RX_IPDU_MAX   1
#define COM_TX_IPDU_MAX   1

static const Com_ConfigType* Com_ConfigPtr = nullptr;
static uint8 Com_RxBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8 Com_TxBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];

void Com_Init(const Com_ConfigType* Config)
{
    (void)Config;
    // Step 3 で実装
}

void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    (void)PduId;
    (void)PduInfoPtr;
    // Step 4 で実装
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr)
{
    (void)SignalId;
    (void)SignalDataPtr;
    return E_NOT_OK; // Step 5 で実装
}

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr)
{
    (void)SignalId;
    (void)SignalDataPtr;
    return E_NOT_OK; // Step 5 で実装
}

Std_ReturnType Com_TriggerIPDUSend(Com_IPduIdType IPduId)
{
    (void)IPduId;
    return E_NOT_OK; // Step 5 で実装
}

void Com_TxConfirmation(PduIdType PduId)
{
    Serial.print("[Com_TxConfirmation] PduId=");
    Serial.print(PduId);
    Serial.println(" : TX complete");
}
