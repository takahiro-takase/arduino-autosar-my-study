#include <Arduino.h>
#include "CanIf.h"
#include "Can.h" // Can_Write / Can_ReturnType を使用するため

// -------------------------------------------------------
// モジュール内部状態
// CanIf_Init で設定を受け取り、以降の API が参照する
// -------------------------------------------------------
static const CanIf_ConfigType* CanIf_ConfigPtr = nullptr;

// -------------------------------------------------------
// CanIf_Init
// 設定テーブルへのポインタを保存する。
// CanDrv の初期化（Can_Init / Can_SetControllerMode）完了後に呼ぶこと。
// -------------------------------------------------------
void CanIf_Init(const CanIf_ConfigType* Config)
{
    CanIf_ConfigPtr = Config;
    Serial.println("[CanIf_Init] initialized");
    Serial.print("  TX PDU count: ");
    Serial.println(Config->TxPduCount);
    Serial.print("  RX PDU count: ");
    Serial.println(Config->RxPduCount);
}

// -------------------------------------------------------
// CanIf_Transmit
// 上位層（PduR / アプリ）からの送信要求。
// TxPduId をキーに TX テーブルを引き、CanDrv の Can_Write を呼ぶ。
//
// AUTOSAR SWS_CANIF_00233 相当
// -------------------------------------------------------
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfo)
{
    // --- 1. 初期化チェック ---
    if (CanIf_ConfigPtr == nullptr)
    {
        return E_NOT_OK;
    }

    // --- 2. TxPduId 範囲チェック ---
    if (TxPduId >= CanIf_ConfigPtr->TxPduCount)
    {
        Serial.println("[CanIf_Transmit] ERROR: invalid TxPduId");
        return E_NOT_OK;
    }

    // --- 3. NULL チェック ---
    if (PduInfo == nullptr || PduInfo->SduDataPtr == nullptr)
    {
        Serial.println("[CanIf_Transmit] ERROR: PduInfo is NULL");
        return E_NOT_OK;
    }

    // --- 4. TX テーブルを引く ---
    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[TxPduId];

    // --- 5. データ長チェック（設定 DLC を超えていないか）---
    if (PduInfo->SduLength > txCfg->Dlc)
    {
        Serial.println("[CanIf_Transmit] ERROR: SduLength exceeds configured DLC");
        return E_NOT_OK;
    }

    // --- 6. CanDrv の言葉（Can_PduType）に変換して Can_Write を呼ぶ ---
    // swPduHandle に TxPduId を書き込む。
    // CanDrv はこの値を保持し、送信完了時に CanIf_TxConfirmation(swPduHandle) で返す。
    Can_PduType canPdu = {
        .swPduHandle = TxPduId,            // スタンプ：CanDrv が TxConfirmation で返す
        .id          = txCfg->CanId,       // テーブルから取得（上位層は知らない）
        .length      = (uint8)PduInfo->SduLength,
        .sdu         = PduInfo->SduDataPtr
    };

    Serial.print("[CanIf_Transmit] TxPduId=");
    Serial.print(TxPduId);
    Serial.print(" -> CanId=0x");
    Serial.print(txCfg->CanId, HEX);
    Serial.print(" HTH=");
    Serial.println(txCfg->Hth);

    Can_ReturnType ret = Can_Write(txCfg->Hth, &canPdu);

    // --- 7. 戻り値マッピング ---
    // CAN_OK     → E_OK（TxConfirmation は Can_Write 内から呼ばれる）
    // CAN_BUSY   → E_NOT_OK（TX バッファが全て使用中。上位層が再送すること）
    // CAN_NOT_OK → E_NOT_OK
    if (ret == CAN_BUSY)
    {
        Serial.println("[CanIf_Transmit] BUSY: TX buffer full");
    }

    return (ret == CAN_OK) ? E_OK : E_NOT_OK;
}

// -------------------------------------------------------
// CanIf_RxIndication
// CanDrv（Can_Isr / Can_MainFunction_Read）から呼ばれる受信コールバック。
// RX テーブルを HRH + CanId で検索し、一致した上位層コールバックを呼ぶ。
//
// AUTOSAR SWS_Can_00396 で CanDrv がこの関数を呼ぶことが規定されている。
// -------------------------------------------------------
void CanIf_RxIndication(Can_HwHandleType Hrh, Can_IdType CanId, uint8 Dlc, const uint8* Data)
{
    // --- 1. 初期化チェック ---
    if (CanIf_ConfigPtr == nullptr)
    {
        return;
    }

    // --- 2. RX テーブルを線形探索（HRH + CanId で一致するエントリを探す）---
    //    CanDrv のハードウェアフィルタをくぐり抜けたフレームに対して、
    //    さらにソフトウェアフィルタとして機能する。
    for (uint8 i = 0; i < CanIf_ConfigPtr->RxPduCount; i++)
    {
        const CanIf_RxPduConfigType* rxCfg = &CanIf_ConfigPtr->RxPduConfig[i];

        if (rxCfg->Hrh == Hrh && rxCfg->CanId == CanId)
        {
            Serial.print("[CanIf_RxIndication] CanId=0x");
            Serial.print(CanId, HEX);
            Serial.print(" -> RxPduId=");
            Serial.println(rxCfg->UpperLayerRxPduId);

            // --- 3. 上位層コールバックを呼ぶ ---
            if (rxCfg->RxIndicationFct != nullptr)
            {
                PduInfoType pduInfo = {
                    .SduDataPtr = (uint8*)Data, // const を外して渡す（上位層が読むだけ）
                    .SduLength  = (PduLengthType)Dlc
                };
                rxCfg->RxIndicationFct(rxCfg->UpperLayerRxPduId, &pduInfo);
            }
            return; // 一致エントリ発見 → 探索終了
        }
    }

    // 一致エントリなし → ソフトウェアフィルタで破棄（何もしない）
    Serial.print("[CanIf_RxIndication] no match for CanId=0x");
    Serial.println(CanId, HEX);
}

// -------------------------------------------------------
// CanIf_TxConfirmation
// CanDrv（Can_Write）から呼ばれる送信完了通知。
// swPduHandle（= TxPduId）をキーに TX テーブルを引き、
// 上位層の TxConfirmFct を呼ぶ。
//
// AUTOSAR SWS_CANIF_00011 相当
// -------------------------------------------------------
void CanIf_TxConfirmation(PduIdType TxPduId)
{
    // --- 1. 初期化チェック ---
    if (CanIf_ConfigPtr == nullptr)
    {
        return;
    }

    // --- 2. 範囲チェック ---
    if (TxPduId >= CanIf_ConfigPtr->TxPduCount)
    {
        return;
    }

    // --- 3. TX テーブルから上位層コールバックを取得して呼ぶ ---
    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[TxPduId];

    Serial.print("[CanIf_TxConfirmation] TxPduId=");
    Serial.println(TxPduId);

    if (txCfg->TxConfirmFct != nullptr)
    {
        txCfg->TxConfirmFct(txCfg->UpperLayerTxPduId);
    }
}
