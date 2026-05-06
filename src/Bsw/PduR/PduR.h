#ifndef PDUR_H
#define PDUR_H

#include "PduR_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void           PduR_Init(const PduR_ConfigType* Config);
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);
void           PduR_CanIfRxIndication(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);
void           PduR_CanIfTxConfirmation(PduIdType SrcPduId);

#ifdef __cplusplus
}
#endif

#endif
