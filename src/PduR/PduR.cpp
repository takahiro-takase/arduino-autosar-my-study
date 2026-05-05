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

// Step 4 で実装
void PduR_CanIfRxIndication(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    (void)SrcPduId;
    (void)PduInfoPtr;
    // Step 4 で実装
}

// Step 5 で実装
void PduR_CanIfTxConfirmation(PduIdType SrcPduId)
{
    (void)SrcPduId;
    // Step 5 で実装
}

// Step 6 で実装
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    (void)SrcPduId;
    (void)PduInfoPtr;
    return E_NOT_OK; // Step 6 で実装
}
