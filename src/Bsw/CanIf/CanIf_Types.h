/**
 * \file    CanIf_Types.h
 * \brief   CAN インタフェース型定義 (AUTOSAR SWS_CANInterface 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANIF_TYPES_H
#define CANIF_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

/* SWS_CANIF_00056: upper-layer RX indication callback. */
typedef void (*CanIf_RxIndicationFctType)(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/* SWS_CANIF_00011: upper-layer TX confirmation callback.
 * result: E_OK = transmitted successfully, E_NOT_OK = failed. */
typedef void (*CanIf_TxConfirmationFctType)(PduIdType TxPduId, Std_ReturnType result);

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

/* RX PDU configuration entry (one row of the routing table).
 * CanIf_RxIndication(Mailbox, PduInfoPtr) searches this table by HOH and
 * CAN ID, then calls RxIndicationFct(UpperLayerRxPduId, PduInfoPtr). */
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
