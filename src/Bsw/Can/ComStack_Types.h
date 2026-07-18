/**
 * \file    ComStack_Types.h
 * \brief   通信スタック共通型定義 (AUTOSAR ComStack_Types)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
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
