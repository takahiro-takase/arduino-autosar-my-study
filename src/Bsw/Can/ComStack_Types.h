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
// ネットワーク（チャネル）の識別子。CanSM/ComM/Nm 等が共通で使う
// （2026-08-30、Nm 対応時に新設。当初は CanSM 独自の CanSM_NetworkHandleType
// と二重定義のまま残していたが、実仕様も CanSM_RequestComMode 等の引数型に
// この共通 NetworkHandleType をそのまま使う設計のため、CanSM 側もこちらへ
// 統合済み（CanSM.h 参照）。今後ネットワークハンドルを扱う新規モジュールは
// この共通型を使うこと）。
typedef uint8 NetworkHandleType;

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
