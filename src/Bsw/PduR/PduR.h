/**
 * \file    PduR.h
 * \brief   PDU ルータ 公開インタフェース (AUTOSAR SWS_PDURouter 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_H
#define PDUR_H

#include "PduR_Types.h"
#include "PduR_Cfg.h"
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
