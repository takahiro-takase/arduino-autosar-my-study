#ifndef PDUR_H
#define PDUR_H

#include "PduR_Types.h"

// -------------------------------------------------------
// PduR_Init
// ルーティングテーブルへのポインタを保存する。
// CanIf_Init の後に呼ぶこと。
// -------------------------------------------------------
void PduR_Init(const PduR_ConfigType* Config);

// -------------------------------------------------------
// PduR_Transmit
// 上位層（アプリ / COM）からの送信要求。
// TX ルーティングテーブルを引き、CanIf_Transmit に転送する。
// -------------------------------------------------------
Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// PduR_CanIfRxIndication
// CanIf から呼ばれる受信通知コールバック。
// RX ルーティングテーブルを引き、上位層コールバックに転送する。
// シグネチャが CanIf_RxIndicationFctType と一致するため
// CanIf RX PDU テーブルの RxIndicationFct に直接登録できる。
// -------------------------------------------------------
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// PduR_CanIfTxConfirmation
// CanIf から呼ばれる送信完了コールバック。
// TX ルーティングテーブルを引き、上位層コールバックに転送する。
// シグネチャが CanIf_TxConfirmationFctType と一致するため
// CanIf TX PDU テーブルの TxConfirmFct に直接登録できる。
// -------------------------------------------------------
void PduR_CanIfTxConfirmation(PduIdType TxPduId);

#endif
