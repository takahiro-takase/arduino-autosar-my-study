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

/* SecOC_IfTransmit() 等、TX 経路上の中間モジュールへの委譲に使う関数ポインタ型。
 * PduR_Transmit() 自身と同じシグネチャ（Std_ReturnType(*)(PduIdType, const
 * PduInfoType*)）を持つため、実 AUTOSAR の SecOC_IfTransmit() のような
 * 「PduR_<Up>Transmit と同じ形の下位層エントリポイント」をそのまま登録できる。 */
typedef Std_ReturnType (*PduR_TxTransmitFctType)(PduIdType DestPduId, const PduInfoType* PduInfoPtr);

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
    /* TX 経路上に中間モジュール（SecOC 等）を挟む場合のみ使用。NULL（既定）なら
     * PduR_Transmit() は従来どおり CanIf_Transmit() へ直接転送する
     * （既存の全 TX パスはこのフィールドを設定しないため無変更）。非 NULL なら
     * PduR_Transmit() は CanIf_Transmit() の代わりにこの関数を
     * TransmitOverrideId 付きで呼び、中間モジュールに変換を委ねる。
     * 中間モジュールは変換完了後、PduR_SecOCTransmit()（PduR_Transmit() とは
     * 別の、TransmitOverrideFct を評価しないエントリポイント）を呼んで
     * CanIf_Transmit() まで到達させる（[SWS_SecOC_00058]〜[SWS_SecOC_00062]、
     * 7.4.1 節の "ad-hoc transmission" フロー相当）。 */
    PduR_TxTransmitFctType    TransmitOverrideFct;
    PduIdType                 TransmitOverrideId;
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
