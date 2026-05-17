#ifndef PDUR_H
#define PDUR_H

#include "PduR_Types.h"
#include "PduR_CanIf.h"
#include "PduR_COM.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWS_PduR_00119 */
void           PduR_Init(const PduR_PBConfigType* ConfigPtr);
/* SWS_PduR_00109 */
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif
