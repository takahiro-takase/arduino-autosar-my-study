#ifndef CANIF_H
#define CANIF_H

#include "CanIf_Types.h"

// -------------------------------------------------------
// CanIf_Init
// CanIf モジュールを初期化する。
// Config: TX/RX PDU テーブルを含む設定へのポインタ
// -------------------------------------------------------
void CanIf_Init(const CanIf_ConfigType* Config);

// -------------------------------------------------------
// CanIf_Transmit
// 上位層（PduR / アプリ）から呼ばれる送信要求。
// TxPduId : 送信したい PDU の論理 ID（TX テーブルのインデックス）
// PduInfo : 送信データ（SduDataPtr + SduLength）
// 戻り値  : E_OK / E_NOT_OK
// -------------------------------------------------------
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfo);

// -------------------------------------------------------
// CanIf_RxIndication
// CanDrv から呼ばれる受信通知（コールバック）。
// Hrh    : 受信元の HRH（RX バッファ識別子）
// CanId  : 受信した CAN ID
// Dlc    : データ長
// Data   : 受信データへのポインタ
// -------------------------------------------------------
void CanIf_RxIndication(Can_HwHandleType Hrh, Can_IdType CanId, uint8 Dlc, const uint8* Data);

// -------------------------------------------------------
// CanIf_TxConfirmation
// CanDrv から呼ばれる送信完了通知（コールバック）。
// TxPduId: 完了した CanDrv 側の HTH に対応する PDU ID
// -------------------------------------------------------
void CanIf_TxConfirmation(PduIdType TxPduId);

#endif
