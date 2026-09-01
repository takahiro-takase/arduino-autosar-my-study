/**
 * \file    PduR_SecOC.h
 * \brief   PduR-SecOC インタフェース定義 (AUTOSAR SWS_PDURouter 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_SECOC_H
#define PDUR_SECOC_H

#include "ComStack_Types.h"
#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWS_SecOC_00062: SecOC が変換完了後の Secured I-PDU を送信する際に呼ぶ。 */
Std_ReturnType PduR_SecOCTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);

/* SWS_PduR_00365 相当（generic な PduR_<Lo>TxConfirmation テンプレートの
 * SecOC 向け実体化）: 下位層（CanIf 等）が PduR_CanIfTxConfirmation() 経由で
 * 通知した TX 確認結果を、SecOC の TX 経路（PduRSrcPduId、SecOC_TxPduConfigType
 * 参照）へルーティングする際に SecOC_TxConfirmation() から呼ぶ。 */
void PduR_SecOCTxConfirmation(PduIdType SrcPduId, Std_ReturnType result);

#ifdef __cplusplus
}
#endif

#endif
