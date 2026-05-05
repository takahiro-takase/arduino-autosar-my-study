#include <Arduino.h>
#include "PduR.h"
#include "CanIf.h"

static const PduR_ConfigType* PduR_ConfigPtr = nullptr;

// -------------------------------------------------------
// PduR_Init
// RoutingPath テーブルへのポインタを保存する。
// CanIf_Init 完了後に呼ぶこと（PduR_Transmit が CanIf に依存するため）。
// -------------------------------------------------------
void PduR_Init(const PduR_ConfigType* Config)
{
    // --- 1. NULL チェック ---
    if (Config == nullptr)
    {
        Serial.println("[PduR_Init] ERROR: Config is NULL");
        return;
    }

    // --- 2. RoutingPath テーブルの最低限検証 ---
    for (uint8 i = 0; i < Config->RxPathCount; i++)
    {
        if (Config->RxPaths[i].Dests == nullptr || Config->RxPaths[i].DestCount == 0)
        {
            Serial.print("[PduR_Init] ERROR: RxPath[");
            Serial.print(i);
            Serial.println("] has no destinations");
            return;
        }
    }

    // --- 3. 設定ポインタを保存 ---
    PduR_ConfigPtr = Config;

    // --- 4. 登録内容をダンプ（学習・デバッグ用）---
    Serial.println("[PduR_Init] initialized");

    Serial.print("  RX RoutingPaths: ");
    Serial.println(Config->RxPathCount);
    for (uint8 i = 0; i < Config->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &Config->RxPaths[i];
        Serial.print("    [");
        Serial.print(i);
        Serial.print("] CanIf RxPduId=");
        Serial.print(path->SrcPduId);
        Serial.print(" -> ");
        Serial.print(path->DestCount);
        Serial.println(" dest(s)");
        for (uint8 d = 0; d < path->DestCount; d++)
        {
            Serial.print("         dest[");
            Serial.print(d);
            Serial.print("] Module=");
            Serial.print(path->Dests[d].Module); // 0=COM, 1=CANTP ...
            Serial.print(" DestPduId=");
            Serial.println(path->Dests[d].DestPduId);
        }
    }

    Serial.print("  TX RoutingPaths: ");
    Serial.println(Config->TxPathCount);
    for (uint8 i = 0; i < Config->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &Config->TxPaths[i];
        Serial.print("    [");
        Serial.print(i);
        Serial.print("] SrcPduId=");
        Serial.print(path->SrcPduId);
        Serial.print(" -> CanIf TxPduId=");
        Serial.print(path->CanIfTxPduId);
        Serial.print("  Confirm -> DestPduId=");
        Serial.println(path->ConfDestPduId);
    }
}

// -------------------------------------------------------
// PduR_CanIfRxIndication
// CanIf から呼ばれる受信通知。
// SrcPduId（CanIf の名前空間）で RX RoutingPath を線形探索し、
// 一致した全 Dests に RxIndFct(DestPduId, data) を呼ぶ。
// -------------------------------------------------------
void PduR_CanIfRxIndication(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    // --- 1. 初期化・NULL チェック ---
    if (PduR_ConfigPtr == nullptr || PduInfoPtr == nullptr)
    {
        return;
    }

    // --- 2. RX RoutingPath テーブルを SrcPduId で線形探索 ---
    for (uint8 i = 0; i < PduR_ConfigPtr->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &PduR_ConfigPtr->RxPaths[i];

        if (path->SrcPduId != SrcPduId)
        {
            continue;
        }

        // --- 3. 一致した RoutingPath の全 Dests にルーティング ---
        for (uint8 d = 0; d < path->DestCount; d++)
        {
            const PduR_RxDestType* dest = &path->Dests[d];

            Serial.print("[PduR_CanIfRxIndication]");
            Serial.print(" CanIf SrcPduId=");
            Serial.print(SrcPduId);
            Serial.print(" -> Module=");
            Serial.print(dest->Module);
            Serial.print(" DestPduId=");
            Serial.println(dest->DestPduId);

            if (dest->RxIndFct != nullptr)
            {
                dest->RxIndFct(dest->DestPduId, PduInfoPtr);
            }
        }

        return;
    }

    Serial.print("[PduR_CanIfRxIndication] no route for SrcPduId=");
    Serial.println(SrcPduId);
}

// -------------------------------------------------------
// PduR_CanIfTxConfirmation
// CanIf から呼ばれる送信完了通知。
// -------------------------------------------------------
void PduR_CanIfTxConfirmation(PduIdType SrcPduId)
{
    if (PduR_ConfigPtr == nullptr)
    {
        return;
    }

    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &PduR_ConfigPtr->TxPaths[i];

        if (path->SrcPduId != SrcPduId)
        {
            continue;
        }

        Serial.print("[PduR_CanIfTxConfirmation]");
        Serial.print(" SrcPduId=");
        Serial.print(SrcPduId);
        Serial.print(" -> ConfDestPduId=");
        Serial.println(path->ConfDestPduId);

        if (path->ConfFct != nullptr)
        {
            path->ConfFct(path->ConfDestPduId);
        }

        return;
    }

    Serial.print("[PduR_CanIfTxConfirmation] no route for SrcPduId=");
    Serial.println(SrcPduId);
}

// -------------------------------------------------------
// PduR_Transmit
// 上位層（アプリ / COM）からの送信要求。
// -------------------------------------------------------
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == nullptr || PduInfoPtr == nullptr)
    {
        return E_NOT_OK;
    }

    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &PduR_ConfigPtr->TxPaths[i];

        if (path->SrcPduId != SrcPduId)
        {
            continue;
        }

        Serial.print("[PduR_Transmit]");
        Serial.print(" SrcPduId=");
        Serial.print(SrcPduId);
        Serial.print(" -> CanIf TxPduId=");
        Serial.println(path->CanIfTxPduId);

        return CanIf_Transmit(path->CanIfTxPduId, PduInfoPtr);
    }

    Serial.print("[PduR_Transmit] no route for SrcPduId=");
    Serial.println(SrcPduId);
    return E_NOT_OK;
}
