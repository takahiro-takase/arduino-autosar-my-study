#ifndef PDUR_TYPES_H
#define PDUR_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

// -------------------------------------------------------
// 上位層（COM / CanTp）への受信通知コールバック型
// CanIf_RxIndicationFctType と同じシグネチャ
// -------------------------------------------------------
typedef void (*PduR_RxIndicationFctType)(PduIdType PduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// 上位層への送信完了通知コールバック型
// CanIf_TxConfirmationFctType と同じシグネチャ
// -------------------------------------------------------
typedef void (*PduR_TxConfirmationFctType)(PduIdType PduId);

// -------------------------------------------------------
// TX ルーティングテーブルの 1 エントリ
//
// PduR_Transmit(PduId) を呼ばれたとき：
//   → CanIf_Transmit(CanIfTxPduId, PduInfo) を呼ぶ
//
// PduR_CanIfTxConfirmation(PduId) を呼ばれたとき：
//   → UpperLayerTxConfirmFct(UpperLayerTxPduId) を呼ぶ
// -------------------------------------------------------
typedef struct
{
    PduIdType                  CanIfTxPduId;           // 転送先 CanIf TX PDU ID
    PduR_TxConfirmationFctType UpperLayerTxConfirmFct; // 上位層 TxConfirmation コールバック
    PduIdType                  UpperLayerTxPduId;      // 上位層に渡す TX PDU ID
} PduR_TxRouteType;

// -------------------------------------------------------
// RX ルーティングテーブルの 1 エントリ
//
// PduR_CanIfRxIndication(RxPduId, PduInfo) を呼ばれたとき：
//   → UpperLayerRxFct(UpperLayerPduId, PduInfo) を呼ぶ
// -------------------------------------------------------
typedef struct
{
    PduR_RxIndicationFctType UpperLayerRxFct;   // 上位層 RxIndication コールバック
    PduIdType                UpperLayerPduId;   // 上位層に渡す RX PDU ID
} PduR_RxRouteType;

// -------------------------------------------------------
// PduR 全体設定（PduR_Init に渡す）
// -------------------------------------------------------
typedef struct
{
    const PduR_TxRouteType* TxRoutes;     // TX ルーティングテーブルの先頭
    uint8                   TxRouteCount; // TX エントリ数
    const PduR_RxRouteType* RxRoutes;     // RX ルーティングテーブルの先頭
    uint8                   RxRouteCount; // RX エントリ数
} PduR_ConfigType;

#endif
