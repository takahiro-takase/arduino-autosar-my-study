/**
 * \file    PduR_Types.h
 * \brief   PDU ルータ型定義 (AUTOSAR SWS_PDURouter 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
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
    PDUR_MODULE_SECOC = 3,
} PduR_DestModuleType;

typedef void (*PduR_RxIndicationFctType)(PduIdType DestPduId, const PduInfoType* PduInfoPtr);

typedef void (*PduR_TxConfirmationFctType)(PduIdType DestPduId, Std_ReturnType result);

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
