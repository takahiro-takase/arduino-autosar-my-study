/**
 * \file    PduR_CanTp.h
 * \brief   PduR-CanTp インタフェース定義 (AUTOSAR SWS_PDURouter 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_CANTP_H
#define PDUR_CANTP_H

#include "ComStack_Types.h"
#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWS_PduR_00406: generic な PduR_<User:Up>Transmit テンプレート（Service ID
 * 0x49）の CanTp 向け実体化。CanTp が PDU 送信を要求する際に呼ぶ。 */
Std_ReturnType PduR_CanTpTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif
