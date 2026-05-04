#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"

// AUTOSAR SWS_ComStackTypes
// PDU（Protocol Data Unit）の識別子
typedef uint16 PduIdType;

// SDU（Service Data Unit）の長さ
typedef uint16 PduLengthType;

// PDU の情報（上位層への受信通知や送信要求で使用）
// CanIf_RxIndication / CanIf_Transmit などの引数になる
typedef struct
{
    uint8*        SduDataPtr; // ペイロードへのポインタ
    PduLengthType SduLength;  // ペイロード長（バイト）
} PduInfoType;

#endif