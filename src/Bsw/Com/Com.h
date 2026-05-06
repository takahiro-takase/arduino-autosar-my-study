#ifndef COM_H
#define COM_H

#include "Com_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void           Com_Init(const Com_ConfigType* Config);
void           Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr);
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr);
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr);
Std_ReturnType Com_TriggerIPDUSend(Com_IPduIdType IPduId);
void           Com_TxConfirmation(PduIdType PduId);

#ifdef __cplusplus
}
#endif

#endif
