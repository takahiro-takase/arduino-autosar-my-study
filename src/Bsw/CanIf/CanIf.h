#ifndef CANIF_H
#define CANIF_H

#include "CanIf_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void           CanIf_Init(const CanIf_ConfigType* ConfigPtr);
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
void           CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr);
void           CanIf_TxConfirmation(PduIdType CanTxPduId);

#ifdef __cplusplus
}
#endif

#endif
