#ifndef COM_H
#define COM_H

#include "Com_Types.h"
#include "Std_Types.h"

// -------------------------------------------------------
// Com_Init
// シグナル設定テーブルを保存し、RX/TXバッファを初期化する。
// PduR_Init の後に呼ぶこと。
// -------------------------------------------------------
void Com_Init(const Com_ConfigType* Config);

// -------------------------------------------------------
// Com_SendSignal
// シグナル値を TX バッファの該当オフセットに書き込む。
// 送信トリガは Com_TriggerIPDUSend で行う（バッファ書き込みのみ）。
// -------------------------------------------------------
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const uint8* SignalDataPtr);

// -------------------------------------------------------
// Com_TriggerIPDUSend
// TX バッファ全体を PduR_Transmit に渡して送信する。
// 複数シグナルをまとめて 1 フレームで送れる。
// -------------------------------------------------------
Std_ReturnType Com_TriggerIPDUSend(void);

// -------------------------------------------------------
// Com_ReceiveSignal
// RX バッファから最後に受信したシグナル値を読み出す。
// -------------------------------------------------------
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint8* SignalDataPtr);

// -------------------------------------------------------
// Com_RxIndication
// PduR から呼ばれる受信通知。
// PDU バイト列を RX バッファにコピーし、シグナル値をログ出力する。
// シグネチャが PduR_RxIndicationFctType と一致するため
// PduR RX RoutingPath の RxIndFct に直接登録できる。
// -------------------------------------------------------
void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// Com_TxConfirmation
// PduR から呼ばれる送信完了通知。
// シグネチャが PduR_TxConfirmationFctType と一致するため
// PduR TX RoutingPath の ConfFct に直接登録できる。
// -------------------------------------------------------
void Com_TxConfirmation(PduIdType PduId);

#endif
