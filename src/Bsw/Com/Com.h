#ifndef COM_H
#define COM_H

#include "Com_Types.h"

// -------------------------------------------------------
// Com_Init
// I-PDU / Signal テーブルへのポインタを保存し、
// RX / TX バッファをゼロクリアする。
// PduR_Init 完了後に呼ぶこと。
// -------------------------------------------------------
void Com_Init(const Com_ConfigType* Config);

// -------------------------------------------------------
// Com_RxIndication
// PduR から呼ばれる受信通知。
// PDU バイト列を RX バッファに保存する（Signal 抽出は ReceiveSignal 側で行う）。
// シグネチャが PduR_RxIndicationFctType と一致するため
// PduR RX RoutingPath の RxIndFct に直接登録できる。
// -------------------------------------------------------
void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// Com_ReceiveSignal
// RX バッファから Signal をビット単位で抽出し、SignalDataPtr に書き出す。
// 戻り値 E_OK / E_NOT_OK（SignalId 未登録など）
//
// SignalDataPtr: 最大 4 バイト（uint32 相当）のバッファを渡すこと
// -------------------------------------------------------
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr);

// -------------------------------------------------------
// Com_SendSignal
// SignalDataPtr の値を TX バッファの該当ビット位置にパックする。
// 実際の送信は Com_TriggerIPDUSend で行う。
//
// SignalDataPtr: 最大 4 バイト（uint32 相当）のバッファを渡すこと
// -------------------------------------------------------
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr);

// -------------------------------------------------------
// Com_TriggerIPDUSend
// TX バッファ全体を PduR_Transmit に渡して送信する。
// 複数シグナルを Com_SendSignal でまとめてから 1 回呼ぶ。
// -------------------------------------------------------
Std_ReturnType Com_TriggerIPDUSend(Com_IPduIdType IPduId);

// -------------------------------------------------------
// Com_TxConfirmation
// PduR から呼ばれる送信完了通知。
// シグネチャが PduR_TxConfirmationFctType と一致する。
// -------------------------------------------------------
void Com_TxConfirmation(PduIdType PduId);

#endif
