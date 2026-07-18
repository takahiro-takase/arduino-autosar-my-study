/**
 * \file    PduR_CanIf.h
 * \brief   PduR-CanIf インタフェース定義 (AUTOSAR SWS_PDURouter 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_CANIF_H
#define PDUR_CANIF_H

#include "ComStack_Types.h"
#include "Std_Types.h"
#include "PduR_COM.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWS_PduR_00362: CanIf->PduR receive indication.
 * Mapped to PduR_ComRxIndication by the #define below. */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
#define PduR_CanIfRxIndication PduR_ComRxIndication

/* SWS_PduR_00365: CanIf->PduR transmit confirmation. */
void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);

#ifdef __cplusplus
}
#endif

#endif
