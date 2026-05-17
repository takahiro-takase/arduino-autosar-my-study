#ifndef PDUR_CANIF_H
#define PDUR_CANIF_H

#include "ComStack_Types.h"
#include "Std_Types.h"
#include "PduR_COM.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWS_PduR_00330: CanIf->PduR receive indication.
 * Mapped to PduR_ComRxIndication by the #define below. */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
#define PduR_CanIfRxIndication PduR_ComRxIndication

/* SWS_PduR_00365: CanIf->PduR transmit confirmation. */
void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

#ifdef __cplusplus
}
#endif

#endif
