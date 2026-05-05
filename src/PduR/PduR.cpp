#include <Arduino.h>
#include "PduR.h"
#include "CanIf.h"

static const PduR_ConfigType* PduR_ConfigPtr = nullptr;

// Step 3 で実装
void PduR_Init(const PduR_ConfigType* Config)
{
    (void)Config;
    // Step 3 で実装
}

// Step 4 で実装
void PduR_CanIfRxIndication(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    (void)SrcPduId;
    (void)PduInfoPtr;
    // Step 4 で実装
}

// Step 5 で実装
void PduR_CanIfTxConfirmation(PduIdType SrcPduId)
{
    (void)SrcPduId;
    // Step 5 で実装
}

// Step 6 で実装
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    (void)SrcPduId;
    (void)PduInfoPtr;
    return E_NOT_OK; // Step 6 で実装
}
