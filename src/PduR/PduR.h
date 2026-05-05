#ifndef PDUR_H
#define PDUR_H

#include "PduR_Types.h"

// -------------------------------------------------------
// PduR_Init
// RoutingPath テーブルへのポインタを保存する。
// CanIf_Init の後に呼ぶこと。
// -------------------------------------------------------
void PduR_Init(const PduR_ConfigType* Config);

// -------------------------------------------------------
// PduR_Transmit
// 上位層（アプリ / COM）からの送信要求。
// SrcPduId で TX RoutingPath を検索し、CanIf_Transmit に転送する。
// -------------------------------------------------------
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// PduR_CanIfRxIndication
// CanIf から呼ばれる受信通知コールバック。
// SrcPduId（CanIf の名前空間）で RX RoutingPath を検索し、
// 全 Dests に RxIndFct(DestPduId, data) を呼ぶ。
//
// シグネチャが CanIf_RxIndicationFctType と一致するため
// CanIf RX PDU テーブルの RxIndicationFct に直接登録できる。
// -------------------------------------------------------
void PduR_CanIfRxIndication(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// PduR_CanIfTxConfirmation
// CanIf から呼ばれる送信完了コールバック。
// SrcPduId（PduR TX RoutingPath の入口 ID）で TX RoutingPath を検索し、
// ConfFct(ConfDestPduId) を呼んで上位層に完了を通知する。
//
// シグネチャが CanIf_TxConfirmationFctType と一致するため
// CanIf TX PDU テーブルの TxConfirmFct に直接登録できる。
// -------------------------------------------------------
void PduR_CanIfTxConfirmation(PduIdType SrcPduId);

#endif
