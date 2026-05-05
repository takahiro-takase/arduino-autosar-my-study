#ifndef COM_TYPES_H
#define COM_TYPES_H

#include "Platform_Types.h"
#include "ComStack_Types.h"

// シグナルID型
typedef uint8 Com_SignalIdType;

// -------------------------------------------------------
// シグナル設定（PDU内のどのバイトに何バイト格納されるか）
// AUTOSAR では ARXML から自動生成される。
// -------------------------------------------------------
typedef struct
{
    Com_SignalIdType SignalId;    // シグナルID（アプリが使うキー）
    PduIdType        IPduId;     // 属するI-PDUのID（PduR空間のSrcPduId）
    uint8            ByteOffset; // PDU先頭からのバイトオフセット
    uint8            ByteLength; // バイト長（1〜4）
} Com_SignalConfigType;

// -------------------------------------------------------
// COM 全体設定（Com_Init に渡す）
// -------------------------------------------------------
typedef struct
{
    const Com_SignalConfigType* Signals;     // シグナル設定テーブル
    uint8                       SignalCount; // シグナル数
    PduIdType                   TxIPduId;   // TX用I-PDUのID（PduR_Transmit に渡す）
    uint8                       TxDlc;      // TX PDU のバイト長
} Com_ConfigType;

#endif
