/**
 * \file    PduR_COM.h
 * \brief   PduR-COM インタフェース定義 (AUTOSAR SWS_PDURouter 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
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
