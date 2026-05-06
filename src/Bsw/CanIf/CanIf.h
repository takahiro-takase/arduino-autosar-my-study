#ifndef CANIF_H
#define CANIF_H

#include "CanIf_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void           CanIf_Init(const CanIf_ConfigType* Config);
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfo);
void           CanIf_RxIndication(Can_HwHandleType Hrh, Can_IdType CanId, uint8 Dlc, const uint8* Data);
void           CanIf_TxConfirmation(PduIdType TxPduId);

#ifdef __cplusplus
}
#endif

#endif
