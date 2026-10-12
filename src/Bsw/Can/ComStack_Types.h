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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Platform_Types.h"
#include "Std_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

// AUTOSAR SWS_ComStackTypes
// ネットワーク（チャネル）の識別子。CanSM/ComM/CanNm 等が共通で使う
// （2026-08-30、CanNm 対応時に新設。当初は CanSM 独自の CanSM_NetworkHandleType
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

// TP（トランスポートプロトコル）のバッファ要求の結果。
// CanTp ⇔ 上位層（Dcm）の Xxx_StartOfReception()/Xxx_CopyRxData()/Xxx_CopyTxData() の戻り値に使う。
typedef enum
{
    BUFREQ_OK       = 0U,  // 要求を満たした
    BUFREQ_E_NOT_OK = 1U,  // 要求を拒否した（受信は中断する）
    BUFREQ_E_BUSY   = 2U,  // 今は満たせないが、後で再試行してよい
    BUFREQ_E_OVFL   = 3U   // 必要な長さのバッファを用意できない（受信は中断する）
} BufReq_ReturnType;

// TP の送信データの状態。Xxx_CopyTxData() の retry 引数で、送信済みデータの確認や再送を指示する。
typedef enum
{
    TP_DATACONF    = 0U,  // これまでにコピーしたデータは確認済みで、バッファから消してよい
    TP_DATARETRY   = 1U,  // コピー済みのデータを、TxTpDataCnt バイト前から再度コピーする
    TP_CONFPENDING = 2U   // 確認待ち。コピー済みのデータは、エラー回復のためバッファに残す
} TpDataStateType;

// Xxx_CopyTxData() の retry 引数
typedef struct
{
    TpDataStateType TpDataState;  // 送信済みデータの状態
    PduLengthType   TxTpDataCnt;  // TP_DATARETRY のとき、現在のコピー位置から戻るバイト数
} RetryInfoType;

#endif
