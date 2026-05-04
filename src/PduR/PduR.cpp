#include <Arduino.h>
#include "PduR.h"
#include "CanIf.h" // CanIf_Transmit を使用するため

static const PduR_ConfigType* PduR_ConfigPtr = nullptr;

// -------------------------------------------------------
// PduR_Init
// -------------------------------------------------------
void PduR_Init(const PduR_ConfigType* Config)
{
    PduR_ConfigPtr = Config;
    Serial.println("[PduR_Init] initialized");
    Serial.print("  TX route count: ");
    Serial.println(Config->TxRouteCount);
    Serial.print("  RX route count: ");
    Serial.println(Config->RxRouteCount);
}

// -------------------------------------------------------
// PduR_Transmit
// 上位層からの送信要求を CanIf に転送する。
//
// 【処理フロー】
//   PduR TxPduId → TX ルート[TxPduId].CanIfTxPduId
//   → CanIf_Transmit(CanIfTxPduId, PduInfoPtr)
// -------------------------------------------------------
Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == nullptr)
        return E_NOT_OK;

    if (TxPduId >= PduR_ConfigPtr->TxRouteCount)
    {
        Serial.println("[PduR_Transmit] ERROR: invalid TxPduId");
        return E_NOT_OK;
    }

    if (PduInfoPtr == nullptr)
        return E_NOT_OK;

    const PduR_TxRouteType* route = &PduR_ConfigPtr->TxRoutes[TxPduId];

    Serial.print("[PduR_Transmit] PduId=");
    Serial.print(TxPduId);
    Serial.print(" -> CanIf TxPduId=");
    Serial.println(route->CanIfTxPduId);

    return CanIf_Transmit(route->CanIfTxPduId, PduInfoPtr);
}

// -------------------------------------------------------
// PduR_CanIfRxIndication
// CanIf から来た受信通知を上位層（COM）に転送する。
//
// 【処理フロー】
//   CanIf RxPduId → RX ルート[RxPduId].UpperLayerRxFct
//   → UpperLayerRxFct(UpperLayerPduId, PduInfoPtr)
// -------------------------------------------------------
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == nullptr)
        return;

    if (RxPduId >= PduR_ConfigPtr->RxRouteCount)
    {
        Serial.println("[PduR_CanIfRxIndication] ERROR: invalid RxPduId");
        return;
    }

    const PduR_RxRouteType* route = &PduR_ConfigPtr->RxRoutes[RxPduId];

    Serial.print("[PduR_CanIfRxIndication] RxPduId=");
    Serial.print(RxPduId);
    Serial.print(" -> UpperLayer PduId=");
    Serial.println(route->UpperLayerPduId);

    if (route->UpperLayerRxFct != nullptr)
    {
        route->UpperLayerRxFct(route->UpperLayerPduId, PduInfoPtr);
    }
}

// -------------------------------------------------------
// PduR_CanIfTxConfirmation
// CanIf から来た送信完了通知を上位層（COM）に転送する。
//
// 引数の TxPduId は CanIf TX テーブルの UpperLayerTxPduId
// （= PduR 側の TxPduId）。
//
// 【処理フロー】
//   CanIf UpperLayerTxPduId → TX ルート[TxPduId].UpperLayerTxConfirmFct
//   → UpperLayerTxConfirmFct(UpperLayerTxPduId)
// -------------------------------------------------------
void PduR_CanIfTxConfirmation(PduIdType TxPduId)
{
    if (PduR_ConfigPtr == nullptr)
        return;

    if (TxPduId >= PduR_ConfigPtr->TxRouteCount)
        return;

    const PduR_TxRouteType* route = &PduR_ConfigPtr->TxRoutes[TxPduId];

    Serial.print("[PduR_CanIfTxConfirmation] -> COM TxPduId=");
    Serial.println(route->UpperLayerTxPduId);

    if (route->UpperLayerTxConfirmFct != nullptr)
    {
        route->UpperLayerTxConfirmFct(route->UpperLayerTxPduId);
    }
}
