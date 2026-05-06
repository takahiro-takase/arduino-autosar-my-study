#ifndef CANIF_TYPES_H
#define CANIF_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

// -------------------------------------------------------
// 上位層（PduR / アプリ）への受信通知コールバック型
// AUTOSAR SWS_CANIF_00056 相当
//   RxPduId  : CanIf が上位層に渡す受信 PDU の論理 ID
//   PduInfoPtr: 受信データ（ポインタ + 長さ）
// -------------------------------------------------------
typedef void (*CanIf_RxIndicationFctType)(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// 上位層への送信完了通知コールバック型
// AUTOSAR SWS_CANIF_00011 相当
//   TxPduId  : 完了した送信 PDU の論理 ID
// -------------------------------------------------------
typedef void (*CanIf_TxConfirmationFctType)(PduIdType TxPduId);

// -------------------------------------------------------
// TX PDU 設定エントリ（テーブルの 1 行）
//
// CanIf_Transmit(TxPduId, PduInfo) が呼ばれると、
// TxPduId をインデックスにこのテーブルを引き、
// Can_Write(Hth, Can_PduType) を構築して CanDrv を呼ぶ。
// -------------------------------------------------------
typedef struct
{
    PduIdType                   UpperLayerTxPduId; // TxConfirmation で上位層に返す PDU ID
    Can_IdType                  CanId;             // 送出する CAN フレームの ID
    uint8                       Dlc;               // 最大データ長（0〜8）
    Can_HwHandleType            Hth;               // 使用する TX バッファ（HTH）
    CanIf_TxConfirmationFctType TxConfirmFct;      // 送信完了コールバック（不要なら NULL）
} CanIf_TxPduConfigType;

// -------------------------------------------------------
// RX PDU 設定エントリ（テーブルの 1 行）
//
// CanDrv から CanIf_RxIndication(Hrh, CanId, Dlc, Data) が来ると、
// このテーブルを検索して一致するエントリを見つけ、
// RxIndicationFct(UpperLayerRxPduId, PduInfo) で上位層に通知する。
// -------------------------------------------------------
typedef struct
{
    Can_IdType                CanId;              // マッチさせる CAN ID
    Can_HwHandleType          Hrh;               // 受信元の HRH（RX バッファ識別子）
    PduIdType                 UpperLayerRxPduId; // 上位層に渡す受信 PDU ID
    CanIf_RxIndicationFctType RxIndicationFct;   // 受信時に呼ぶ上位層コールバック
} CanIf_RxPduConfigType;

// -------------------------------------------------------
// CanIf 全体設定（CanIf_Init に渡す）
//
// TX/RX テーブルへのポインタとエントリ数をまとめたもの。
// AUTOSAR では ARXML から自動生成されるが、
// ここでは main.cpp で静的に定義して渡す。
// -------------------------------------------------------
typedef struct
{
    const CanIf_TxPduConfigType* TxPduConfig; // TX PDU テーブルの先頭
    uint8                        TxPduCount;  // TX エントリ数
    const CanIf_RxPduConfigType* RxPduConfig; // RX PDU テーブルの先頭
    uint8                        RxPduCount;  // RX エントリ数
} CanIf_ConfigType;

#endif
