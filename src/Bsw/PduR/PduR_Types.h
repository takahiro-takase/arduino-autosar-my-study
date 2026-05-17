#ifndef PDUR_TYPES_H
#define PDUR_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

typedef enum
{
    PDUR_MODULE_COM   = 0,
    PDUR_MODULE_CANTP = 1,
    PDUR_MODULE_DCM   = 2,
} PduR_DestModuleType;

typedef void (*PduR_RxIndicationFctType)(PduIdType DestPduId, const PduInfoType* PduInfoPtr);

typedef void (*PduR_TxConfirmationFctType)(PduIdType DestPduId);

typedef struct
{
    PduR_DestModuleType      Module;
    PduIdType                DestPduId;
    PduR_RxIndicationFctType RxIndFct;
} PduR_RxDestType;

typedef struct
{
    PduIdType               SrcPduId;
    const PduR_RxDestType*  Dests;
    uint8                   DestCount;
} PduR_RxRoutingPathType;

typedef struct
{
    PduIdType                 SrcPduId;
    PduIdType                 CanIfTxPduId;
    PduIdType                 ConfDestPduId;
    PduR_TxConfirmationFctType ConfFct;
} PduR_TxRoutingPathType;

/* SWS_PduR_00328: post-build configuration type */
typedef struct
{
    const PduR_RxRoutingPathType* RxPaths;
    uint8                         RxPathCount;
    const PduR_TxRoutingPathType* TxPaths;
    uint8                         TxPathCount;
} PduR_PBConfigType;

#endif
