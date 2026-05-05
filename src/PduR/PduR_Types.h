#ifndef PDUR_TYPES_H
#define PDUR_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

// -------------------------------------------------------
// 転送先モジュールの種別
// PduR が PDU を届けるモジュールを列挙する。
// 将来のモジュール追加はここに enum 値を追加するだけでよい。
// -------------------------------------------------------
typedef enum
{
    PDUR_MODULE_COM   = 0, // COM モジュール（シグナル管理）
    PDUR_MODULE_CANTP = 1, // CAN Transport Protocol（大容量データ転送）
    PDUR_MODULE_DCM   = 2, // Diagnostic Communication Manager
    // 将来追加例: PDUR_MODULE_NM, PDUR_MODULE_XCP ...
} PduR_DestModuleType;

// -------------------------------------------------------
// 受信通知コールバック型
// 転送先モジュールの RxIndication 関数のシグネチャ
//   DestPduId : 転送先モジュール内での PduId（CanIf の PduId とは別の名前空間）
//   PduInfoPtr: 受信データ
// -------------------------------------------------------
typedef void (*PduR_RxIndicationFctType)(PduIdType DestPduId, const PduInfoType* PduInfoPtr);

// -------------------------------------------------------
// 送信完了通知コールバック型
// 転送先モジュールの TxConfirmation 関数のシグネチャ
//   DestPduId : 転送先モジュール内での PduId
// -------------------------------------------------------
typedef void (*PduR_TxConfirmationFctType)(PduIdType DestPduId);

// -------------------------------------------------------
// RX RoutingPath の「転送先 1 エントリ」
//
// 1 つの受信 PDU を複数の宛先へ届けられる（マルチキャスト）。
// 例: CanIf RxPduId=0 → COM へ AND CanTp へ同時転送
// -------------------------------------------------------
typedef struct
{
    PduR_DestModuleType      Module;     // 転送先モジュールの種別
    PduIdType                DestPduId;  // 転送先モジュール内の PduId
                                         //（CanIf の RxPduId とは別の名前空間）
    PduR_RxIndicationFctType RxIndFct;  // 転送先の RxIndication 関数
} PduR_RxDestType;

// -------------------------------------------------------
// RX RoutingPath（受信経路の 1 エントリ）
//
// CanIf が PduR_CanIfRxIndication(SrcPduId, data) を呼ぶと、
// PduR は SrcPduId でこのテーブルを検索し、
// 全 Dests[i] に対して RxIndFct(DestPduId, data) を呼ぶ。
//
//   SrcPduId -------------- CanIf の名前空間（CanIf が使う ID）
//   Dests[i].DestPduId ---- 転送先モジュールの名前空間（COM 等が使う ID）
// -------------------------------------------------------
typedef struct
{
    PduIdType               SrcPduId;  // CanIf RxPduId（入口の ID）
    const PduR_RxDestType*  Dests;     // 転送先の配列（複数可）
    uint8                   DestCount; // 転送先の数
} PduR_RxRoutingPathType;

// -------------------------------------------------------
// TX RoutingPath（送信経路の 1 エントリ）
//
// 上位層が PduR_Transmit(SrcPduId, data) を呼ぶと：
//   → CanIf_Transmit(CanIfTxPduId, data) を呼ぶ
//
// CanIf が PduR_CanIfTxConfirmation(SrcPduId) を呼ぶと：
//   → ConfFct(ConfDestPduId) を呼ぶ（上位層に完了を通知）
//
//   SrcPduId ------- PduR の TX 入口 ID（上位層が PduR_Transmit に渡す）
//   CanIfTxPduId --- CanIf の名前空間（CanIf_Transmit に渡す）
//   ConfDestPduId -- 確認通知先の名前空間（COM に返す TxPduId）
// -------------------------------------------------------
typedef struct
{
    PduIdType                 SrcPduId;       // 上位層からの TX 入口 ID
    PduIdType                 CanIfTxPduId;   // 転送先 CanIf TX PduId
    PduIdType                 ConfDestPduId;  // TxConfirmation で上位層に返す PduId
    PduR_TxConfirmationFctType ConfFct;       // TxConfirmation コールバック
} PduR_TxRoutingPathType;

// -------------------------------------------------------
// PduR 全体設定（PduR_Init に渡す）
//
// RX / TX それぞれのルーティングテーブルへのポインタと
// エントリ数をまとめたもの。
// 実際の AUTOSAR では ARXML から自動生成される。
// -------------------------------------------------------
typedef struct
{
    const PduR_RxRoutingPathType* RxPaths;     // RX RoutingPath テーブルの先頭
    uint8                         RxPathCount; // RX エントリ数
    const PduR_TxRoutingPathType* TxPaths;     // TX RoutingPath テーブルの先頭
    uint8                         TxPathCount; // TX エントリ数
} PduR_ConfigType;

#endif
