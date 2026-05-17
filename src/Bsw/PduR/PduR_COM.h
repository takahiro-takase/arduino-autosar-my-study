#ifndef PDUR_COM_H
#define PDUR_COM_H

#include "ComStack_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWS_PduR_00369 */
void PduR_ComRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif
