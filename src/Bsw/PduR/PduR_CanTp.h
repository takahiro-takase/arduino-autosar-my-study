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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "ComStack_Types.h"
#include "Std_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* SWS 8.3.4 章: 下位層の TP（CanTp）が上位層（Dcm）を呼ぶ API。PduR が、TP のルーティングパス
 * （PduR_TpRxRoutingPathType / PduR_TpTxRoutingPathType）で上位層の PDU ID へ変換して転送する。 */
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType id, const PduInfoType* info, PduLengthType TpSduLength,
                                             PduLengthType* bufferSizePtr);                          /* SWS_PduR_00507 */
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType id, const PduInfoType* info,
                                       PduLengthType* bufferSizePtr);                                /* SWS_PduR_00512 */
void              PduR_CanTpRxIndication(PduIdType id, Std_ReturnType result);                       /* SWS_PduR_00375 */
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType id, const PduInfoType* info, const RetryInfoType* retry,
                                       PduLengthType* availableDataPtr);                             /* SWS_PduR_00518 */
void              PduR_CanTpTxConfirmation(PduIdType id, Std_ReturnType result);                     /* SWS_PduR_00381 */

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif
